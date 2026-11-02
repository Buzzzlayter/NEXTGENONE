#include "Input/NGGameplayInputComponent.h"

#include "AbilitySystem/NGAbilitySystem.h"
#include "AbilitySystemGlobals.h"
#include "Character/NGCharacterDefinition.h"
#include "Input/NGGameplayInputConfig.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UNGAbilitySystemComponent* UNGGameplayInputComponent::GetInputASC() const
{
	return Cast<UNGAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner()));
}

bool UNGGameplayInputComponent::CanProcessInput() const
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	return Pawn && Pawn->IsLocallyControlled() && Pawn->GetController() && Pawn->GetController()->GetPawn() == Pawn && !bGameplayBlocked && BoundInput.IsValid();
}

void UNGGameplayInputComponent::InitializeInput(UInputComponent* Input)
{
	ShutdownInput();
	const APawn* Pawn = Cast<APawn>(GetOwner());
	auto* Enhanced = Cast<UEnhancedInputComponent>(Input);
	if (!Pawn || !Pawn->IsLocallyControlled() || !Pawn->GetController() || Pawn->GetController()->GetPawn() != Pawn || !Enhanced) return;
	const UNGGameplayInputConfig* Config = CharacterDefinition ? CharacterDefinition->InputConfig.Get() : nullptr;
	if (!Config)
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: assign CharacterDefinition.InputConfig on the Pawn's GameplayInput component; no legacy bindings are installed."), *GetNameSafe(GetOwner()));
		return;
	}
	BoundInput = Enhanced;
	TSet<const UInputAction*> BoundActions;
	for (const FNGTaggedInputAction& Entry : Config->NativeInputActions)
	{
		if (!Entry.InputAction || !Entry.InputTag.IsValid() || BoundActions.Contains(Entry.InputAction)) continue;
		BoundActions.Add(Entry.InputAction);
		for (ETriggerEvent Phase : { ETriggerEvent::Started, ETriggerEvent::Triggered, ETriggerEvent::Completed, ETriggerEvent::Canceled })
			BindingHandles.Add(Enhanced->BindAction(Entry.InputAction, Phase, this, &ThisClass::NativeInput, Entry.InputTag, Phase).GetHandle());
	}
	for (const FNGTaggedInputAction& Entry : Config->AbilityInputActions)
	{
		if (!Entry.InputAction || !Entry.InputTag.IsValid() || BoundActions.Contains(Entry.InputAction)) continue;
		BoundActions.Add(Entry.InputAction);
		BindingHandles.Add(Enhanced->BindAction(Entry.InputAction, ETriggerEvent::Triggered, this, &ThisClass::AbilityPressed, Entry.InputTag).GetHandle());
		for (ETriggerEvent Phase : { ETriggerEvent::Completed, ETriggerEvent::Canceled })
			BindingHandles.Add(Enhanced->BindAction(Entry.InputAction, Phase, this, &ThisClass::AbilityReleased, Entry.InputTag).GetHandle());
	}
	AcquireInputContexts();
}

void UNGGameplayInputComponent::NativeInput(const FInputActionValue& Value, FGameplayTag Tag, ETriggerEvent Phase)
{
	const bool bReleased = Phase == ETriggerEvent::Completed || Phase == ETriggerEvent::Canceled;
	if (bReleased)
	{
		SuppressedUntilRelease.Remove(Tag);
		if (HeldNative.Remove(Tag)) HandleNativeInput(Tag, FInputActionValue(Value.GetValueType(), FVector::ZeroVector), Phase);
		return;
	}
	if (!CanProcessInput()) { SuppressedUntilRelease.Add(Tag); return; }
	if (SuppressedUntilRelease.Contains(Tag)) return;
	HeldNative.Add(Tag, Value);
	HandleNativeInput(Tag, Value, Phase);
}

void UNGGameplayInputComponent::AbilityPressed(const FInputActionValue& Value, FGameplayTag Tag)
{
	if (!CanProcessInput()) { SuppressedUntilRelease.Add(Tag); return; }
	if (SuppressedUntilRelease.Contains(Tag) || HeldAbilities.Contains(Tag)) return;
	if (UNGAbilitySystemComponent* ASC = GetInputASC())
	{
		if (ASC->GetAvatarActor() != GetOwner()) return;
		PressedASC = ASC;
		HeldAbilities.Add(Tag);
		ASC->AbilityInputTagPressed(Tag);
	}
}

void UNGGameplayInputComponent::AbilityReleased(const FInputActionValue& Value, FGameplayTag Tag)
{
	SuppressedUntilRelease.Remove(Tag);
	if (HeldAbilities.Remove(Tag))
		if (UNGAbilitySystemComponent* ASC = PressedASC.Get(); ASC && ASC->GetAvatarActor() == GetOwner()) ASC->AbilityInputTagReleased(Tag);
}

void UNGGameplayInputComponent::ReleaseHeldInput()
{
	const TSet<FGameplayTag> Abilities = MoveTemp(HeldAbilities);
	const TMap<FGameplayTag, FInputActionValue> Native = MoveTemp(HeldNative);
	if (UNGAbilitySystemComponent* ASC = PressedASC.Get(); ASC && ASC->GetAvatarActor() == GetOwner())
		for (FGameplayTag Tag : Abilities) { SuppressedUntilRelease.Add(Tag); ASC->AbilityInputTagReleased(Tag); }
	for (const auto& Entry : Native)
	{
		SuppressedUntilRelease.Add(Entry.Key);
		HandleNativeInput(Entry.Key, FInputActionValue(Entry.Value.GetValueType(), FVector::ZeroVector), ETriggerEvent::Canceled);
	}
	PressedASC.Reset();
}

void UNGGameplayInputComponent::SetGameplayInputBlocked(bool bBlocked)
{
	if (bGameplayBlocked == bBlocked) return;
	bGameplayBlocked = bBlocked;
	if (bBlocked) ReleaseHeldInput();
	else SuppressedUntilRelease.Reset(); // contexts are rebuilt with IgnoreAllPressedKeysUntilRelease by the coordinator
}

void UNGGameplayInputComponent::ShutdownInput()
{
	ReleaseHeldInput();
	ReleaseInputContexts();
	if (UEnhancedInputComponent* Input = BoundInput.Get())
		for (uint32 Handle : BindingHandles) Input->RemoveBindingByHandle(Handle);
	BindingHandles.Reset(); BoundInput.Reset(); SuppressedUntilRelease.Reset(); bGameplayBlocked = false;
}

void UNGGameplayInputComponent::EndPlay(EEndPlayReason::Type Reason)
{
	ShutdownInput();
	Super::EndPlay(Reason);
}
