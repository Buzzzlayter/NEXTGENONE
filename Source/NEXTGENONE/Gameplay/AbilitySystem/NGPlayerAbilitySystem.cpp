#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "GameplayTags/NGGameplayTags.h"

#include "Gameplay/Interaction/NGInteraction.h"
#include "GameFramework/Actor.h"

void UNGPlayerAbilitySystemComponent::InitAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	// Commit the identity before Super can notify abilities about the new Avatar.
	if (InOwnerActor && InOwnerActor->HasAuthority() && InAvatarActor && LastLifeAvatar.Get() != InAvatarActor)
	{
		LastLifeAvatar = InAvatarActor;
		++AvatarGeneration;
	}
	Super::InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

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

void UNGPlayerAbilitySystemComponent::OnRep_ActivateAbilities()
{
	OnAbilitySpecsReplicated.Broadcast(this);
	Super::OnRep_ActivateAbilities();
	OnAbilitySpecsReplicated.Broadcast(this);
}
