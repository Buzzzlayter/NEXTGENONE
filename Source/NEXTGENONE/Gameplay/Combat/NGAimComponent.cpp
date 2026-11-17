#include "Gameplay/Combat/NGAimComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "SceneView.h"
#include "Character/ALSBaseCharacter.h"

UNGAimComponent::UNGAimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bAllowTickOnDedicatedServer = false;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}
bool UNGAimComponent::GetViewRay(FVector& Origin,FVector& Direction) const
{
	Origin=FVector::ZeroVector; Direction=FVector::ForwardVector;
	const auto* Pawn=Cast<APawn>(GetOwner());
	const auto* Controller=Pawn ? Pawn->GetController() : nullptr;
	if (!Controller || Controller->GetPawn() != Pawn) return false;
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
void UNGAimComponent::TickComponent(float Delta,ELevelTick Tick,FActorComponentTickFunction* ThisTick)
{
	Super::TickComponent(Delta,Tick,ThisTick);
	OnAimUpdated.Broadcast();
}
