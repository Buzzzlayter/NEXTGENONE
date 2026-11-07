#include "AbilitySystem/NGAbilitySystem.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"


bool UNGAbilitySystemComponent::TryActivateAbilityByInputTag(FGameplayTag InputTag)
{
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
		AActor* Avatar = ASC->GetAvatarActor();
		// The server creates this context with the *hit Pawn*, never a client target.
		// Prevent deferred damage intended for an old corpse from reaching a new life.
		const AActor* IntendedPawn = Cast<AActor>(Data.EffectSpec.GetContext().GetSourceObject());
		if (Avatar && !ASC->HasMatchingGameplayTag(TAG_NGDead) && IntendedPawn == Avatar && FMath::IsFinite(Damage) && Damage > 0.f)
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
	// State cancellation callbacks can re-enter cleanup.
	const auto OwnedAbilities = MoveTemp(Abilities);
	const auto OwnedEffects = MoveTemp(Effects);
	Abilities.Reset(); Effects.Reset();
	for (const auto& Handle : OwnedAbilities) { ASC->CancelAbilityHandle(Handle); ASC->ClearAbility(Handle); }
	for (const auto& Handle : OwnedEffects) ASC->RemoveActiveGameplayEffect(Handle);
}
FGameplayAbilitySpecHandle FNGAbilityGrant::Give(UAbilitySystemComponent* ASC, UObject* Source) const
{
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || !Ability || Level < 1) return {};
	FGameplayAbilitySpec Spec(Ability, Level); Spec.SourceObject = Source;
	if (InputTag.IsValid()) Spec.GetDynamicSpecSourceTags().AddTag(InputTag);
	return ASC->GiveAbility(Spec);
}
