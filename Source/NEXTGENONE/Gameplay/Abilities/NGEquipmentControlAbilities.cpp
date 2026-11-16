#include "Gameplay/Abilities/NGEquipmentControlAbilities.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitInputRelease.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayTag.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "GameplayTags/NGGameplayTags.h"

namespace
{
	constexpr float FireViewUpdateInterval = .05f;
	constexpr double FireViewMaxAge = .25;
}

UNGPrimaryAttackAbility::UNGPrimaryAttackAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	FGameplayTagContainer AbilityType;
	AbilityType.AddTag(TAG_NGPrimaryInputAbility);
	SetAssetTags(AbilityType);
	ActivationRequiredTags.AddTag(TAG_NGWeaponAiming);
	for (const FGameplayTag Tag : {TAG_NGDead.GetTag(), TAG_NGActivatingItem.GetTag(), TAG_NGRollActive.GetTag(), TAG_NGRagdollActive.GetTag(),
		TAG_NGDashActive.GetTag(), TAG_NGMantleActive.GetTag(), TAG_NGReloadActive.GetTag()}) ActivationBlockedTags.AddTag(Tag);
}

void UNGPrimaryAttackAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	AActor* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
	auto* Combat = Avatar ? Avatar->FindComponentByClass<UNGCombatComponent>() : nullptr;
	auto* Equipment = Avatar ? Avatar->FindComponentByClass<UNGEquipmentComponent>() : nullptr;
	if (!Info || !Combat || !Equipment || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!IsActive()) return;
	ActiveCombat = Combat;
	ActiveEquipment = Equipment;
	PrimaryRevision = Equipment->GetHandsRevision();
	if (!HasCurrentWeapon())
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	EquipmentChangedHandle = Equipment->OnEquipmentChanged.AddUObject(this, &ThisClass::EquipmentChanged);
	auto* ASC = Info->AbilitySystemComponent.Get();
	StateTagHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::StateTagChanged);
	auto* Release = UAbilityTask_WaitInputRelease::WaitInputRelease(this, true);
	Release->OnRelease.AddDynamic(this, &ThisClass::HandleInputReleased);
	Release->ReadyForActivation();
	if (!IsActive()) return;
	if (Info->IsNetAuthority())
	{
		if (Info->IsLocallyControlled()) ContinuePrimaryInput();
		else
		{
			const auto Key = Activation.GetActivationPredictionKey();
			ViewDataHandle = ASC->AbilityTargetDataSetDelegate(Handle, Key).AddUObject(this, &ThisClass::ReceiveFireView);
			ASC->CallReplicatedTargetDataDelegatesIfSet(Handle, Key);
		}
	}
	else if (Info->IsLocallyControlled()) SendFireView();
}

bool UNGPrimaryAttackAbility::HasCurrentWeapon() const
{
	const auto* Combat = ActiveCombat.Get();
	const auto* Weapon = Combat ? Combat->GetWeaponInstance() : nullptr;
	return CurrentActorInfo && Combat && CurrentActorInfo->AvatarActor.Get() == Combat->GetOwner()
		&& ActiveEquipment.IsValid() && ActiveEquipment->IsCurrent(Weapon)
		&& PrimaryRevision == ActiveEquipment->GetHandsRevision();
}

