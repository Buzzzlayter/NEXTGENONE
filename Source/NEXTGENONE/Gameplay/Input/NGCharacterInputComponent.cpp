#include "Gameplay/Input/NGCharacterInputComponent.h"
#include "GameplayTags/NGGameplayTags.h"

#include "Input/PlayerInputSubsystem.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Character/ALSBaseCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

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
	if (UPlayerInputSubsystem* Input = LocalInput.Get())
	{
		Input->OnGameplayInputBlockedChanged.RemoveDynamic(this, &ThisClass::GameplayBlocked);
	}
	LocalInput.Reset();
}

void UNGCharacterInputComponent::GameplayBlocked(bool bBlocked)
{
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
		const FVector2D Move = Value.Get<FVector2D>();
		if (ALS) { ALS->ForwardMovementAction(Move.Y); ALS->RightMovementAction(Move.X); }
		return;
	}
	if ((Tag == TAG_InputLookMouse || Tag == TAG_InputLookStick) && Phase == ETriggerEvent::Triggered)
	{
		if (Value.GetValueType() != EInputActionValueType::Axis2D) return;
		const FVector2D Look = Value.Get<FVector2D>();
		const bool bStick = Tag == TAG_InputLookStick;
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
		if (bStarted) ASC->AbilityInputTagPressed(Tag);
		else if (bReleased) ASC->AbilityInputTagReleased(Tag);
	}

}
