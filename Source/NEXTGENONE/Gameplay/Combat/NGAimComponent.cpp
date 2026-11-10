#include "Gameplay/Combat/NGAimComponent.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "GameplayTags/NGGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "Input/PlayerInputSubsystem.h"
#include "Gameplay/Interaction/NGInteraction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "Character/ALSBaseCharacter.h"

UNGAimComponent::UNGAimComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool UNGAimComponent::GetViewRay(FVector& Origin,FVector& Direction) const
{
	Origin=FVector::ZeroVector; Direction=FVector::ForwardVector;
	const auto* Pawn=Cast<APawn>(GetOwner());
	const auto* Controller=Pawn ? Pawn->GetController() : nullptr;
	if (!Controller || Controller->GetPawn()!=Pawn) return false;
	const auto* PC=Cast<APlayerController>(Controller);
	FRotator Rotation;
	Pawn->GetActorEyesViewPoint(Origin,Rotation); Rotation=Controller->GetControlRotation(); Direction=Rotation.Vector();
	if (PC && PC->IsLocalController() && Pawn->GetNetMode()!=NM_DedicatedServer)
	{
		// Use this player's constrained view rect: also correct for split-screen and letterboxing.
		const auto* Local=PC->GetLocalPlayer(); FSceneViewProjectionData Projection;
		if (Local && Local->ViewportClient && Local->ViewportClient->Viewport && Local->GetProjectionData(Local->ViewportClient->Viewport,Projection))
		{
			const FIntRect Rect=Projection.GetConstrainedViewRect();
			if (PC->DeprojectScreenPositionToWorld((Rect.Min.X+Rect.Max.X)*.5f,(Rect.Min.Y+Rect.Max.Y)*.5f,Origin,Direction))
				return !Origin.ContainsNaN() && !Direction.ContainsNaN() && Direction.Normalize();
		}
		PC->GetPlayerViewPoint(Origin,Rotation); Direction=Rotation.Vector();
	}
	return !Origin.ContainsNaN() && !Direction.ContainsNaN() && Direction.Normalize();
}
bool UNGAimComponent::ValidateShotView(const FVector& Origin, const FVector& Direction) const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || !Pawn->HasAuthority() || !Pawn->GetController() || Origin.ContainsNaN() || Direction.ContainsNaN()
		|| !FMath::IsFinite(MaxViewOriginDistance) || MaxViewOriginDistance <= 0.f
		|| !FMath::IsFinite(MaxViewAngleDifference) || !Direction.IsNormalized()) return false;
	FVector Eye; FRotator Rotation;
	Pawn->GetActorEyesViewPoint(Eye, Rotation);
	if (FVector::DistSquared(Eye, Origin) > FMath::Square(MaxViewOriginDistance)) return false;
	const FVector ServerDirection = Pawn->GetController()->GetControlRotation().Vector();
	if (FVector::DotProduct(ServerDirection, Direction) < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxViewAngleDifference, 0.f, 90.f)))) return false;
	// Use the same collision origin, radius and channel as the ALS camera.
	// A line from the eyes alone misses obstacles touching the camera's volume.
	FVector TraceOrigin = Eye;
	float Radius = 0.f;
	ECollisionChannel Channel = ECC_Visibility;
	if (auto* ALS = Cast<AALSBaseCharacter>(GetOwner()))
		Channel = ALS->GetThirdPersonTraceParams(TraceOrigin, Radius);
	if (TraceOrigin.ContainsNaN() || !FMath::IsFinite(Radius) || Radius < 0.f) return false;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGShotView), false, Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit;
	// Preserve the eye-to-view check as well: the ALS pivot can be below eye level.
	if (GetWorld()->LineTraceSingleByChannel(Hit, Eye, Origin, ECC_Visibility, Params)) return false;
	const bool bBlocked = Radius > 0.f
		? GetWorld()->SweepSingleByChannel(Hit, TraceOrigin, Origin, FQuat::Identity, Channel, FCollisionShape::MakeSphere(Radius), Params)
		: GetWorld()->LineTraceSingleByChannel(Hit, TraceOrigin, Origin, Channel, Params);
	return !bBlocked;
}
FVector UNGAimComponent::GetTargetingOrigin(const FVector& ViewOrigin, const FVector& ViewDirection) const
{
	// Moving along the view ray preserves screen alignment and avoids tracing behind the shooter.
	const float Distance = FVector::DotProduct(GetOwner()->GetActorLocation() - ViewOrigin, ViewDirection);
	return ViewOrigin + ViewDirection * FMath::Max(0.f, Distance);
}
bool UNGAimComponent::ShouldShowReticle() const
{
	const auto* Pawn=Cast<APawn>(GetOwner()); const auto* PC=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const auto* ASC = NGPlayerContext::GetCurrentASC(Pawn, false);
	const auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this);
	return bShowReticle && PC && PC->IsLocalController() && ASC
		&& !ASC->HasMatchingGameplayTag(TAG_NGDead) && !(Input && Input->IsGameplayInputBlocked());
}
FLinearColor UNGAimComponent::GetReticleColor() const
{
	const auto* Interaction = GetOwner()->FindComponentByClass<UNGInteractionComponent>();
	if (Interaction && Interaction->GetFocusedActor()) return Interaction->CanUseFocusedActor() ? InteractionColor : UnavailableColor;
	return IdleColor;
}
void UNGAimComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick)
{
	Super::TickComponent(Delta,Tick,ThisTick);
	FVector Origin,Direction;
	if (!ShouldShowReticle() || !GetViewRay(Origin,Direction))
	{
		if (bHit || AimTarget.IsValid() || !AimPoint.IsZero()) { AimTarget.Reset(); AimPoint=FVector::ZeroVector; bHit=false; OnAimUpdated.Broadcast(nullptr,AimPoint,false); }
		return;
	}
	Origin = GetTargetingOrigin(Origin, Direction);
	const FVector End=Origin+Direction*(FMath::IsFinite(TraceDistance) ? FMath::Clamp(TraceDistance,100.f,100000.f) : 10000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGAim),false,GetOwner()); TArray<AActor*> Attached; GetOwner()->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit; const bool bNewHit=GetWorld()->LineTraceSingleByChannel(Hit,Origin,End,ECC_Visibility,Params);
	const FVector NewPoint=bNewHit ? Hit.ImpactPoint : End; AActor* NewTarget=bNewHit ? Hit.GetActor() : nullptr;
	const bool bChanged=NewTarget!=AimTarget.Get() || bNewHit!=bHit || !NewPoint.Equals(AimPoint,.1f);
	AimPoint=NewPoint; AimTarget=NewTarget; bHit=bNewHit;
	if (bChanged) OnAimUpdated.Broadcast(NewTarget,AimPoint,bHit);
}
