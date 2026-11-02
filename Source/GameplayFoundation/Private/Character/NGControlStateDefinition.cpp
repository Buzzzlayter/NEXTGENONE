#include "Character/NGControlStateDefinition.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "InputMappingContext.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UNGControlStateDefinition::ValidateResources(FString& Error) const
{
	TSet<const UNGAbilitySet*> Sets;
	TSet<UClass*> Abilities;
	TSet<FGameplayTag> Tags;
	auto ValidEffect = [](TSubclassOf<UGameplayEffect> Effect)
	{
		return Effect && Effect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Instant &&
			Effect.GetDefaultObject()->GetStackingType() == EGameplayEffectStackingType::None;
	};
	for (const UNGAbilitySet* Set : AbilitySets)
	{
		if (!Set || Sets.Contains(Set)) { Error = TEXT("Null or duplicate Ability Set."); return false; }
		Sets.Add(Set);
		for (const auto& Grant : Set->Abilities)
		{
			if (!Grant.Ability || Grant.Level < 1 || Abilities.Contains(Grant.Ability.Get()) || (Grant.InputTag.IsValid() && Tags.Contains(Grant.InputTag)))
			{ Error = TEXT("Invalid or duplicate state ability class/input tag."); return false; }
			Abilities.Add(Grant.Ability.Get());
			if (Grant.InputTag.IsValid()) Tags.Add(Grant.InputTag);
		}
		for (auto Effect : Set->Effects)
			if (!ValidEffect(Effect)) { Error = TEXT("State effects require Duration/Infinite and StackingType=None: Instant cannot be undone and aggregated handles can affect another scope."); return false; }
	}
	for (auto Effect : Effects)
		if (!ValidEffect(Effect)) { Error = TEXT("State effects require Duration/Infinite and StackingType=None."); return false; }
	TSet<const UInputMappingContext*> Contexts;
	for (const auto& Entry : MappingContexts)
	{
		if (!Entry.MappingContext || Entry.Priority < 0 || Contexts.Contains(Entry.MappingContext))
		{ Error = TEXT("Null/duplicate mapping context or negative priority."); return false; }
		Contexts.Add(Entry.MappingContext);
	}
	Error.Reset(); return true;
}
#if WITH_EDITOR
EDataValidationResult UNGControlStateDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const auto Parent = Super::IsDataValid(Context);
	FString Error;
	if (!ValidateResources(Error)) { Context.AddError(FText::FromString(Error)); return EDataValidationResult::Invalid; }
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
