// Copyright Wonder Interactive, Inc. All Rights Reserved.

#include "SimplyStreamBulkDataCookedIndex.h"

#include "CoreMinimal.h"
#include "Engine/Texture.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/ScopeLock.h"
#include "Serialization/ArchiveSavePackageDataBuffer.h"
#include "Serialization/ArchiveUObject.h"
#include "Serialization/BulkData.h"
#include "Serialization/BulkDataCookedIndex.h"
#include "Serialization/CustomVersion.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogSimplyStreamBulkDataCook, Log, All);

namespace UE::SimplyStream::BulkDataCookedIndex
{

// Keep separately stored bulk aggregates small enough for selective IAS reads and bounded
// first-use latency. An individual UE bulk payload is indivisible here and may exceed the
// packing target; the publish audit reports those assets and the physical IAS layer extents them.
constexpr uint64 TargetChunkSize = 32ull * 1024ull * 1024ull;
constexpr uint64 MaximumSafeChunkSize = uint64(MAX_uint32) - (64ull * 1024ull * 1024ull);

struct FAssignment
{
	FBulkDataCookedIndex CookedIndex;
	FString Error;

	bool IsOk() const
	{
		return Error.IsEmpty();
	}
};

class FChunkAllocator
{
public:
	explicit FChunkAllocator(
		uint64 InTargetChunkSize = TargetChunkSize,
		uint64 InMaximumSafeChunkSize = MaximumSafeChunkSize)
		: TargetSize(InTargetChunkSize)
		, MaximumSize(InMaximumSafeChunkSize)
	{
		check(TargetSize > 0);
		check(MaximumSize >= TargetSize);
		GroupSizes.Add(0);
	}

	FAssignment Assign(uint64 PayloadSize)
	{
		if (PayloadSize > MaximumSize)
		{
			return {
				FBulkDataCookedIndex::Default,
				FString::Printf(
					TEXT("one indivisible bulk payload is %.2f GiB; the IAS-safe maximum is %.2f GiB"),
					double(PayloadSize) / double(1ull << 30),
					double(MaximumSize) / double(1ull << 30))
			};
		}

		const bool bCurrentGroupWouldOverflow =
			GroupSizes[CurrentGroup] > 0
			&& (PayloadSize > TargetSize || GroupSizes[CurrentGroup] > TargetSize - PayloadSize);
		if (bCurrentGroupWouldOverflow)
		{
			if (CurrentGroup == TNumericLimits<FBulkDataCookedIndex::ValueType>::Max())
			{
				return {
					FBulkDataCookedIndex::Default,
					TEXT("the package needs more than 256 cooked bulk groups")
				};
			}
			++CurrentGroup;
			GroupSizes.Add(0);
		}

		GroupSizes[CurrentGroup] += PayloadSize;
		return {FBulkDataCookedIndex(FBulkDataCookedIndex::ValueType(CurrentGroup)), FString()};
	}

	int32 GetGroupCount() const
	{
		return GroupSizes.Num();
	}

	FString DescribeGroups() const
	{
		TArray<FString> Descriptions;
		Descriptions.Reserve(GroupSizes.Num());
		for (int32 Index = 0; Index < GroupSizes.Num(); ++Index)
		{
			Descriptions.Add(FString::Printf(
				TEXT("%03d=%.2fMiB"),
				Index,
				double(GroupSizes[Index]) / double(1ull << 20)));
		}
		return FString::Join(Descriptions, TEXT(", "));
	}

private:
	uint64 TargetSize;
	uint64 MaximumSize;
	TArray<uint64> GroupSizes;
	uint16 CurrentGroup = 0;
};

struct FPackageState
{
	FCriticalSection Mutex;
	TSet<const UObject*> ProcessedObjects;
	FChunkAllocator BulkSegments;
	FChunkAllocator OptionalSegments;
	uint64 UnindexableMemoryMappedBytes = 0;
	uint64 UnindexableDuplicateOptionalBytes = 0;
};

class FIndexingArchive final : public FArchiveUObject
{
public:
	FIndexingArchive(
		FPackageState& InState,
		UPackage* InPackage,
		const ITargetPlatform* InTargetPlatform)
		: State(InState)
		, SavePackageData(InTargetPlatform, InPackage)
		, PackageName(InPackage->GetName())
	{
		SetIsSaving(true);
		SetIsPersistent(true);
		SetPortFlags(UE::SavePackageUtilities::Private::GetSavePackagePortFlags());
		SetFilterEditorOnly(!InTargetPlatform->HasEditorOnlyData());
		SetCustomVersions(FCurrentCustomVersions::GetAll());
		SetSavePackageData(&SavePackageData);
	}

