// Copyright Epic Games, Inc. All Rights Reserved.
//
// wndr IAS benchmark (Phase 2) — per-package repartitioner.
//
// Reads a cooked on-demand container (.utoc/.ucas) and re-emits it as a per-PACKAGE on-demand layout: every chunk's
// owning package id is the first 8 bytes of its FIoChunkId (the same fact ConvertToc.cpp:27-37 GetPackageChunkId
// relies on), so we sort chunks by (packageId, chunkId) and start a new partition whenever the package id changes
// (or a max-size cap is exceeded, to bound pathologically large packages). Each partition's bytes are the concatenated
// encoded-chunk bytes — content-hashed, no per-partition header — written to <Out>/chunks/<2hex>/<hash>.iopart, exactly
// the flat content-addressed namespace the SimplyStream dispatcher fetches (it composes <host>/chunks/<2hex>/<hash>
// from the partition hash). The .uondemandtoc is emitted via the SAME FOnDemandTocWriter the engine's IasTool uses, so
// the output is a valid on-demand TOC that mounts + streams identically — only the partition grouping differs.
//
// This is the 3rd layout in the head-to-head benchmark:
//   per-chunk : IasTool Upload -MaxPartitionSize=0     (1 partition / chunk — maximal dedup, most requests)
//   grouped   : IasTool Upload  (default 2 MiB)        (container-order coalesced — fewest requests, ZERO x-project dedup)
//   per-package: THIS tool                              (package-grain partitions — the balanced middle; package-grain
//                                                         cross-project dedup: a byte-identical shared package => identical
//                                                         partition hash => one shared .iopart)
//
// Standalone host Program in the SimplyStream platform tree (no engine-source edit). Mac dependency closure is shared
// with IasTool, so it builds incrementally.
//
//   WndrRepartition -Container=/path/Foo.utoc -Out=/tmp/ias_perpackage [-MaxPartitionSize=67108864] [-Host=https://cdn…]

#include <CoreGlobals.h>
#include <HAL/LowLevelMemTracker.h>
#include <LaunchEngineLoop.h>
#include <Modules/ModuleManager.h>
#include <Misc/ScopeExit.h>

#include <RequiredProgramMainCPPInclude.h>

#include "Algo/Sort.h"
#include "Containers/Set.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "IO/IoChunkEncoding.h"
#include "IO/IoChunkId.h"
#include "IO/IoDispatcher.h" // FIoStoreReader, FIoStoreTocChunkInfo, FIoStoreCompressedReadResult
#include "IO/IoHash.h"
#include "IO/Serialization/OnDemandContainerToc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/StringBuilder.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

IMPLEMENT_APPLICATION(WndrRepartition, "WndrRepartition");

DEFINE_LOG_CATEGORY_STATIC(LogWndrRepartition, Log, All);

namespace UE::SimplyStream
{
using namespace UE::IoStore;
using namespace UE::IoStore::Serialization;

constexpr int32 CompleteChunkReportLimit = 5000;

// The package id is the first 8 bytes of every FIoChunkId (export-bundle/bulk carry the real package id; shader/header
// chunks carry their own deterministic 8-byte prefix => they simply group by that, i.e. effectively per-chunk for the
// uniquely-keyed ones — still deterministic and valid).
static uint64 PackageKeyOf(const FIoChunkId& Id)
{
	uint64 Key = 0;
	FMemory::Memcpy(&Key, Id.GetData(), sizeof(uint64));
	return Key;
}

// Order chunks so equal package ids are adjacent (primary), with a stable per-id tiebreak (secondary) so a given
// package always serialises its chunks in the same order => the partition bytes (hence the content hash) are
// reproducible across runs and across projects (the cross-project dedup property).
static bool ChunkLess(const FIoStoreTocChunkInfo& A, const FIoStoreTocChunkInfo& B)
{
	const uint64 Ka = PackageKeyOf(A.Id), Kb = PackageKeyOf(B.Id);
	if (Ka != Kb)
	{
		return Ka < Kb;
	}
	return FMemory::Memcmp(A.Id.GetData(), B.Id.GetData(), A.Id.GetSize()) < 0;
}

static TSharedRef<FJsonObject> AuditChunkToJson(const FIoStoreTocChunkInfo& Info)
{
	TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("id"), LexToString(Info.Id));
	Json->SetStringField(TEXT("type"), LexToString(Info.ChunkType));
	Json->SetStringField(TEXT("file"), Info.FileName);
	Json->SetNumberField(TEXT("rawBytes"), double(Info.Size));
	Json->SetNumberField(TEXT("diskBytes"), double(Info.SizeOnDisk));
	Json->SetNumberField(TEXT("compressedBytes"), double(Info.CompressedSize));
	return Json;
}

