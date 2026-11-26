#include "NGCharacterStateResources.h"
#include "NGCharacterStateMachineComponent.h"
#include "Character/NGControlStateDefinition.h"
#include "Input/NGGameplayInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Input/PlayerInputSubsystem.h"
#include "InputMappingContext.h"
#if WITH_EDITOR
#include "Engine/World.h"
#include "Logging/MessageLog.h"
#endif

UNGCharacterStateMachineComponent* UNGCharacterStateResources::GetMachine() const
{
	return CastChecked<UNGCharacterStateMachineComponent>(GetOuter());
}

void UNGCharacterStateResources::Enter(APawn* StatePawn, UNGControlStateDefinition* StateDefinition)
{
	Pawn = StatePawn;
	Definition = StateDefinition;
	FString Error;
	if (!Pawn.IsValid() || !Definition || !Definition->ValidateResources(Error))
	{ FailEntry(FString::Printf(TEXT("Invalid Pawn/Definition. %s"), *Error)); return; }
	const auto* GameplayInput = Pawn->FindComponentByClass<UNGGameplayInputComponent>();
	const auto* InputConfig = GameplayInput ? GameplayInput->InputConfig.Get() : nullptr;
	if (!Definition->ValidateInputMappings(InputConfig, Error))
	{
#if WITH_EDITOR
		if (Pawn->IsPlayerControlled() && Pawn->GetWorld()->WorldType == EWorldType::PIE)
		{
			FMessageLog PlayLog(TEXT("PIE"));
			PlayLog.Error(FText::Format(NSLOCTEXT("NGCharacterStateResources", "InputSetupFailure", "Character input setup failed for {0}. {1}"),
				FText::FromString(Definition->GetName()), FText::FromString(Error)));
			PlayLog.Notify(NSLOCTEXT("NGCharacterStateResources", "InputSetupNotification", "Fix the character input setup in the editor."));
		}
#endif
		FailEntry(Error);
		return;
	}
	bEntered = true;
	bEntryReady = false;
	if (Pawn->HasAuthority())
	{
		if (!AcquireGASResources()) { FailEntry(TEXT("Failed to acquire GAS resources; see preceding diagnostics.")); return; }
	}
	RefreshResources();
	if (!bEntered) return;
	if (Pawn->HasAuthority() && !ActivateEntryAbilities())
	{
		FailEntry(TEXT("Failed to activate an entry ability."));
		return;
	}
	if (!bEntered) return;
	bEntryReady = true;
	GetMachine()->NotifyResourcesReady(this);
}

void UNGCharacterStateResources::FailEntry(const FString& Reason)
{
	GetMachine()->NotifyResourceFailure(Reason);
}

bool UNGCharacterStateResources::AcquireGASResources()
{
	if (Definition->Abilities.IsEmpty() && Definition->Effects.IsEmpty()) return true;
	auto* ASC = GetMachine()->GetReadyASC();
	if (!ASC)
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
			UE_LOG(LogTemp, Error, TEXT("State %s failed to grant ability %s."), *GetNameSafe(Definition.Get()), *GetNameSafe(Grant.Ability.Get()));
			ReleaseResources(); return false;
		}
		Handles.Abilities.Add(Handle);
	}
	for (auto Effect : Definition->Effects)
	{
		const auto Handle = ASC->ApplyGameplayEffectToSelf(Effect.GetDefaultObject(), 1.f, ASC->MakeEffectContext());
		if (!Handle.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("State %s failed to apply effect %s."), *GetNameSafe(Definition.Get()), *GetNameSafe(Effect.Get()));
			ReleaseResources(); return false;
		}
		Handles.Effects.Add(Handle);
	}
	return true;
}

bool UNGCharacterStateResources::OwnsAbility(const UAbilitySystemComponent* ASC, const FGameplayAbilitySpecHandle& Handle) const
{
	return bEntered && GrantedASC.Get() == ASC && Handles.Abilities.Contains(Handle);
}

bool UNGCharacterStateResources::ActivateEntryAbilities()
{
	auto* ASC = GrantedASC.Get();
	const auto* Machine = GetMachine();
	TArray<FGameplayAbilitySpecHandle> ToActivate;
	for (int32 Index = 0; Index < Definition->Abilities.Num(); ++Index)
		if (Definition->Abilities[Index].bActivateOnEnter) ToActivate.Add(Handles.Abilities[Index]);
	for (const auto Handle : ToActivate)
	{
		// Activation can finalize death and hand this ASC to the next Pawn synchronously.
		if (!ASC || ASC->GetAvatarActor() != Pawn.Get() || !Machine->OwnsStateAbility(ASC, Handle)) break;
		if (!ASC->TryActivateAbility(Handle))
		{
			UE_LOG(LogTemp, Error, TEXT("State %s failed to activate an entry ability."), *GetNameSafe(Definition.Get()));
			return false;
		}
	}
	return true;
}

void UNGCharacterStateResources::RefreshResources()
{
	if (!bEntered || !Pawn.IsValid()) return;
	auto* Controller = Cast<APlayerController>(Pawn->GetController());
	auto* LocalPlayer = Controller && Controller->IsLocalController() && Controller->GetPawn() == Pawn.Get()
		? Controller->GetLocalPlayer() : nullptr;
	auto* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
	if (LocalInput.Get() == Input) return;

	ReleaseLocalResources();
	if (!Input) return;
	LocalInput = Input;
	for (const auto& Entry : Definition->MappingContexts)
	{
		const FGuid Handle = Input->AcquireMappingContext(this, Entry.MappingContext, Entry.Priority, true);
		if (!Handle.IsValid())
		{
			FailEntry(FString::Printf(TEXT("Failed to acquire mapping context %s."), *GetNameSafe(Entry.MappingContext.Get())));
			return;
		}
		ContextHandles.Add(Handle);
	}

	FPlayerInputPolicy Policy;
	Policy.bSetInputMode = false;
	Policy.bBlockGameplay = Definition->bBlockGameplayInput;
	PolicyHandle = Input->AcquireInputPolicy(this, Policy);
	if (!PolicyHandle.IsValid()) FailEntry(TEXT("Failed to acquire input policy."));
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
	GrantedASC.Reset();
}

void UNGCharacterStateResources::Exit()
{
	bEntered = false;
	ReleaseResources();
	Pawn.Reset();
}
