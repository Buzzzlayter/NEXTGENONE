#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"

#include "Gameplay/Interaction/NGInteraction.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

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

bool UNGPlayerAbilitySystemComponent::IsCurrentServerAvatar(const AActor* ExpectedAvatar) const
{
	return IsValid(ExpectedAvatar) && GetOwnerActor() && GetOwnerActor()->HasAuthority()
		&& GetAvatarActor() == ExpectedAvatar && GetWorld();
}

bool UNGPlayerAbilitySystemComponent::CanFireNow(const AActor* ExpectedAvatar) const
{
	return IsCurrentServerAvatar(ExpectedAvatar) && GetWorld()->GetTimeSeconds() >= NextFireTime;
}

bool UNGPlayerAbilitySystemComponent::TryCommitFire(const AActor* ExpectedAvatar, float Interval)
{
	if (!CanFireNow(ExpectedAvatar) || !FMath::IsFinite(Interval) || Interval <= 0.f) return false;
	NextFireTime = GetWorld()->GetTimeSeconds() + Interval;
	return true;
}

bool UNGPlayerAbilitySystemComponent::TryCommitInteraction(const AActor* ExpectedAvatar, float Interval)
{
	if (!IsCurrentServerAvatar(ExpectedAvatar) || !FMath::IsFinite(Interval) || Interval <= 0.f) return false;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextInteractionTime) return false;
	NextInteractionTime = Now + Interval;
	return true;
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
