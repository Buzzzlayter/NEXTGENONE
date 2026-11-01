#include "NGAimComponent.h"
#include "NextgenoneCharacter.h"
#include "NextgenonePlayerController.h"
#include "NGInteraction.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "SceneView.h"

UNGAimComponent::UNGAimComponent() { PrimaryComponentTick.bCanEverTick=true; }
bool UNGAimComponent::GetViewRay(FVector& Origin,FVector& Direction) const
{
	Origin=FVector::ZeroVector; Direction=FVector::ForwardVector;
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner());
	const auto* PC=Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!PC || PC->GetPawn()!=Pawn) return false;
	FRotator Rotation;
	Pawn->GetActorEyesViewPoint(Origin,Rotation); Rotation=PC->GetControlRotation(); Direction=Rotation.Vector();
	if (PC->IsLocalController() && Pawn->GetNetMode()!=NM_DedicatedServer)
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
bool UNGAimComponent::ShouldShowReticle() const
{
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner()); const auto* PC=Pawn ? Cast<ANextgenonePlayerController>(Pawn->GetController()) : nullptr;
	return bShowReticle && PC && PC->IsLocalController() && PC->GetPawn()==Pawn && !Pawn->IsDead() && !PC->bInventoryUIOpen && !PC->bSettingsUIOpen;
}
FLinearColor UNGAimComponent::GetReticleColor() const
{
	const auto* Pawn=Cast<ANextgenoneCharacter>(GetOwner());
	if (Pawn && Pawn->Interaction && Pawn->Interaction->GetFocusedActor()) return Pawn->Interaction->CanUseFocusedActor() ? InteractionColor : UnavailableColor;
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
	const FVector End=Origin+Direction*(FMath::IsFinite(TraceDistance) ? FMath::Clamp(TraceDistance,100.f,100000.f) : 10000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGAim),false,GetOwner()); TArray<AActor*> Attached; GetOwner()->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit; const bool bNewHit=GetWorld()->LineTraceSingleByChannel(Hit,Origin,End,ECC_Visibility,Params);
	const FVector NewPoint=bNewHit ? Hit.ImpactPoint : End; AActor* NewTarget=bNewHit ? Hit.GetActor() : nullptr;
	const bool bChanged=NewTarget!=AimTarget.Get() || bNewHit!=bHit || !NewPoint.Equals(AimPoint,.1f);
	AimPoint=NewPoint; AimTarget=NewTarget; bHit=bNewHit;
	if (bChanged) OnAimUpdated.Broadcast(NewTarget,AimPoint,bHit);
}