	virtual void Serialize(void*, int64 Length) override
	{
		if (Length < 0 || Position > MAX_int64 - Length)
		{
			SetError();
			return;
		}
		Position += Length;
		MaximumPosition = FMath::Max(MaximumPosition, Position);
	}

	virtual int64 Tell() override
	{
		return Position;
	}

	virtual int64 TotalSize() override
	{
		return MaximumPosition;
	}

	virtual void Seek(int64 InPosition) override
	{
		if (InPosition < 0)
		{
			SetError();
			return;
		}
		Position = InPosition;
		MaximumPosition = FMath::Max(MaximumPosition, Position);
	}

	virtual FString GetArchiveName() const override
	{
		return TEXT("SimplyStreamBulkDataCookedIndex");
	}

	virtual bool SerializeBulkData(
		FBulkData& BulkData,
		const FBulkDataSerializationParams&) override
	{
		const EBulkDataFlags Flags = EBulkDataFlags(BulkData.GetBulkDataFlags());
		const bool bStoredSeparately =
			FBulkData::HasFlags(Flags, BULKDATA_Force_NOT_InlinePayload)
			&& !FBulkData::HasFlags(Flags, BULKDATA_ForceInlinePayload);
		if (!bStoredSeparately)
		{
			return true;
		}

		const int64 SignedPayloadSize = BulkData.GetBulkDataSize();
		if (SignedPayloadSize < 0)
		{
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Fatal,
				TEXT("SimplyStream IAS bulk grouping found a negative payload size"));
			return true;
		}
		const uint64 PayloadSize = uint64(SignedPayloadSize);

		if (FBulkData::HasFlags(Flags, BULKDATA_MemoryMappedPayload))
		{
			State.UnindexableMemoryMappedBytes += PayloadSize;
			ValidateUnindexableAggregate(
				State.UnindexableMemoryMappedBytes,
				TEXT("memory-mapped bulk payloads"));
			return true;
		}

		FChunkAllocator& Allocator = FBulkData::HasFlags(Flags, BULKDATA_OptionalPayload)
			? State.OptionalSegments
			: State.BulkSegments;
		const FAssignment Assignment = Allocator.Assign(PayloadSize);
		if (!Assignment.IsOk())
		{
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Fatal,
				TEXT("SimplyStream IAS cannot segment package '%s': %s"),
				*PackageName,
				*Assignment.Error);
			return true;
		}
		BulkData.SetCookedIndex(Assignment.CookedIndex);

		if (FBulkData::HasFlags(Flags, BULKDATA_DuplicateNonOptionalPayload))
		{
			// UE 5.8 deliberately ignores CookedIndex for the duplicate optional copy. Track that
			// default optional chunk independently and fail before IasTool encounters an opaque limit.
			State.UnindexableDuplicateOptionalBytes += PayloadSize;
			ValidateUnindexableAggregate(
				State.UnindexableDuplicateOptionalBytes,
				TEXT("duplicate non-optional bulk payloads"));
		}

		// Returning true prevents this inspection archive from reading or copying the payload. The real
		// FLinkerSave serialization later consumes the CookedIndex assigned above.
		return true;
	}

private:
	void ValidateUnindexableAggregate(uint64 AggregateSize, const TCHAR* PayloadDescription)
	{
		if (AggregateSize > MaximumSafeChunkSize)
		{
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Fatal,
				TEXT("SimplyStream IAS cannot segment %s in this package because UE 5.8 ignores their CookedIndex; aggregate is %.2f GiB"),
				PayloadDescription,
				double(AggregateSize) / double(1ull << 30));
		}
	}

	FPackageState& State;
	FArchiveSavePackageDataBuffer SavePackageData;
	FString PackageName;
	int64 Position = 0;
	int64 MaximumPosition = 0;
};