static int32 AuditContainer(
	const FString& ContainerArg,
	const FString& ReportPath,
	int32 MaxPartitionSize,
	int32 MaxExtentSize,
	const TArray<FIoStoreTocChunkInfo>& Chunks)
{
	TArray<const FIoStoreTocChunkInfo*> InContainerOrder;
	TArray<const FIoStoreTocChunkInfo*> BySize;
	InContainerOrder.Reserve(Chunks.Num());
	BySize.Reserve(Chunks.Num());
	for (const FIoStoreTocChunkInfo& Info : Chunks)
	{
		InContainerOrder.Add(&Info);
		BySize.Add(&Info);
	}
	InContainerOrder.Sort([](const FIoStoreTocChunkInfo& A, const FIoStoreTocChunkInfo& B)
	{
		return A.Offset < B.Offset;
	});

	uint64 TotalRawBytes = 0;
	uint64 TotalDiskBytes = 0;
	uint64 CurrentPartitionBytes = 0;
	uint64 PlannedPartitionBytes = 0;
	uint64 UnsafeRawBytes = 0;
	uint64 UnsafeDiskBytes = 0;
	uint64 PlannedPartitions = 0;
	uint64 OversizedLogicalChunks = 0;
	uint64 OversizedLogicalDiskBytes = 0;
	uint64 MaxLogicalDiskBytes = 0;
	const FIoStoreTocChunkInfo* LargestUnsafe = nullptr;
	TArray<TSharedPtr<FJsonValue>> UnsafeChunks;
	TArray<uint64> PlannedPartitionSizes;

	for (const FIoStoreTocChunkInfo* InfoPtr : InContainerOrder)
	{
		const FIoStoreTocChunkInfo& Info = *InfoPtr;
		TotalRawBytes += Info.Size;
		TotalDiskBytes += Info.SizeOnDisk;
		MaxLogicalDiskBytes = FMath::Max(MaxLogicalDiskBytes, Info.SizeOnDisk);
		if (Info.SizeOnDisk > uint64(MaxPartitionSize))
		{
			++OversizedLogicalChunks;
			OversizedLogicalDiskBytes += Info.SizeOnDisk;
		}

		const bool bUnsafe = Info.Size > MAX_uint32
			|| Info.SizeOnDisk > MAX_uint32
			|| Info.CompressedSize > MAX_uint32;
		if (bUnsafe)
		{
			UnsafeRawBytes += Info.Size;
			UnsafeDiskBytes += Info.SizeOnDisk;
			UnsafeChunks.Add(MakeShared<FJsonValueObject>(AuditChunkToJson(Info)));
			if (!LargestUnsafe || Info.Size > LargestUnsafe->Size)
			{
				LargestUnsafe = &Info;
			}
		}

		const uint64 EncodedBytes = Info.SizeOnDisk;
		if (CurrentPartitionBytes > 0
			&& CurrentPartitionBytes + EncodedBytes >= uint64(MaxPartitionSize))
		{
			++PlannedPartitions;
			PlannedPartitionBytes += CurrentPartitionBytes;
			PlannedPartitionSizes.Add(CurrentPartitionBytes);
			CurrentPartitionBytes = 0;
		}
		CurrentPartitionBytes += EncodedBytes;
	}
	if (CurrentPartitionBytes > 0)
	{
		++PlannedPartitions;
		PlannedPartitionBytes += CurrentPartitionBytes;
		PlannedPartitionSizes.Add(CurrentPartitionBytes);
	}

	uint64 MaxPlannedPartitionBytes = 0;
	uint64 OversizedPlannedPartitions = 0;
	uint64 HardExtentCount = 0;
	uint64 HardDescriptorBytes = 0;
	for (uint64 PartitionBytes : PlannedPartitionSizes)
	{
		MaxPlannedPartitionBytes = FMath::Max(MaxPlannedPartitionBytes, PartitionBytes);
		if (PartitionBytes > uint64(MaxPartitionSize))
		{
			++OversizedPlannedPartitions;
		}
		const uint64 ExtentCount = FMath::DivideAndRoundUp(PartitionBytes, uint64(MaxExtentSize));
		HardExtentCount += ExtentCount;
		const FString Descriptor = FString::Printf(
			TEXT("{\"version\":1,\"size\":%llu,\"extentBytes\":%d,\"extents\":%llu}\n"),
			PartitionBytes,
			MaxExtentSize,
			ExtentCount);
		HardDescriptorBytes += FTCHARToUTF8(*Descriptor).Length();
	}

	BySize.Sort([](const FIoStoreTocChunkInfo& A, const FIoStoreTocChunkInfo& B)
	{
		return A.Size > B.Size;
	});

	TArray<TSharedPtr<FJsonValue>> LargestChunks;
	const int32 LargestCount = FMath::Min(50, BySize.Num());
	LargestChunks.Reserve(LargestCount);
	for (int32 Index = 0; Index < LargestCount; ++Index)
	{
		LargestChunks.Add(MakeShared<FJsonValueObject>(AuditChunkToJson(*BySize[Index])));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schema"), 1);
	Root->SetStringField(TEXT("container"), ContainerArg);
	Root->SetNumberField(TEXT("logicalChunks"), Chunks.Num());
	Root->SetNumberField(TEXT("totalRawBytes"), double(TotalRawBytes));
	Root->SetNumberField(TEXT("totalDiskBytes"), double(TotalDiskBytes));
	Root->SetNumberField(TEXT("maxLogicalChunkBytes"), double(MAX_uint32));
	Root->SetNumberField(TEXT("partitionSizeBytes"), MaxPartitionSize);
	Root->SetNumberField(TEXT("extentSizeBytes"), MaxExtentSize);
	Root->SetNumberField(TEXT("plannedPartitions"), double(PlannedPartitions));
	Root->SetNumberField(TEXT("plannedPartitionBytes"), double(PlannedPartitionBytes));
	Root->SetNumberField(TEXT("maxPlannedPartitionBytes"), double(MaxPlannedPartitionBytes));
	Root->SetNumberField(TEXT("oversizedPlannedPartitions"), double(OversizedPlannedPartitions));
	Root->SetNumberField(TEXT("hardExtentCount"), double(HardExtentCount));
	Root->SetNumberField(TEXT("hardObjectCount"), double(HardExtentCount + PlannedPartitions));
	Root->SetNumberField(TEXT("hardPhysicalBytes"), double(PlannedPartitionBytes + HardDescriptorBytes));
	Root->SetNumberField(TEXT("oversizedLogicalChunks"), double(OversizedLogicalChunks));
	Root->SetNumberField(TEXT("oversizedLogicalDiskBytes"), double(OversizedLogicalDiskBytes));
	Root->SetNumberField(TEXT("maxLogicalDiskBytes"), double(MaxLogicalDiskBytes));
	Root->SetNumberField(TEXT("unsafeRawBytes"), double(UnsafeRawBytes));
	Root->SetNumberField(TEXT("unsafeDiskBytes"), double(UnsafeDiskBytes));
	const int32 UnsafeChunkCount = UnsafeChunks.Num();
	Root->SetArrayField(TEXT("unsafeChunks"), MoveTemp(UnsafeChunks));
	Root->SetArrayField(TEXT("largestChunks"), MoveTemp(LargestChunks));
	const bool bCompleteChunkReport = Chunks.Num() <= CompleteChunkReportLimit;
	Root->SetBoolField(TEXT("allChunksIncluded"), bCompleteChunkReport);
	if (bCompleteChunkReport)
	{
		TArray<TSharedPtr<FJsonValue>> AllChunks;
		AllChunks.Reserve(InContainerOrder.Num());
		for (const FIoStoreTocChunkInfo* Info : InContainerOrder)
		{
			AllChunks.Add(MakeShared<FJsonValueObject>(AuditChunkToJson(*Info)));
		}
		Root->SetArrayField(TEXT("chunks"), MoveTemp(AllChunks));
	}

	FString JsonText;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonText);
	if (!FJsonSerializer::Serialize(Root, Writer)
		|| !FFileHelper::SaveStringToFile(JsonText, *ReportPath))
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Failed to write IAS audit report '%s'"), *ReportPath);
		return -1;
	}

	UE_LOG(
		LogWndrRepartition,
		Display,
		TEXT("[wndr-ias-audit] chunks=%d rawGiB=%.2f diskGiB=%.2f plannedPartitions=%llu oversizedLogical=%llu oversizedPartitions=%llu hardExtents=%llu maxPartitionMiB=%.2f unsafe=%d report='%s'"),
		Chunks.Num(),
		double(TotalRawBytes) / double(1ull << 30),
		double(TotalDiskBytes) / double(1ull << 30),
		PlannedPartitions,
		OversizedLogicalChunks,
		OversizedPlannedPartitions,
		HardExtentCount,
		double(MaxPlannedPartitionBytes) / double(1ull << 20),
		UnsafeChunkCount,
		*ReportPath);

	const int32 OversizedLogCount = FMath::Min(10, BySize.Num());
	for (int32 Index = 0; Index < OversizedLogCount; ++Index)
	{
		const FIoStoreTocChunkInfo& Info = *BySize[Index];
		if (Info.SizeOnDisk <= uint64(MaxPartitionSize))
		{
			break;
		}
		UE_LOG(
			LogWndrRepartition,
			Warning,
			TEXT("[wndr-ias-audit] oversized #%d diskMiB=%.2f rawMiB=%.2f extents=%llu file='%s' id=%s"),
			Index + 1,
			double(Info.SizeOnDisk) / double(1ull << 20),
			double(Info.Size) / double(1ull << 20),
			FMath::DivideAndRoundUp(Info.SizeOnDisk, uint64(MaxExtentSize)),
			*Info.FileName,
			*LexToString(Info.Id));
	}

	if (UnsafeChunkCount > 0)
	{
		check(LargestUnsafe);
		UE_LOG(
			LogWndrRepartition,
			Error,
			TEXT("[wndr-ias-audit] unsafe logical chunk: '%s' raw=%.2fGiB disk=%.2fGiB id=%s"),
			*LargestUnsafe->FileName,
			double(LargestUnsafe->Size) / double(1ull << 30),
			double(LargestUnsafe->SizeOnDisk) / double(1ull << 30),
			*LexToString(LargestUnsafe->Id));
		return 2;
	}
	return 0;
}

