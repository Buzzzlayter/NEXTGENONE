#include "Gameplay/Abilities/NGInteractionAbility.h"

#include "Gameplay/Interaction/NGInteraction.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/NGGameplayTags.h"

UNGInteractionAbility::UNGInteractionAbility()
{
	FGameplayTagContainer InteractionAssetTags; InteractionAssetTags.AddTag(TAG_NGInteractionAbility); SetAssetTags(InteractionAssetTags);
	InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor; NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy=EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	FAbilityTriggerData Trigger; Trigger.TriggerTag=TAG_NGInteractEvent; Trigger.TriggerSource=EGameplayAbilityTriggerSource::GameplayEvent; AbilityTriggers.Add(Trigger);
}
bool UNGInteractionAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
    const auto* Avatar = Info->AvatarActor.Get();
    const auto* Interaction = Avatar ? Avatar->FindComponentByClass<UNGInteractionComponent>() : nullptr;
    return Interaction && !Interaction->IsInteractionBlocked();
}

void UNGInteractionAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,FGameplayAbilityActivationInfo Activation,const FGameplayEventData* Event)
{
	auto* Pawn=Info ? Cast<APawn>(Info->AvatarActor.Get()) : nullptr;
	auto* Interaction = Pawn ? Pawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	bool bSuccess=false;
	if (Info && Info->IsNetAuthority() && Interaction && Event && Event->Instigator.Get()==Pawn && Interaction->IsCurrentInteractor())
	{
		auto* Target=Cast<ANGInteractableActor>(const_cast<AActor*>(Event->Target.Get()));
		bSuccess=Target && Target->ExecuteInteraction(Pawn,[&]()
		{
			if (!Interaction->ValidateTarget(Target) || !INGInteractable::Execute_CanInteract(Target,Pawn)) return false;
			if (!CommitAbility(Handle,Info,Activation)) return false;
			return Interaction->IsCurrentInteractor() && Interaction->ValidateTarget(Target)
				&& INGInteractable::Execute_CanInteract(Target,Pawn);
		});
	}
	if (Interaction) Interaction->ClientInteractionResult(bSuccess);
	EndAbility(Handle,Info,Activation,true,!bSuccess);
}
