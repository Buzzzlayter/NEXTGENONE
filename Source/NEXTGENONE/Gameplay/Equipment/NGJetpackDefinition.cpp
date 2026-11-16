#include "Gameplay/Equipment/NGJetpackDefinition.h"
#include "Gameplay/Abilities/NGJetpackAbility.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UNGJetpackDefinition::UNGJetpackDefinition()
{
	SlotTag = TAG_NGSlotEquipmentBack; AttachSocket = TEXT("spine_03");
	FNGEquipmentAbilityGrant Thrust; Thrust.Grant.Ability = UNGJetpackAbility::StaticClass(); Thrust.Grant.InputTag = TAG_NGInputJetpack;
	Abilities.Add(Thrust); Effects.Add(UNGJetpackWearingEffect::StaticClass());
}
bool UNGJetpackDefinition::ValidateDefinition(FString& Error) const
{
	if (!Super::ValidateDefinition(Error)) return false;
	if (SlotTag != TAG_NGSlotEquipmentBack || !Movement.IsValid() || !FMath::IsFinite(MaxBurnDuration) || MaxBurnDuration < .1f || MaxBurnDuration > 30.f)
	{ Error = TEXT("Jetpack requires Back and finite movement/burn settings."); return false; }
	return true;
}
UNGJetpackWearingEffect::UNGJetpackWearingEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Infinite;
	FInheritedTagContainer Tags; Tags.AddTag(TAG_NGJetpackEquipped);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("WearingTags"));
	GEComponents.Add(TargetTags); TargetTags->SetAndApplyTargetTagChanges(Tags);
}