static bool WriteIoPart(const FString& OutDir, const FIoHash& Hash, const FIoBuffer& Buffer, TSet<FIoHash>& Written, uint64& OutDedupBytes)
{
	if (Written.Contains(Hash))
	{
		OutDedupBytes += Buffer.GetSize(); // an identical partition already emitted (intra-container dedup)
		return true;
	}
	Written.Add(Hash);

	const FString HashStr = LexToString(Hash);
	IFileManager& Ifm = IFileManager::Get();
	const FString Dir = OutDir / TEXT("chunks") / HashStr.Left(2);
	Ifm.MakeDirectory(*Dir, /*Tree*/ true);

	const FString Path = Dir / (HashStr + TEXT(".iopart"));
	TUniquePtr<FArchive> Ar(Ifm.CreateFileWriter(*Path));
	if (!Ar)
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Failed to open '%s' for writing"), *Path);
		return false;
	}
	Ar->Serialize(const_cast<uint8*>(Buffer.GetData()), int64(Buffer.GetSize()));
	return Ar->Close();
}

static int32 RepartitionMain()
{
	GPrintLogTimes = ELogTimes::None;

	FString ContainerArg, OutDir;
	const bool bAuditOnly = FParse::Param(FCommandLine::Get(), TEXT("AuditOnly"));
	if (!FParse::Value(FCommandLine::Get(), TEXT("Container="), ContainerArg) || ContainerArg.IsEmpty())
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Missing -Container=<path to .utoc>"));
		return -1;
	}
	if (!bAuditOnly
		&& (!FParse::Value(FCommandLine::Get(), TEXT("Out="), OutDir) || OutDir.IsEmpty()))
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Missing -Out=<output directory>"));
		return -1;
	}

	int32 MaxPartitionSize = bAuditOnly
		? 2 * 1024 * 1024
		: 64 * 1024 * 1024; // bound huge packages; packages are normally far below this
	FParse::Value(FCommandLine::Get(), TEXT("MaxPartitionSize="), MaxPartitionSize);
	if (MaxPartitionSize <= 0)
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("MaxPartitionSize must be greater than zero"));
		return -1;
	}
	int32 MaxExtentSize = 4 * 1024 * 1024;
	FParse::Value(FCommandLine::Get(), TEXT("MaxExtentSize="), MaxExtentSize);
	if (MaxExtentSize <= 0 || MaxExtentSize > 4 * 1024 * 1024)
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("MaxExtentSize must be between 1 and 4194304 bytes"));
		return -1;
	}

	FString Host, TargetPlatform = TEXT("SimplyStream"), BuildVersion = TEXT("wndr-perpackage");
	FParse::Value(FCommandLine::Get(), TEXT("Host="), Host);
	FParse::Value(FCommandLine::Get(), TEXT("TargetPlatform="), TargetPlatform);
	FParse::Value(FCommandLine::Get(), TEXT("BuildVersion="), BuildVersion);

	const FString BasePath      = FPaths::ChangeExtension(ContainerArg, TEXT(""));
	const FString ContainerName = FPaths::GetBaseFilename(ContainerArg);

	// 1) Open the cooked container.
	FIoStoreReader Reader;
	const TMap<FGuid, FAES::FAESKey> NoKeys;
	if (FIoStatus Status = Reader.Initialize(BasePath, NoKeys); !Status.IsOk())
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Failed to open container '%s': %s"), *BasePath, *Status.ToString());
		return -1;
	}

	// 2) Enumerate chunks + hash the .utoc (the on-demand TOC records this to detect a stale source).
	TArray<FIoStoreTocChunkInfo> Chunks;
	Reader.EnumerateChunks([&Chunks](FIoStoreTocChunkInfo&& Info) { Chunks.Emplace(MoveTemp(Info)); return true; });
	if (Chunks.IsEmpty())
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Container '%s' has no chunks"), *BasePath);
		return -1;
	}
	if (bAuditOnly)
	{
		FString ReportPath;
		if (!FParse::Value(FCommandLine::Get(), TEXT("Report="), ReportPath) || ReportPath.IsEmpty())
		{
			UE_LOG(LogWndrRepartition, Error, TEXT("AuditOnly requires -Report=<output json>"));
			return -1;
		}
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), /*Tree*/ true);
		return AuditContainer(ContainerArg, ReportPath, MaxPartitionSize, MaxExtentSize, Chunks);
	}

	FIoHash UTocHash;
	{
		TArray<uint8> UTocBytes;
		const FString UTocPath = FPaths::ChangeExtension(ContainerArg, TEXT(".utoc"));
		if (!FFileHelper::LoadFileToArray(UTocBytes, *UTocPath))
		{
			UE_LOG(LogWndrRepartition, Error, TEXT("Failed to read '%s'"), *UTocPath);
			return -1;
		}
		UTocHash = FIoHash::HashBuffer(UTocBytes.GetData(), UTocBytes.Num());
	}

	// 3) Read the container header chunk (carries the package store; embedded into the .uondemandtoc by BeginContainer).
	FIoBuffer Header;
	{
		const FIoChunkId HeaderId = CreateContainerHeaderChunkId(Reader.GetContainerId());
		if (TIoStatusOr<FIoStoreCompressedReadResult> R = Reader.ReadCompressed(HeaderId, FIoReadOptions(), /*bDecrypt*/ false); R.IsOk())
		{
			Header = R.ConsumeValueOrDie().IoBuffer;
		}
	}

	// 4) Group chunks by package id.
	Algo::Sort(Chunks, ChunkLess);

	FOnDemandTocWriter Writer;
	// Mark the container BOTH StreamOnDemand AND InstallOnDemand: streaming serves per-access reads (the normal path),
	// and InstallOnDemand lets the engine's content installer (IOnDemandIoStore::Install + TagSets) PREFETCH a tagged
	// boot set into the install CAS — UE's built-in "single prefetch", which we use instead of a bespoke boot partition.
	// The flags live in the TOC header only; partition bytes/hashes are unchanged, so already-uploaded chunks stay valid.
	Writer.BeginContainer(
		ContainerName,
		Reader.GetContainerId(),
		UTocHash,
		Reader.GetEncryptionKeyGuid(),
		Reader.GetCompressionBlockSize(),
		uint32(Reader.GetContainerFlags()),
		EOnDemandContainerEntryFlags::StreamOnDemand | EOnDemandContainerEntryFlags::InstallOnDemand,
		Header);

	FName    ContainerCompressionFormat = NAME_None;
	int32    Partition     = INDEX_NONE;
	uint64   PartitionSize = 0;
	uint64   CurrentKey    = 0;
	bool     bHaveKey      = false;

	TSet<FIoHash> WrittenPartitions;
	uint64 NumPartitions = 0, NumChunks = 0, TotalEncoded = 0, TotalPartitionBytes = 0, DedupBytes = 0, NumPackages = 0;

	auto FlushPartition = [&]() -> bool
	{
		FIoHash   Hash;
		FIoBuffer Chunk = Writer.EndPartition(Partition, Hash);
		Partition     = INDEX_NONE;
		PartitionSize = 0;
		++NumPartitions;
		TotalPartitionBytes += Chunk.GetSize();
		return WriteIoPart(OutDir, Hash, Chunk, WrittenPartitions, DedupBytes);
	};

	for (const FIoStoreTocChunkInfo& Info : Chunks)
	{
		const uint64 Key = PackageKeyOf(Info.Id);

		TIoStatusOr<FIoStoreCompressedReadResult> ReadStatus = Reader.ReadCompressed(Info.Id, FIoReadOptions(), /*bDecrypt*/ false);
		if (!ReadStatus.IsOk())
		{
			UE_LOG(LogWndrRepartition, Error, TEXT("Failed to read chunk '%s': %s"), *LexToString(Info.Id), *ReadStatus.Status().ToString());
			return -1;
		}

		FIoStoreCompressedReadResult ReadResult = ReadStatus.ConsumeValueOrDie();
		FMemoryView          EncodedBlocks    = ReadResult.IoBuffer.GetView();
		TArray<uint32>       BlockSizes;
		TArray<FIoBlockHash> BlockHashes;
		uint64               RawChunkSize     = 0;
		uint64               EncodedChunkSize = 0;
		FName                CompressionFormat = NAME_None;

		for (const FIoStoreCompressedBlockInfo& BlockInfo : ReadResult.Blocks)
		{
			const uint64 EncodedBlockSize = BlockInfo.AlignedSize;
			BlockSizes.Add(uint32(BlockInfo.CompressedSize));

			FMemoryView EncodedBlock = EncodedBlocks.Left(EncodedBlockSize);
			EncodedBlocks += EncodedBlock.GetSize();
			BlockHashes.Add(FIoChunkEncoding::HashBlock(EncodedBlock));

			EncodedChunkSize += EncodedBlockSize;
			RawChunkSize     += BlockInfo.UncompressedSize;
			if (CompressionFormat.IsNone() && BlockInfo.CompressionMethod != NAME_None)
			{
				CompressionFormat = BlockInfo.CompressionMethod;
			}
		}
		if (ContainerCompressionFormat.IsNone())
		{
			ContainerCompressionFormat = CompressionFormat;
		}

		// Partition boundary: the package id changed, OR the cap would be exceeded (split a huge package at a chunk
		// boundary, never mid-chunk — same rule as IasTool's size-only path, Upload.cpp:646).
		const bool bKeyChanged = bHaveKey && Key != CurrentKey;
		const bool bSizeCap    = (PartitionSize > 0 && (PartitionSize + EncodedChunkSize >= uint64(MaxPartitionSize)));
		if (Partition != INDEX_NONE && (bKeyChanged || bSizeCap))
		{
			if (!FlushPartition()) { return -1; }
		}
		if (bKeyChanged || !bHaveKey)
		{
			++NumPackages;
		}

		if (Partition == INDEX_NONE)
		{
			Partition = Writer.BeginPartition();
			check(Partition >= 0);
		}

		PartitionSize = Writer.AddChunk(Partition, Info.Id, ReadResult.IoBuffer, BlockSizes, BlockHashes, RawChunkSize);
		CurrentKey    = Key;
		bHaveKey      = true;
		++NumChunks;
		TotalEncoded += EncodedChunkSize;
	}

	if (Partition != INDEX_NONE)
	{
		if (!FlushPartition()) { return -1; }
	}

	Writer.EndContainer(ContainerCompressionFormat);
	Writer.SetMetadata(BuildVersion, TargetPlatform);
	Writer.SetChunksDirectory(TEXT("chunks"));
	if (!Host.IsEmpty())
	{
		Writer.SetHostGroup(Host);
	}

	const FString TocPath = OutDir / (ContainerName + TEXT(".uondemandtoc"));
	if (TIoStatusOr<uint64> Status = Writer.Write(TocPath); !Status.IsOk())
	{
		UE_LOG(LogWndrRepartition, Error, TEXT("Failed to write TOC '%s': %s"), *TocPath, *Status.Status().ToString());
		return -1;
	}

	UE_LOG(LogWndrRepartition, Display, TEXT("[wndr-repartition] '%s' -> '%s'"), *ContainerName, *TocPath);
	UE_LOG(LogWndrRepartition, Display,
		TEXT("[wndr-repartition] packages=%llu chunks=%llu partitions=%llu uniquePartitions=%d encodedMiB=%.1f partitionMiB=%.1f dedupMiB=%.1f"),
		NumPackages, NumChunks, NumPartitions, WrittenPartitions.Num(),
		double(TotalEncoded) / 1048576.0, double(TotalPartitionBytes) / 1048576.0, double(DedupBytes) / 1048576.0);

	return 0;
}

} // namespace UE::SimplyStream

INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	if (int Ret = GEngineLoop.PreInit(ArgC, ArgV, TEXT("-stdout")); Ret != 0)
	{
		return Ret;
	}

	ON_SCOPE_EXIT
	{
		LLM(FLowLevelMemTracker::Get().UpdateStatsPerFrame());
		RequestEngineExit(TEXT("Exiting"));
		FEngineLoop::AppPreExit();
		FModuleManager::Get().UnloadModulesAtShutdown();
		FEngineLoop::AppExit();
	};

	return UE::SimplyStream::RepartitionMain();
}
