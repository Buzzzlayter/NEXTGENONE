#include "Gameplay/Abilities/NGItemUseAbility.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "Gameplay/Abilities/NGALSActionAbilities.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryFragment_ItemActions.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_NGUsingItem, "State.UsingItem");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGItemUseAbility, "Ability.Type.ItemUse");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_NGHealingAmount, "Data.Healing");

UNGItemUseAbility::UNGItemUseAbility()
{
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnly;
    ActivationBlockedTags.AddTag(TAG_NGDead);
    ActivationBlockedTags.AddTag(TAG_NGUsingItem);
    ActivationOwnedTags.AddTag(TAG_NGUsingItem);
    FGameplayTagContainer ItemAbilityTags; ItemAbilityTags.AddTag(TAG_NGItemUseAbility); SetAssetTags(ItemAbilityTags);
}

UNGInventoryItemInstance* UNGItemUseAbility::GetAssociatedItem() const
{
    return Cast<UNGInventoryItemInstance>(GetSourceObject(CurrentSpecHandle, CurrentActorInfo));
}
UNGPlayerInventoryManagerComponent* UNGItemUseAbility::GetInventory() const
{
    auto* ASC = CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr;
    auto* Owner = ASC ? ASC->GetOwnerActor() : nullptr;
    return Owner ? Owner->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
}
bool UNGItemUseAbility::HasCurrentItem() const
{
    auto* Item = GetAssociatedItem();
    auto* Inventory = GetInventory();
    const auto* Pawn = CurrentActorInfo ? Cast<APawn>(CurrentActorInfo->AvatarActor.Get()) : nullptr;
    return Inventory && IsValid(Item) && Inventory->Contains(Item) && Inventory->GetCount(Item) > 0
        && NGPlayerContext::GetCurrentASC(Pawn, true) == CurrentActorInfo->AbilitySystemComponent.Get();
}
bool UNGItemUseAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
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

