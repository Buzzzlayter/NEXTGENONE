#include "Gameplay/Input/NGCharacterInputComponent.h"
#include "GameplayTags/NGGameplayTags.h"

#include "Input/PlayerInputSubsystem.h"
#include "NGGameUserSettings.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Character/ALSBaseCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UNGCharacterInputComponent::BindInputPolicy()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UPlayerInputSubsystem* Input = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
	if (!Input) return;
	LocalInput = Input;
	Input->OnGameplayInputBlockedChanged.AddUniqueDynamic(this, &ThisClass::GameplayBlocked);
	GameplayBlocked(Input->IsGameplayInputBlocked());
}

void UNGCharacterInputComponent::UnbindInputPolicy()
{
	CancelPendingDash();
	if (UPlayerInputSubsystem* Input = LocalInput.Get())
	{
		Input->OnGameplayInputBlockedChanged.RemoveDynamic(this, &ThisClass::GameplayBlocked);
	}
	LocalInput.Reset();
}

void UNGCharacterInputComponent::GameplayBlocked(bool bBlocked)
{
	if (bBlocked)
	{
		CancelPendingDash();
	}
	SetGameplayInputBlocked(bBlocked);
	if (bBlocked)
		if (UNGAbilitySystemComponent* ASC = GetInputASC(); ASC && ASC->GetAvatarActor() == GetOwner())
		{
			FGameplayTagContainer Movement; Movement.AddTag(TAG_NGMovementAbility);
			ASC->CancelAbilities(&Movement);
		}
}

void UNGCharacterInputComponent::HandleNativeInput_Implementation(FGameplayTag Tag, const FInputActionValue& Value, ETriggerEvent Phase)
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	AALSBaseCharacter* ALS = Cast<AALSBaseCharacter>(Pawn);
	const bool bReleased = Phase == ETriggerEvent::Completed || Phase == ETriggerEvent::Canceled;
	const bool bStarted = Phase == ETriggerEvent::Triggered && !ActivatedNativeActions.Contains(Tag);
	if (Phase == ETriggerEvent::Triggered) ActivatedNativeActions.Add(Tag);
	if (bReleased) ActivatedNativeActions.Remove(Tag);
	if (!Pawn) return;
	if (Tag == TAG_InputMove && Phase != ETriggerEvent::Started)
	{
		if (Value.GetValueType() != EInputActionValueType::Axis2D) return;
		FVector2D Move = Value.Get<FVector2D>();
		if (const auto* Settings = UNGGameUserSettings::Get(); Settings && LocalInput.IsValid() && LocalInput->IsGamepad())
		{
			Move = Settings->FilterMove(Move);
		}
		if (ALS) { ALS->ForwardMovementAction(Move.Y); ALS->RightMovementAction(Move.X); }
		return;
	}
	if ((Tag == TAG_InputLookMouse || Tag == TAG_InputLookStick) && Phase == ETriggerEvent::Triggered)
	{
		if (Value.GetValueType() != EInputActionValueType::Axis2D) return;
		FVector2D Look = Value.Get<FVector2D>();
		const bool bStick = Tag == TAG_InputLookStick;
		if (const auto* Settings = UNGGameUserSettings::Get())
		{
			const auto* ASC = GetInputASC();
			Look = Settings->ScaleLook(Look, bStick, ASC && ASC->HasMatchingGameplayTag(TAG_NGWeaponAiming));
		}
		const float Delta = GetWorld()->GetDeltaSeconds();
		Pawn->AddControllerYawInput(Look.X * (bStick ? StickYawDegreesPerSecond * Delta : 1.f));
		Pawn->AddControllerPitchInput(Look.Y * (bStick ? StickPitchDegreesPerSecond * Delta : 1.f));
		return;
	}
	if (Tag == TAG_InputSprint && ALS && (bStarted || bReleased)) { ALS->SprintAction(bStarted); return; }
	if (bStarted && ALS)
	{
		if (Tag == TAG_InputStance) { ALS->StanceAction(); return; }
		else if (Tag == TAG_InputWalk) { ALS->WalkAction(); return; }
		else if (Tag == TAG_InputCameraTap) { ALS->CameraTapAction(); return; }
		else if (Tag == TAG_InputCameraHeld) { ALS->CameraHeldAction(); return; }
		else if (Tag == TAG_InputVelocity) { ALS->VelocityDirectionAction(); return; }
		else if (Tag == TAG_InputLooking) { ALS->LookingDirectionAction(); return; }
	}
	// Native compatibility: unknown action tags are forwarded without domain-specific routing.
	if (UNGAbilitySystemComponent* ASC = GetInputASC(); ASC && ASC->GetAvatarActor() == Pawn)
	{
		if (bStarted) DispatchAbilityPress(*ASC, Tag);
		else if (bReleased) ASC->AbilityInputTagReleased(Tag);
	}
}

void UNGCharacterInputComponent::DispatchAbilityPress(UNGAbilitySystemComponent& ASC, FGameplayTag Tag)
{
	const auto* Settings = UNGGameUserSettings::Get();
	UWorld* World = GetWorld();
	const float Delay = Settings ? FMath::Clamp(Settings->GetDashDirectionRegistrationDelay(), 0.f, .5f) : 0.f;
	if (Tag != TAG_NGInputDash || !World || Delay <= UE_SMALL_NUMBER)
	{
		Super::DispatchAbilityPress(ASC, Tag);
		return;
	}

	FTimerManager& Timers = World->GetTimerManager();
	if (!Timers.TimerExists(DashInputTimer))
	{
		Timers.SetTimer(DashInputTimer, this, &ThisClass::DispatchDash, Delay, false);
	}
}

void UNGCharacterInputComponent::DispatchDash()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	auto* ASC = GetInputASC();
	if (!CanProcessInput() || !ASC || ASC->GetAvatarActor() != Pawn)
	{
		return;
	}

	ASC->AbilityInputTagPressed(TAG_NGInputDash);
	if (!IsAbilityInputHeld(TAG_NGInputDash) && !ActivatedNativeActions.Contains(TAG_NGInputDash))
	{
		ASC->AbilityInputTagReleased(TAG_NGInputDash);
	}
}

void UNGCharacterInputComponent::CancelPendingDash()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DashInputTimer);
	}
}