void UNGPrimaryAttackAbility::EquipmentChanged()
{
	if (IsActive() && !HasCurrentWeapon())
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UNGPrimaryAttackAbility::StateTagChanged(FGameplayTag Tag, int32 Count)
{
	if (IsActive() && ((Tag == TAG_NGWeaponAiming && Count == 0) || (Count > 0 && Tag.MatchesAny(ActivationBlockedTags))))
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UNGPrimaryAttackAbility::SendFireView()
{
	if (!IsActive()) return;
	if (!HasCurrentWeapon())
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	const auto* Aim = ActiveCombat->GetOwner()->FindComponentByClass<UNGAimComponent>();
	FVector Origin, Direction;
	if (!Aim || !Aim->GetViewRay(Origin, Direction))
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	auto* View = new FNGPrimaryFireView();
	View->Origin = Origin; View->Direction = Direction; View->EquipmentRevision = PrimaryRevision;
	FGameplayAbilityTargetDataHandle Data(View);
	auto* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	FScopedPredictionWindow Prediction(ASC, true);
	ASC->CallServerSetReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey(),
		Data, FGameplayTag(), ASC->ScopedPredictionKey);
	if (!IsActive()) return;
	// Transport frequency is independent of weapon cadence; this never requests a shot.
	auto* Delay = UAbilityTask_WaitDelay::WaitDelay(this, FireViewUpdateInterval);
	Delay->OnFinish.AddDynamic(this, &ThisClass::SendFireView);
	Delay->ReadyForActivation();
}

void UNGPrimaryAttackAbility::ReceiveFireView(const FGameplayAbilityTargetDataHandle& Data, FGameplayTag ApplicationTag)
{
	if (!IsActive()) return;
	const auto* Raw = Data.Num() == 1 ? Data.Get(0) : nullptr;
	TOptional<FNGPrimaryFireView> Received;
	if (Raw && Raw->GetScriptStruct() == FNGPrimaryFireView::StaticStruct()) Received = *static_cast<const FNGPrimaryFireView*>(Raw);
	auto* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	ASC->ConsumeClientReplicatedTargetData(CurrentSpecHandle, CurrentActivationInfo.GetActivationPredictionKey());
	if (!HasCurrentWeapon() || !Received.IsSet() || Received->EquipmentRevision != PrimaryRevision
		|| Received->Origin.ContainsNaN() || Received->Direction.ContainsNaN() || !Received->Direction.IsNormalized())
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	ServerView = Received;
	LastViewReceivedTime = GetWorld()->GetTimeSeconds();
	// Updates may provide the first view, but cannot bypass a running cooldown wait.
	if (!CooldownWait) ContinuePrimaryInput();
}

void UNGPrimaryAttackAbility::ContinuePrimaryInput()
{
	if (!IsActive()) return;
	if (!HasCurrentWeapon() || !ActiveCombat->IsAiming() || ActiveCombat->GetMagazineAmmo() <= 0)
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	auto* ASC = CurrentActorInfo->AbilitySystemComponent.Get();
	if (!CurrentActorInfo->IsLocallyControlled()
		&& (!ServerView.IsSet() || GetWorld()->GetTimeSeconds() - LastViewReceivedTime > FireViewMaxAge))
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	if (!ASC->HasMatchingGameplayTag(TAG_NGFireCooldown))
	{
		FVector Origin, Direction;
		if (CurrentActorInfo->IsLocallyControlled())
		{
			const auto* Aim = ActiveCombat->GetOwner()->FindComponentByClass<UNGAimComponent>();
			if (!Aim || !Aim->GetViewRay(Origin, Direction))
			{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
		}
		else { Origin = ServerView->Origin; Direction = ServerView->Direction; }
		auto* WeaponActor = ActiveCombat->GetWeaponActor();
		if (!WeaponActor)
		{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
		FGameplayAbilitySpecHandle FireHandle;
		int32 Matches = 0;
		{
			FScopedAbilityListLock Lock(*ASC);
			for (const auto& Spec : ASC->GetActivatableAbilities())
				if (!Spec.PendingRemove && Spec.GetDynamicSpecSourceTags().HasTagExact(TAG_NGFireInput))
				{
					++Matches;
					if (Spec.SourceObject.Get() == WeaponActor->GetEquipmentSource()) FireHandle = Spec.Handle;
				}
		}
		if (Matches != 1 || !FireHandle.IsValid())
		{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
		auto* View = new FNGPrimaryFireView();
		View->Origin = Origin;
		View->Direction = Direction;
		View->EquipmentRevision = PrimaryRevision;
		FGameplayEventData Shot;
		Shot.TargetData.Add(View);
		ASC->TriggerAbilityFromGameplayEvent(FireHandle, ASC->AbilityActorInfo.Get(), TAG_NGFireInput, &Shot, *ASC);
		if (!IsActive()) return;
		if (!HasCurrentWeapon() || !ActiveCombat->GetEquipment().Definition->bAutomatic)
		{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); return; }
	}
	if (!ASC->HasMatchingGameplayTag(TAG_NGFireCooldown))
	{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
	auto* Cooldown = UAbilityTask_WaitGameplayTagRemoved::WaitGameplayTagRemove(this, TAG_NGFireCooldown, nullptr, true);
	CooldownWait = Cooldown;
	Cooldown->Removed.AddDynamic(this, &ThisClass::FireCooldownFinished);
	Cooldown->ReadyForActivation();
}

void UNGPrimaryAttackAbility::FireCooldownFinished()
{
	CooldownWait = nullptr;
	ContinuePrimaryInput();
}

void UNGPrimaryAttackAbility::HandleInputReleased(float TimeHeld)
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UNGPrimaryAttackAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (!IsEndAbilityValid(Handle, Info)) return;
	if (auto* Equipment = ActiveEquipment.Get()) Equipment->OnEquipmentChanged.Remove(EquipmentChangedHandle);
	if (auto* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr)
	{
		ASC->RegisterGenericGameplayTagEvent().Remove(StateTagHandle);
		if (ViewDataHandle.IsValid())
		{
			ASC->AbilityTargetDataSetDelegate(Handle, Activation.GetActivationPredictionKey()).Remove(ViewDataHandle);
			ASC->ConsumeClientReplicatedTargetData(Handle, Activation.GetActivationPredictionKey());
		}
	}
	EquipmentChangedHandle.Reset(); StateTagHandle.Reset(); ViewDataHandle.Reset();
	ActiveCombat.Reset(); ActiveEquipment.Reset(); ServerView.Reset(); CooldownWait = nullptr;
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}

UNGEquipmentControlAbility::UNGEquipmentControlAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	CancelAbilitiesWithTag.AddTag(TAG_NGPrimaryInputAbility);
}

void UNGEquipmentControlAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	const auto* Avatar = Info ? Info->AvatarActor.Get() : nullptr;
	const auto* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr;
	const auto* Owner = ASC ? ASC->GetOwnerActor() : nullptr;
	auto* Inventory = Owner ? Owner->FindComponentByClass<UNGPlayerInventoryManagerComponent>() : nullptr;
	bool bDispatched = false;
	if (Info && Info->IsLocallyControlled() && Inventory && CommitAbility(Handle, Info, Activation))
	{
		if (!IsActive()) return;
		if (ASC->GetAvatarActor() != Avatar)
		{ EndAbility(Handle, Info, Activation, false, true); return; }
		auto* Item = Inventory->GetItemAtSlot(Inventory->FindSlotByTag(TAG_NGSlotEquipmentWeapon, 0));
		if (Item)
		{
			bDispatched = bEquip || Inventory->RequestActivationItem(Item);
		}
		else if (!bEquip) bDispatched = true;
		else
			for (int32 Index = 0; Index < Inventory->GetSlotCount(); ++Index)
			{
				auto* Candidate = Inventory->GetItemAtSlot(Index);
				const auto* Definition = UNGEquipmentComponent::GetDefinition(Candidate);
				if (Definition && Definition->SlotTag == TAG_NGSlotEquipmentWeapon)
				{
					bDispatched = Inventory->RequestActivationItem(Candidate);
					break;
				}
			}
	}
	EndAbility(Handle, Info, Activation, false, !bDispatched);
}
