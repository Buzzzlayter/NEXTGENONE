#include "Gameplay/Abilities/NGActivateActionSlotAbility.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "GameplayTags/NGGameplayTags.h"

UNGActivateActionSlotAbility::UNGActivateActionSlotAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
	CancelAbilitiesWithTag.AddTag(TAG_NGPrimaryInputAbility);
}

void UNGActivateActionSlotAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	const APawn* Avatar = Info ? Cast<APawn>(Info->AvatarActor.Get()) : nullptr;
	const APlayerState* Player = Avatar ? Avatar->GetPlayerState() : nullptr;
	UNGPlayerInventoryManagerComponent* Inventory = Player ? Player->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	const bool bDispatched = Info && Info->IsLocallyControlled() && Inventory && ActionSlot >= 0
		&& CommitAbility(Handle, Info, Activation) && Inventory->RequestActivationSlot(ActionSlot);
	EndAbility(Handle, Info, Activation, false, !bDispatched);
}
