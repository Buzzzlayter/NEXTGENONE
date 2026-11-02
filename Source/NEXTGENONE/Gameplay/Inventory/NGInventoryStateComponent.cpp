#include "NGInventoryStateComponent.h"
#include "../NextgenonePlayerController.h"
#include "../NextgenoneCharacter.h"
#include "../NGAbilitySystem.h"
#include "Input/PlayerInputSubsystem.h"
#include "Blueprint/UserWidget.h"

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
	return true;
}
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
	PreviousMode = Input ? Input->GetInputMode() : EInputMode::Game; bPreviousCursor = PC->bShowMouseCursor;
	PC->bInventoryUIOpen = true; PC->SetIgnoreMoveInput(true); PC->SetIgnoreLookInput(true); PC->bShowMouseCursor = true;
	if (auto* Pawn = Cast<ANextgenoneCharacter>(PC->GetPawn()))
		if (auto* ASC = Pawn->GetAbilitySystemComponent()) { FGameplayTagContainer Tags; Tags.AddTag(TAG_NGMovementAbility); ASC->CancelAbilities(&Tags); }
	if (Input) { Input->SetPlayerInputMode(EInputMode::GameAndUI, true); Input->SetWidgetToFocus(Mounter->GetWidget()); }
	if (auto* Widget = Mounter->GetWidget()) Widget->SetUserFocus(PC);
}
void UNGInventoryStateComponent::Close()
{
	if (!Mounter) return;
	Mounter->Exit(NAME_None); Mounter = nullptr;
	if (auto* PC = Controller.Get())
	{
		PC->bInventoryUIOpen = false; PC->SetIgnoreMoveInput(false); PC->SetIgnoreLookInput(false); PC->bShowMouseCursor = bPreviousCursor;
		if (auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this)) Input->SetPlayerInputMode(PreviousMode, true);
	}
}
void UNGInventoryStateComponent::Unbind()
{
	Close();
	if (Controller.IsValid()) Controller->InventoryUIRequested.RemoveDynamic(this, &ThisClass::Toggle);
	Controller.Reset();
}
void UNGInventoryStateComponent::Exit(FName Event) { Unbind(); Super::Exit(Event); }
void UNGInventoryStateComponent::AbortEnter() { Unbind(); Super::Exit(NAME_None); }
