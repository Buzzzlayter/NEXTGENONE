#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGEquipmentVisualActor.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UNGEquipmentComponent::UNGEquipmentComponent() { SetIsReplicatedByDefault(true); }
UNGEquipmentDefinition* UNGEquipmentComponent::GetDefinition(const UNGInventoryItemInstance* Item)
{
	return Item && Item->GetItemDef() ? Cast<UNGEquipmentDefinition>(GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef())->GetEquipmentDefinition()) : nullptr;
}
void UNGEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();
	RefreshReplicatedState();
}
void UNGEquipmentComponent::EndPlay(EEndPlayReason::Type Reason)
{
	bEnding = true; Shutdown();
	BindContext(nullptr, nullptr, nullptr);
	if (GetOwner()->HasAuthority()) for (ANGEquipmentVisualActor* Actor : Visuals) if (IsValid(Actor)) Actor->Destroy();
	Visuals.Reset();
	for (const auto& Source : ObservedSources)
		if (Source.IsValid()) Source->OnSourceReplicated.RemoveAll(this);
	ObservedSources.Reset();
	for (const auto& Visual : ObservedVisuals)
		if (Visual.IsValid()) Visual->OnSourceReplicated.RemoveAll(this);
	ObservedVisuals.Reset();
	Super::EndPlay(Reason);
}
void UNGEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(ThisClass, GrantedAbilities, COND_OwnerOnly);
	DOREPLIFETIME(ThisClass, Installations); DOREPLIFETIME(ThisClass, HandsRevision); DOREPLIFETIME(ThisClass, Visuals);
}
bool UNGEquipmentComponent::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* Flags)
{
	bool Wrote = Super::ReplicateSubobjects(Channel, Bunch, Flags);
	for (UNGEquipmentInstance* Instance : Installations) if (Instance) Wrote |= Channel->ReplicateSubobject(Instance, *Bunch, *Flags);
	return Wrote;
}
void UNGEquipmentComponent::ReadyForReplication()
{
	Super::ReadyForReplication();
	if (IsUsingRegisteredSubObjectList()) for (UNGEquipmentInstance* Instance : Installations) if (Instance) AddReplicatedSubObject(Instance);
}
void UNGEquipmentComponent::BindContext(UAbilitySystemComponent* ASC,
	UNGInventoryManagerComponent* InInventory, USkeletalMeshComponent* InAttachmentMesh)
{
	if (bEnding) { ASC = nullptr; InInventory = nullptr; InAttachmentMesh = nullptr; }
	const bool bContextChanged = ObservedASC.Get() != ASC || Inventory.Get() != InInventory;
	if (bContextChanged)
	{
		Shutdown();
		if (Inventory.IsValid()) Inventory->OnInventoryChanged.RemoveDynamic(this, &ThisClass::InventoryChanged);
		if (auto* Previous = ObservedASC.Get())
		{
			Previous->OnActiveGameplayEffectAddedDelegateToSelf.Remove(EffectAddedHandle);
			Previous->OnAbilityEnded.Remove(AbilityEndedHandle);
		}
		if (ObservedASC.Get() != ASC) StartupAttempted.Reset();
		ObservedASC = ASC;
		Inventory = InInventory;
		EffectAddedHandle.Reset(); AbilityEndedHandle.Reset();
		if (ASC)
		{
			EffectAddedHandle = ASC->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(this, &ThisClass::EffectAdded);
			AbilityEndedHandle = ASC->OnAbilityEnded.AddUObject(this, &ThisClass::AbilityEnded);
		}
		if (InInventory) InInventory->OnInventoryChanged.AddUniqueDynamic(this, &ThisClass::InventoryChanged);
	}
	auto* Target = InAttachmentMesh;
	const bool bTargetChanged = AttachmentMesh.Get() != Target;
	if (bTargetChanged)
	{
		if (GetOwner()->HasAuthority()) for (ANGEquipmentVisualActor* Actor : Visuals) if (IsValid(Actor)) Actor->Destroy();
		if (GetOwner()->HasAuthority()) Visuals.Reset();
		AttachmentMesh = Target;
	}
	if (bContextChanged || bTargetChanged) RefreshReplicatedState();
}
void UNGEquipmentComponent::ActivateEquipment()
{
	if (bEnding) return;
	bSuspended = false;
	if (GetOwner()->HasAuthority()) Reconcile();
	else RefreshReplicatedState();
}
bool UNGEquipmentComponent::CanApply() const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* ASC = ObservedASC.Get();
	return !bEnding && !bSuspended && Pawn && ASC && Inventory.IsValid()
		&& Inventory->GetOwner() == ASC->GetOwnerActor()
		&& NGPlayerContext::GetCurrentASC(Pawn, false) == ASC
		&& !ASC->HasMatchingGameplayTag(TAG_NGDead)
		&& ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) > 0.f;
}
UNGEquipmentInstance* UNGEquipmentComponent::FindInstallation(FGameplayTag Slot) const
{
	for (UNGEquipmentInstance* Instance : Installations) if (Instance && Instance->GetSlot() == Slot) return Instance;
	if (GetOwner()->HasAuthority())
		for (const auto& Pair : Resources)
			if (Pair.Key && Pair.Key->GetSlot() == Slot) return Pair.Key;
	return nullptr;
}
UNGEquipmentInstance* UNGEquipmentComponent::FindInstance(FGameplayTag Slot) const
{
	for (UNGEquipmentInstance* Instance : Installations)
		if (Instance && Instance->GetSlot() == Slot) return Instance;
	return nullptr;
}
ANGEquipmentVisualActor* UNGEquipmentComponent::GetVisualActor(FGameplayTag Slot) const
{
	const auto* Instance = FindInstance(Slot);
	if (!Instance) return nullptr;
	for (ANGEquipmentVisualActor* Actor : Visuals)
		if (IsValid(Actor) && Actor->GetEquipmentSource() == Instance) return Actor;
	return nullptr;
}

