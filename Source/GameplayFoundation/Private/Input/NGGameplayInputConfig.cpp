#include "Input/NGGameplayInputConfig.h"

#include "InputAction.h"
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
			if (bAbility && Entry.InputAction->ValueType != EInputActionValueType::Boolean)
				Error(Label + TEXT(": GAS pressed/released bindings require a Boolean action; analog movement/look uses native actions."));
		}
	};
	ValidateActions(NativeInputActions, TEXT("NativeInputActions"), false);
	ValidateActions(AbilityInputActions, TEXT("AbilityInputActions"), true);
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
