#include "NGDebugState.h"
#include "NextgenonePlayerController.h"
#include "Components/InputComponent.h"
#include "Blueprint/UserWidget.h"
#include "HFSMState.h"
#include "GameFramework/Pawn.h"
#include "UObject/StructOnScope.h"

// GCFSMUtilities is reflected but is not DLL-exported by the plugin. Use its public UFunction.
bool NGTriggerUIEvent(UHFSMState* State,FName Event)
{
	auto* Class=LoadClass<UObject>(nullptr,TEXT("/Script/GCFSM.GCFSMUtilities"));
	auto* Function=Class ? Class->FindFunctionByName(TEXT("TriggerEvent")) : nullptr;
	if (!State || !Function) return false;
	FStructOnScope Params(Function);
	auto* Context=FindFProperty<FObjectPropertyBase>(Function,TEXT("stateOrContext"));
	auto* Name=FindFProperty<FNameProperty>(Function,TEXT("eventName"));
	auto* Propagate=FindFProperty<FBoolProperty>(Function,TEXT("propagateToSubstates"));
	auto* Internal=FindFProperty<FBoolProperty>(Function,TEXT("triggerInternalEvents"));
	if (!Context || !Name || !Propagate || !Internal) return false;
	Context->SetObjectPropertyValue_InContainer(Params.GetStructMemory(),State); Name->SetPropertyValue_InContainer(Params.GetStructMemory(),Event);
	Propagate->SetPropertyValue_InContainer(Params.GetStructMemory(),true); Internal->SetPropertyValue_InContainer(Params.GetStructMemory(),true);
	if (auto* Expiry=FindFProperty<FFloatProperty>(Function,TEXT("expireAfter"))) Expiry->SetPropertyValue_InContainer(Params.GetStructMemory(),.5f);
	Class->GetDefaultObject()->ProcessEvent(Function,Params.GetStructMemory()); return true;
}

UNGRuntimeUIStateComponent::UNGRuntimeUIStateComponent() {}
bool UNGRuntimeUIStateComponent::Enter()
{
	if (!Super::Enter()) return false;
	Controller=GetPlayerController(); if (!Controller.IsValid() || !Controller->InputComponent) { Super::Exit(NAME_None); return false; }
	auto& Binding=Controller->InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&ThisClass::OpenSettings); Binding.bConsumeInput=true;
	return true;
}
void UNGRuntimeUIStateComponent::OpenSettings()
{
	if (const auto* PC=Cast<ANextgenonePlayerController>(Controller.Get())) if (PC->bInventoryUIOpen || PC->bSettingsUIOpen) return;
	NGTriggerUIEvent(GetState(),TEXT("Settings"));
}
void UNGRuntimeUIStateComponent::Release()
{
	if (Controller.IsValid() && Controller->InputComponent) Controller->InputComponent->ClearBindingsForObject(this);
	Controller.Reset();
}
void UNGRuntimeUIStateComponent::Exit(FName Event) { Release(); Super::Exit(Event); }
void UNGRuntimeUIStateComponent::AbortEnter() { Release(); Super::Exit(NAME_None); }