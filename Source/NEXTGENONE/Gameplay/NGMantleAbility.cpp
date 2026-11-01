#include "NGMantleAbility.h"
#include "NextgenoneCharacter.h"
#include "Components/ALSMantleComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/TimelineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Library/ALSMathLibrary.h"
#include "Engine/World.h"

void UNGALSMantleExecutor::BeginPlay()
{
	// A Blueprint component-class migration can lose a nested default-subobject reference.
	// Recover before the original ALS BeginPlay dereferences the timeline.
	if (!IsValid(MantleTimeline)) MantleTimeline = Cast<UTimelineComponent>(GetDefaultSubobjectByName(TEXT("MantleTimeline")));
	if (!IsValid(MantleTimeline) || MantleTimeline->IsTemplate())
	{
		MantleTimeline = NewObject<UTimelineComponent>(GetOwner());
		MantleTimeline->RegisterComponent();
	}
	Super::BeginPlay();
}
void UNGALSMantleExecutor::StopExecution() { if (IsValid(MantleTimeline)) MantleTimeline->Stop(); }

void UNGALSMantleExecutor::Server_MantleStart_Implementation(float Height, const FALSComponentAndTransform& Ledge, EALSMantleType Type)
{
	// A client-provided ledge must never move the authoritative Pawn.
	// UNGMantleAdapterComponent performs its own probe and multicast instead.
}