class FManager
{
public:
	void Install()
	{
		if (!ObjectPreSaveHandle.IsValid())
		{
			ObjectPreSaveHandle = FCoreUObjectDelegates::OnObjectPreSave.AddRaw(
				this,
				&FManager::OnObjectPreSave);
			PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(
				this,
				&FManager::OnPackageSaved);
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Display,
				TEXT("[SimplyStream IAS] cooked bulk indexing installed"));
		}
	}

	void Uninstall()
	{
		FCoreUObjectDelegates::OnObjectPreSave.Remove(ObjectPreSaveHandle);
		UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
		ObjectPreSaveHandle.Reset();
		PackageSavedHandle.Reset();

		FScopeLock Lock(&PackagesMutex);
		Packages.Empty();
		ValidatedVirtualTextureTargets.Empty();
	}

private:
	static bool IsSimplyStreamCook(const FObjectPreSaveContext& Context)
	{
		const ITargetPlatform* TargetPlatform = Context.GetTargetPlatform();
		return Context.IsCooking()
			&& TargetPlatform
			&& TargetPlatform->IniPlatformName().Equals(TEXT("SimplyStream"), ESearchCase::IgnoreCase);
	}

	static FString MakeSaveKey(UPackage* Package, const TCHAR* TargetFilename)
	{
		if (TargetFilename && *TargetFilename)
		{
			return FString(TargetFilename);
		}
		return Package->GetPathName();
	}

	void ValidateVirtualTextureConfiguration(const ITargetPlatform* TargetPlatform)
	{
		const FString TargetName = TargetPlatform->IniPlatformName();
		{
			FScopeLock Lock(&PackagesMutex);
			if (ValidatedVirtualTextureTargets.Contains(TargetName))
			{
				return;
			}
			ValidatedVirtualTextureTargets.Add(TargetName);
		}

		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		const bool bConfigured = TargetPlatform->SupportsFeature(
			ETargetPlatformFeatures::VirtualTextureStreaming);
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
		const bool bEffective = UTexture::IsVirtualTexturingEnabled(TargetPlatform);
		UE_LOG(
			LogSimplyStreamBulkDataCook,
			Display,
			TEXT("[SimplyStream VT] target=%s configured=%s effective=%s"),
			*TargetName,
			bConfigured ? TEXT("true") : TEXT("false"),
			bEffective ? TEXT("true") : TEXT("false"));
		if (bConfigured != bEffective)
		{
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Fatal,
				TEXT("SimplyStream virtual-texture configuration mismatch for %s: configured=%s but the UE 5.8 texture cook path resolved effective=%s"),
				*TargetName,
				bConfigured ? TEXT("true") : TEXT("false"),
				bEffective ? TEXT("true") : TEXT("false"));
		}
	}

	void OnObjectPreSave(UObject* Object, FObjectPreSaveContext Context)
	{
		if (!Object || !IsSimplyStreamCook(Context))
		{
			return;
		}

		ValidateVirtualTextureConfiguration(Context.GetTargetPlatform());

		UPackage* Package = Object->GetOutermost();
		const FString SaveKey = MakeSaveKey(Package, Context.GetTargetFilename());
		TSharedRef<FPackageState> PackageState = GetOrCreatePackageState(SaveKey);

		FScopeLock StateLock(&PackageState->Mutex);
		if (PackageState->ProcessedObjects.Contains(Object))
		{
			return;
		}
		PackageState->ProcessedObjects.Add(Object);

		FIndexingArchive Archive(*PackageState, Package, Context.GetTargetPlatform());
		Object->Serialize(Archive);
		if (Archive.IsError())
		{
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Fatal,
				TEXT("SimplyStream IAS bulk inspection failed while serializing '%s'"),
				*Object->GetPathName());
		}
	}

	void OnPackageSaved(
		const FString& Filename,
		UPackage* Package,
		FObjectPostSaveContext Context)
	{
		if (!Package || !Context.IsCooking() || !Context.GetTargetPlatform()
			|| !Context.GetTargetPlatform()->IniPlatformName().Equals(
				TEXT("SimplyStream"),
				ESearchCase::IgnoreCase))
		{
			return;
		}

		TSharedPtr<FPackageState> PackageState;
		{
			FScopeLock Lock(&PackagesMutex);
			const FString SaveKey = MakeSaveKey(Package, *Filename);
			PackageState = Packages.FindRef(SaveKey);
			Packages.Remove(SaveKey);
		}
		if (!PackageState)
		{
			return;
		}

		FScopeLock StateLock(&PackageState->Mutex);
		if (PackageState->BulkSegments.GetGroupCount() > 1
			|| PackageState->OptionalSegments.GetGroupCount() > 1)
		{
			UE_LOG(
				LogSimplyStreamBulkDataCook,
				Display,
				TEXT("[SimplyStream IAS] segmented %s: bulk[%s], optional[%s]"),
				*Package->GetName(),
				*PackageState->BulkSegments.DescribeGroups(),
				*PackageState->OptionalSegments.DescribeGroups());
		}
	}

	TSharedRef<FPackageState> GetOrCreatePackageState(const FString& SaveKey)
	{
		FScopeLock Lock(&PackagesMutex);
		TSharedPtr<FPackageState>& State = Packages.FindOrAdd(SaveKey);
		if (!State)
		{
			State = MakeShared<FPackageState>();
		}
		return State.ToSharedRef();
	}

	FCriticalSection PackagesMutex;
	TMap<FString, TSharedPtr<FPackageState>> Packages;
	TSet<FString> ValidatedVirtualTextureTargets;
	FDelegateHandle ObjectPreSaveHandle;
	FDelegateHandle PackageSavedHandle;
};

