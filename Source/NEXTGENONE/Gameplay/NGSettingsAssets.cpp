// Offline asset migration only; no gameplay world or PIE is started.
#if WITH_EDITOR
#include "NGSettingsPanel.h"
#include "NGDebugState.h"
#include "NGDebugSettingsPanel.h"
#include "NextgenonePlayerController.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "HFSMState.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Engine/Blueprint.h"
#include "Engine/Engine.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"

namespace
{
bool Backup(UObject* Asset)
{
	const FString File=FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());
	if (!IFileManager::Get().FileExists(*File)) return true;
	const FString Dir=FPaths::ProjectSavedDir()/TEXT("RefactorBackup/Settings"); IFileManager::Get().MakeDirectory(*Dir,true);
	return IFileManager::Get().Copy(*(Dir/(Asset->GetName()+TEXT("_")+FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S"))+TEXT(".uasset"))),*File,false)==COPY_OK;
}
bool SaveBP(UBlueprint* BP)
{
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status==BS_Error) return false;
	FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
	return UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
}
UBlueprint* LoadBP(const TCHAR* Path) { return LoadObject<UBlueprint>(nullptr,Path); }
bool AddStateComponent(UBlueprint* BP,UClass* Type,const TCHAR* Name)
{
	auto* State=Cast<UHFSMState>(BP->GeneratedClass->GetDefaultObject()); auto* P=FindFProperty<FArrayProperty>(UHFSMState::StaticClass(),TEXT("Components"));
	if (!State || !P) return false; auto* Inner=CastField<FObjectPropertyBase>(P->Inner); if (!Inner) return false;
	FScriptArrayHelper List(P,P->ContainerPtrToValuePtr<void>(State));
	for (int32 I=0;I<List.Num();++I) if (auto* C=Inner->GetObjectPropertyValue(List.GetRawPtr(I))) if (C->IsA(Type)) return true;
	Inner->SetObjectPropertyValue(List.GetRawPtr(List.AddValue()),NewObject<UObject>(State,Type,Name,RF_Transactional)); return true;
}
bool MigrateDebug()
{
	auto* Copy=LoadBP(TEXT("/Game/Gameplay/UI/BP_NGDebugComponent.BP_NGDebugComponent"));
	if (!Copy)
	{
		auto* Original=LoadBP(TEXT("/ALSV4_CPP/AdvancedLocomotionV4/Blueprints/Components/DebugComponent.DebugComponent")); if (!Original) return false;
		Copy=Cast<UBlueprint>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("BP_NGDebugComponent"),TEXT("/Game/Gameplay/UI"),Original));
		if (!Copy) return false; Copy->ParentClass=UNGDebugComponent::StaticClass();
		TArray<UEdGraph*> Graphs; Copy->GetAllGraphs(Graphs);
		for (auto* Graph:Graphs) for (int32 I=Graph->Nodes.Num()-1;I>=0;--I) if (auto* Event=Cast<UK2Node_Event>(Graph->Nodes[I]))
		{
			const auto Name=Event->EventReference.GetMemberName();
			if (Name==TEXT("OnPlayerControllerInitialized")) Event->BreakAllNodeLinks();
			if (Name==TEXT("OpenOverlayMenu") || Name==TEXT("OverlayMenuCycle")) FBlueprintEditorUtils::RemoveNode(Copy,Event,true);
		}
		FBlueprintEditorUtils::RefreshAllNodes(Copy); if (!SaveBP(Copy)) return false;
	}
	if (!Copy->GeneratedClass->IsChildOf(UNGDebugComponent::StaticClass())) return false;
	auto* Character=LoadBP(TEXT("/Game/Blueprints/BP_NextgenoneCharacter.BP_NextgenoneCharacter")); if (!Character || !Character->SimpleConstructionScript || !Backup(Character)) return false;
	for (auto* Node:Character->SimpleConstructionScript->GetAllNodes())
	{
		if (!Node->ComponentClass || !Node->ComponentClass->IsChildOf(UALSDebugComponent::StaticClass())) continue;
		if (Node->ComponentClass==Copy->GeneratedClass) return true;
		auto* Old=Node->ComponentTemplate.Get(); auto* Outer=Old->GetOuter(); const FName Name=Old->GetFName();
		auto* Replacement=NewObject<UActorComponent>(Outer,Copy->GeneratedClass,NAME_None,RF_Public|RF_ArchetypeObject);
		UEngine::FCopyPropertiesForUnrelatedObjectsParams Params; Params.bDoDelta=false; Params.bClearReferences=false; UEngine::CopyPropertiesForUnrelatedObjects(Old,Replacement,Params);
		Old->Rename(nullptr,GetTransientPackage(),REN_DontCreateRedirectors|REN_NonTransactional); Replacement->Rename(*Name.ToString(),Outer,REN_DontCreateRedirectors|REN_NonTransactional);
		Node->ComponentClass=Copy->GeneratedClass; Node->ComponentTemplate=Replacement;
		FBlueprintEditorUtils::RefreshAllNodes(Character); return SaveBP(Character);
	}
	return false;
}
bool ConfigureStates(UClass* PanelClass)
{
	auto* Settings=LoadBP(TEXT("/Game/Blueprints/States/FSMState_GameSettings.FSMState_GameSettings"));
	auto* Runtime=LoadBP(TEXT("/Game/Blueprints/States/FSMState_GameRuntime.FSMState_GameRuntime"));
	if (!Settings || !Runtime || !Backup(Settings) || !Backup(Runtime)) return false;
	auto* State=Cast<UHFSMState>(Settings->GeneratedClass->GetDefaultObject()); if (!State) return false;
	// Replace only the accidental HealthPanel mounter; keep authored layout/input components.
	auto* Array=FindFProperty<FArrayProperty>(UHFSMState::StaticClass(),TEXT("Components")); auto* Inner=CastField<FObjectPropertyBase>(Array->Inner);
	FScriptArrayHelper List(Array,Array->ContainerPtrToValuePtr<void>(State));
	for (int32 I=List.Num()-1;I>=0;--I)
	{
		auto* C=Inner->GetObjectPropertyValue(List.GetRawPtr(I));
		if (C && C->IsA<UUIWidgetMounterComponent>() && !C->IsA<UNGSettingsScreenMounter>())
		{
			auto* P=FindFProperty<FClassProperty>(C->GetClass(),TEXT("WidgetClass")); auto* Class=Cast<UClass>(P->GetObjectPropertyValue_InContainer(C));
			if (Class && Class->GetPathName().Contains(TEXT("WBP_HealthPanel"))) List.RemoveValues(I);
		}
		if (C && C->IsA(LoadClass<UObject>(nullptr,TEXT("/Script/UIModule.HFSMInputComponent"))))
		{
			auto* Enabled=FindFProperty<FBoolProperty>(C->GetClass(),TEXT("bSetupInputMode")); Enabled->SetPropertyValue_InContainer(C,true);
			auto* Mode=FindFProperty<FEnumProperty>(C->GetClass(),TEXT("InputMode")); if (!Mode) return false;
			Mode->GetUnderlyingProperty()->SetIntPropertyValue(Mode->ContainerPtrToValuePtr<void>(C),int64(0)); // EInputMode::UI
		}
	}
	if (!AddStateComponent(Settings,UNGSettingsScreenMounter::StaticClass(),TEXT("SettingsScreen")) || !AddStateComponent(Runtime,UNGRuntimeUIStateComponent::StaticClass(),TEXT("RuntimeDebugUI"))) return false;
	for (auto* M:State->GetComponents(UNGSettingsScreenMounter::StaticClass())) FindFProperty<FClassProperty>(M->GetClass(),TEXT("WidgetClass"))->SetObjectPropertyValue_InContainer(M,PanelClass);
	return SaveBP(Settings) && SaveBP(Runtime);
}
bool ConnectHUD()
{
	auto* HUD=LoadBP(TEXT("/Game/Blueprints/BP_NextgenoneHUD.BP_NextgenoneHUD")); if (!HUD || !Backup(HUD)) return false;
	TArray<UEdGraph*> Graphs; HUD->GetAllGraphs(Graphs); UEdGraphNode* Runtime=nullptr; UEdGraphNode* Settings=nullptr; UEdGraph* Graph=nullptr;
	for (auto* G:Graphs) for (auto N:G->Nodes) if (auto* P=N->FindPin(TEXT("ImplementationClass")))
	{
		if (P->DefaultObject && P->DefaultObject->GetPathName().Contains(TEXT("FSMState_GameRuntime_C"))) { Runtime=N; Graph=G; }
		if (P->DefaultObject && P->DefaultObject->GetPathName().Contains(TEXT("FSMState_GameSettings_C"))) Settings=N;
	}
	if (!Runtime || !Graph) return false;
	auto AddTransition=[](UEdGraphNode* Node,FName Event)
	{
		auto* P=FindFProperty<FArrayProperty>(Node->GetClass(),TEXT("transitionEvents")); if (!P) return false;
		FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(Node)); for (int32 I=0;I<A.Num();++I) if (*reinterpret_cast<FName*>(A.GetRawPtr(I))==Event) return true;
		*reinterpret_cast<FName*>(A.GetRawPtr(A.AddValue()))=Event; Node->ReconstructNode(); return true;
	};
	if (!AddTransition(Runtime,TEXT("Settings"))) return false;
	if (!Settings)
	{
		Settings=NewObject<UEdGraphNode>(Graph,Runtime->GetClass(),NAME_None,RF_Transactional); Graph->AddNode(Settings,false,false); Settings->CreateNewGuid(); Settings->PostPlacedNewNode(); Settings->AllocateDefaultPins();
		Settings->NodePosX=Runtime->NodePosX+450; Settings->NodePosY=Runtime->NodePosY+180;
		auto* BP=LoadBP(TEXT("/Game/Blueprints/States/FSMState_GameSettings.FSMState_GameSettings")); if (!BP) return false;
		Graph->GetSchema()->TrySetDefaultObject(*Settings->FindPin(TEXT("ImplementationClass")),BP->GeneratedClass);
		if (Settings->FindPin(TEXT("ImplementationClass"))->DefaultObject!=BP->GeneratedClass) return false;
	}
	if (!AddTransition(Settings,TEXT("Back"))) return false;
	// Remove the new node's unused default event; preserve any user-connected transition.
	if (auto* Default=Settings->FindPin(TEXT("Event")); Default && Default->LinkedTo.IsEmpty())
	{
		auto* P=FindFProperty<FArrayProperty>(Settings->GetClass(),TEXT("transitionEvents"));
		FScriptArrayHelper Events(P,P->ContainerPtrToValuePtr<void>(Settings));
		for (int32 I=Events.Num()-1;I>=0;--I) if (*reinterpret_cast<FName*>(Events.GetRawPtr(I))==TEXT("Event")) Events.RemoveValues(I);
		Settings->ReconstructNode();
	}
	auto Input=[](UEdGraphNode* N)->UEdGraphPin* { for (auto* P:N->Pins) if (P->Direction==EGPD_Input && P->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec) return P; return nullptr; };
	if (!Input(Settings) || !Input(Runtime)) return false;
	if (!Graph->GetSchema()->TryCreateConnection(Runtime->FindPin(TEXT("Settings")),Input(Settings)) || !Graph->GetSchema()->TryCreateConnection(Settings->FindPin(TEXT("Back")),Input(Runtime))) return false;
	for (auto* Node:{Runtime,Settings}) for (auto* Pin:Node->Pins)
		if (Pin->PinType.PinCategory==UEdGraphSchema_K2::PC_Exec)
			UE_LOG(LogTemp,Display,TEXT("NG_UI_GRAPH: %s pin=%s links=%d"),*Node->FindPin(TEXT("ImplementationClass"))->DefaultObject->GetName(),*Pin->PinName.ToString(),Pin->LinkedTo.Num());
	return SaveBP(HUD);
}
}
int32 NGCreateSettingsAssets()
{
	const FString Path=TEXT("/Game/Gameplay/UI/WBP_GameSettings"); auto* Panel=LoadBP(*(Path+TEXT(".WBP_GameSettings")));
	if (!Panel)
	{
		Panel=FKismetEditorUtilities::CreateBlueprint(UNGSettingsPanel::StaticClass(),CreatePackage(*Path),TEXT("WBP_GameSettings"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("NGSettings"));
		if (!Panel || !SaveBP(Panel)) return 90;
	}
	if (!Panel->GeneratedClass->IsChildOf(UNGSettingsPanel::StaticClass())) return 91;
	if (!MigrateDebug()) return 92;
	if (!ConfigureStates(Panel->GeneratedClass)) return 93;
	if (!ConnectHUD()) return 94;
	UE_LOG(LogTemp,Display,TEXT("NG_SETTINGS_ASSETS_SAVED: settings screen, Runtime debug ownership, Settings/Back HUD transitions; ALS plugin unchanged")); return 0;
}
int32 NGRemoveALSMenuAssets()
{
	auto* ControllerBP=LoadBP(TEXT("/Game/Blueprints/BP_NextgenonePlayerController.BP_NextgenonePlayerController"));
	auto* RuntimeBP=LoadBP(TEXT("/Game/Blueprints/States/FSMState_GameRuntime.FSMState_GameRuntime"));
	auto* SettingsBP=LoadBP(TEXT("/Game/Gameplay/UI/WBP_GameSettings.WBP_GameSettings"));
	if (!ControllerBP || !RuntimeBP || !SettingsBP || !Backup(ControllerBP) || !Backup(RuntimeBP) || !Backup(SettingsBP)) return 140;
	auto* Controller=CastChecked<ANextgenonePlayerController>(ControllerBP->GeneratedClass->GetDefaultObject());
	auto* Mapping=LoadObject<UInputMappingContext>(nullptr,TEXT("/Game/Gameplay/Input/IMC_NextgenoneDebug.IMC_NextgenoneDebug"));
	if (!Mapping)
	{
		if (!Controller->DebugInputMappingContext) return 141;
		Mapping=Cast<UInputMappingContext>(FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().DuplicateAsset(TEXT("IMC_NextgenoneDebug"),TEXT("/Game/Gameplay/Input"),Controller->DebugInputMappingContext));
	}
	if (!Mapping || !Backup(Mapping)) return 142;
	const TSet<FName> Legacy={TEXT("DebugOpenOverlayMenuAction"),TEXT("DebugOverlayMenuCycleAction"),TEXT("DebugToggleHudAction"),TEXT("DebugToggleCharacterInfoAction"),TEXT("DebugToggleDebugMeshAction"),TEXT("DebugToggleDebugViewAction"),TEXT("DebugToggleLayerColorsAction"),TEXT("DebugToggleShapesAction"),TEXT("DebugToggleTracesAction"),TEXT("DebugFocusedCharacterCycleAction"),TEXT("DebugToggleSlomoAction")};
	TSet<const UInputAction*> Remove;
	for (const auto& Entry:Mapping->GetMappings()) if (Entry.Action && Legacy.Contains(Entry.Action->GetFName())) Remove.Add(Entry.Action);
	for (const auto* Action:Remove) Mapping->UnmapAllKeysFromAction(Action);
	FSavePackageArgs Args; Args.TopLevelFlags=RF_Public|RF_Standalone;
	if (!UPackage::SavePackage(Mapping->GetOutermost(),Mapping,*FPackageName::LongPackageNameToFilename(Mapping->GetOutermost()->GetName(),TEXT(".uasset")),Args)) return 143;
	Controller->DebugInputMappingContext=Mapping;
	auto* State=CastChecked<UHFSMState>(RuntimeBP->GeneratedClass->GetDefaultObject());
	for (auto* Component:State->GetComponents(UNGRuntimeUIStateComponent::StaticClass()))
	{
		auto* Runtime=CastChecked<UNGRuntimeUIStateComponent>(Component); Runtime->DebugHUDClass.Reset(); Runtime->OverlayClass.Reset();
	}
	auto* Panel=LoadBP(TEXT("/Game/Gameplay/UI/WBP_DebugSettings.WBP_DebugSettings"));
	if (!Panel)
	{
		Panel=FKismetEditorUtilities::CreateBlueprint(UNGDebugSettingsPanel::StaticClass(),CreatePackage(TEXT("/Game/Gameplay/UI/WBP_DebugSettings")),TEXT("WBP_DebugSettings"),BPTYPE_Normal,UWidgetBlueprint::StaticClass(),UWidgetBlueprintGeneratedClass::StaticClass(),TEXT("NGDebugSettings"));
		if (!Panel || !SaveBP(Panel)) return 144;
	}
	if (!Panel->GeneratedClass->IsChildOf(UNGDebugSettingsPanel::StaticClass())) return 145;
	auto* Screen=CastChecked<UNGSettingsPanel>(SettingsBP->GeneratedClass->GetDefaultObject()); if (!Screen->DebugPanelClass) Screen->DebugPanelClass=Panel->GeneratedClass;
	if (!SaveBP(ControllerBP) || !SaveBP(RuntimeBP) || !SaveBP(SettingsBP)) return 146;
	UE_LOG(LogTemp,Display,TEXT("NG_ALS_MENU_REMOVED: no Runtime ALS HUD/overlay; debug controls moved to settings; legacy mappings removed=%d remainingCustomMappings=%d; plugin unchanged"),Remove.Num(),Mapping->GetMappings().Num()); return 0;
}
#endif