UNGMantleAdapterComponent::UNGMantleAdapterComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = .05f;
	TraceSettings.MinLedgeHeight = 40.f; TraceSettings.MaxLedgeHeight = 200.f;
	TraceSettings.ReachDistance = 80.f; TraceSettings.ForwardTraceRadius = 30.f; TraceSettings.DownwardTraceRadius = 30.f;
}
void UNGMantleAdapterComponent::BeginPlay()
{
	Super::BeginPlay();
	ALS = GetOwner()->FindComponentByClass<UALSMantleComponent>();
	if (ALS)
	{
		// Disable automatic/direct ALS initiation, but keep its timeline and Blueprint assets.
		ALS->SetComponentTickEnabled(false);
		ALS->PrimaryComponentTick.bCanEverTick = false;
		// Disable the original RPC path accepting client-supplied ledges.
		ALS->SetIsReplicated(false);
		if (auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner()))
			Pawn->JumpPressedDelegate.RemoveDynamic(ALS, &UALSMantleComponent::OnOwnerJumpInput);
	}
}
bool UNGMantleAdapterComponent::CanStart() const
{
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	return ALS && ALS->IsA<UNGALSMantleExecutor>() && Pawn && !Pawn->IsDead() && Pawn->GetController() && Pawn->GetAbilitySystemComponent()
		&& !bRunning && Pawn->GetMovementAction() == EALSMovementAction::None
		&& (Pawn->GetMovementState() == EALSMovementState::Grounded || Pawn->GetMovementState() == EALSMovementState::InAir)
		&& Pawn->GetCharacterMovement()->GetGravityDirection().Equals(FVector(0,0,-1), .01f);
}
bool UNGMantleAdapterComponent::Probe(float& Height, FALSComponentAndTransform& Ledge, EALSMantleType& Type) const
{
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn || !CanStart()) return false;
	const auto* Movement = Pawn->GetCharacterMovement();
	const FVector Base = UALSMathLibrary::GetCapsuleBaseLocation(2.f, Pawn->GetCapsuleComponent());
	const FVector Forward = Pawn->GetActorForwardVector();
	const float MinHeight = FMath::Clamp(TraceSettings.MinLedgeHeight, 10.f, 250.f);
	const float MaxHeight = FMath::Clamp(TraceSettings.MaxLedgeHeight, MinHeight + 1.f, 300.f);
	const float Radius = FMath::Clamp(TraceSettings.ForwardTraceRadius, 5.f, 50.f);
	FVector Start = Base - Forward * 30.f + FVector(0,0,(MinHeight+MaxHeight)*.5f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGMantle), false, Pawn);
	FHitResult Wall;
	if (!GetWorld()->SweepSingleByProfile(Wall, Start, Start + Forward * FMath::Clamp(TraceSettings.ReachDistance, 10.f, 150.f),
		FQuat::Identity, TEXT("IgnoreOnlyPawn"), FCollisionShape::MakeCapsule(Radius, FMath::Max(Radius, (MaxHeight-MinHeight)*.5f+1.f)), Params)
		|| !Wall.IsValidBlockingHit() || Movement->IsWalkable(Wall)) return false;
	if (!Wall.GetComponent() || Wall.GetComponent()->GetComponentVelocity().Size() > 10.f) return false;
	FVector End = Wall.ImpactPoint - Wall.ImpactNormal * 15.f; End.Z = Base.Z;
	FHitResult Top;
	const float DownRadius = FMath::Clamp(TraceSettings.DownwardTraceRadius, 5.f, 50.f);
	if (!GetWorld()->SweepSingleByChannel(Top, End + FVector(0,0,MaxHeight+DownRadius+1.f), End, FQuat::Identity,
		ECC_Visibility, FCollisionShape::MakeSphere(DownRadius), Params) || !Top.IsValidBlockingHit() || !Movement->IsWalkable(Top)) return false;
	if (!Top.GetComponent() || Top.GetComponent()->GetComponentVelocity().Size() > 10.f) return false;
	const FVector Target = UALSMathLibrary::GetCapsuleLocationFromBase(FVector(Top.Location.X, Top.Location.Y, Top.ImpactPoint.Z), 2.f, Pawn->GetCapsuleComponent());
	if (!UALSMathLibrary::CapsuleHasRoomCheck(Pawn->GetCapsuleComponent(), Target, 0.f, 0.f, EDrawDebugTrace::None, false)) return false;
	Height = Target.Z - Pawn->GetActorLocation().Z;
	Type = Pawn->GetMovementState() == EALSMovementState::InAir ? EALSMantleType::FallingCatch :
		(Height > 125.f ? EALSMantleType::HighMantle : EALSMantleType::LowMantle);
	Ledge.Component = Top.GetComponent();
	Ledge.Transform = FTransform((-Wall.ImpactNormal * FVector(1,1,0)).Rotation(), Target);
	// ALS MantleStart asserts if its existing Blueprint cannot supply an asset.
	const auto Asset = ALS->GetMantleAsset(Type, Pawn->GetOverlayState());
	return Asset.PositionCorrectionCurve && FMath::IsFinite(Asset.LowPlayRate) && Asset.LowPlayRate > 0.f
		&& FMath::IsFinite(Asset.HighPlayRate) && Asset.HighPlayRate > 0.f;
}
bool UNGMantleAdapterComponent::StartOnServer(TFunctionRef<bool()> Commit)
{
	if (!GetOwner()->HasAuthority()) return false;
	float Height; FALSComponentAndTransform Ledge; EALSMantleType Type;
	if (!Probe(Height, Ledge, Type)) return false;
	if (!Commit()) return false;
	bRunning = true; FinishDeadline = GetWorld()->GetTimeSeconds() + 6.f;
	MulticastStart(Height, Ledge, Type);
	return true;
}
void UNGMantleAdapterComponent::MulticastStart_Implementation(float Height, FALSComponentAndTransform Ledge, EALSMantleType Type)
{
	const auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (ALS && Pawn && !Pawn->IsDead() && IsValid(Ledge.Component)) ALS->MantleStart(Height, Ledge, Type);
}
void UNGMantleAdapterComponent::MulticastCancel_Implementation()
{
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!ALS || !Pawn || Pawn->GetMovementState() != EALSMovementState::Mantling) return;
	if (auto* Executor = Cast<UNGALSMantleExecutor>(ALS)) Executor->StopExecution();
	if (auto* Anim = Pawn->GetMesh()->GetAnimInstance()) Anim->Montage_Stop(.1f);
	if (Pawn->IsDead() || !Pawn->GetController()) Pawn->GetCharacterMovement()->DisableMovement();
	else Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
}
void UNGMantleAdapterComponent::CancelOnServer()
{
	if (!GetOwner()->HasAuthority() || !bRunning) return;
	bRunning = false; MulticastCancel();
	OnMantleFinished.Broadcast(true);
}
void UNGMantleAdapterComponent::TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* ThisTick)
{
	Super::TickComponent(Delta, Tick, ThisTick);
	if (!GetOwner()->HasAuthority()) return;
	auto* Pawn = Cast<ANextgenoneCharacter>(GetOwner());
	if (!Pawn) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (bRunning)
	{
		if (Pawn->IsDead() || !Pawn->GetController() || Now > FinishDeadline) CancelOnServer();
		else if (Pawn->GetMovementState() != EALSMovementState::Mantling) { bRunning = false; OnMantleFinished.Broadcast(false); }
		return;
	}
	if (bAutomaticFallingCatch && Now >= NextAutoAttempt && Pawn->GetMovementState() == EALSMovementState::InAir && Pawn->HasMovementInput())
	{
		NextAutoAttempt = Now + .15;
		if (auto* ASC = Cast<UNGAbilitySystemComponent>(Pawn->GetAbilitySystemComponent())) ASC->TryActivateAbilityByInputTag(TAG_NGInputMantle);
	}
}
void UNGMantleAdapterComponent::EndPlay(EEndPlayReason::Type Reason)
{
	CancelOnServer(); Super::EndPlay(Reason);
}
UNGMantleAbility::UNGMantleAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead); ActivationBlockedTags.AddTag(TAG_NGDashActive); ActivationBlockedTags.AddTag(TAG_NGMantleActive);
	ActivationOwnedTags.AddTag(TAG_NGMantleActive);
	ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	FGameplayTagContainer Tags; Tags.AddTag(TAG_NGMovementAbility); SetAssetTags(Tags);
}
bool UNGMantleAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	const auto* Pawn = Cast<ANextgenoneCharacter>(Info->AvatarActor.Get());
	return Pawn && Pawn->MantleAdapter && Pawn->GetAbilitySystemComponent() == Info->AbilitySystemComponent.Get() && Pawn->MantleAdapter->CanStart();
}
void UNGMantleAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Pawn = Info ? Cast<ANextgenoneCharacter>(Info->AvatarActor.Get()) : nullptr;
	if (!Pawn || !Info->IsNetAuthority() || !Pawn->MantleAdapter->StartOnServer([&]() { return CommitAbility(Handle, Info, Activation); }))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	ActiveAdapter = Pawn->MantleAdapter;
	ActiveAdapter->OnMantleFinished.AddDynamic(this, &ThisClass::Finished);
}
void UNGMantleAbility::Finished(bool bCancelled) { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled); }
void UNGMantleAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (auto* Adapter = ActiveAdapter.Get())
	{
		Adapter->OnMantleFinished.RemoveDynamic(this, &ThisClass::Finished);
		if (bCancelled) Adapter->CancelOnServer();
	}
	ActiveAdapter.Reset(); Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
