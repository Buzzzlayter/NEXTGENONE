#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Gameplay/Equipment/NGEquipmentDefinition.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Inventory/NGPlayerInventoryManagerComponent.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "GameFramework/Pawn.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
	for (const auto& Pair : Visuals) if (Pair.Value) Pair.Value->DestroyComponent();
	Visuals.Reset();
	Super::EndPlay(Reason);
}
void UNGEquipmentComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Installations); DOREPLIFETIME(ThisClass, HandsRevision);
	DOREPLIFETIME_CONDITION(ThisClass, SelectedItem, COND_OwnerOnly);
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
	UNGPlayerInventoryManagerComponent* InInventory, USkeletalMeshComponent* InAttachmentMesh)
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
		for (const auto& Pair : Visuals) if (Pair.Value) Pair.Value->DestroyComponent();
		Visuals.Reset(); AttachmentMesh = Target;
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
    for (UNGEquipmentInstance* Instance : Installations) if (Instance && Instance->Slot == Slot) return Instance;
    return nullptr;
}
UNGEquipmentInstance* UNGEquipmentComponent::FindInstance(FGameplayTag Slot) const
{
    auto* Instance = FindInstallation(Slot); return Instance && Instance->bInstalled ? Instance : nullptr;
}
bool UNGEquipmentComponent::IsCurrent(const UNGEquipmentInstance* Instance) const
{
	if (!Instance || !Instance->Definition || Instance->GetAvatar() != GetOwner() || !Installations.Contains(Instance) || !CanApply()) return false;
	if (GetOwner()->HasAuthority())
	{
		if (Inventory->GetCount(Instance->Item) != 1) return false;
		const auto Slot = Inventory->GetSlotTag(Inventory->GetSlotIndex(Instance->Item));
		return Instance->Slot == TAG_NGEquipmentHands ? Slot.MatchesTag(TAG_NGSlotAction) : Slot == Instance->Slot;
	}
	return Instance->Item && Instance->Item->GetItemDef();
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
	if ((Definition->Mesh || Definition->SkeletalMesh) && (!Target || Definition->AttachSocket.IsNone() || !Target->DoesSocketExist(Definition->AttachSocket)))
    { Error = TEXT("Equipment visual requires an existing socket or bone on the Avatar mesh."); return false; }
	const auto* Replaced = FindInstallation(Definition->SlotTag);
	for (const auto& Grant : Grants) if (Grant.Grant.InputTag.IsValid())
		for (const auto& Spec : ASC->GetActivatableAbilities())
			if (Spec.SourceObject.Get() != Replaced && Spec.GetDynamicSpecSourceTags().HasTagExact(Grant.Grant.InputTag))
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
void UNGEquipmentComponent::RequestEquip(bool bEquip)
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !CanApply() || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())) return;
	if (bEquip) { if (Inventory.IsValid() && SelectedItem) Inventory->RequestActivationItem(SelectedItem); }
	else if (Pawn->HasAuthority()) UnequipHands(); else ServerUnequipHands();
}
void UNGEquipmentComponent::ServerUnequipHands_Implementation() { UnequipHands(); }
void UNGEquipmentComponent::UnequipHands()
{
	if (!GetOwner()->HasAuthority()) return;
	Remove(FindInstallation(TAG_NGEquipmentHands)); Publish();
}
bool UNGEquipmentComponent::EquipHands(UNGInventoryItemInstance* Item)
{
	if (!GetOwner()->HasAuthority() || bChanging || !Inventory.IsValid() || !Inventory->Contains(Item) || Inventory->GetCount(Item) != 1) return false;
	auto* Definition = GetDefinition(Item); FString Error;
	if (!Definition || Definition->SlotTag != TAG_NGEquipmentHands) return false;
	TGuardValue<bool> Changing(bChanging, true);
	if (!Inventory->GetSlotTag(Inventory->GetSlotIndex(Item)).MatchesTag(TAG_NGSlotAction) || !Preflight(Item, Definition, Error))
	{ UE_LOG(LogTemp, Warning, TEXT("Equipment preflight rejected %s: %s"), *GetNameSafe(Item), *Error); return false; }
	if (auto* Existing = FindInstance(TAG_NGEquipmentHands); Existing && Existing->GetItem() == Item) return true;
	Remove(FindInstallation(TAG_NGEquipmentHands));
	SelectedItem = Item;
	const bool Success = Install(Item, Definition); Publish(); return Success;
}
bool UNGEquipmentComponent::Install(UNGInventoryItemInstance* Item, UNGEquipmentDefinition* Definition)
{
	if (!CanApply() || Inventory->GetCount(Item) != 1 || GetDefinition(Item) != Definition) return false;
	auto* ASC = ObservedASC.Get();
	auto* Instance = NewObject<UNGEquipmentInstance>(GetOwner());
	Instance->Item = Item; Instance->Definition = Definition; Instance->Slot = Definition->SlotTag; Instance->Generation = ++NextGeneration;
	Installations.Add(Instance);
	FNGGrantedHandles Handles;
	auto Abort = [&]() { Handles.Remove(ASC); Remove(Instance); FailedItems.Add(Item); return false; };
	TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
	Definition->ResolveResources(Grants, Effects);
	for (const auto& Entry : Grants)
	{
		const auto Handle = Entry.Grant.Give(ASC, Instance); Handles.Abilities.Add(Handle);
		if (!Handle.IsValid() || !IsCurrent(Instance)) return Abort();
	}
	for (const auto& Class : Effects)
	{
		auto Context = ASC->MakeEffectContext(); Context.AddSourceObject(Instance);
		const auto Handle = ASC->ApplyGameplayEffectToSelf(Class.GetDefaultObject(), 1.f, Context); Handles.Effects.Add(Handle);
		if (!Handle.IsValid() || !IsCurrent(Instance)) return Abort();
	}
	FNGEquipmentResources Owned; Owned.GrantedASC = ASC; Owned.Handles = MoveTemp(Handles); Resources.Add(Instance, MoveTemp(Owned));
	Instance->AbilityHandles = Resources.FindChecked(Instance).Handles.Abilities;
    // GiveAbility called by an item ability may be deferred by GAS's ability-list lock.
    // Do not start or publish equipment until every returned spec really exists.
    for (auto Handle : Instance->AbilityHandles)
        if (!ASC->FindAbilitySpecFromHandle(Handle)) { QueueDeferredUpdate(); return true; }
    return FinishInstall(Instance);
}
bool UNGEquipmentComponent::FinishInstall(UNGEquipmentInstance* Instance)
{
    auto Abort = [&]() { UE_LOG(LogTemp, Error, TEXT("Equipment installation failed after resource acquisition: %s"), *GetNameSafe(Instance)); FailedItems.Add(Instance->Item); Remove(Instance); return false; };
    const auto* Owned = Resources.Find(Instance);
    if (!Owned || !IsCurrent(Instance)) return Abort();
    auto* ASC = Owned->GrantedASC.Get();
    FString Error;
    if (!Preflight(Instance->Item, Instance->Definition, Error) || !IsCurrent(Instance)) return Abort();
    for (auto Handle : Instance->AbilityHandles)
    {
        const auto* Spec = ASC ? ASC->FindAbilitySpecFromHandle(Handle) : nullptr;
        if (!Spec || !Spec->Ability || Spec->PendingRemove || Spec->SourceObject.Get() != Instance) return Abort();
    }
    TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
    Instance->Definition->ResolveResources(Grants, Effects);
    if (Grants.Num() != Instance->AbilityHandles.Num()) return Abort();
	Instance->bInstalled = true;
	for (int32 I = 0; I < Grants.Num(); ++I)
	{
		const auto& Entry = Grants[I]; if (!Entry.Grant.bActivateOnEnter) continue;
		const auto Policy = Entry.Grant.Ability.GetDefaultObject()->GetNetExecutionPolicy();
		if (Policy != EGameplayAbilityNetExecutionPolicy::ServerOnly && Policy != EGameplayAbilityNetExecutionPolicy::ServerInitiated) continue;
		const auto* OwnedResources = Resources.Find(Instance);
		if (!OwnedResources || !IsCurrent(Instance)) return Abort();
		const auto Handle = OwnedResources->Handles.Abilities[I];
		if (Entry.bRequiredStartup) Resources.FindChecked(Instance).PendingRequiredStartup.Add(Handle);
		const bool Started = ASC->TryActivateAbility(Handle);
		if (!IsCurrent(Instance) || (Entry.bRequiredStartup && !Started)) return Abort();
	}
	if (!Preflight(Instance->Item, Instance->Definition, Error) || !IsCurrent(Instance)) return Abort();
	if (IsUsingRegisteredSubObjectList() && IsReadyForReplication()) AddReplicatedSubObject(Instance);
	if (Instance->Slot == TAG_NGEquipmentHands) ++HandsRevision;
	Instance->Item->SetEquipped(true);
	if (!IsCurrent(Instance)) return Abort();
	return true;
}
void UNGEquipmentComponent::Remove(UNGEquipmentInstance* Instance)
{
	if (!Instance || !Installations.Contains(Instance)) return;
	// Invalidate before any cancellation/GE/item callback, including same-item re-equip.
	Instance->bInstalled = false; Installations.Remove(Instance);
	if (Instance->Slot == TAG_NGEquipmentHands) ++HandsRevision;
	FNGEquipmentResources Owned;
	Resources.RemoveAndCopyValue(Instance, Owned);
	Owned.Handles.Remove(Owned.GrantedASC);
	if (Instance->Item) Instance->Item->SetEquipped(false);
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
		const auto Previous = Installations;
		for (UNGEquipmentInstance* Instance : Previous) Remove(Instance);
		Publish();
	}
    else RefreshReplicatedState();
}
void UNGEquipmentComponent::Reconcile()
{
	if (!GetOwner()->HasAuthority() || bChanging || !CanApply()) return;
	TGuardValue<bool> Changing(bChanging, true);
	const auto Previous = Installations;
	for (UNGEquipmentInstance* Instance : Previous) if (Instance && !IsCurrent(Instance)) Remove(Instance);
	if (SelectedItem && !Inventory->Contains(SelectedItem)) SelectedItem = nullptr;
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
	Publish();
}
void UNGEquipmentComponent::InventoryChanged() { if (bChanging) bReconcilePending = true; else Reconcile(); QueueDeferredUpdate(); }
void UNGEquipmentComponent::Publish()
{
	if (bEnding) return;
	RefreshReplicatedState(); GetOwner()->ForceNetUpdate();
}
void UNGEquipmentComponent::OnRep_Equipment() { RefreshReplicatedState(); }
void UNGEquipmentComponent::RefreshReplicatedState()
{
	if (bEnding) return;
	if (!GetOwner()->HasAuthority() && ObservedASC.IsValid())
    {
        TArray<FGameplayAbilitySpecHandle> Stale;
        for (const auto& Spec : ObservedASC->GetActivatableAbilities())
            if (const auto* Source = Cast<UNGEquipmentInstance>(Spec.SourceObject.Get()); Source && Source->GetEquipment() == this && !Source->IsCurrent()) Stale.Add(Spec.Handle);
        for (auto Handle : Stale) ObservedASC->CancelAbilityHandle(Handle);
    }
    RefreshVisuals(); OnEquipmentChanged.Broadcast(); QueueDeferredUpdate();
}
void UNGEquipmentComponent::RefreshVisuals()
{
	// The existing ALS held-object path remains the single Hands presentation adapter.
	auto* Target = AttachmentMesh.Get();
	for (auto It = Visuals.CreateIterator(); It; ++It)
    {
        if (!Target || !Installations.Contains(It.Key()) || !It.Key()->bInstalled)
        { if (It.Value()) It.Value()->DestroyComponent(); It.RemoveCurrent(); }
    }
    if (!Target || GetNetMode() == NM_DedicatedServer) return;
	for (UNGEquipmentInstance* Instance : Installations)
	{
		const auto* Def = Instance ? Instance->GetDefinition() : nullptr;
		if (!Instance || !Instance->bInstalled || !Def || Instance->Slot == TAG_NGEquipmentHands || Visuals.Contains(Instance)) continue;
		USceneComponent* Visual = nullptr;
		if (Def->Mesh) { auto* Mesh = NewObject<UStaticMeshComponent>(GetOwner()); Mesh->SetStaticMesh(Def->Mesh); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual = Mesh; }
		else if (Def->SkeletalMesh) { auto* Mesh = NewObject<USkeletalMeshComponent>(GetOwner()); Mesh->SetSkeletalMesh(Def->SkeletalMesh); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual = Mesh; }
		if (Visual) { Visual->SetupAttachment(Target, Def->AttachSocket); Visual->SetRelativeTransform(Def->VisualTransform); Visual->RegisterComponent(); Visuals.Add(Instance, Visual); }
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
	if (Instance && Instance->GetEquipment() == this) QueueDeferredUpdate();
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
        const auto Pending = Installations;
        for (UNGEquipmentInstance* Instance : Pending)
            if (Instance && !Instance->bInstalled)
            { FinishInstall(Instance); Changed = true; }
        if (Changed) Publish();
    }
	const auto* Pawn = Cast<APawn>(GetOwner()); auto* ASC = ObservedASC.Get();
	if (!CanApply() || !Pawn->IsLocallyControlled()) return;
	const auto Current = Installations;
	for (UNGEquipmentInstance* Instance : Current)
	{
		if (!Instance || !Instance->IsCurrent()) continue;
		TArray<FNGEquipmentAbilityGrant> Grants; TArray<TSubclassOf<UGameplayEffect>> Effects;
		Instance->GetDefinition()->ResolveResources(Grants, Effects);
		const auto Handles = Instance->AbilityHandles;
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
            if (!Instance->StartupAttempted.Contains(Handle) && Instance->IsCurrent())
            { Instance->StartupAttempted.Add(Handle); ASC->TryActivateAbility(Handle); }
		}
	}
}
