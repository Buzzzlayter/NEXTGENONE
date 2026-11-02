#include "Input/NGGameplayInputConfig.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace
{
	UInputAction* FindAction(const TArray<FNGTaggedInputAction>& Actions, FGameplayTag Tag)
	{
		if (!Tag.IsValid()) return nullptr;
		for (const FNGTaggedInputAction& Entry : Actions)
		{
			if (Entry.InputTag == Tag) return Entry.InputAction.Get();
		}
		return nullptr;
	}
}

UInputAction* UNGGameplayInputConfig::FindNativeInputAction(FGameplayTag InputTag) const
{
	return FindAction(NativeInputActions, InputTag);
}

UInputAction* UNGGameplayInputConfig::FindAbilityInputAction(FGameplayTag InputTag) const
{
	return FindAction(AbilityInputActions, InputTag);
}

#if WITH_EDITOR
EDataValidationResult UNGGameplayInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;
	auto Error = [&Context, &bValid](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		bValid = false;
	};

	TSet<const UInputMappingContext*> Contexts;
	TSet<const UInputAction*> MappedActions;
	for (const FNGInputMappingContextEntry& Entry : MappingContexts)
	{
		if (!Entry.MappingContext)
		{
			Error(TEXT("MappingContexts contains an empty reference."));
			continue;
		}
		if (Entry.Priority < 0) Error(TEXT("Mapping context priority must be non-negative."));
		if (Contexts.Contains(Entry.MappingContext.Get()))
			Error(FString::Printf(TEXT("Duplicate mapping context: %s."), *Entry.MappingContext->GetName()));
		Contexts.Add(Entry.MappingContext.Get());
		for (const FEnhancedActionKeyMapping& Mapping : Entry.MappingContext->GetMappings())
			if (Mapping.Action) MappedActions.Add(Mapping.Action.Get());
	}

	for (const UInputMappingContext* StateContext : StateMappingContexts)
	{
		if (!StateContext || Contexts.Contains(StateContext)) { Error(TEXT("Null or duplicate StateMappingContext (including base contexts).")); continue; }
		Contexts.Add(StateContext);
		for (const FEnhancedActionKeyMapping& Mapping : StateContext->GetMappings())
			if (Mapping.Action) MappedActions.Add(Mapping.Action.Get());
	}
	// One physical action has one route. Multiple device keys belong to that action's IMC mappings.
	TSet<const UInputAction*> BoundActions;
	TSet<FGameplayTag> BoundTags;
	auto ValidateActions = [&](const TArray<FNGTaggedInputAction>& Actions, const TCHAR* ListName, bool bAbility)
	{
		for (int32 Index = 0; Index < Actions.Num(); ++Index)
		{
			const FNGTaggedInputAction& Entry = Actions[Index];
			const FString Label = FString::Printf(TEXT("%s[%d]"), ListName, Index);
			if (!Entry.InputTag.IsValid()) Error(Label + TEXT(": InputTag is required."));
			else if (BoundTags.Contains(Entry.InputTag)) Error(Label + TEXT(": InputTag is already bound in this config."));
			else BoundTags.Add(Entry.InputTag);
			if (!Entry.InputAction)
			{
				Error(Label + TEXT(": InputAction is required."));
				continue;
			}
			if (BoundActions.Contains(Entry.InputAction.Get())) Error(Label + TEXT(": InputAction is already routed in this config."));
			BoundActions.Add(Entry.InputAction.Get());
			if (!MappedActions.Contains(Entry.InputAction.Get())) Error(Label + TEXT(": InputAction is missing from this config's mapping contexts."));
			if (bAbility && Entry.InputAction->ValueType != EInputActionValueType::Boolean)
				Error(Label + TEXT(": GAS pressed/released bindings require a Boolean action; analog movement/look uses native actions."));
		}
	};
	ValidateActions(NativeInputActions, TEXT("NativeInputActions"), false);
	ValidateActions(AbilityInputActions, TEXT("AbilityInputActions"), true);
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