bool UNGEquipmentComponent::IsCurrent(const UNGEquipmentInstance* Instance) const
{
	return Instance && FindInstance(Instance->GetSlot()) == Instance && IsInstallationValid(Instance);
}
bool UNGEquipmentComponent::IsInstallationValid(const UNGEquipmentInstance* Instance) const
{
	if (!Instance || !Instance->GetDefinition() || Instance->GetAvatar() != GetOwner() || !CanApply() || FindInstallation(Instance->GetSlot()) != Instance) return false;
	if (GetOwner()->HasAuthority())
	{
		if (Inventory->GetCount(Instance->GetItem()) != 1) return false;
		const auto Slot = Inventory->GetSlotTag(Inventory->GetSlotIndex(Instance->GetItem()));
		return Slot == Instance->GetSlot();
	}
	return Instance->GetItem() && Instance->GetItem()->GetItemDef();
}
bool UNGEquipmentComponent::Preflight(UNGInventoryItemInstance* Item, UNGEquipmentDefinition* Definition, FString& Error) const
{
	if (!CanApply() || !Item || !Item->GetItemDef() || !Definition || !Definition->ValidateDefinition(Error)) return false;
	TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
	Definition->ResolveResources(Grants, Effects);
	// Resource loading can invoke callbacks. Read the bound context only after it finishes.
	if (!CanApply()) return false;
	auto* ASC = ObservedASC.Get();
	const auto* Target = AttachmentMesh.Get();
	if (Definition->VisualActorClass && (!Target || Definition->AttachSocket.IsNone() || !Target->DoesSocketExist(Definition->AttachSocket)))
	{ Error = TEXT("Equipment visual requires an existing socket or bone on the Avatar mesh."); return false; }
	const auto* Replaced = FindInstallation(Definition->SlotTag);
	// GAS defers ClearAbility during item activation; retired specs no longer reserve input tags.
	for (const auto& Grant : Grants) if (Grant.Grant.InputTag.IsValid())
		for (const auto& Spec : ASC->GetActivatableAbilities())
			if (!Spec.PendingRemove && Spec.SourceObject.Get() != Replaced && Spec.GetDynamicSpecSourceTags().HasTagExact(Grant.Grant.InputTag))
			{ Error = FString::Printf(TEXT("InputTag %s already belongs to another grant."), *Grant.Grant.InputTag.ToString()); return false; }
	Error.Reset(); return true;
}
bool UNGEquipmentComponent::PreflightPlacement(UNGInventoryItemInstance* Item, int32 SlotIndex, FString& Error) const
{
	if (!Inventory.IsValid()) return false;
	const auto Slot = Inventory->GetSlotTag(SlotIndex);
	if (!Slot.MatchesTag(TAG_NGSlotEquipment)) return true;
	const auto* ItemDef = Item && Item->GetItemDef() ? GetDefault<UNGInventoryItemDefinition>(Item->GetItemDef()) : nullptr;
	auto* Definition = GetDefinition(Item);
	if (!ItemDef || ItemDef->MaxStackSize != 1 || !Definition || Definition->SlotTag != Slot)
	{ Error = TEXT("Item does not fit this equipment slot."); return false; }
	return Preflight(Item, Definition, Error);
}
bool UNGEquipmentComponent::Install(UNGInventoryItemInstance* Item, UNGEquipmentDefinition* Definition)
{
	if (!CanApply() || Inventory->GetCount(Item) != 1 || GetDefinition(Item) != Definition) return false;
	auto* ASC = ObservedASC.Get();
	auto* Instance = NewObject<UNGEquipmentInstance>(GetOwner());
	Instance->Initialize(Item, Definition, ++NextGeneration);
	Resources.Add(Instance).GrantedASC = ASC;
	FNGGrantedHandles Handles;
	auto Abort = [&]() { Handles.Remove(ASC); Remove(Instance); FailedItems.Add(Item); return false; };
	TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
	Definition->ResolveResources(Grants, Effects);
	if (!IsInstallationValid(Instance)) return Abort();
	for (const auto& Entry : Grants)
	{
		const auto Handle = Entry.Grant.Give(ASC, Instance); Handles.Abilities.Add(Handle);
		if (!Handle.IsValid() || !IsInstallationValid(Instance)) return Abort();
	}
	for (const auto& Class : Effects)
	{
		auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(Instance);
		const auto Handle = ASC->ApplyGameplayEffectToSelf(Class.GetDefaultObject(), 1.f, Context); Handles.Effects.Add(Handle);
		if (!Handle.IsValid() || !IsInstallationValid(Instance)) return Abort();
	}
	FNGEquipmentResources Owned; Owned.GrantedASC = ASC; Owned.Handles = MoveTemp(Handles); Resources.Add(Instance, MoveTemp(Owned));
	// GiveAbility called by an item ability may be deferred by GAS's ability-list lock.
	// Do not start or publish equipment until every returned spec really exists.
	for (auto Handle : Resources.FindChecked(Instance).Handles.Abilities)
		if (!ASC->FindAbilitySpecFromHandle(Handle)) { QueueDeferredUpdate(); return true; }
	return FinishInstall(Instance);
}
bool UNGEquipmentComponent::FinishInstall(UNGEquipmentInstance* Instance)
{
	auto Abort = [&]() { UE_LOG(LogTemp, Error, TEXT("Equipment installation failed after resource acquisition: %s"), *GetNameSafe(Instance)); FailedItems.Add(Instance->GetItem()); Remove(Instance); return false; };
	const auto* Owned = Resources.Find(Instance);
	if (!Owned || !IsInstallationValid(Instance)) return Abort();
	auto* ASC = Owned->GrantedASC.Get();
	FString Error;
	if (!Preflight(Instance->GetItem(), Instance->GetDefinition(), Error) || !IsInstallationValid(Instance)) return Abort();
	for (auto Handle : Resources.FindChecked(Instance).Handles.Abilities)
	{
		const auto* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
		if (!Spec || !Spec->Ability || Spec->PendingRemove || Spec->SourceObject.Get() != Instance) return Abort();
	}
	TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
	Instance->GetDefinition()->ResolveResources(Grants, Effects);
	if (!IsInstallationValid(Instance) || Grants.Num() != Resources.FindChecked(Instance).Handles.Abilities.Num()) return Abort();
	Installations.Add(Instance);
	GrantedAbilities.Append(Resources.FindChecked(Instance).Handles.Abilities);
	RefreshVisuals();
	if (Instance->GetDefinition()->VisualActorClass && !GetVisualActor(Instance->GetSlot())) return Abort();
	OnContextChanged.Broadcast();
	if (!IsInstallationValid(Instance)) return Abort();
	for (int32 I = 0; I < Grants.Num(); ++I)
	{
		const auto& Entry = Grants[I]; if (!Entry.Grant.bActivateOnEnter) continue;
		const auto Policy = Entry.Grant.Ability.GetDefaultObject()->GetNetExecutionPolicy();
		if (Policy != EGameplayAbilityNetExecutionPolicy::ServerOnly && Policy != EGameplayAbilityNetExecutionPolicy::ServerInitiated) continue;
		const auto* OwnedResources = Resources.Find(Instance);
		if (!OwnedResources || !IsInstallationValid(Instance)) return Abort();
		const auto Handle = OwnedResources->Handles.Abilities[I];
		if (Entry.bRequiredStartup) Resources.FindChecked(Instance).PendingRequiredStartup.Add(Handle);
		const bool Started = ASC->TryActivateAbility(Handle);
		if (!IsInstallationValid(Instance) || (Entry.bRequiredStartup && !Started)) return Abort();
	}
	if (!Preflight(Instance->GetItem(), Instance->GetDefinition(), Error) || !IsInstallationValid(Instance)) return Abort();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Instance);
	if (Instance->GetSlot() == TAG_NGSlotEquipmentWeapon) ++HandsRevision;
	Instance->GetItem()->SetEquipped(true);
	if (!IsInstallationValid(Instance)) return Abort();
	return true;
}
void UNGEquipmentComponent::Remove(UNGEquipmentInstance* Instance)
{
	if (!Instance || !Resources.Contains(Instance)) return;
	// Invalidate before any cancellation/GE/item callback, including same-item re-equip.
	Installations.Remove(Instance);
	if (Instance->GetSlot() == TAG_NGSlotEquipmentWeapon) ++HandsRevision;
	FNGEquipmentResources Owned;
	Resources.RemoveAndCopyValue(Instance, Owned);
	OnContextChanged.Broadcast();
	const auto RetiringVisuals = Visuals;
	for (ANGEquipmentVisualActor* Visual : RetiringVisuals)
		if (IsValid(Visual) && Visual->GetEquipmentSource() == Instance)
			Visual->Initialize(Instance, nullptr);
	for (auto Handle : Owned.Handles.Abilities)
	{
		GrantedAbilities.Remove(Handle);
		StartupAttempted.Remove(Handle);
	}
	Owned.Handles.Remove(Owned.GrantedASC);
	if (Instance->GetItem()) Instance->GetItem()->SetEquipped(false);
	if (IsUsingRegisteredSubObjectList()) RemoveReplicatedSubObject(Instance);
}
void UNGEquipmentComponent::Shutdown()
{
	bSuspended = true;
	bReconcilePending = false;
	AttachmentMesh.Reset();
	if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ReadinessTimer);
	if (GetOwner()->HasAuthority())
	{
		{
			TGuardValue<bool> Changing(bChanging, true);
			TArray<TObjectPtr<UNGEquipmentInstance>> Previous;
			Resources.GetKeys(Previous);
			for (UNGEquipmentInstance* Instance : Previous) Remove(Instance);
		}
		Publish();
	}
	else RefreshReplicatedState();
}
void UNGEquipmentComponent::Reconcile()
{
	if (!GetOwner()->HasAuthority() || bChanging || !CanApply()) return;
	{
		TGuardValue<bool> Changing(bChanging, true);
		TArray<TObjectPtr<UNGEquipmentInstance>> Previous;
		Resources.GetKeys(Previous);
		for (UNGEquipmentInstance* Instance : Previous) if (Instance && !IsInstallationValid(Instance)) Remove(Instance);
		for (auto It = FailedItems.CreateIterator(); It; ++It)
			if (!It->IsValid() || !Inventory->Contains(It->Get()) || !Inventory->GetSlotTag(Inventory->GetSlotIndex(It->Get())).MatchesTag(TAG_NGSlotEquipment)) It.RemoveCurrent();
		for (int32 I = 0; I < Inventory->GetSlotCount(); ++I)
		{
			const auto Slot = Inventory->GetSlotTag(I); auto* Item = Inventory->GetItemAtSlot(I);
			if (!Slot.MatchesTag(TAG_NGSlotEquipment) || !Item || FindInstallation(Slot) || FailedItems.Contains(Item)) continue;
			FString Error;
			if (!PreflightPlacement(Item, I, Error) || !Install(Item, GetDefinition(Item)))
			{ FailedItems.Add(Item); UE_LOG(LogTemp, Error, TEXT("Equipment application failed: %s slot=%s %s; placement retained, no automatic retry."), *GetNameSafe(Item), *Slot.ToString(), *Error); }
		}
	}
	Publish();
}
void UNGEquipmentComponent::InventoryChanged() { if (bChanging) bReconcilePending = true; else Reconcile(); QueueDeferredUpdate(); }
void UNGEquipmentComponent::Publish()
{
	if (bEnding || bChanging) return;
	RefreshReplicatedState();
	OnEquipmentChanged.Broadcast();
	GetOwner()->ForceNetUpdate();
}
void UNGEquipmentComponent::OnRep_Equipment()
{
	for (auto It = StartupAttempted.CreateIterator(); It; ++It)
		if (!GrantedAbilities.Contains(*It)) It.RemoveCurrent();
	RefreshReplicatedState();
}
void UNGEquipmentComponent::RefreshReplicatedState()
{
	if (bEnding) return;
	for (auto It = ObservedVisuals.CreateIterator(); It; ++It)
	{
		if (It->IsValid() && Visuals.Contains(It->Get())) continue;
		if (It->IsValid()) It->Get()->OnSourceReplicated.RemoveAll(this);
		It.RemoveCurrent();
	}
	for (ANGEquipmentVisualActor* Visual : Visuals)
		if (IsValid(Visual) && !ObservedVisuals.Contains(Visual))
		{
			Visual->OnSourceReplicated.AddUObject(this, &ThisClass::RefreshReplicatedState);
			ObservedVisuals.Add(Visual);
		}
	for (auto It = ObservedSources.CreateIterator(); It; ++It)
	{
		if (It->IsValid() && Installations.Contains(It->Get())) continue;
		if (It->IsValid()) It->Get()->OnSourceReplicated.RemoveAll(this);
		It.RemoveCurrent();
	}
	for (UNGEquipmentInstance* Instance : Installations)
		if (Instance && !ObservedSources.Contains(Instance))
		{
			Instance->OnSourceReplicated.AddUObject(this, &ThisClass::RefreshReplicatedState);
			ObservedSources.Add(Instance);
		}
	if (!GetOwner()->HasAuthority() && ObservedASC.IsValid())
	{
		TArray<FGameplayAbilitySpecHandle> Stale;
		for (const auto& Spec : ObservedASC->GetActivatableAbilities())
			if (const auto* Source = Cast<UNGEquipmentInstance>(Spec.SourceObject.Get()); Source && Source->GetAvatar() == GetOwner() && !IsCurrent(Source)) Stale.Add(Spec.Handle);
		for (auto Handle : Stale) ObservedASC->CancelAbilityHandle(Handle);
	}
	RefreshVisuals();
	OnContextChanged.Broadcast();
	if (!GetOwner()->HasAuthority()) OnEquipmentChanged.Broadcast();
	QueueDeferredUpdate();
}
void UNGEquipmentComponent::RefreshVisuals()
{
	auto* Target = AttachmentMesh.Get();
	if (!GetOwner()->HasAuthority())
	{
		// Actors arrive from the server. Source/ASC/attachment may resolve in either order.
		for (ANGEquipmentVisualActor* Actor : Visuals) if (IsValid(Actor))
		{
			auto* Source = Actor->GetEquipmentSource();
			const bool bInstalled = Source && Installations.Contains(Source);
			Actor->SetActorHiddenInGame(!Target || !bInstalled);
			Actor->Initialize(Source, Target && bInstalled ? ObservedASC.Get() : nullptr);
		}
		return;
	}
	for (int32 Index = Visuals.Num() - 1; Index >= 0; --Index)
	{
		auto* Actor = Visuals[Index].Get();
		const auto* Source = IsValid(Actor) ? Actor->GetEquipmentSource() : nullptr;
		if (!Target || !Source || !Installations.Contains(Source))
		{
			Visuals.RemoveAt(Index);
			if (IsValid(Actor)) Actor->Destroy();
		}
	}
	if (!Target) return;
	const auto CurrentInstallations = Installations;
	for (UNGEquipmentInstance* Instance : CurrentInstallations)
	{
		const auto* Def = Instance ? Instance->GetDefinition() : nullptr;
		if (!Instance || !Installations.Contains(Instance) || !Def || !Def->VisualActorClass) continue;
		if (auto* Existing = GetVisualActor(Instance->GetSlot()))
		{
			Existing->Initialize(Instance, ObservedASC.Get());
			continue;
		}
		const FTransform Transform = Def->VisualTransform * Target->GetSocketTransform(Def->AttachSocket);
		auto* Actor = GetWorld()->SpawnActorDeferred<ANGEquipmentVisualActor>(Def->VisualActorClass, Transform,
			GetOwner(), Cast<APawn>(GetOwner()), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Actor) continue;
		Actor->SetReplicates(true);
		Actor->SetReplicateMovement(false);
		Visuals.Add(Actor);
		Actor->OnSourceReplicated.AddUObject(this, &ThisClass::RefreshReplicatedState);
		ObservedVisuals.Add(Actor);
		Actor->Initialize(Instance, ObservedASC.Get());
		Actor->FinishSpawning(Transform);
		if (!IsValid(Actor)) continue;
		// Blueprint construction/BeginPlay may unequip the source or release this Avatar.
		if (!Installations.Contains(Instance) || AttachmentMesh.Get() != Target)
		{ Visuals.Remove(Actor); Actor->Destroy(); continue; }
		Actor->SetActorEnableCollision(false);
		Actor->AttachToComponent(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Def->AttachSocket);
		Actor->SetActorRelativeTransform(Def->VisualTransform);
		Actor->ForceNetUpdate();
	}
}

