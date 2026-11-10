// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/Abilities/NGGameplayAbilityFromEquipment.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(NGGameplayAbilityFromEquipment)

UNGGameplayAbilityFromEquipment::UNGGameplayAbilityFromEquipment(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UNGGameplayAbilityFromEquipment::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    const auto* Source = Info ? Cast<UNGEquipmentInstance>(GetSourceObject(Handle, Info)) : nullptr;
    return Source && Source->IsCurrent() && Source->GetAvatar() == Info->AvatarActor.Get()
        && Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags);
}
UNGEquipmentInstance* UNGGameplayAbilityFromEquipment::GetEquipmentInstance() const
{
    const auto* Spec = UGameplayAbility::GetCurrentAbilitySpec();
    auto* Source = Spec ? Cast<UNGEquipmentInstance>(Spec->SourceObject.Get()) : nullptr;
    return Source && Source->IsCurrent() ? Source : nullptr;
}
UNGInventoryItemInstance* UNGGameplayAbilityFromEquipment::GetAssociatedItem() const
{
    const auto* Source = GetEquipmentInstance(); return Source ? Source->GetItem() : nullptr;
}

#if WITH_EDITOR
EDataValidationResult UNGGameplayAbilityFromEquipment::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (InstancingPolicy != EGameplayAbilityInstancingPolicy::InstancedPerActor && InstancingPolicy != EGameplayAbilityInstancingPolicy::InstancedPerExecution)
	{
		Context.AddError(NSLOCTEXT("NEXTGENONE", "EquipmentAbilityMustBeInstanced", "Equipment ability must be instanced"));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}

#endif
