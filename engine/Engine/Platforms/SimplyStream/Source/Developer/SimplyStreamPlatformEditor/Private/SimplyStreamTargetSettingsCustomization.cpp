// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SimplyStreamTargetSettingsCustomization.h"
#include "SimplyStreamTargetSettings.h"
#include "PropertyHandle.h"
#include "IDetailPropertyRow.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "Misc/EngineBuildSettings.h"

#include "DetailWidgetRow.h"
//#include "Widgets/DeclarativeSyntaxSupport.h"
//#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
//#include "Widgets/SWidget.h"
#include "Widgets/Images/SImage.h"
//#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
//#include "Widgets/Layout/SBox.h"
//#include "Widgets/Layout/SWidgetSwitcher.h"
//#include "Widgets/Notifications/SNotificationList.h"
//#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SimplyStreamTargetSettings"
DEFINE_LOG_CATEGORY_STATIC(LogIOSTargetSettings, Log, All);

//////////////////////////////////////////////////////////////////////////
// FSimplyStreamTargetSettingsCustomization
namespace FSimplyStreamTargetSettingsCustomizationConstants
{
	const FText DisabledTip = LOCTEXT("GitHubSourceRequiredToolTip", "This requires GitHub source.");
}



TSharedRef<IDetailCustomization> FSimplyStreamTargetSettingsCustomization::MakeInstance()
{
	return MakeShareable(new FSimplyStreamTargetSettingsCustomization);
}

FSimplyStreamTargetSettingsCustomization::FSimplyStreamTargetSettingsCustomization()
{
}

FSimplyStreamTargetSettingsCustomization::~FSimplyStreamTargetSettingsCustomization()
{
}

void FSimplyStreamTargetSettingsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	BuildEmscriptenCategory(DetailLayout);
	AudioPluginWidgetManager.BuildAudioCategory(DetailLayout, FString("SimplyStream"));
}

void FSimplyStreamTargetSettingsCustomization::BuildEmscriptenCategory(IDetailLayoutBuilder& DetailLayout)
{
	IDetailCategoryBuilder& EmscriptenCategory = DetailLayout.EditCategory(TEXT("Emscripten"));

#define SETUP_SOURCEONLY_PROP(PropName, Category) \
	{ \
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(USimplyStreamTargetSettings, PropName)); \
		Category.AddProperty(PropertyHandle) \
			.IsEnabled(FEngineBuildSettings::IsSourceDistribution()) \
			.ToolTip(FEngineBuildSettings::IsSourceDistribution() ? PropertyHandle->GetToolTipText() : FSimplyStreamTargetSettingsCustomizationConstants::DisabledTip); \
	}

}

void FSimplyStreamTargetSettingsCustomization::BuildDeprecationMessage(IDetailLayoutBuilder& DetailLayout)
{
}

//////////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
