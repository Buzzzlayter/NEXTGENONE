#include "Gameplay/Interaction/NGInteraction.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "Gameplay/AbilitySystem/NGPlayerAbilitySystem.h"
#include "Gameplay/Interaction/NGItemPickup.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Input/PlayerInputSubsystem.h"
#include "Misc/ScopeExit.h"
#include "Engine/World.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif


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

bool ANGInteractableActor::ExecuteInteraction(APawn* Pawn, TFunctionRef<bool()> Prepare)
{
	if (!HasAuthority() || !IsValid(this) || !IsValid(Pawn) || !Pawn->HasAuthority()
		|| Pawn->GetWorld() != GetWorld() || bInteractionInProgress) return false;
	TGuardValue<bool> Guard(bInteractionInProgress, true);
	if (!Prepare() || !IsValid(this) || !IsValid(Pawn)) return false;
	ExecutingInteractor = Pawn;
	ON_SCOPE_EXIT { ExecutingInteractor.Reset(); };
	return INGInteractable::Execute_Interact(this, Pawn);
}

bool ANGInteractableActor::IsExecutingInteraction(const APawn* Pawn) const
{
	const auto* Interaction = IsValid(Pawn) ? Pawn->FindComponentByClass<UNGInteractionComponent>() : nullptr;
	return bInteractionInProgress && ExecutingInteractor.Get() == Pawn && Interaction
		&& Pawn->GetWorld() == GetWorld() && Interaction->IsCurrentInteractor();
}

bool UNGInteractionComponent::IsCurrentInteractor(bool bAuthority) const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* ASC = NGPlayerContext::GetCurrentASC(Pawn, bAuthority);
	return ASC && !ASC->HasMatchingGameplayTag(TAG_NGDead) && !ASC->HasAnyMatchingGameplayTags(InterruptingTags)
		&& ASC->GetNumericAttribute(UNGHealthSet::GetHealthAttribute()) > 0.f;
}

AActor* UNGInteractionComponent::FindTarget() const
{
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!IsCurrentInteractor(false) || !FMath::IsFinite(MaxDistance) || !FMath::IsFinite(CaptureRadius)) return nullptr;
	FVector View,Direction;
	const auto* Aim = Pawn->FindComponentByClass<UNGAimComponent>();
	if (Aim)
	{
		if (!Aim->GetViewRay(View,Direction)) return nullptr;
	}
	else
	{
		FRotator ViewRotation;
		Pawn->GetController()->GetPlayerViewPoint(View, ViewRotation);
		Direction = ViewRotation.Vector();
		if (View.ContainsNaN() || Direction.ContainsNaN() || !Direction.Normalize()) return nullptr;
	}
	FVector Eye; FRotator Rotation; Pawn->GetActorEyesViewPoint(Eye,Rotation);
	const float Range=FMath::Clamp(MaxDistance,50.f,500.f);
	const float Radius=FMath::Clamp(CaptureRadius,0.f,100.f);
	const float RayLength=FVector::Distance(View,Eye)+Range;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGInteraction),false,Pawn);
	TArray<AActor*> Attached; Pawn->GetAttachedActors(Attached); Params.AddIgnoredActors(Attached);
	FHitResult Hit; GetWorld()->LineTraceSingleByChannel(Hit,View,View+Direction*RayLength,NGInteractionChannel,Params);
	auto* Target=Hit.GetActor();
	// A precise center hit wins. The wider capture only assists when the center misses an interactable.
	if (ValidateTarget(Target)) return Target;
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
		if (OffAxisSquared>FMath::Square(Radius) || !ValidateTarget(Candidate)) continue;
		FHitResult CameraLOS;
		GetWorld()->LineTraceSingleByChannel(CameraLOS,View,Candidate->GetActorLocation(),NGInteractionChannel,Params);
		if (CameraLOS.bBlockingHit && CameraLOS.GetActor()!=Candidate) continue;
		const float Score=OffAxisSquared+FVector::DistSquared(Pawn->GetActorLocation(),Candidate->GetActorLocation())*.001f;
		if (Score<BestScore) { BestScore=Score; Best=Candidate; }
	}
	return Best;
}
bool UNGInteractionComponent::ValidateTarget(AActor* Target) const
{
	const auto* Pawn = Cast<APawn>(GetOwner());
	if (!IsCurrentInteractor(false) || !IsValid(Target) || !Target->IsA<ANGInteractableActor>() || Target->GetWorld()!=GetWorld() || !Target->GetIsReplicated() || !Target->Implements<UNGInteractable>() || !FMath::IsFinite(MaxDistance)) return false;
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
UNGInteractionComponent::UNGInteractionComponent()
{
	DroppedPickupClass = ANGItemPickup::StaticClass();
	InterruptingTags = DefaultInteractionInterruptions();
	SetIsReplicatedByDefault(true); PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.1f;
}
#if WITH_EDITOR
EDataValidationResult UNGInteractionComponent::IsDataValid(FDataValidationContext& ValidationContext) const
{
	const auto Parent = Super::IsDataValid(ValidationContext);
	if (!DroppedPickupClass || DroppedPickupClass->HasAnyClassFlags(CLASS_Abstract))
	{
		ValidationContext.AddError(FText::FromString(TEXT("Interaction requires a concrete DroppedPickupClass; assign its Blueprint here to include it in cook.")));
		return EDataValidationResult::Invalid;
	}
	return Parent == EDataValidationResult::Invalid ? Parent : EDataValidationResult::Valid;
}
#endif
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
	auto* Pawn=Cast<APawn>(GetOwner());
	const auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this);
	if (!Pawn || !Pawn->IsLocallyControlled() || (Input && Input->IsGameplayInputBlocked())) { UpdateFocus(nullptr); return; }
	UpdateFocus(FindTarget());
}
void UNGInteractionComponent::UpdateFocus(AActor* Target)
{
	auto* Pawn=Cast<APawn>(GetOwner());
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
	auto* Pawn=Cast<APawn>(GetOwner()); if (!Pawn || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())) return;
	const auto* Input = UPlayerInputSubsystem::GetInputSubsystem(this); if (Input && Input->IsGameplayInputBlocked()) return;
	AActor* Target=FindTarget();
	if (!Target) { OnInteractionResult.Broadcast(false); return; }
	if (Pawn->HasAuthority()) ServerRequestInteract_Implementation(Pawn,Target); else ServerRequestInteract(Pawn,Target);
}
void UNGInteractionComponent::ServerRequestInteract_Implementation(APawn* ExpectedPawn,AActor* ProposedTarget)
{
	auto* Pawn=Cast<APawn>(GetOwner());
	if (Pawn!=ExpectedPawn || !IsCurrentInteractor()) return;
	auto* ASC = Cast<UNGPlayerAbilitySystemComponent>(NGPlayerContext::GetCurrentASC(Pawn, true));
	if (!ASC || !ASC->TryCommitInteraction(Pawn, .15f)) return;
	FGameplayEventData Event; Event.EventTag=TAG_NGInteractEvent; Event.Instigator=Pawn; Event.Target=ProposedTarget;
	ASC->HandleGameplayEvent(TAG_NGInteractEvent,&Event);
}
void UNGInteractionComponent::ClientInteractionResult_Implementation(bool bSuccess) { OnInteractionResult.Broadcast(bSuccess); }
