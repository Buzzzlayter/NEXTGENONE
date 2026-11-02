#include "NGCharacterStateResources.h"
#include "NGCharacterStateMachineComponent.h"
#include "Character/NGControlStateDefinition.h"
#include "AbilitySystemGlobals.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Input/PlayerInputSubsystem.h"

namespace
{
	UAbilitySystemComponent* ResolvePawnASC(APawn* Pawn)
	{
		if (!Pawn) return nullptr;
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		return ASC ? ASC : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn->GetPlayerState());
	}
}

bool UNGCharacterStateResources::Enter()
{
	Pawn = Cast<APawn>(GetContextOwner());
	FString Error;
	if (!Pawn.IsValid() || !Definition || !Definition->ValidateResources(Error))
	{ UE_LOG(LogTemp, Error, TEXT("Character state resources: invalid Pawn/Definition. %s"), *Error); return false; }
	if (Pawn->HasAuthority())
	{
		Pawn->FlushNetDormancy();
		if (!AcquireGASResources()) return false;
	}
	bEntered = true;
	if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->RegisterResources(this);
	RefreshResources();
	if (!Super::Enter()) { ReleaseResources(); bEntered = false; if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->UnregisterResources(this); return false; }
	return true;
}

bool UNGCharacterStateResources::AcquireGASResources()
{
	if (bGASAcquired || !Pawn.IsValid() || !Pawn->HasAuthority()) return true;
	if (Definition->AbilitySets.IsEmpty() && Definition->Effects.IsEmpty()) { bGASAcquired = true; return true; }
	UAbilitySystemComponent* ASC = ResolvePawnASC(Pawn.Get());
	if (!ASC || ASC->GetAvatarActor() != Pawn.Get())
	{ UE_LOG(LogTemp, Error, TEXT("%s state needs its current ASC Avatar; corpse-only states must not depend on detached GAS."), *GetNameSafe(Pawn.Get())); return false; }
	// Do not grant a second copy of an already available base/equipment ability.
	for (const UNGAbilitySet* Set : Definition->AbilitySets)
		for (const auto& Grant : Set->Abilities)
			for (const auto& Existing : ASC->GetActivatableAbilities())
				if ((Existing.Ability && Existing.Ability->GetClass() == Grant.Ability.Get()) || (Grant.InputTag.IsValid() && Existing.GetDynamicSpecSourceTags().HasTagExact(Grant.InputTag)))
				{ UE_LOG(LogTemp, Error, TEXT("Duplicate state ability/input tag in %s"), *Definition->GetName()); return false; }
	GrantedASC = ASC;
	for (const UNGAbilitySet* Set : Definition->AbilitySets)
	{
		FNGGrantedHandles SetHandles; Set->Give(ASC, Pawn.Get(), SetHandles);
		Handles.Abilities.Append(SetHandles.Abilities); Handles.Effects.Append(SetHandles.Effects);
		if (SetHandles.Effects.ContainsByPredicate([](const auto& Handle) { return !Handle.IsValid(); }))
		{ ReleaseResources(); return false; }
	}
	for (auto Effect : Definition->Effects)
	{
		const auto Handle = ASC->ApplyGameplayEffectToSelf(Effect.GetDefaultObject(), 1.f, ASC->MakeEffectContext());
		if (!Handle.IsValid()) { ReleaseResources(); return false; }
		Handles.Effects.Add(Handle);
	}
	bGASAcquired = true;
	return true;
}

void UNGCharacterStateResources::RefreshResources()
{
	if (!bEntered || !Pawn.IsValid()) return;
	if (Pawn->HasAuthority() && !bGASAcquired)
	{
		// An unpossessed body can keep its FSM while the player's ASC serves another Avatar.
		UAbilitySystemComponent* ASC = ResolvePawnASC(Pawn.Get());
		if (ASC && ASC->GetAvatarActor() == Pawn.Get() && !AcquireGASResources()) return;
	}
	APlayerController* PC = Cast<APlayerController>(Pawn->GetController());
	UPlayerInputSubsystem* Input = PC && PC->GetPawn() == Pawn.Get() && PC->IsLocalController() && PC->GetLocalPlayer()
		? PC->GetLocalPlayer()->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
	if (LocalInput.Get() == Input) return;
	ReleaseLocalResources();
	if (!Input) return; // normal for simulated proxies/dedicated server and before possession arrives
	LocalInput = Input;
	for (const auto& Entry : Definition->MappingContexts)
		ContextHandles.Add(Input->AcquireMappingContext(this, Entry.MappingContext, Entry.Priority, true));
	FPlayerInputPolicy Policy; Policy.bSetInputMode = false; Policy.bBlockGameplay = Definition->bBlockGameplayInput;
	PolicyHandle = Input->AcquireInputPolicy(this, Policy);
}

void UNGCharacterStateResources::ReleaseLocalResources()
{
	if (auto* Input = LocalInput.Get())
	{
		for (FGuid Handle : ContextHandles) Input->ReleaseMappingContext(Handle);
		Input->ReleaseInputPolicy(PolicyHandle);
	}
	LocalInput.Reset(); ContextHandles.Reset(); PolicyHandle.Invalidate();
}

void UNGCharacterStateResources::ReleaseResources()
{
	ReleaseLocalResources();
	if (auto* ASC = GrantedASC.Get())
	{
		if (!ASC->GetAvatarActor() || ASC->GetAvatarActor() == Pawn.Get()) Handles.Remove(ASC);
		else { UE_LOG(LogTemp, Warning, TEXT("Late state cleanup skipped after Avatar replacement; lifecycle must release before swapping ASC.")); Handles.Abilities.Reset(); Handles.Effects.Reset(); }
	}
	Handles.Abilities.Reset(); Handles.Effects.Reset();
	GrantedASC.Reset(); bGASAcquired = false;
}

void UNGCharacterStateResources::Exit(FName Event)
{
	bEntered = false;
	if (Pawn.IsValid()) if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->UnregisterResources(this);
	ReleaseResources(); Pawn.Reset(); Super::Exit(Event);
}
void UNGCharacterStateResources::AbortEnter() { Exit(NAME_None); }
