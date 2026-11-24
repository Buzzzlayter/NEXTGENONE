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
	if (!InputTag.IsValid() || !AbilityActorInfo.IsValid() ||
		(!AbilityActorInfo->IsLocallyControlled() && !IsOwnerActorAuthoritative())) return;
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
	if (!InputTag.IsValid() || !AbilityActorInfo.IsValid() ||
		(!AbilityActorInfo->IsLocallyControlled() && !IsOwnerActorAuthoritative())) return;
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

UNGHealthSet::UNGHealthSet() : Health(100.f), MaxHealth(100.f), Shield(100.f), MaxShield(100.f), IncomingDamage(0.f) {}
FGameplayAttribute UNGHealthSet::GetHealthAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, Health)); }
FGameplayAttribute UNGHealthSet::GetMaxHealthAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, MaxHealth)); }
FGameplayAttribute UNGHealthSet::GetShieldAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, Shield)); }
FGameplayAttribute UNGHealthSet::GetMaxShieldAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, MaxShield)); }
FGameplayAttribute UNGHealthSet::GetIncomingDamageAttribute() { return FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UNGHealthSet, IncomingDamage)); }
void UNGHealthSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION_NOTIFY(UNGHealthSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNGHealthSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNGHealthSet, Shield, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UNGHealthSet, MaxShield, COND_None, REPNOTIFY_Always);
}
void UNGHealthSet::OnRep_Health(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNGHealthSet, Health, Old); }
void UNGHealthSet::OnRep_MaxHealth(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNGHealthSet, MaxHealth, Old); }
void UNGHealthSet::OnRep_Shield(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNGHealthSet, Shield, Old); }
void UNGHealthSet::OnRep_MaxShield(const FGameplayAttributeData& Old) { GAMEPLAYATTRIBUTE_REPNOTIFY(UNGHealthSet, MaxShield, Old); }
void UNGHealthSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& Value)
{
	Super::PreAttributeChange(Attribute, Value);
	if (!FMath::IsFinite(Value)) Value = 0.f;
	if (Attribute == GetMaxHealthAttribute()) Value = FMath::Max(1.f, Value);
	if (Attribute == GetHealthAttribute()) Value = FMath::Clamp(Value, 0.f, MaxHealth.GetCurrentValue());
	if (Attribute == GetMaxShieldAttribute()) Value = FMath::Max(0.f, Value);
	if (Attribute == GetShieldAttribute()) Value = FMath::Clamp(Value, 0.f, MaxShield.GetCurrentValue());
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
		if (!Avatar || IntendedPawn != Avatar || ASC->HasMatchingGameplayTag(TAG_NGDead) || !FMath::IsFinite(Damage) || Damage <= 0.f)
			return;

		const float CurrentShield = Shield.GetCurrentValue();
		const float Absorbed = FMath::Min(CurrentShield, Damage);
		if (Absorbed > 0.f) ASC->SetNumericAttributeBase(GetShieldAttribute(), CurrentShield - Absorbed);
		const float HealthDamage = Damage - Absorbed;
		if (HealthDamage > 0.f && ASC->GetAvatarActor() == Avatar && !ASC->HasMatchingGameplayTag(TAG_NGDead))
			ASC->SetNumericAttributeBase(GetHealthAttribute(), FMath::Clamp(Health.GetCurrentValue() - HealthDamage, 0.f, MaxHealth.GetCurrentValue()));
		return;
	}
	if (Data.EvaluatedData.Attribute == GetMaxHealthAttribute() || Data.EvaluatedData.Attribute == GetHealthAttribute())
		ASC->SetNumericAttributeBase(GetHealthAttribute(), FMath::Clamp(Health.GetCurrentValue(), 0.f, FMath::Max(1.f, MaxHealth.GetCurrentValue())));
	if (Data.EvaluatedData.Attribute == GetMaxShieldAttribute() || Data.EvaluatedData.Attribute == GetShieldAttribute())
		ASC->SetNumericAttributeBase(GetShieldAttribute(), FMath::Clamp(Shield.GetCurrentValue(), 0.f, MaxShield.GetCurrentValue()));
}
UNGInitHealthEffect::UNGInitHealthEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat InitialHealth;
	InitialHealth.DataName = InitialHealthMagnitude;
	FGameplayModifierInfo Max;
	Max.Attribute = UNGHealthSet::GetMaxHealthAttribute(); Max.ModifierOp = EGameplayModOp::Override; Max.ModifierMagnitude = FGameplayEffectModifierMagnitude(InitialHealth);
	Modifiers.Add(Max);
	FGameplayModifierInfo HP = Max; HP.Attribute = UNGHealthSet::GetHealthAttribute(); Modifiers.Add(HP);
	FSetByCallerFloat InitialShield;
	InitialShield.DataName = InitialShieldMagnitude;
	Max.Attribute = UNGHealthSet::GetMaxShieldAttribute(); Max.ModifierMagnitude = FGameplayEffectModifierMagnitude(InitialShield); Modifiers.Add(Max);
	Max.Attribute = UNGHealthSet::GetShieldAttribute(); Modifiers.Add(Max);
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
