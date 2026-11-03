#include "NGMantleAbility.h"
#include "AbilitySystemGlobals.h"
#include "Components/TimelineComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"

UNGALSMantleExecutor::UNGALSMantleExecutor()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.TickInterval = .05f;
}
void UNGALSMantleExecutor::BeginPlay()
{
	SetIsReplicated(true); // Older component Blueprint templates predate this network bridge.
	// Preserve the original ALS timeline; older Blueprint class migrations could lose its reference.
	if (!IsValid(MantleTimeline)) MantleTimeline = Cast<UTimelineComponent>(GetDefaultSubobjectByName(TEXT("MantleTimeline")));
	if (!IsValid(MantleTimeline) || MantleTimeline->IsTemplate())
	{
		MantleTimeline = NewObject<UTimelineComponent>(GetOwner());
		MantleTimeline->RegisterComponent();
	}
	Super::BeginPlay();
	// The component still executes ALS, but initiation must pass through a granted GAS ability.
	if (auto* Pawn = Cast<AALSBaseCharacter>(GetOwner()))
		Pawn->JumpPressedDelegate.RemoveDynamic(this, &UALSMantleComponent::OnOwnerJumpInput);
}
bool UNGALSMantleExecutor::CanStart()
{
	auto* Pawn = Cast<AALSBaseCharacter>(GetOwner());
	auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!Pawn || !ASC || ASC->GetAvatarActor() != Pawn || ASC->HasMatchingGameplayTag(TAG_NGDead) ||
		!Pawn->GetController() || bRunning || !IsValid(MantleTimeline) || !MantleTimelineCurve ||
		Pawn->GetMovementAction() != EALSMovementAction::None ||
		(!Pawn->GetCharacterMovement()->IsMovingOnGround() && !Pawn->GetCharacterMovement()->IsFalling()) ||
		!Pawn->GetCharacterMovement()->GetGravityDirection().Equals(FVector(0,0,-1), .01f)) return false;
	// ALS asserts on missing curves. Validate the Blueprint's assets without duplicating its probe.
	for (const auto Type : {EALSMantleType::LowMantle, EALSMantleType::HighMantle, EALSMantleType::FallingCatch})
	{
		const auto Asset = GetMantleAsset(Type, Pawn->GetOverlayState());
		if (!Asset.PositionCorrectionCurve || !FMath::IsFinite(Asset.LowPlayRate) || Asset.LowPlayRate <= 0.f ||
			!FMath::IsFinite(Asset.HighPlayRate) || Asset.HighPlayRate <= 0.f) return false;
	}
	return true;
}
bool UNGALSMantleExecutor::StartFromAbility()
{
	if (!GetOwner()->HasAuthority() || !CanStart()) return false;
	auto* Pawn = CastChecked<AALSBaseCharacter>(GetOwner());
	TGuardValue<bool> ProbeGuard(bCheckingOnServer, true);
	bRunning = true;
	// Entire geometry/motion algorithm is the original ALS implementation.
	if (!MantleCheck(Pawn->GetMovementState() == EALSMovementState::InAir ? FallingTraceSettings : GroundedTraceSettings,
		EDrawDebugTrace::None) || !bRunning)
	{ bRunning = false; return false; }
	GetWorld()->GetTimerManager().SetTimer(ExecutionTimer, this, &ThisClass::MonitorExecution, .05f, true);
	return true;
}
void UNGALSMantleExecutor::Server_MantleStart_Implementation(float Height, const FALSComponentAndTransform& Ledge, EALSMantleType Type)
{
	// Only accept the synchronous result of our server-side ALS check, never a client-supplied ledge.
	if (GetOwner()->HasAuthority() && bCheckingOnServer && bRunning) MulticastAbilityStart(Height, Ledge, Type);
}
void UNGALSMantleExecutor::MulticastAbilityStart_Implementation(float Height, FALSComponentAndTransform Ledge, EALSMantleType Type)
{
	// ALS MantleCheck already started the server locally. The owner must also receive the server result.
	if (GetOwner()->HasAuthority()) return;
	auto* Pawn = Cast<AALSBaseCharacter>(GetOwner());
	if (Pawn && IsValid(Ledge.Component) && Pawn->GetMovementState() != EALSMovementState::Ragdoll)
		MantleStart(Height, Ledge, Type);
}
void UNGALSMantleExecutor::MulticastAbilityCancel_Implementation()
{
	if (IsValid(MantleTimeline)) MantleTimeline->Stop();
	auto* Pawn = Cast<AALSBaseCharacter>(GetOwner());
	if (!Pawn || Pawn->GetMovementState() != EALSMovementState::Mantling) return;
	if (auto* Anim = Pawn->GetMesh()->GetAnimInstance(); Anim && MantleParams.AnimMontage)
		Anim->Montage_Stop(.1f, MantleParams.AnimMontage);
	const auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!Pawn->GetController() || (ASC && ASC->HasMatchingGameplayTag(TAG_NGDead))) Pawn->GetCharacterMovement()->DisableMovement();
	else Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	SetComponentTickEnabled(true);
}
void UNGALSMantleExecutor::CancelFromAbility()
{
	if (!GetOwner()->HasAuthority() || !bRunning) return;
	bRunning = false;
	GetWorld()->GetTimerManager().ClearTimer(ExecutionTimer);
	MulticastAbilityCancel();
	OnMantleFinished.Broadcast(true);
}
void UNGALSMantleExecutor::MonitorExecution()
{
	auto* Pawn = Cast<AALSBaseCharacter>(GetOwner());
	auto* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn);
	if (!Pawn || !Pawn->GetController() || !ASC || ASC->GetAvatarActor() != Pawn || ASC->HasMatchingGameplayTag(TAG_NGDead) ||
		Pawn->GetMovementState() == EALSMovementState::Ragdoll) { CancelFromAbility(); return; }
	if (IsValid(MantleTimeline) && !MantleTimeline->IsPlaying())
	{
		bRunning = false;
		GetWorld()->GetTimerManager().ClearTimer(ExecutionTimer);
		OnMantleFinished.Broadcast(false);
	}
}
void UNGALSMantleExecutor::TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* ThisTick)
{
	// Do not call the base Tick: its automatic check would initiate mantle outside GAS.
	if (!GetOwner()->HasAuthority() || !bAutomaticFallingCatch || bRunning) return;
	auto* Pawn = Cast<AALSBaseCharacter>(GetOwner());
	if (!Pawn || Pawn->GetMovementState() != EALSMovementState::InAir || !Pawn->HasMovementInput()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now < NextAutoAttempt) return;
	NextAutoAttempt = Now + FMath::Max(.05f, AutoCatchInterval);
	if (auto* ASC = Cast<UNGAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pawn)))
		ASC->TryActivateAbilityByInputTag(TAG_NGInputMantle);
}
void UNGALSMantleExecutor::EndPlay(EEndPlayReason::Type Reason)
{
	CancelFromAbility();
	GetWorld()->GetTimerManager().ClearTimer(ExecutionTimer);
	Super::EndPlay(Reason);
}

