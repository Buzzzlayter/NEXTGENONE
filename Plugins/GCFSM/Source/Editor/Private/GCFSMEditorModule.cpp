// Copyright (c) 2018 Gamecentric, All rights reserved

#include "GCFSMEditorModule.h"
#include "Interfaces/IPluginManager.h"
#include "ISettingsModule.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "PropertyEditorModule.h"
#include "GCFSMDetailCustomization.h"
#include "GCFSMEditorSettings.h"

#define LOCTEXT_NAMESPACE "GCFSM"

class FGCFSMEditorModule : public IGCFSMEditorModule
{
	/** IModuleInterface implementation */
	void StartupModule() override;
	void ShutdownModule() override;

	FSlateStyleSet styleSet{ "GCFSM" };
};

// General Log
DEFINE_LOG_CATEGORY(LogGCFSM);

IMPLEMENT_MODULE(FGCFSMEditorModule, GCFSMEditor)

void FGCFSMEditorModule::StartupModule()
{
	const FString contentDir = IPluginManager::Get().FindPlugin("GCFSM")->GetBaseDir() + "/Content";

	auto addIcon = [&](FSlateStyleSet& set, const char* name, const char* filename, FVector2D size = { 16.0f, 16.0f })
	{
		set.Set(name, new FSlateImageBrush(contentDir + filename, size));
	};

	addIcon(styleSet, "NodeIcon.FSMStart", "/GCFSM_StartNode.png");
	addIcon(styleSet, "NodeIcon.FSMBasicState", "/GCFSM_BasicStateNode.png");
	addIcon(styleSet, "NodeIcon.FSMLocalState", "/GCFSM_LocalStateNode.png");
	addIcon(styleSet, "NodeIcon.FSMSubmachineState", "/GCFSM_SubmachineStateNode.png");
	addIcon(styleSet, "NodeIcon.FSMState", "/GCFSM_StateNode.png");
	addIcon(styleSet, "NodeIcon.FSMStop", "/GCFSM_StopNode.png");
	addIcon(styleSet, "NodeIcon.LaunchFSM", "/GCFSM_LaunchFSM.png");
	addIcon(styleSet, "NodeIcon.TriggerEvent", "/GCFSM_TriggerEvent.png");
	addIcon(styleSet, "NodeIcon.InternalEvent", "/GCFSM_InternalEvent.png");
	addIcon(styleSet, "NodeIcon.IsFSMRunning", "/GCFSM_GetActiveState.png");
	addIcon(styleSet, "NodeIcon.GetActiveState", "/GCFSM_GetActiveState.png");
	addIcon(styleSet, "NodeIcon.FunctionWrapper", "/GCFSM_FunctionWrapper.png");

	addIcon(styleSet, "TargetPolicy.Context", "/GCFSM_TP_Context.png");
	addIcon(styleSet, "TargetPolicy.State", "/GCFSM_TP_State.png");
	addIcon(styleSet, "PropagatePolicy.Substates", "/GCFSM_StateNode.png");
	addIcon(styleSet, "PropagatePolicy.NoSubstates", "/GCFSM_PP_NoSubstates.png");
	addIcon(styleSet, "InternalEventPolicy.Allow", "/GCFSM_InternalEvent.png");
	addIcon(styleSet, "InternalEventPolicy.Disallow", "/GCFSM_IP_Disallow.png");
	addIcon(styleSet, "QueuePolicy.JustQueue", "/GCFSM_QP_JustQueue.png");
	addIcon(styleSet, "QueuePolicy.KeepFirst", "/GCFSM_QP_KeepFirst.png");
	addIcon(styleSet, "QueuePolicy.KeepLast", "/GCFSM_QP_KeepLast.png");
	addIcon(styleSet, "ExpirePolicy.NoExpiration", "/GCFSM_EP_NoExpiration.png");
	addIcon(styleSet, "ExpirePolicy.WithExpiration", "/GCFSM_EP_WithExpiration.png");
	addIcon(styleSet, "Policy.ManagedByPin", "/GCFSM_ManagedByPin.png");

	FSlateStyleRegistry::RegisterSlateStyle(styleSet);

	// Behaviour Tree node icons have to be added in the EditorStyle set (why did they hardcode that and make it that difficult???)
	if (auto editorStyle = FSlateStyleRegistry::FindSlateStyle("EditorStyle"))
	{
		FSlateStyleSet& editorStyleSet = *static_cast<FSlateStyleSet*>(const_cast<ISlateStyle*>(editorStyle));
		addIcon(editorStyleSet, "GCFSM.BTTaskIcon.LaunchFSM", "/GCFSM_LaunchFSM.png");
		addIcon(editorStyleSet, "GCFSM.BTTaskIcon.TriggerEvent", "/GCFSM_TriggerEvent.png");
	}

	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout("GCFSMTriggerEventNode", FOnGetDetailCustomizationInstance::CreateStatic(&IGCFSMDetailCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("GCFSMStateNode", FOnGetDetailCustomizationInstance::CreateStatic(&IGCFSMDetailCustomization::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("GCFSMLaunchNode", FOnGetDetailCustomizationInstance::CreateStatic(&IGCFSMDetailCustomization::MakeInstance));
	}

	// Register settings
	if (auto settingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		settingsModule->RegisterSettings("Project", "Plugins", "GCFSM",
			LOCTEXT("RuntimeSettingsName", "GC FSM"),
			LOCTEXT("RuntimeSettingsDescription", "Configure the GC FSM plugin"),
			GetMutableDefault<UGCFSMEditorSettings>()
		);
	}
}

void FGCFSMEditorModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(styleSet);

	if (UObjectInitialized())
	{
		if (auto settingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			settingsModule->UnregisterSettings("Project", "Plugins", "GCFSM");
		}
	}
}

FText GCFSMMenuCategory()
{
	return LOCTEXT("Nodes.MenuCategory", "GC FSM");
}

#undef LOCTEXT_NAMESPACE