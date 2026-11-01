#include "NGAbilitySystem.h"
#include "NextgenoneCharacter.h"
#include "NGInteraction.h"
#include "GameplayEffectExtension.h"
#include "NativeGameplayTags.h"
#include "Net/UnrealNetwork.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGDamage, "Data.Damage");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGDead, "State.Dead");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputJump, "Input.Jump");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputDash, "Input.Dash");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputMantle, "Input.Mantle");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputPickup, "Input.Interaction.PickupWeapon");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGDashActive, "State.Action.Dash");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGMantleActive, "State.Action.Mantle");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputPunch, "Input.Combat.Punch");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGPunchActive, "State.Action.Punch");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGMovementAbility, "Ability.Type.Movement");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGReloadActive, "State.Action.Reload");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGMagazineAmmo, "Weapon.MagazineAmmo");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGReserveAmmo, "Weapon.ReserveAmmo");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInputInteract, "Input.Interaction.Interact");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInteractEvent, "Event.Interaction.Request");

bool UNGAbilitySystemComponent::TryActivateAbilityByInputTag(FGameplayTag InputTag)
{
	if (InputTag==TAG_NGInputInteract || InputTag==TAG_NGInputPickup)
	{
		if (auto* Pawn=Cast<ANextgenoneCharacter>(GetAvatarActor())) { Pawn->Interaction->RequestInteract(); return true; }
		return false;
	}
	if (!InputTag.IsValid() || !AbilityActorInfo.IsValid() ||
		(!AbilityActorInfo->IsLocallyControlled() && !IsOwnerActorAuthoritative())) return false;
	FGameplayAbilitySpecHandle Handle;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (const FGameplayAbilitySpec& Spec : GetActivatableAbilities())
			if (Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) { Handle = Spec.Handle; break; }
	}
	return Handle.IsValid() && TryActivateAbility(Handle);
}

void UNGAbilitySystemComponent::NotifyAbilityInput(FGameplayAbilitySpec& Spec, bool bPressed)
{
	if (bPressed) AbilitySpecInputPressed(Spec);
	else AbilitySpecInputReleased(Spec);
	// Ability tasks (including Blueprint Wait Input Release) use generic events.
	for (UGameplayAbility* Instance : Spec.GetAbilityInstances())
	{
		if (Instance && Instance->IsActive())
			InvokeReplicatedEvent(bPressed ? EAbilityGenericReplicatedEvent::InputPressed : EAbilityGenericReplicatedEvent::InputReleased,
				Spec.Handle, Instance->GetCurrentActivationInfo().GetActivationPredictionKey());
	}
}
void UNGAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (InputTag==TAG_NGInputInteract || InputTag==TAG_NGInputPickup) { TryActivateAbilityByInputTag(InputTag); return; }
	if (!InputTag.IsValid() || !AbilityActorInfo.IsValid() || !AbilityActorInfo->IsLocallyControlled()) return;
	TArray<FGameplayAbilitySpecHandle> ToActivate;
	{
		ABILITYLIST_SCOPE_LOCK();
		for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
		{
			if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag) || Spec.InputPressed) continue;
			Spec.InputPressed = true;
			if (Spec.IsActive()) NotifyAbilityInput(Spec, true);
			else ToActivate.Add(Spec.Handle);
		}
	}
	for (const auto Handle : ToActivate) TryActivateAbility(Handle);
}
void UNGAbilitySystemComponent::AbilityInputTagReleased(FGameplayTag InputTag)
{
	if (!InputTag.IsValid() || !AbilityActorInfo.IsValid() || !AbilityActorInfo->IsLocallyControlled()) return;
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities())
	{
		if (!Spec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;
		Spec.InputPressed = false;
		if (Spec.IsActive()) NotifyAbilityInput(Spec, false);
	}
}
void UNGAbilitySystemComponent::ClearAbilityInput()
{
	ABILITYLIST_SCOPE_LOCK();
	for (FGameplayAbilitySpec& Spec : GetActivatableAbilities()) Spec.InputPressed = false;
}

