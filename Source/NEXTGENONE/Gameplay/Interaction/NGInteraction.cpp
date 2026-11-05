#include "Gameplay/Interaction/NGInteraction.h"
#include "Gameplay/Abilities/NGALSActionAbilities.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "Gameplay/Interaction/NGItemPickup.h"
#include "Gameplay/Combat/NGCombat.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Engine/OverlapResult.h"
#include "Gameplay/Character/NextgenoneCharacter.h"
#include "Gameplay/Player/NextgenonePlayerState.h"
#include "Input/PlayerInputSubsystem.h"
#include "Inventory/NGInventoryManagerComponent.h"
#include "Inventory/NGInventoryItemDefinition.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"

UE_DEFINE_GAMEPLAY_TAG(TAG_NGInteractionBlocked, "State.Interaction.Blocked");
UE_DEFINE_GAMEPLAY_TAG(TAG_NGInteractionAbility, "Ability.Type.Interaction");

namespace
{
    FGameplayTagContainer DefaultInteractionInterruptions()
    {
        FGameplayTagContainer Tags;
        Tags.AddTag(TAG_NGInteractionBlocked); Tags.AddTag(TAG_NGDead);
        Tags.AddTag(TAG_NGDashActive); Tags.AddTag(TAG_NGMantleActive);
        Tags.AddTag(TAG_NGPunchActive); Tags.AddTag(TAG_NGReloadActive);
		Tags.AddTag(TAG_NGRollActive); Tags.AddTag(TAG_NGRagdollActive);
        return Tags;
    }
}