void UNGEquipmentComponent::AbilityEnded(const FAbilityEndedData& Data)
{
	if (!GetOwner()->HasAuthority()) return;
	UNGEquipmentInstance* Failed = nullptr;
	for (auto& Pair : Resources)
		if (Pair.Value.PendingRequiredStartup.Remove(Data.AbilitySpecHandle) > 0)
		{ if (Data.bWasCancelled) Failed = Pair.Key; break; }
	if (Failed)
	{
		FailedItems.Add(Failed->GetItem());
		UE_LOG(LogTemp, Error, TEXT("Required equipment startup cancelled: %s"), *GetNameSafe(Failed));
		Remove(Failed); Publish();
	}
}
void UNGEquipmentComponent::EffectAdded(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& Spec, FActiveGameplayEffectHandle Handle)
{
	const auto* Instance = Cast<UNGEquipmentInstance>(Spec.GetContext().GetSourceObject());
	if (Instance && Instance->GetAvatar() == GetOwner()) QueueDeferredUpdate();
}
void UNGEquipmentComponent::QueueDeferredUpdate()
{
	if (bEnding || !GetWorld() || ReadinessTimer.IsValid()) return;
	ReadinessTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::ProcessDeferredUpdate);
}
void UNGEquipmentComponent::ProcessDeferredUpdate()
{
	ReadinessTimer.Invalidate();
	if (bReconcilePending) { bReconcilePending = false; Reconcile(); }
	if (GetOwner()->HasAuthority())
	{
		bool Changed = false;
		{
			TGuardValue<bool> Changing(bChanging, true);
			TArray<TObjectPtr<UNGEquipmentInstance>> Pending;
			Resources.GetKeys(Pending);
			for (UNGEquipmentInstance* Instance : Pending)
				if (Instance && !Installations.Contains(Instance))
				{ FinishInstall(Instance); Changed = true; }
		}
		if (Changed) Publish();
	}
	const auto* Pawn = Cast<APawn>(GetOwner()); auto* ASC = ObservedASC.Get();
	if (!CanApply() || !Pawn->IsLocallyControlled()) return;
	const auto Current = Installations;
	for (UNGEquipmentInstance* Instance : Current)
	{
		if (!Instance || !IsCurrent(Instance)) continue;
		TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
		Instance->GetDefinition()->ResolveResources(Grants, Effects);
		TArray<FGameplayAbilitySpecHandle> Handles;
		for (auto Handle : GrantedAbilities)
		{
			const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle);
			if (Spec && Spec->SourceObject.Get() == Instance) Handles.Add(Handle);
		}
		if (Handles.Num() != Grants.Num()) continue;
		bool bSpecsReady = true;
		for (auto Handle : Handles)
		{ const auto* Spec = ASC->FindAbilitySpecFromHandle(Handle); if (!Spec || !Spec->Ability || Spec->PendingRemove || Spec->SourceObject.Get() != Instance) { bSpecsReady = false; break; } }
		if (!bSpecsReady) continue;
		// GAS effects and specs may arrive in either order. GE arrival requeues readiness.
		int32 EffectCount = 0;
		for (const auto Handle : ASC->GetActiveEffects(FGameplayEffectQuery()))
			if (const auto* Effect = ASC->GetActiveGameplayEffect(Handle); Effect && Effect->Spec.GetContext().GetSourceObject() == Instance) ++EffectCount;
		if (EffectCount < Effects.Num()) continue;
		for (int32 I = 0; I < Grants.Num(); ++I)
		{
			const auto& Grant = Grants[I].Grant; if (!Grant.bActivateOnEnter) continue;
			const auto Policy = Grant.Ability.GetDefaultObject()->GetNetExecutionPolicy();
			if (Policy != EGameplayAbilityNetExecutionPolicy::LocalOnly && Policy != EGameplayAbilityNetExecutionPolicy::LocalPredicted) continue;
			const auto Handle = Handles[I];
			if (IsCurrent(Instance) && !StartupAttempted.Contains(Handle))
			{ StartupAttempted.Add(Handle); ASC->TryActivateAbility(Handle); }
		}
	}
}