UNGMantleAbility::UNGMantleAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead); ActivationBlockedTags.AddTag(TAG_NGDashActive);
	ActivationBlockedTags.AddTag(TAG_NGMantleActive); ActivationBlockedTags.AddTag(TAG_NGPunchActive);
	ActivationOwnedTags.AddTag(TAG_NGMantleActive);
	FGameplayTagContainer Tags; Tags.AddTag(TAG_NGMovementAbility); SetAssetTags(Tags);
}
bool UNGMantleAbility::CanActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags, FGameplayTagContainer* RelevantTags) const
{
	if (!Info || !Super::CanActivateAbility(Handle, Info, SourceTags, TargetTags, RelevantTags)) return false;
	auto* Component = Info->AvatarActor.IsValid() ? Info->AvatarActor->FindComponentByClass<UNGALSMantleExecutor>() : nullptr;
	return Component && Component->CanStart();
}
void UNGMantleAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	auto* Component = Info && Info->AvatarActor.IsValid() ? Info->AvatarActor->FindComponentByClass<UNGALSMantleExecutor>() : nullptr;
	if (!Component || !Info->IsNetAuthority() || !CommitAbility(Handle, Info, Activation))
	{ EndAbility(Handle, Info, Activation, true, true); return; }
	ExecutionComponent = Component;
	Component->OnMantleFinished.AddDynamic(this, &ThisClass::Finished);
	if (!Component->StartFromAbility()) EndAbility(Handle, Info, Activation, true, true);
}
void UNGMantleAbility::Finished(bool bCancelled)
{ EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, bCancelled); }
void UNGMantleAbility::EndAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, bool bReplicateEnd, bool bCancelled)
{
	if (!IsActive()) return;
	auto* Component = ExecutionComponent.Get(); ExecutionComponent.Reset();
	if (Component)
	{
		Component->OnMantleFinished.RemoveDynamic(this, &ThisClass::Finished);
		if (bCancelled) Component->CancelFromAbility();
	}
	Super::EndAbility(Handle, Info, Activation, bReplicateEnd, bCancelled);
}
