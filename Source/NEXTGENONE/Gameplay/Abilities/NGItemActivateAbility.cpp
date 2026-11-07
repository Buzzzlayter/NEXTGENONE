#include "Gameplay/Abilities/NGItemActivateAbility.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryFragment_ItemActions.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"


UNGItemActivateAbility::UNGItemActivateAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
    ActivationBlockedTags.AddTag(TAG_NGDead);
    ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
    ActivationOwnedTags.AddTag(TAG_NGActivatingItem);
    FGameplayTagContainer ItemAbilityTags; ItemAbilityTags.AddTag(TAG_NGItemActivateAbility); SetAssetTags(ItemAbilityTags);
}

UNGInventoryItemInstance* UNGItemActivateAbility::GetAssociatedItem() const
{
    return Cast<UNGInventoryItemInstance>(GetSourceObject(CurrentSpecHandle, CurrentActorInfo));
}
UNGPlayerInventoryManagerComponent* UNGItemActivateAbility::GetInventory() const
{
    auto* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    auto* Owner = ASC ? ASC->GetOwnerActor() : nullptr;
    return Owner ? Owner->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
}
bool UNGItemActivateAbility::HasCurrentItem() const
{
    auto* Item = GetAssociatedItem();
    auto* Inventory = GetInventory();
    const auto* Pawn = CurrentActorInfo ? Cast<APawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    return Inventory && IsValid(Item) && Inventory->Contains(Item) && Inventory->GetCount(Item) > 0
        && NGPlayerContext::GetCurrentASC(Pawn, true) == CurrentActorInfo->AbilitySystemComponent.Get();
}
bool UNGItemActivateAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    if (!Info || !Info->IsNetAuthority() || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
    auto* Item = Cast<UNGInventoryItemInstance>(GetSourceObject(Handle, Info));
    auto* Owner = Info->OwnerActor.Get();
    auto* Inventory = Owner ? Owner->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
    return IsValid(Item) && Inventory && Inventory->Contains(Item) && Inventory->GetCount(Item) > 0
        && Info->AbilitySystemComponent->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) > 0.f
        && NGPlayerContext::GetCurrentASC(Cast<APawn>(Info->AvatarActor.Get()), true) == Info->AbilitySystemComponent.Get();
}

UNGActivatingItemEffect::UNGActivatingItemEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FInheritedTagContainer Tags; Tags.AddTag(TAG_NGActivatingItem);
    auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("ActivatingItemTags"));
    GEComponents.Add(TargetTags); TargetTags->SetAndApplyTargetTagChanges(Tags);
}

void UNGEquipWeaponItemAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
    auto* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
    auto* Combat = Avatar ? Avatar->FindComponentByClass<UNGCombatComponent>() : nullptr;
    const bool bEquipped = HasCurrentItem() && Combat && CommitAbility(Handle, Info, Activation)
        && Combat->EquipItemFromAbility(GetAssociatedItem());
    EndAbility(Handle, Info, Activation, true, !bEquipped);
}
UNGHealingEffect::UNGHealingEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Instant;
    FGameplayModifierInfo Mod;
    Mod.Attribute = UNGHealthSet::GetHealthAttribute(); Mod.ModifierOp = EGameplayModOp::Additive;
    FSetByCallerFloat Amount; Amount.DataTag = TAG_NGHealingAmount;
    Mod.ModifierMagnitude = FGameplayEffectModifierMagnitude(Amount);
    Modifiers.Add(Mod);
}
UNGActivateMedkitAbility::UNGActivateMedkitAbility()
{
    ActivationBlockedTags.AddTag(TAG_NGDashActive); ActivationBlockedTags.AddTag(TAG_NGMantleActive);
    ActivationBlockedTags.AddTag(TAG_NGPunchActive); ActivationBlockedTags.AddTag(TAG_NGReloadActive);
    ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
}
bool UNGActivateMedkitAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
    const auto* Item = Cast<UNGInventoryItemInstance>(GetSourceObject(Handle, Info));
    const auto* Medical = Item ? Item->FindFragmentByClass<UNGInventoryFragment_Medical>() : nullptr;
    const auto* ASC = Info->AbilitySystemComponent.Get();
    const auto* Character = Cast<ANextgenoneCharacter>(Info->AvatarActor.Get());
    const float Health = ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute());
    return Medical && FMath::IsFinite(Medical->Healing) && Medical->Healing > 0.f
        && FMath::IsFinite(Medical->ActivationDuration) && Medical->ActivationDuration >= .1f
        && Health > 0.f && Health < ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute())
        && (!Character || Character->GetGait() != EALSGait::Sprinting);
}
void UNGActivateMedkitAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
    if (!HasCurrentItem() || !CommitAbility(Handle, Info, Activation))
    { EndAbility(Handle, Info, Activation, true, true); return; }
    const auto* Medical = GetAssociatedItem()->FindFragmentByClass<UNGInventoryFragment_Medical>();
    if (!Medical) { EndAbility(Handle, Info, Activation, true, true); return; }
    HealingAmount = Medical->Healing;
    ActivationASC = Info->AbilitySystemComponent; ActivationAvatar = Info->AvatarActor;
    ActivationInventory = GetInventory(); ActivationSlot = ActivationInventory->GetSlotIndex(GetAssociatedItem());
    ActivationCount = ActivationInventory->GetCount(GetAssociatedItem());
    ActivationEffect = ActivationASC->ApplyGameplayEffectToSelf(GetDefault<UNGActivatingItemEffect>(), 1.f, ActivationASC->MakeEffectContext());
    if (!ActivationEffect.IsValid()) { CancelActivation(); return; }
    HealthHandle = ActivationASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HealthChanged);
    TagHandle = ActivationASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::BlockingTagChanged);
    ActivationInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::InventoryChanged);
    auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, Medical->ActivationDuration);
    Task->OnFinish.AddDynamic(this, &ThisClass::FinishActivation); Task->ReadyForActivation();
}
void UNGActivateMedkitAbility::CancelActivation()
{
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void UNGActivateMedkitAbility::InventoryChanged()
{
    if (!HasCurrentItem() || !ActivationInventory.IsValid() || ActivationInventory->GetSlotIndex(GetAssociatedItem()) != ActivationSlot
        || ActivationInventory->GetCount(GetAssociatedItem()) != ActivationCount) CancelActivation();
}
void UNGActivateMedkitAbility::HealthChanged(const FOnAttributeChangeData& Change)
{
    if (Change.NewValue < Change.OldValue) CancelActivation();
}
void UNGActivateMedkitAbility::BlockingTagChanged(FGameplayTag Tag, int32 Count)
{
    if (Count > 0 && Tag != TAG_NGActivatingItem && ActivationBlockedTags.HasTag(Tag)) CancelActivation();
}
void UNGActivateMedkitAbility::FinishActivation()
{
    auto* ASC = ActivationASC.Get();
    if (!HasCurrentItem() || !ASC || ASC->GetAvatarActor() != ActivationAvatar.Get() || !ActivationInventory.IsValid()
        || ActivationInventory->GetSlotIndex(GetAssociatedItem()) != ActivationSlot || ActivationInventory->GetCount(GetAssociatedItem()) != ActivationCount)
    { CancelActivation(); return; }
    const float Health = ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute());
    const float MaxHealth = ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute());
    if (Health <= 0.f || Health >= MaxHealth || ASC->HasMatchingGameplayTag(TAG_NGDead)) { CancelActivation(); return; }
    auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(ActivationAvatar.Get());
    auto Spec = ASC->MakeOutgoingSpec(UNGHealingEffect::StaticClass(), 1.f, Context);
    if (!Spec.IsValid()) { CancelActivation(); return; }
    Spec.Data->SetSetByCallerMagnitude(TAG_NGHealingAmount, FMath::Min(HealingAmount, MaxHealth - Health));
    // Our own successful debit must not be mistaken for an external item loss.
    ActivationInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
    const bool bActivated = ActivationInventory->ConsumeItem(GetAssociatedItem(), 1, [ASC, Spec]
    {
        ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    });
    // Healing callbacks may end the ability synchronously (for example, project-specific lifecycle code).
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bActivated);
}
void UNGActivateMedkitAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
    if (!IsEndAbilityValid(Handle, Info)) return;
    if (auto* ASC = ActivationASC.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).Remove(HealthHandle);
        ASC->RegisterGenericGameplayTagEvent().Remove(TagHandle);
        if (ActivationEffect.IsValid()) ASC->RemoveActiveGameplayEffect(ActivationEffect);
    }
    if (ActivationInventory.IsValid()) ActivationInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
    ActivationASC.Reset(); ActivationInventory.Reset(); ActivationAvatar.Reset(); HealthHandle.Reset(); TagHandle.Reset(); ActivationSlot = INDEX_NONE;
    ActivationEffect.Invalidate();
    Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
