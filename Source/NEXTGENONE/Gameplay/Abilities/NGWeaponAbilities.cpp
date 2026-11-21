#include "Gameplay/Abilities/NGWeaponAbilities.h"
#include "AbilitySystem/NGAbilitySystem.h"
#include "Gameplay/Combat/NGWeaponActor.h"
#include "Gameplay/Combat/NGWeaponDefinition.h"
#include "Gameplay/Combat/NGWeaponTargetData.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemInstance.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "GameFramework/Pawn.h"
#include "AbilitySystemComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

namespace
{
	UNGInventoryManagerComponent* FindAbilityInventory(const FGameplayAbilityActorInfo* Info)
	{
		const auto* Owner = Info ? Info->OwnerActor.Get() : nullptr;
		return Owner ? Owner->FindComponentByClass<UNGInventoryManagerComponent>() : nullptr;
	}

	UClass* FindReloadAmmo(const UNGInventoryManagerComponent& Inventory, const UNGInventoryItemInstance& WeaponItem,
		const TArray<FNGWeaponAmmoOption>& Options, FGameplayTag* OutLoadedTag = nullptr)
	{
		const bool bHasLoadedAmmo = WeaponItem.GetStatTagStackCount(TAG_NGMagazineAmmo) > 0;
		FGameplayTag MagazineAmmoTag;
		if (bHasLoadedAmmo)
		{
			for (const auto& Option : Options)
				if (Option.LoadedTag.IsValid() && WeaponItem.HasStatTag(Option.LoadedTag))
				{
					MagazineAmmoTag = Option.LoadedTag;
					break;
				}
		}
		for (const auto& Option : Options)
		{
			if (bHasLoadedAmmo && Option.LoadedTag != MagazineAmmoTag) continue;
			auto* AmmoClass = Option.ItemClass.LoadSynchronous();
			if (!AmmoClass || Inventory.GetTotalItemCountByDefinition(AmmoClass) <= 0) continue;
			if (OutLoadedTag) *OutLoadedTag = Option.LoadedTag;
			return AmmoClass;
		}
		return nullptr;
	}
}

UNGFireCooldownEffect::UNGFireCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(1.f); // The ability sets the weapon's FireInterval on the spec.
	FInheritedTagContainer Tags; Tags.AddTag(TAG_NGFireCooldown);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("FireCooldownTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}

UNGFireAbility::UNGFireAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	CooldownGameplayEffectClass = UNGFireCooldownEffect::StaticClass();
	ActivationRequiredTags.AddTag(TAG_NGWeaponAiming);
	ActivationBlockedTags.AddTag(TAG_NGRollActive);
	ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGReloadActive);
}

UNGReloadAbility::UNGReloadAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	ActivationBlockedTags.AddTag(TAG_NGActivatingItem);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGReloadActive);
	ActivationBlockedTags.AddTag(TAG_NGRollActive);
	ActivationBlockedTags.AddTag(TAG_NGRagdollActive);
	ActivationOwnedTags.AddTag(TAG_NGReloadActive);
}

void UNGReloadAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	FNGWeaponAbilityContext Context;
	FString DefinitionError;
	const auto* Inventory = FindAbilityInventory(Info);
	if (!Info || !Info->IsNetAuthority() || !Inventory || !ResolveWeaponContext(Handle, Info, Context)
		|| !Context.Definition->ValidateDefinition(DefinitionError)
		|| Context.Weapon->GetMagazineAmmo() >= Context.Definition->MagazineCapacity)
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	TArray<FNGWeaponAmmoOption> AmmoOptions;
	Context.Definition->GetReloadAmmoOptions(AmmoOptions);
	if (!FindReloadAmmo(*Inventory, *Context.Source->GetItem(), AmmoOptions)
		|| !ResolveWeaponContext(Handle, Info, Context) || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!IsActive()) return;
	// Commit and cosmetic callbacks can remove this spec or change the Avatar.
	if (!ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	const float Duration = Context.Definition->ReloadDuration;
	if (Context.Definition->CharacterReloadMontage)
		Info->AbilitySystemComponent->PlayMontage(this, Activation, Context.Definition->CharacterReloadMontage, 1.f);
	if (!IsActive()) return;
	if (!ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	Context.Weapon->MulticastReload(Context.Definition);
	if (!IsActive()) return;
	if (!ResolveWeaponContext(Handle, Info, Context))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	auto* Task = UAbilityTask_WaitDelay::WaitDelay(this, Duration);
	Task->OnFinish.AddDynamic(this, &ThisClass::FinishReload);
	Task->ReadyForActivation();
}

void UNGReloadAbility::FinishReload()
{
	const bool bReloaded = CompleteReloadOnServer();
	if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, !bReloaded);
}

