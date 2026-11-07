#include "NGCharacterStateResources.h"
#include "NGCharacterStateMachineComponent.h"
#include "Character/NGControlStateDefinition.h"
#include "Input/NGGameplayInputComponent.h"
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
		if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>())
			if (auto* ASC = Machine->GetReadyASC()) return ASC;
		UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
		return ASC ? ASC : UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn->GetPlayerState());
	}
}

void UNGCharacterStateResources::Enter()
{
	Pawn = Cast<APawn>(GetContextOwner());
	if (Pawn.IsValid())
		if (const auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>(); Machine && Machine->HasResourceFailure()) return;
	FString Error;
	if (!Pawn.IsValid() || !Definition || !Definition->ValidateResources(Error))
	{ FailEntry(FString::Printf(TEXT("Invalid Pawn/Definition. %s"), *Error)); return; }
	const auto* GameplayInput = Pawn->FindComponentByClass<UNGGameplayInputComponent>();
	const auto* InputConfig = GameplayInput ? GameplayInput->InputConfig.Get() : nullptr;
	if (!Definition->ValidateInputMappings(InputConfig, Error))
	{ FailEntry(Error); return; }
	bEntered = true;
	bEntryReady = false;
	if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->RegisterResources(this);
	if (Pawn->HasAuthority())
	{
		Pawn->FlushNetDormancy();
		if (!AcquireGASResources()) { FailEntry(TEXT("Failed to acquire GAS resources; see preceding diagnostics.")); return; }
	}
	RefreshResources();
	if (!bEntered) return;
	Super::Enter();
	if (!bEntered) return;
	if (Pawn->HasAuthority() && !ActivateEntryAbilities())
	{
		FailEntry(TEXT("Failed to activate an entry ability."));
		return;
	}
	bEntryReady = true;
	if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->NotifyResourcesReady(this);
}

void UNGCharacterStateResources::FailEntry(const FString& Reason)
{
	auto* Machine = Pawn.IsValid() ? Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>() : nullptr;
	const FGameplayTag StateTag = Definition ? Definition->StateTag : FGameplayTag();
	UE_LOG(LogTemp, Error, TEXT("Character state resources %s (definition=%s): %s"), *GetPathName(), *GetNameSafe(Definition), *Reason);
	Exit(NAME_None);
	if (Machine) Machine->NotifyResourceFailure(StateTag, Reason);
}

bool UNGCharacterStateResources::AcquireGASResources()
{
	if (bGASAcquired || !Pawn.IsValid() || !Pawn->HasAuthority()) return true;
	if (Definition->Abilities.IsEmpty() && Definition->Effects.IsEmpty()) { bGASAcquired = true; return true; }
	UAbilitySystemComponent* ASC = ResolvePawnASC(Pawn.Get());
	if (!ASC || ASC->GetAvatarActor() != Pawn.Get())
	{ UE_LOG(LogTemp, Error, TEXT("%s state needs its current ASC Avatar; corpse-only states must not depend on detached GAS."), *GetNameSafe(Pawn.Get())); return false; }
	// Reject overlap with equipment or a parent state rather than silently duplicating specs.
	for (const auto& Grant : Definition->Abilities)
			for (const auto& Existing : ASC->GetActivatableAbilities())
				if ((Existing.Ability && Existing.Ability->GetClass() == Grant.Ability.Get()) || (Grant.InputTag.IsValid() && Existing.GetDynamicSpecSourceTags().HasTagExact(Grant.InputTag)))
				{ UE_LOG(LogTemp, Error, TEXT("Duplicate state ability/input tag in %s"), *Definition->GetName()); return false; }
	GrantedASC = ASC;
	for (const auto& Grant : Definition->Abilities)
	{
		const auto Handle = Grant.Give(ASC, Pawn.Get());
		if (!Handle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("State %s failed to grant ability %s."), *GetNameSafe(Definition), *GetNameSafe(Grant.Ability.Get()));
			ReleaseResources(); return false;
		}
		Handles.Abilities.Add(Handle);
	}
	for (auto Effect : Definition->Effects)
	{
		const auto Handle = ASC->ApplyGameplayEffectToSelf(Effect.GetDefaultObject(), 1.f, ASC->MakeEffectContext());
		if (!Handle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("State %s failed to apply effect %s."), *GetNameSafe(Definition), *GetNameSafe(Effect.Get()));
			ReleaseResources(); return false;
		}
		Handles.Effects.Add(Handle);
	}
	bGASAcquired = true;
	return true;
}

bool UNGCharacterStateResources::OwnsAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const
{
	return bEntered && bGASAcquired && GrantedASC.Get() == ASC && Handles.Abilities.Contains(Handle);
}

bool UNGCharacterStateResources::ActivateEntryAbilities()
{
	if (bEntryActivationAttempted || !bGASAcquired) return true;
	bEntryActivationAttempted = true;
	auto* ASC = GrantedASC.Get();
	TArray<FGameplayAbilitySpecHandle> ToActivate;
	for (int32 Index = 0; Index < Definition->Abilities.Num(); ++Index)
		if (Definition->Abilities[Index].bActivateOnEnter) ToActivate.Add(Handles.Abilities[Index]);
	for (const auto Handle : ToActivate)
	{
		// Activation can finalize death and hand this ASC to the next Pawn synchronously.
		if (!bGASAcquired || !ASC || ASC->GetAvatarActor() != Pawn.Get()) break;
		if (!ASC->TryActivateAbility(Handle))
		{
			UE_LOG(LogTemp, Error, TEXT("State %s failed to activate an entry ability."), *GetNameSafe(Definition));
			return false;
		}
	}
	return true;
}

void UNGCharacterStateResources::RefreshResources()
{
	if (!bEntered || !Pawn.IsValid()) return;
	if (const auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>(); Machine && Machine->HasResourceFailure()) return;
	if (Pawn->HasAuthority() && !bGASAcquired)
	{
		// An unpossessed body can keep its FSM while the player's ASC serves another Avatar.
		UAbilitySystemComponent* ASC = ResolvePawnASC(Pawn.Get());
		if (Pawn->GetController() && ASC && ASC->GetAvatarActor() == Pawn.Get())
		{
			if (!AcquireGASResources() || !ActivateEntryAbilities()) { FailEntry(TEXT("Failed to reacquire GAS resources or activate an entry ability.")); return; }
			bEntryReady = true;
			if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->NotifyResourcesReady(this);
		}
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
	bEntryReady = false;
	ReleaseLocalResources();
	if (auto* ASC = GrantedASC.Get())
	{
		if (!ASC->GetAvatarActor() || ASC->GetAvatarActor() == Pawn.Get()) Handles.Remove(ASC);
		else { UE_LOG(LogTemp, Warning, TEXT("Late state cleanup skipped after Avatar replacement; lifecycle must release before swapping ASC.")); Handles.Abilities.Reset(); Handles.Effects.Reset(); }
	}
	Handles.Abilities.Reset(); Handles.Effects.Reset();
	GrantedASC.Reset(); bGASAcquired = false; bEntryActivationAttempted = false;
}

void UNGCharacterStateResources::Exit(FName Event)
{
	bEntered = false;
	if (Pawn.IsValid()) if (auto* Machine = Pawn->FindComponentByClass<UNGCharacterStateMachineComponent>()) Machine->UnregisterResources(this);
	ReleaseResources(); Pawn.Reset(); Super::Exit(Event);
}