bool UNGInteractionSubsystem::IsCurrentInteractor(const ANextgenoneCharacter* Pawn,bool bAuthority) const
{
	if (!IsValid(Pawn) || (Pawn->Interaction && Pawn->Interaction->IsInteractionBlocked()) || Pawn->GetWorld()!=GetWorld() || (bAuthority && !Pawn->HasAuthority()) || Pawn->IsDead() || !Pawn->GetController() || Pawn->GetController()->GetPawn()!=Pawn) return false;
	const auto* PS=Pawn->GetPlayerState<ANextgenonePlayerState>();
	const auto* ASC=Pawn->GetAbilitySystemComponent();
	return PS && ASC && ASC==PS->GetAbilitySystemComponent() && ASC->GetAvatarActor()==Pawn && Pawn->GetHealth()>0.f;
}
AActor* UNGInteractionSubsystem::FindTarget(ANextgenoneCharacter* Pawn,float MaxDistance,float CaptureRadius) const
{
	if (!IsCurrentInteractor(Pawn,false) || !FMath::IsFinite(MaxDistance) || !FMath::IsFinite(CaptureRadius)) return nullptr;
	FVector View,Direction;
	if (!Pawn->Aim || !Pawn->Aim->GetViewRay(View,Direction)) return nullptr;
	FVector Eye; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye,Rotation);
	const float Range=FMath::Clamp(MaxDistance,50.f,500.f);
	const float Radius=FMath::Clamp(CaptureRadius,0.f,100.f);
	const float RayLength=FVector::Distance(View,Eye)+Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGInteraction),false,Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,View,View+Direction*RayLength,NGInteractionChannel,Params);
	auto* Target=Hit.GetActor();
	// A precise center hit wins. The wider capture only assists when the center misses an interactable.
	if (ValidateTarget(Pawn,Target,Range)) return Target;
	if (Radius<=0.f) return nullptr;
	TArray<FOverlapResult> Nearby;
	GetWorld()->OverlapMultiByChannel(Nearby,Pawn->GetActorLocation(),FQuat::Identity,NGInteractionChannel,FCollisionShape::MakeSphere(Range),Params);
	AActor* Best=nullptr; float BestScore=TNumericLimits<float>::Max(); TSet<AActor*> Visited;
	for (const auto& Overlap:Nearby)
	{
		auto* Candidate=Overlap.GetActor();
		if (!IsValid(Candidate) || Visited.Contains(Candidate)) continue;
		Visited.Add(Candidate);
		if (!Candidate->Implements<UNGInteractable>()) continue;
		const FVector Offset=Candidate->GetActorLocation()-View;
		const float Along=FVector::DotProduct(Offset,Direction);
		if (Along<0.f || Along>RayLength) continue;
		const float OffAxisSquared=(Offset-Direction*Along).SizeSquared();
		if (OffAxisSquared>FMath::Square(Radius) || !ValidateTarget(Pawn,Candidate,Range)) continue;
		FHitResult CameraLOS;
		GetWorld()->LineTraceSingleByChannel(CameraLOS,View,Candidate->GetActorLocation(),NGInteractionChannel,Params);
		if (CameraLOS.bBlockingHit && CameraLOS.GetActor()!=Candidate) continue;
		const float Score=OffAxisSquared+FVector::DistSquared(Pawn->GetActorLocation(),Candidate->GetActorLocation())*.001f;
		if (Score<BestScore) { BestScore=Score; Best=Candidate; }
	}
	return Best;
}
bool UNGInteractionSubsystem::ValidateTarget(ANextgenoneCharacter* Pawn,AActor* Target,float MaxDistance) const
{
	if (!IsCurrentInteractor(Pawn,false) || !IsValid(Target) || Target->GetWorld()!=GetWorld() || !Target->GetIsReplicated() || !Target->Implements<UNGInteractable>() || !FMath::IsFinite(MaxDistance)) return false;
	if (FVector::DistSquared(Pawn->GetActorLocation(),Target->GetActorLocation())>FMath::Square(FMath::Clamp(MaxDistance,50.f,500.f))) return false;
	FVector Eye; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye,Rotation); Rotation=Pawn->GetController()->GetControlRotation();
	const FVector ToTarget=Target->GetActorLocation()-Eye;
	if (Eye.ContainsNaN() || Rotation.ContainsNaN() || ToTarget.ContainsNaN()) return false;
	// A bounded view cone accommodates third-person camera parallax; never trust the client's camera position.
	if (ToTarget.SizeSquared()>FMath::Square(40.f) && FVector::DotProduct(Rotation.Vector(),ToTarget.GetSafeNormal())<.5f) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGInteractionLOS),false,Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,Eye,Target->GetActorLocation(),NGInteractionChannel,Params);
	return !Hit.bBlockingHit || Hit.GetActor()==Target;
}
bool UNGInteractionSubsystem::TryInteract(ANextgenoneCharacter* Pawn,AActor* Target,float MaxDistance,TFunctionRef<bool()> Commit)
{
	if (!IsCurrentInteractor(Pawn) || !ValidateTarget(Pawn,Target,MaxDistance) || BusyActors.Contains(Target)) return false;
	BusyActors.Add(Target); ON_SCOPE_EXIT { BusyActors.Remove(Target); };
	if (!INGInteractable::Execute_CanInteract(Target,Pawn) || !Commit() || !IsCurrentInteractor(Pawn) || !ValidateTarget(Pawn,Target,MaxDistance) || !INGInteractable::Execute_CanInteract(Target,Pawn)) return false;
	ActiveInteractions.Add(Target,Pawn); ON_SCOPE_EXIT { ActiveInteractions.Remove(Target); };
	return INGInteractable::Execute_Interact(Target,Pawn);
}
bool UNGInteractionSubsystem::IsExecutingInteraction(const AActor* Target,const ANextgenoneCharacter* Pawn) const
{
	const auto* Active=ActiveInteractions.Find(const_cast<AActor*>(Target));
	return Active && Active->Get()==Pawn && IsCurrentInteractor(Pawn);
}
bool UNGInteractionSubsystem::TakePickup(ANGItemPickup* Pickup,ANextgenoneCharacter* Pawn)
{
	if (!IsValid(Pickup) || !IsExecutingInteraction(Pickup,Pawn) || !Pickup->IsAvailable() || ActivePickups.Contains(Pickup)) return false;
	ActivePickups.Add(Pickup); ON_SCOPE_EXIT { ActivePickups.Remove(Pickup); };
	auto* PS=Pawn->GetPlayerState<ANextgenonePlayerState>();
	const auto State=Pickup->GetStateForTransfer();
	if (!PS->Inventory->CanAddItemDefinition(State.ItemClass,State.Count)) return false;
	// Initial placed items use definition fragments; dropped items restore the exact saved stats.
	auto* Item=Pickup->HasSavedItemState() ? PS->Inventory->AddItemState(State,false) : PS->Inventory->AddItemDefinitionDeferred(State.ItemClass,State.Count);
	if (!Item) return false;
	const bool bAutoEquip=Pickup->bAutoEquipWhenHandsEmpty;
	Pickup->ConsumeForTransfer();
	PS->Inventory->NotifyItemChange(Item,0,State.Count);
	const auto* Def=GetDefault<UNGInventoryItemDefinition>(State.ItemClass);
	if (bAutoEquip && UNGCombatComponent::GetWeaponDefinition(Def) && !Pawn->Combat->GetEquipment().bEquipped) Pawn->Combat->EquipInventoryItemOnServer(Item);
	return true;
}
ANGItemPickup* UNGInteractionSubsystem::DropInventoryItem(ANextgenoneCharacter* Pawn,UNGInventoryItemInstance* Item,int32 Count)
{
	if (!IsCurrentInteractor(Pawn) || !Item || ActiveDrops.Contains(Item)) return nullptr;
	auto* PS=Pawn->GetPlayerState<ANextgenonePlayerState>();
	const int32 OldCount=PS->Inventory->GetCount(Item); if (Count==0) Count=OldCount;
	if (Count<1 || Count>OldCount) return nullptr;
	const auto State=Item->CaptureState(Count);
	if (!State.ItemClass) return nullptr;
	// Stateful stacks need an explicit split policy; do not duplicate ammo/charges into two instances.
	if (Count<OldCount && !State.StatStacks.IsEmpty()) return nullptr;
	ActiveDrops.Add(Item); ON_SCOPE_EXIT { ActiveDrops.Remove(Item); };
	const FVector Up=-Pawn->GetCharacterMovement()->GetGravityDirection();
	const FVector Forward=FVector::VectorPlaneProject(Pawn->GetActorForwardVector(),Up).GetSafeNormal();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGDrop),false,Pawn);
	FVector Position=Pawn->GetActorLocation()+Forward*140.f;
	FHitResult Wall; if (GetWorld()->SweepSingleByChannel(Wall,Pawn->GetActorLocation(),Position,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(26.f),Params)) Position=Wall.Location-Forward*28.f;
	FHitResult Ground; if (!GetWorld()->LineTraceSingleByChannel(Ground,Position,Position-Up*600.f,ECC_Visibility,Params)) return nullptr;
	Position=Ground.ImpactPoint+Up*28.f;
	if (GetWorld()->OverlapBlockingTestByChannel(Position,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeSphere(24.f),Params)) return nullptr;
	if (FVector::DistSquared(Position,Pawn->GetActorLocation())>FMath::Square(650.f)) return nullptr;
	UClass* Class=LoadClass<ANGItemPickup>(nullptr,TEXT("/Game/Gameplay/Interaction/BP_ItemPickup.BP_ItemPickup_C"));
	if (!Class) Class=ANGItemPickup::StaticClass();
	const FTransform Transform(FRotationMatrix::MakeFromXZ(Forward,Up).Rotator(),Position);
	auto* Pickup=GetWorld()->SpawnActorDeferred<ANGItemPickup>(Class,Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup) return nullptr;
	Pickup->InitializeDroppedState(State); Pickup->FinishSpawning(Transform);
	// Construction scripts cannot debit the source or modify it behind the saved snapshot.
	if (!IsValid(Pickup) || !IsCurrentInteractor(Pawn) || PS->Inventory->GetCount(Item)!=OldCount ||
		!Item->CaptureState(Count).StatStacks.OrderIndependentCompareEqual(State.StatStacks) || !PS->Inventory->RemoveQuantity(Item,Count,false))
	{ if (IsValid(Pickup)) Pickup->Destroy(); return nullptr; }
	Pickup->PublishDrop();
	PS->Inventory->NotifyItemChange(Item,OldCount,OldCount-Count);
	return Pickup;
}
UNGInteractionComponent::UNGInteractionComponent()
{
	InterruptingTags = DefaultInteractionInterruptions();
	SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.1f;
}
void UNGInteractionComponent::BindAbilitySystem(UAbilitySystemComponent* ASC)
{
    if (BoundASC.Get() == ASC) return;
    if (auto* Previous = BoundASC.Get())
        for (const auto& Pair : TagHandles)
            Previous->RegisterGameplayTagEvent(Pair.Key).Remove(Pair.Value);
    TagHandles.Reset();
    BoundASC = ASC;
    if (!ASC) { EndInteraction(); return; }
    for (const auto& Tag : InterruptingTags)
        TagHandles.Add(Tag, ASC->RegisterGameplayTagEvent(Tag).AddUObject(this, &ThisClass::BlockingTagChanged));
    if (IsInteractionBlocked()) EndInteraction();
}