UNGHealthSet::UNGHealthSet() : Health(100.f), MaxHealth(100.f), IncomingDamage(0.f) {}
FGameplayAttribute UNGHealthSet::GetHealthAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, Health)); }
FGameplayAttribute UNGHealthSet::GetMaxHealthAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, MaxHealth)); }
FGameplayAttribute UNGHealthSet::GetIncomingDamageAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, IncomingDamage)); }
void UNGHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UNGHealthSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNGHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
}
void UNGHealthSet::OnRep_Health(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNGHealthSet, Health, Old); }
void UNGHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNGHealthSet, MaxHealth, Old); }
void UNGHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& Value)
{
	Super::PreAttributeChange(Attribute, Value);
	if (!FMath::IsFinite(Value)) Value = 0.f;
	if (Attribute == GetMaxHealthAttribute()) Value = FMath::Max(1.f, Value);
	if (Attribute == GetHealthAttribute()) Value = FMath::Clamp(Value, 0.f, MaxHealth.GetCurrentValue());
}
void UNGHealthSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		const float Damage = IncomingDamage.GetCurrentValue();
		ASC->SetNumericAttributeBase(GetIncomingDamageAttribute(), 0.f);
		ANextgenoneCharacter* Avatar = Cast<ANextgenoneCharacter>(ASC->GetAvatarActor());
		// The server creates this context with the *hit Pawn*, never a client target.
		// Prevent deferred damage intended for an old corpse from reaching a new life.
		const AActor* IntendedPawn = Cast<AActor>(Data.EffectSpec.GetContext().GetSourceObject());
		if (Avatar && !Avatar->IsDead() && IntendedPawn == Avatar && FMath::IsFinite(Damage) && Damage > 0.f)
			ASC->SetNumericAttributeBase(GetHealthAttribute(), FMath::Clamp(Health.GetCurrentValue() - Damage, 0.f, MaxHealth.GetCurrentValue()));
	}
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute() || Data.EvaluatedData.Attribute == GetHealthAttribute())
		ASC->SetNumericAttributeBase(GetHealthAttribute(), FMath::Clamp(Health.GetCurrentValue(), 0.f, FMath::Max(1.f, MaxHealth.GetCurrentValue())));
}
UNGInitHealthEffect::UNGInitHealthEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Max;
	Max.Attribute = UNGHealthSet::GetMaxHealthAttribute(); Max.ModifierOp = EGameplayModOp::Override; Max.ModifierMagnitude = FScalableFloat(100.f);
	Modifiers.Add(Max);
	FGameplayModifierInfo HP = Max; HP.Attribute = UNGHealthSet::GetHealthAttribute(); Modifiers.Add(HP);
}
UNGDamageEffect::UNGDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FGameplayModifierInfo Mod;
	Mod.Attribute = UNGHealthSet::GetIncomingDamageAttribute(); Mod.ModifierOp = EGameplayModOp::Additive;
	FSetByCallerFloat Magnitude; Magnitude.DataTag = TAG_NGDamage;
	Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(Magnitude); Modifiers.Add(Mod);
}
void FNGGrantedHandles::Remove(UAbilitySystemComponent* ASC)
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative()) return;
	for (const auto& Handle : Abilities) { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); }
	for (const auto& Handle : Effects) ASC->RemoveActiveGameplayEffect(Handle);
	Abilities.Reset(); Effects.Reset();
}
void UNGAbilitySet::Give(UAbilitySystemComponent* ASC, UObject* Source, FNGGrantedHandles& Out) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || !Out.Abilities.IsEmpty() || !Out.Effects.IsEmpty()) return;
	for (const FNGAbilityGrant& Grant : Abilities)
	{
		if (!Grant.Ability) continue;
		FGameplayAbilitySpec Spec(Grant.Ability, FMath::Max(1, Grant.Level)); Spec.SourceObject = Source;
		if (Grant.InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(Grant.InputTag);
		Out.Abilities.Add(ASC->GiveAbility(Spec));
	}
	for (auto Effect : Effects)
		if (Effect) Out.Effects.Add(ASC->ApplyGameplayEffectToSelf(Effect->GetDefaultObject<UGameplayEffect>(), 1.f, ASC->MakeEffectContext()));
}
