#include "Character/NGControlStateDefinition.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "InputMappingContext.h"
#include "Input/NGGameplayInputConfig.h"
#include "InputAction.h"
#include "AbilitySystemGlobals.h"
#include "GameplayCueNotify_Static.h"
#include "GameplayCueManager.h"
#include "GameplayCueSet.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UNGControlStateDefinition::ValidateResources(FString& Error) const
{
	if (!StateTag.IsValid()) { Error = TEXT("StateTag is required."); return false; }
	if (PresentationCue && !PresentationCue.GetDefaultObject()->GameplayCueTag.IsValid())
	{ Error = TEXT("PresentationCue requires a valid GameplayCue tag."); return false; }
	TSet<UClass*> AbilityClasses;
	TSet<FGameplayTag> Tags;
	auto ValidEffect = [](TSubclassOf<UGameplayEffect> Effect)
	{
		return Effect && Effect.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Instant &&
			Effect.GetDefaultObject()->GetStackingType() == EGameplayEffectStackingType::None;
	};
	for (const auto& Grant : Abilities)
	{
		if (!Grant.Ability || Grant.Level < 1 || AbilityClasses.Contains(Grant.Ability.Get()) || (Grant.InputTag.IsValid() && Tags.Contains(Grant.InputTag)))
		{ Error = TEXT("Invalid or duplicate state ability class/input tag."); return false; }
		AbilityClasses.Add(Grant.Ability.Get());
		if (Grant.InputTag.IsValid()) Tags.Add(Grant.InputTag);
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

FGameplayTag UNGControlStateDefinition::RegisterPresentationCue() const
{
	if (!PresentationCue) return {};
	const FGameplayTag Tag = PresentationCue.GetDefaultObject()->GameplayCueTag;
	if (Tag.IsValid())
		if (auto* Manager = UAbilitySystemGlobals::Get().GetGameplayCueManager())
		{
			if (!Manager->GetRuntimeCueSet()) Manager->InitializeRuntimeObjectLibrary();
			if (auto* Set = Manager->GetRuntimeCueSet()) Set->AddCues({FGameplayCueReferencePair(Tag, FSoftObjectPath(PresentationCue.Get()))});
			else return {};
		}
	return Tag;
}

bool UNGControlStateDefinition::ValidateInputMappings(const UNGGameplayInputConfig* InputConfig, FString& Error) const
{
	if (MappingContexts.IsEmpty()) { Error.Reset(); return true; }
	if (!InputConfig) { Error = TEXT("State mapping contexts require the Pawn's CharacterDefinition.InputConfig routes."); return false; }
	TSet<const UInputAction*> RoutedActions;
	for (const auto& Route : InputConfig->NativeInputActions) if (Route.InputAction) RoutedActions.Add(Route.InputAction.Get());
	for (const auto& Route : InputConfig->AbilityInputActions) if (Route.InputAction) RoutedActions.Add(Route.InputAction.Get());
	for (const auto& Entry : MappingContexts)
	{
		if (!Entry.MappingContext) { Error = TEXT("Null state mapping context."); return false; }
		for (const auto& Mapping : Entry.MappingContext->GetMappings())
			if (Mapping.Action && !RoutedActions.Contains(Mapping.Action.Get()))
			{
				Error = FString::Printf(TEXT("State context %s maps %s without a Native/GAS route in %s."), *Entry.MappingContext->GetName(), *Mapping.Action->GetName(), *InputConfig->GetName());
				return false;
			}
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