UNGUsingItemEffect::UNGUsingItemEffect()
{
    DurationPolicy = EGameplayEffectDurationType::Infinite;
    FInheritedTagContainer Tags; Tags.AddTag(TAG_NGUsingItem);
    auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("UsingItemTags"));
    GEComponents.Add(TargetTags); TargetTags->SetAndApplyTargetTagChanges(Tags);
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
UNGUseMedkitAbility::UNGUseMedkitAbility()
{
    ActivationBlockedTags.AddTag(TAG_NGDashActive); ActivationBlockedTags.AddTag(TAG_NGMantleActive);
    ActivationBlockedTags.AddTag(TAG_NGPunchActive); ActivationBlockedTags.AddTag(TAG_NGReloadActive);
    ActivationBlockedTags.AddTag(TAG_NGRollActive); ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
}
bool UNGUseMedkitAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
    if (!Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
    const auto* Item = Cast<UNGInventoryItemInstance>(GetSourceObject(Handle, Info));
    const auto* Medical = Item ? Item->FindFragmentByClass<UNGInventoryFragment_Medical>() : nullptr;
    const auto* ASC = Info->AbilitySystemComponent.Get();
    const auto* Character = Cast<ANextgenoneCharacter>(Info->AvatarActor.Get());
    const float Health = ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute());
    return Medical && FMath::IsFinite(Medical->Healing) && Medical->Healing > 0.f
        && FMath::IsFinite(Medical->UseDuration) && Medical->UseDuration >= .1f
        && Health > 0.f && Health < ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute())
        && (!Character || Character->GetGait() != EALSGait::Sprinting);
}
void UNGUseMedkitAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
    if (!HasCurrentItem() || !CommitAbility(Handle, Info, Activation))
    { EndAbility(Handle, Info, Activation, true, true); return; }
    const auto* Medical = GetAssociatedItem()->FindFragmentByClass<UNGInventoryFragment_Medical>();
    if (!Medical) { EndAbility(Handle, Info, Activation, true, true); return; }
    HealingAmount = Medical->Healing;
    UseASC = Info->AbilitySystemComponent; UseAvatar = Info->AvatarActor;
    UseInventory = GetInventory(); UseSlot = UseInventory->GetSlotIndex(GetAssociatedItem());
    UseCount = UseInventory->GetCount(GetAssociatedItem());
    UsingEffect = UseASC->ApplyGameplayEffectToSelf(GetDefault<UNGUsingItemEffect>(), 1.f, UseASC->MakeEffectContext());
    if (!UsingEffect.IsValid()) { CancelUse(); return; }
    HealthHandle = UseASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).AddUObject(this, &ThisClass::HealthChanged);
    TagHandle = UseASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::BlockingTagChanged);
    UseInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::InventoryChanged);
    auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, Medical->UseDuration);
    Task->OnFinish.AddDynamic(this, &ThisClass::FinishUse); Task->ReadyForActivation();
}
void UNGUseMedkitAbility::CancelUse()
{
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
void UNGUseMedkitAbility::InventoryChanged()
{
    if (!HasCurrentItem() || !UseInventory.IsValid() || UseInventory->GetSlotIndex(GetAssociatedItem()) != UseSlot
        || UseInventory->GetCount(GetAssociatedItem()) != UseCount) CancelUse();
}
void UNGUseMedkitAbility::HealthChanged(const FOnAttributeChangeData& Change)
{
    if (Change.NewValue < Change.OldValue) CancelUse();
}
void UNGUseMedkitAbility::BlockingTagChanged(FGameplayTag Tag, int32 Count)
{
    if (Count > 0 && Tag != TAG_NGUsingItem && ActivationBlockedTags.HasTag(Tag)) CancelUse();
}
void UNGUseMedkitAbility::FinishUse()
{
    auto* ASC = UseASC.Get();
    if (!HasCurrentItem() || !ASC || ASC->GetAvatarActor() != UseAvatar.Get() || !UseInventory.IsValid()
        || UseInventory->GetSlotIndex(GetAssociatedItem()) != UseSlot || UseInventory->GetCount(GetAssociatedItem()) != UseCount)
    { CancelUse(); return; }
    const float Health = ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute());
    const float MaxHealth = ASC->GetNumericAttribute(UNGHealthSet::GetMaxHealthAttribute());
    if (Health <= 0.f || Health >= MaxHealth || ASC->HasMatchingGameplayTag(TAG_NGDead)) { CancelUse(); return; }
    auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(UseAvatar.Get());
    auto Spec = ASC->MakeOutgoingSpec(UNGHealingEffect::StaticClass(), 1.f, Context);
    if (!Spec.IsValid()) { CancelUse(); return; }
    Spec.Data->SetSetByCallerMagnitude(TAG_NGHealingAmount, FMath::Min(HealingAmount, MaxHealth - Health));
    // Our own successful debit must not be mistaken for an external item loss.
    UseInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
    const bool bUsed = UseInventory->ConsumeItem(GetAssociatedItem(), 1, [ASC, Spec]
    {
        ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    });
    // Healing callbacks may end the ability synchronously (for example, project-specific lifecycle code).
    if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bUsed);
}
void UNGUseMedkitAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
    FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
    if (!IsEndAbilityValid(Handle, Info)) return;
    if (auto* ASC = UseASC.Get())
    {
        ASC->GetGameplayAttributeValueChangeDelegate(UNGHealthSet::GetHealthAttribute()).Remove(HealthHandle);
        ASC->RegisterGenericGameplayTagEvent().Remove(TagHandle);
        if (UsingEffect.IsValid()) ASC->RemoveActiveGameplayEffect(UsingEffect);
    }
    if (UseInventory.IsValid()) UseInventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
    UseASC.Reset(); UseInventory.Reset(); UseAvatar.Reset(); HealthHandle.Reset(); TagHandle.Reset(); UseSlot = INDEX_NONE;
    UsingEffect.Invalidate();
    Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
