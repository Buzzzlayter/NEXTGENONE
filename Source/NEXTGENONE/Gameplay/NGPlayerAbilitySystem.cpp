#include "NGPlayerAbilitySystem.h"

#include "NGInteraction.h"
#include "GameFramework/Actor.h"

void UNGPlayerAbilitySystemComponent::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (InputTag == TAG_NGInputInteract || InputTag == TAG_NGInputPickup)
	{
		TryActivateAbilityByInputTag(InputTag);
		return;
	}
	Super::AbilityInputTagPressed(InputTag);
}

bool UNGPlayerAbilitySystemComponent::TryActivateAbilityByInputTag(FGameplayTag InputTag)
{
	if (InputTag == TAG_NGInputInteract || InputTag == TAG_NGInputPickup)
	{
		AActor* Avatar = GetAvatarActor();
		if (UNGInteractionComponent* Interaction = Avatar ? Avatar->FindComponentByClass<UNGInteractionComponent>() : nullptr)
		{
			Interaction->RequestInteract();
			return true;
		}
		return false;
	}
	return Super::TryActivateAbilityByInputTag(InputTag);
}