bool UNGReloadAbility::CompleteReloadOnServer()
{
	FNGWeaponAbilityContext Context;
	auto* Inventory = FindAbilityInventory(CurrentActorInfo);
	if (!IsActive() || !CurrentActorInfo || !CurrentActorInfo->IsNetAuthority() || !Inventory
		|| !ResolveWeaponContext(CurrentSpecHandle, CurrentActorInfo, Context)) return false;
	TArray<FNGWeaponAmmoOption> AmmoOptions;
	Context.Definition->GetReloadAmmoOptions(AmmoOptions);
	FGameplayTag LoadedTag;
	UClass* AmmoClass = FindReloadAmmo(*Inventory, *Context.Source->GetItem(), AmmoOptions, &LoadedTag);
	if (!AmmoClass || !ResolveWeaponContext(CurrentSpecHandle, CurrentActorInfo, Context)) return false;
	auto* Item = Context.Source->GetItem();
	const int32 Amount = FMath::Min(FMath::Max(0, Context.Definition->MagazineCapacity - Context.Weapon->GetMagazineAmmo()),
		Inventory->GetTotalItemCountByDefinition(AmmoClass));
	if (Amount <= 0) return false;
	return Inventory->ConsumeItemsByDefinition(AmmoClass, Amount, [Item, Amount, LoadedTag, &AmmoOptions]
	{
		for (const auto& Option : AmmoOptions)
		{
			if (!Option.LoadedTag.IsValid() || Option.LoadedTag == LoadedTag) continue;
			if (const int32 Count = Item->GetStatTagStackCount(Option.LoadedTag); Count > 0)
				Item->RemoveStatTagStack(Option.LoadedTag, Count);
		}
		if (LoadedTag.IsValid() && !Item->HasStatTag(LoadedTag)) Item->AddStatTagStack(LoadedTag, 1);
		Item->AddStatTagStack(TAG_NGMagazineAmmo, Amount);
	});
}

void UNGFireAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, const FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	FNGWeaponAbilityContext Context;
	FString DefinitionError;
	if (!Info || !Info->IsNetAuthority() || !ResolveWeaponContext(Handle, Info, Context)
		|| !Context.Definition->ValidateDefinition(DefinitionError) || Context.Weapon->GetMagazineAmmo() <= 0)
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	const auto* Pawn = Cast<APawn>(Info->AvatarActor.Get());
	const auto* Aim = Pawn ? Pawn->FindComponentByClass<UNGAimComponent>() : nullptr;
	FVector Origin, Direction;
	if (Event)
	{
		const auto* Raw = Event->TargetData.Num() == 1 ? Event->TargetData.Get(0) : nullptr;
		if (!Raw || Raw->GetScriptStruct() != FNGPrimaryFireView::StaticStruct())
		{ EndAbility(Handle, Info, Activation, true, true); return; }
		const auto* View = static_cast<const FNGPrimaryFireView*>(Raw);
		Origin = View->Origin;
		Direction = View->Direction;
	}
	else if (!Pawn || (Pawn->IsPlayerControlled() && !Pawn->IsLocallyControlled()) || !Aim || !Aim->GetViewRay(Origin, Direction))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!Aim || !Aim->ValidateShotView(Origin, Direction) || !ResolveWeaponContext(Handle, Info, Context)
		|| !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	if (!IsActive()) return;
	// Commit callbacks may remove the spec or change equipment/Avatar.
	const bool bFired = ResolveWeaponContext(Handle, Info, Context) && Context.Weapon->FireOnServer(Origin, Direction);
	if (bFired && IsActive() && ResolveWeaponContext(Handle, Info, Context) && Context.Definition->CharacterFireMontage)
		Info->AbilitySystemComponent->PlayMontage(this, Activation, Context.Definition->CharacterFireMontage, 1.f);
	if (bFired && IsActive() && ResolveWeaponContext(Handle, Info, Context)
		&& Context.Weapon->GetMagazineAmmo() == 0)
	{
		if (auto* ASC = Cast<UNGAbilitySystemComponent>(Info->AbilitySystemComponent.Get()))
			ASC->TryActivateAbilityByInputTag(TAG_NGReloadInput);
	}
	if (IsActive()) EndAbility(Handle, Info, Activation, true, !bFired);
}

void UNGFireAbility::ApplyCooldown(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info, FGameplayAbilityActivationInfo Activation) const
{
	FNGWeaponAbilityContext Context;
	if (!ResolveWeaponContext(Handle, Info, Context)) return;
	auto Spec = MakeOutgoingGameplayEffectSpec(Handle, Info, Activation, CooldownGameplayEffectClass, GetAbilityLevel(Handle, Info));
	if (!Spec.IsValid()) return;
	Spec.Data->SetDuration(Context.Definition->FireInterval, true);
	ApplyGameplayEffectSpecToOwner(Handle, Info, Activation, Spec);
}

void UNGReloadAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bWasCancelled)
{
	if (!IsEndAbilityValid(Handle, Info)) return;
	if (auto* ASC = Info ? Info->AbilitySystemComponent.Get() : nullptr;
		bWasCancelled && ASC && ASC->GetAnimatingAbility() == this) ASC->CurrentMontageStop();
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bWasCancelled);
}