bool UNGInteractionComponent::IsInteractionBlocked() const
{
    const auto* ASC = BoundASC.Get();
    return ASC && ASC->GetAvatarActor() == GetOwner() && ASC->HasAnyMatchingGameplayTags(InterruptingTags);
}

void UNGInteractionComponent::BlockingTagChanged(FGameplayTag Tag, int32 Count)
{
    if (Count > 0 && IsInteractionBlocked()) EndInteraction();
}

void UNGInteractionComponent::EndInteraction()
{
    if (bEndingInteraction) return;
    TGuardValue<bool> Guard(bEndingInteraction, true);
    const auto Previous = Context;
    Context = {}; // Clear first: cancellation and session cleanup may re-enter gameplay code.
    if (auto* ASC = BoundASC.Get(); ASC && ASC->GetAvatarActor() == GetOwner() && GetOwner()->HasAuthority())
    {
        FGameplayTagContainer AbilityTags; AbilityTags.AddTag(TAG_NGInteractionAbility);
        ASC->CancelAbilities(&AbilityTags);
    }
    if (Previous.Type.IsValid())
    {
        OnContextChanged.Broadcast();
        OnInteractionEnded.Broadcast(Previous.Source.Get());
    }
}

void UNGInteractionComponent::SetConfirmedContext(FGameplayTag Type, AActor* Target, UObject* Source)
{
    Context.Type = Type; Context.Target = Target; Context.Source = Source;
    if (IsInteractionBlocked()) { EndInteraction(); return; }
    OnContextChanged.Broadcast();
}
void UNGInteractionComponent::ClearContext(UObject* ExpectedSource)
{
    if (Context.Source.Get() != ExpectedSource) return;
    Context = {};
    OnContextChanged.Broadcast();
}
void UNGInteractionComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick)
{
	Super::TickComponent(Delta,Tick,ThisTick);
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner());
	const auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this);
	if (!Pawn || !Pawn->IsLocallyControlled() || (Input && Input->IsGameplayInputBlocked())) { UpdateFocus(nullptr); return; }
	auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>(); UpdateFocus(System ? System->FindTarget(Pawn,MaxDistance,CaptureRadius) : nullptr);
}
void UNGInteractionComponent::UpdateFocus(AActor* Target)
{
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner());
	const FText Prompt=Target ? INGInteractable::Execute_GetInteractionPrompt(Target,Pawn) : FText::GetEmpty();
	const bool bAvailable=Target && INGInteractable::Execute_CanInteract(Target,Pawn);
	const bool bChanged=FocusedActor.Get()!=Target || !FocusedPrompt.EqualTo(Prompt) || bFocusedAvailable!=bAvailable;
	if (FocusedActor.Get()!=Target)
	{
		if (auto* Old=FocusedActor.Get()) INGInteractable::Execute_SetInteractionFocus(Old,Pawn,false);
		FocusedActor=Target; if (Target) INGInteractable::Execute_SetInteractionFocus(Target,Pawn,true);
	}
	FocusedPrompt=Prompt; bFocusedAvailable=bAvailable;
	if (bChanged) OnFocusChanged.Broadcast(Target,Prompt,bAvailable);
}
void UNGInteractionComponent::EndPlay(EEndPlayReason::Type Reason) { EndInteraction(); BindAbilitySystem(nullptr); UpdateFocus(nullptr); Super::EndPlay(Reason); }
void UNGInteractionComponent::RequestInteract()
{
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); if (!Pawn || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())) return;
	const auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this); if (Input && Input->IsGameplayInputBlocked()) return;
	auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>(); AActor* Target=System ? System->FindTarget(Pawn,MaxDistance,CaptureRadius) : nullptr;
	if (!Target) { OnInteractionResult.Broadcast(false); return; }
	if (Pawn->HasAuthority()) ServerRequestInteract_Implementation(Pawn,Target); else ServerRequestInteract(Pawn,Target);
}
void UNGInteractionComponent::ServerRequestInteract_Implementation(ANextgenoneCharacter* ExpectedPawn,AActor* ProposedTarget)
{
	auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (Pawn!=ExpectedPawn || !System || !System->IsCurrentInteractor(Pawn)) return;
	auto* PS=Pawn->GetPlayerState<ANextgenonePlayerState>();
	auto* ASC = PS ? Cast<UNGPlayerAbilitySystemComponent>(PS->GetAbilitySystemComponent()) : nullptr;
	if (!ASC || !ASC->TryCommitInteraction(Pawn, .15f)) return;
	FGameplayEventData Event; Event.EventTag=TAG_NGInteractEvent; Event.Instigator=Pawn; Event.Target=ProposedTarget;
	ASC->HandleGameplayEvent(TAG_NGInteractEvent,&Event);
}
void UNGInteractionComponent::ClientInteractionResult_Implementation(bool bSuccess) { OnInteractionResult.Broadcast(bSuccess); }
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
	auto* Pawn=Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	auto* System=Pawn ? Pawn->GetWorld()->GetSubsystem<UNGInteractionSubsystem>() : nullptr;
	bool bSuccess=false;
	if (Info && Info->IsNetAuthority() && System && Event && Event->Instigator.Get()==Pawn && System->IsCurrentInteractor(Pawn))
	{
		AActor* Target=const_cast<AActor*>(Event->Target.Get());
		const float Range=Pawn->Interaction ? Pawn->Interaction->MaxDistance : MaxDistance;
		bSuccess=System->TryInteract(Pawn,Target,Range,[&]() { return CommitAbility(Handle,Info,Activation); });
	}
	if (Pawn && Pawn->Interaction) Pawn->Interaction->ClientInteractionResult(bSuccess);
	EndAbility(Handle,Info,Activation,true,!bSuccess);
}
ANGInteractiveObject::ANGInteractiveObject()
{
	bReplicates=true; Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh")); SetRootComponent(Mesh); Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube")); Mesh->SetStaticMesh(Cube.Object);
	InteractionText=NSLOCTEXT("NEXTGENONE","UseObject","Использовать");
}
void ANGInteractiveObject::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ANGInteractiveObject,bEnabled); DOREPLIFETIME(ANGInteractiveObject,bActive);
}
FText ANGInteractiveObject::GetInteractionPrompt_Implementation(ANextgenoneCharacter* Interactor) const { return InteractionText; }
bool ANGInteractiveObject::CanInteract_Implementation(ANextgenoneCharacter* Interactor) const { return bEnabled && (!bOneShot || !bActive) && Interactor && !Interactor->IsDead(); }
bool ANGInteractiveObject::Interact_Implementation(ANextgenoneCharacter* Interactor)
{
	auto* System=GetWorld()->GetSubsystem<UNGInteractionSubsystem>();
	if (!HasAuthority() || !System || !System->IsExecutingInteraction(this,Interactor) || !CanInteract_Implementation(Interactor)) return false;
	bActive=!bActive; OnRep_Active(); ForceNetUpdate(); OnInteractedOnServer(Interactor); return true;
}
void ANGInteractiveObject::OnRep_Active() { OnStateChanged.Broadcast(bActive); }
void ANGInteractiveObject::SetInteractionFocus_Implementation(ANextgenoneCharacter* Interactor,bool bFocused) { Mesh->SetRenderCustomDepth(bFocused); }