TUniquePtr<FManager> Manager;

} // namespace UE::SimplyStream::BulkDataCookedIndex

void InstallSimplyStreamBulkDataCookedIndexing()
{
	using namespace UE::SimplyStream::BulkDataCookedIndex;
	if (!Manager)
	{
		Manager = MakeUnique<FManager>();
		Manager->Install();
	}
}

void UninstallSimplyStreamBulkDataCookedIndexing()
{
	using namespace UE::SimplyStream::BulkDataCookedIndex;
	if (Manager)
	{
		Manager->Uninstall();
		Manager.Reset();
	}
}

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FSimplyStreamBulkDataCookedIndexAllocatorTest,
	"SimplyStream.IAS.BulkDataCookedIndexAllocator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSimplyStreamBulkDataCookedIndexAllocatorTest::RunTest(const FString&)
{
	using namespace UE::SimplyStream::BulkDataCookedIndex;

	TestEqual(TEXT("production aggregate target is 32 MiB"), TargetChunkSize, 32ull * 1024ull * 1024ull);

	FChunkAllocator Allocator(10, 15);
	TestEqual(TEXT("first payload uses default group"), Allocator.Assign(6).CookedIndex.GetValue(), uint8(0));
	TestEqual(TEXT("payload fitting the target stays in default group"), Allocator.Assign(4).CookedIndex.GetValue(), uint8(0));
	TestEqual(TEXT("overflow starts group 1"), Allocator.Assign(1).CookedIndex.GetValue(), uint8(1));

	FChunkAllocator LargeSinglePayloadAllocator(10, 15);
	const FAssignment LargeSinglePayload = LargeSinglePayloadAllocator.Assign(12);
	TestTrue(TEXT("one payload may exceed the packing target while remaining IAS-safe"), LargeSinglePayload.IsOk());
	TestEqual(TEXT("large single payload uses an empty group"), LargeSinglePayload.CookedIndex.GetValue(), uint8(0));
	TestEqual(TEXT("the following payload advances to a new group"), LargeSinglePayloadAllocator.Assign(1).CookedIndex.GetValue(), uint8(1));
	TestFalse(TEXT("one payload beyond the IAS-safe maximum is rejected"), LargeSinglePayloadAllocator.Assign(16).IsOk());

	FChunkAllocator ExhaustionAllocator(1, 1);
	for (int32 Index = 0; Index < 256; ++Index)
	{
		const FAssignment Assignment = ExhaustionAllocator.Assign(1);
		TestTrue(FString::Printf(TEXT("group %d is available"), Index), Assignment.IsOk());
		TestEqual(
			FString::Printf(TEXT("payload %d uses its matching cooked group"), Index),
			Assignment.CookedIndex.GetValue(),
			uint8(Index));
	}
	TestFalse(TEXT("a 257th cooked group is rejected"), ExhaustionAllocator.Assign(1).IsOk());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
