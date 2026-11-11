#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "GameplayTags/NGGameplayTags.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

void UNGEquipmentDefinition::ResolveResources(TArray<FNGEquipmentAbilityGrant>& OutAbilities, TArray<TSubclassOf<UGameplayEffect>>& OutEffects) const
{
	for (const UNGEquipmentAbilitySet* Set : AbilitySets) if (Set) { OutAbilities.Append(Set->Abilities); OutEffects.Append(Set->Effects); }
	OutAbilities.Append(Abilities); OutEffects.Append(Effects);
}

bool UNGEquipmentDefinition::ValidateDefinition(FString& Error) const
{
	if (!SlotTag.MatchesTag(TAG_NGSlotEquipment) || SlotTag == TAG_NGSlotEquipment || VisualTransform.ContainsNaN())
	{ Error = TEXT("Equipment needs a typed inventory equipment slot and a finite visual transform."); return false; }
	if (VisualMode != ENGEquipmentVisualMode::SocketAttachment)
	{ Error = TEXT("Modular skeletal clothing requires its separate presentation package; socket attachment is supported here."); return false; }
	for (const UNGEquipmentAbilitySet* Set : AbilitySets) if (!Set) { Error = TEXT("Null equipment ability set."); return false; }
	TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> ResolvedEffects;
	ResolveResources(Grants, ResolvedEffects);
	FGameplayTagContainer Inputs;
	for (const auto& Entry : Grants)
	{
		const auto& Grant = Entry.Grant;
		const auto* Ability = Grant.Ability.GetDefaultObject();
		if (!Ability || Grant.Ability->HasAnyClassFlags(CLASS_Abstract) || Grant.Level < 1
			|| !Grant.Ability->IsChildOf(UNGGameplayAbilityFromEquipment::StaticClass())
			|| (Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerActor
				&& Ability->GetInstancingPolicy() != EGameplayAbilityInstancingPolicy::InstancedPerExecution))
		{ Error = TEXT("Equipment grants require concrete instanced equipment abilities and a positive level."); return false; }
		if (Entry.bRequiredStartup && (!Grant.bActivateOnEnter ||
			(Ability->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerOnly && Ability->GetNetExecutionPolicy() != EGameplayAbilityNetExecutionPolicy::ServerInitiated)))
		{ Error = TEXT("Required startup must execute on the server."); return false; }
		if (Grant.InputTag.IsValid() && Inputs.HasTagExact(Grant.InputTag))
		{ Error = TEXT("Ambiguous equipment command InputTag."); return false; }
		if (Grant.InputTag.IsValid()) Inputs.AddTag(Grant.InputTag);
	}
	for (const auto& EffectClass : ResolvedEffects)
	{
		const auto* Effect = EffectClass.GetDefaultObject();
		if (!Effect || EffectClass->HasAnyClassFlags(CLASS_Abstract) || Effect->DurationPolicy != EGameplayEffectDurationType::Infinite
			|| Effect->GetStackingType() != EGameplayEffectStackingType::None || !Effect->Executions.IsEmpty() || Effect->Period.GetValueAtLevel(1.f) != 0.f)
		{ Error = TEXT("While-equipped effects must be infinite, nonperiodic and nonstacking; one handle per installation."); return false; }
	}
	Error.Reset(); return true;
}
#if WITH_EDITOR
EDataValidationResult UNGEquipmentDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const auto Parent = Super::IsDataValid(Context); FString Error;
	if (!ValidateDefinition(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
