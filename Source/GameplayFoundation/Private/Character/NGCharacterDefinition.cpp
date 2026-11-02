#include "Character/NGCharacterDefinition.h"

#include "Input/NGGameplayInputConfig.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "GameFramework/Actor.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UNGCharacterDefinition::ValidateAbilityGrants(FString& Error) const
{
	TSet<const UNGAbilitySet*> SeenSets;
	TSet<UClass*> SeenAbilities;
	TSet<FGameplayTag> SeenInputTags;
	for (const UNGAbilitySet* Set : AbilitySets)
	{
		if (!Set || SeenSets.Contains(Set)) { Error = TEXT("AbilitySets contains a null or duplicate set."); return false; }
		SeenSets.Add(Set);
		for (const FNGAbilityGrant& Grant : Set->Abilities)
		{
			if (!Grant.Ability || Grant.Level < 1 || SeenAbilities.Contains(Grant.Ability.Get()))
			{ Error = FString::Printf(TEXT("%s contains an invalid or duplicate ability class/level."), *Set->GetName()); return false; }
			SeenAbilities.Add(Grant.Ability.Get());
			if (Grant.InputTag.IsValid())
			{
				if (SeenInputTags.Contains(Grant.InputTag))
				{ Error = FString::Printf(TEXT("Duplicate base ability input tag: %s."), *Grant.InputTag.ToString()); return false; }
				SeenInputTags.Add(Grant.InputTag);
			}
		}
		for (auto Effect : Set->Effects)
			if (!Effect) { Error = FString::Printf(TEXT("%s contains an empty effect class."), *Set->GetName()); return false; }
	}
	Error.Reset();
	return true;
}

bool UNGCharacterDefinition::GiveAbilitySets(UAbilitySystemComponent* ASC, AActor* Avatar, FNGGrantedHandles& Out, bool bNewLife) const
{
	FString Error;
	if (!ASC || !ASC->IsOwnerActorAuthoritative() || !IsValid(Avatar) || ASC->GetAvatarActor() != Avatar ||
		!Out.Abilities.IsEmpty() || !Out.Effects.IsEmpty()) return false;
	if (!ValidateAbilityGrants(Error))
	{
		UE_LOG(LogTemp, Error, TEXT("CharacterDefinition %s: %s"), *GetName(), *Error);
		return false;
	}
	for (const UNGAbilitySet* Set : AbilitySets)
	{
		FNGGrantedHandles SetHandles;
		Set->Give(ASC, Avatar, SetHandles, bNewLife);
		Out.Abilities.Append(SetHandles.Abilities);
		Out.Effects.Append(SetHandles.Effects);
	}
	return true;
}

#if WITH_EDITOR
EDataValidationResult UNGCharacterDefinition::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	bool bValid = ParentResult != EDataValidationResult::Invalid;
	auto Error = [&Context, &bValid](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		bValid = false;
	};
	if (!InputConfig) Error(TEXT("InputConfig is required for a locally controlled character."));
	else if (InputConfig->IsDataValid(Context) == EDataValidationResult::Invalid) bValid = false;
	FString GrantError;
	if (!ValidateAbilityGrants(GrantError)) Error(GrantError);

	TSet<const UNGAbilitySet*> SeenSets;
	TSet<FGameplayTag> AbilityTags;
	for (const UNGAbilitySet* Set : AbilitySets)
	{
		if (!Set) { Error(TEXT("AbilitySets contains an empty reference.")); continue; }
		if (SeenSets.Contains(Set)) Error(TEXT("AbilitySets contains the same set more than once."));
		SeenSets.Add(Set);
		for (const FNGAbilityGrant& Grant : Set->Abilities)
		{
			if (!Grant.Ability || Grant.Level < 1) Error(FString::Printf(TEXT("%s has an invalid ability class or level."), *Set->GetName()));
			if (Grant.InputTag.IsValid()) AbilityTags.Add(Grant.InputTag);
		}
		for (const TSubclassOf<UGameplayEffect>& Effect : Set->Effects)
			if (!Effect) Error(FString::Printf(TEXT("%s has an empty effect class."), *Set->GetName()));
	}
	// Equipment/temporary states can grant additional abilities later; their absence here is not an error.
	if (InputConfig)
		for (const FNGTaggedInputAction& Entry : InputConfig->AbilityInputActions)
			if (Entry.InputTag.IsValid() && !AbilityTags.Contains(Entry.InputTag))
				Context.AddWarning(FText::FromString(FString::Printf(TEXT("Input tag %s has no base ability grant; it must be provided by equipment or a temporary state."), *Entry.InputTag.ToString())));
	return bValid ? EDataValidationResult::Valid : EDataValidationResult::Invalid;
}
#endif
