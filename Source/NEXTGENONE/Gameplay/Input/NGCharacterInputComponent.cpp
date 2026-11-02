#include "NGCharacterInputComponent.h"

#include "Character/NGCharacterDefinition.h"
#include "Input/NGGameplayInputConfig.h"
#include "Input/PlayerInputSubsystem.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Character/ALSBaseCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "../NGCombat.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputMove, "Input.Move");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputLookMouse, "Input.Look.Mouse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputLookStick, "Input.Look.Stick");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputSprint, "Input.Sprint");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputAim, "Input.Aim");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputStance, "Input.Stance");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputWalk, "Input.Walk");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputRagdoll, "Input.Ragdoll");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputCameraTap, "Input.Camera.Tap");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputCameraHeld, "Input.Camera.Held");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputVelocity, "Input.Rotation.Velocity");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputLooking, "Input.Rotation.Looking");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputPrimary, "Input.Combat.Primary");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputEquip, "Input.Equipment.Equip");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputUnequip, "Input.Equipment.Unequip");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputSlot, "Input.Equipment.SelectSlot");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_InputReload, "Input.Equipment.Reload");

void UNGCharacterInputComponent::AcquireInputContexts()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	UPlayerInputSubsystem* Input = PC && PC->GetLocalPlayer() ? PC->GetLocalPlayer()->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
	if (!Input || !CharacterDefinition || !CharacterDefinition->InputConfig) return;
	LocalInput = Input;
	Input->OnGameplayInputBlockedChanged.AddUniqueDynamic(this, &ThisClass::GameplayBlocked);
	GameplayBlocked(Input->IsGameplayInputBlocked());
	for (const auto& Entry : CharacterDefinition->InputConfig->MappingContexts)
		ContextHandles.Add(Input->AcquireMappingContext(this, Entry.MappingContext, Entry.Priority, true));
}

void UNGCharacterInputComponent::ReleaseInputContexts()
{
	if (UPlayerInputSubsystem* Input = LocalInput.Get())
	{
		Input->OnGameplayInputBlockedChanged.RemoveDynamic(this, &ThisClass::GameplayBlocked);
		for (FGuid Handle : ContextHandles) Input->ReleaseMappingContext(Handle);
	}
	ContextHandles.Reset(); LocalInput.Reset();
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
	UNGCombatComponent* Combat = Pawn ? Pawn->FindComponentByClass<UNGCombatComponent>() : nullptr;
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
	if (Tag == TAG_InputPrimary && Combat)
	{
		if (bStarted) Combat->StartPrimaryInput();
		else if (bReleased) Combat->StopPrimaryInput();
		return;
	}
	if (Tag == TAG_InputAim && (bStarted || bReleased))
	{
		if (ALS) ALS->AimAction(bStarted);
		else if (Combat) Combat->RequestAim(bStarted);
		return;
	}
	if (Tag == TAG_NGInputJump && ALS && (bStarted || bReleased)) { ALS->JumpAction(bStarted); return; }
	if (Tag == TAG_InputSprint && ALS && (bStarted || bReleased)) { ALS->SprintAction(bStarted); return; }
	if (bStarted && Combat)
	{
		if (Tag == TAG_InputEquip) { Combat->StopPrimaryInput(); Combat->RequestEquip(true); }
		else if (Tag == TAG_InputUnequip) { Combat->StopPrimaryInput(); Combat->RequestEquip(false); }
		else if (Tag == TAG_InputSlot) { Combat->StopPrimaryInput(); Combat->RequestSelectSlot(FMath::RoundToInt(Value.GetMagnitude()) - 1); }
		else if (Tag == TAG_InputReload) Combat->RequestReload();
	}
	if (bStarted && ALS)
	{
		if (Tag == TAG_InputStance) ALS->StanceAction();
		else if (Tag == TAG_InputWalk) ALS->WalkAction();
		else if (Tag == TAG_InputRagdoll) ALS->RagdollAction();
		else if (Tag == TAG_InputCameraTap) ALS->CameraTapAction();
		else if (Tag == TAG_InputCameraHeld) ALS->CameraHeldAction();
		else if (Tag == TAG_InputVelocity) ALS->VelocityDirectionAction();
		else if (Tag == TAG_InputLooking) ALS->LookingDirectionAction();
	}
}
