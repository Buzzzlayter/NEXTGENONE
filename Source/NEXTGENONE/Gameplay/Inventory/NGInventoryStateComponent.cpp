#include "NGInventoryStateComponent.h"
#include "../NextgenonePlayerController.h"
#include "../NextgenoneCharacter.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Input/PlayerInputSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"

void UNGInventoryWindowMounter::Configure(TSubclassOf<UUserWidget> Class)
{
	WidgetClass = Class;
	// Use the existing mounter's owning-player fallback until the UI has a dedicated inventory placeholder.
	bAddToViewportIfNoLayout = true; FallbackZOrder = 50; PlaceholderID = FGameplayTag();
}
bool UNGInventoryStateComponent::Enter()
{
	if (!Super::Enter()) return false;
	Controller = Cast<ANextgenonePlayerController>(GetPlayerController());
	if (!Controller.IsValid()) { Super::Exit(NAME_None); return false; }
	Controller->InventoryUIRequested.AddUniqueDynamic(this, &ThisClass::Toggle);
	if (auto* Input = Cast<UEnhancedInputComponent>(Controller->InputComponent); Input && ToggleAction)
	{
		BoundInput = Input;
		ToggleBinding = Input->BindAction(ToggleAction, ETriggerEvent::Started, this, &ThisClass::RequestToggle).GetHandle();
	}
	return true;
}
void UNGInventoryStateComponent::RequestToggle() { if (Controller.IsValid()) Controller->ToggleInventoryUI(); }
void UNGInventoryStateComponent::Toggle(UNGInventoryManagerComponent* Inventory)
{
	auto* PC = Controller.Get(); if (!PC || !Inventory) return;
	if (Mounter) { Close(); return; }
	UClass* WidgetClass = PanelClass.LoadSynchronous();
	if (!WidgetClass) WidgetClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/Gameplay/UI/WBP_InventoryPanel.WBP_InventoryPanel_C"));
	if (!WidgetClass) return;
	auto* Mount = NewObject<UNGInventoryWindowMounter>(this); Mount->Configure(WidgetClass); Mount->Init(GetState());
	if (!Mount->Enter()) { Mount->AbortEnter(); return; }
	Mounter = Mount;
	auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this);
	if (!Input) { Close(); return; }
	InputSubsystem = Input;
	FPlayerInputPolicy Policy; Policy.InputMode = EInputMode::GameAndUI; Policy.bBlockGameplay = true;
	Policy.bSetCursorMode = true; Policy.CursorMode = EGamepadCursorMode::Grid;
	PolicyHandle = Input->AcquireInputPolicy(this, Policy);
	PC->bInventoryUIOpen = true;
	Input->SetWidgetToFocus(Mounter->GetWidget());
	if (auto* Widget = Mounter->GetWidget()) Widget->SetUserFocus(PC);
}
void UNGInventoryStateComponent::Close()
{
	if (!Mounter) return;
	Mounter->Exit(NAME_None); Mounter = nullptr;
	if (auto* Input = InputSubsystem.Get()) Input->ReleaseInputPolicy(PolicyHandle);
	PolicyHandle.Invalidate(); InputSubsystem.Reset();
	if (auto* PC = Controller.Get()) PC->bInventoryUIOpen = false;
}
void UNGInventoryStateComponent::Unbind()
{
	Close();
	if (auto* Input = BoundInput.Get()) Input->RemoveBindingByHandle(ToggleBinding);
	BoundInput.Reset(); ToggleBinding = 0;
	if (Controller.IsValid()) Controller->InventoryUIRequested.RemoveDynamic(this, &ThisClass::Toggle);
	Controller.Reset();
}
void UNGInventoryStateComponent::Exit(FName Event) { Unbind(); Super::Exit(Event); }
void UNGInventoryStateComponent::AbortEnter() { Unbind(); Super::Exit(NAME_None); }
