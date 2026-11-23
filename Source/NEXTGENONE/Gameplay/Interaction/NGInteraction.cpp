#include "Gameplay/Interaction/NGInteraction.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Gameplay/Player/NGPlayerContext.h"
#include "AbilitySystemComponent.h"
#include "Gameplay/Interaction/NGItemPickup.h"
#include "Gameplay/Combat/NGAimComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Input/PlayerInputSubsystem.h"
#include "Input/NGGameplayInputComponent.h"
#include "Input/NGGameplayInputConfig.h"
#include "Gameplay/UI/NGInteractionMarkerModel.h"
#include "Map/UIMarkerComponent.h"
#include "Map/UIMarkerSubsystem.h"
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
        Tags.AddTag(TAG_NGReloadActive);
		Tags.AddTag(TAG_NGRollActive); Tags.AddTag(TAG_NGRagdollActive);
        return Tags;
    }
}

bool ANGInteractableActor::ExecuteInteraction(APawn* Pawn, TFunctionRef<bool()> Prepare, TFunctionRef<bool()> Execute)
{
	if (!HasAuthority() || !IsValid(this) || !IsValid(Pawn) || !Pawn->HasAuthority()
		|| Pawn->GetWorld() != GetWorld() || bInteractionInProgress) return false;
	TGuardValue<bool> Guard(bInteractionInProgress, true);
	if (!Prepare() || !IsValid(this) || !IsValid(Pawn)) return false;
	ExecutingInteractor = Pawn;
	ON_SCOPE_EXIT { ExecutingInteractor.Reset(); };
	return Execute();
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
	const auto* Pawn = Cast<APawn>(GetOwner());
	const auto* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	const auto* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	const auto* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
	if (!LocalPlayer || (Input && Input->IsGameplayInputBlocked())) { UpdateFocus(nullptr); return; }
	UpdateFocus(FindTarget());
}
void UNGInteractionComponent::UpdateFocus(AActor* Target)
{
	auto* Pawn=Cast<APawn>(GetOwner());
	FText Prompt = Target ? INGInteractable::Execute_GetInteractionPrompt(Target, Pawn) : FText::GetEmpty();
	if (Target && Prompt.IsEmptyOrWhitespace())
		Prompt = NSLOCTEXT("NEXTGENONE", "InteractionDefaultLabel", "Interact");
	const bool bAvailable=Target && INGInteractable::Execute_CanInteract(Target,Pawn);
	const bool bTargetChanged = FocusedActor.Get() != Target;
	const bool bChanged = bTargetChanged || !FocusedPrompt.EqualTo(Prompt) || bFocusedAvailable != bAvailable;
	if (bTargetChanged)
	{
		if (auto* Old=FocusedActor.Get()) INGInteractable::Execute_SetInteractionFocus(Old,Pawn,false);
		FocusedActor=Target; if (Target) INGInteractable::Execute_SetInteractionFocus(Target,Pawn,true);
	}
	FocusedPrompt=Prompt; bFocusedAvailable=bAvailable;
	if (bChanged || (Target && !FocusMarker.IsValid()))
		UpdateFocusMarker(Pawn, Target, bTargetChanged);
	if (bChanged)
		OnFocusChanged.Broadcast(Target,Prompt,bAvailable);
}

void UNGInteractionComponent::UpdateFocusMarker(APawn* Pawn, AActor* Target, bool bTargetChanged)
{
	auto* Marker = FocusMarker.Get();
	if (!Target && !Marker) return;
	const auto* Player = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	auto* LocalPlayer = Player ? Player->GetLocalPlayer() : nullptr;
	auto* MarkerSubsystem = UWorld::GetSubsystem<UUIMarkerSubsystem>(GetWorld());
	if (!MarkerSubsystem) return;
	const bool bShowMarker = IsValid(Target) && LocalPlayer && !FocusedPrompt.IsEmpty();
	if (Marker && (bTargetChanged || !bShowMarker))
	{
		Marker->SetViewer(nullptr);
		MarkerSubsystem->UnregisterMarker(Marker);
		FocusMarker.Reset();
		Marker = nullptr;
	}
	if (!bShowMarker) return;

	const auto* GameplayInput = Pawn->FindComponentByClass<UNGGameplayInputComponent>();
	const auto* InputConfig = GameplayInput ? GameplayInput->InputConfig.Get() : nullptr;
	const auto* Action = InputConfig ? InputConfig->FindAbilityInputAction(TAG_NGInputInteract) : nullptr;
	if (!Action && InputConfig) Action = InputConfig->FindNativeInputAction(TAG_NGInputInteract);
	if (Marker)
	{
		Marker->SetFocus(Target, Action, FocusedPrompt, bFocusedAvailable);
		return;
	}

	TInlineComponentArray<UUIMarkerComponent*> MarkerComponents(Target);
	for (auto* Component : MarkerComponents)
	{
		Marker = Cast<UNGInteractionMarkerModel>(Component->CreateRuntimeMarker(MarkerSubsystem, UNGInteractionMarkerModel::StaticClass()));
		if (!Marker) continue;
		Marker->SetAnchor(Component);
		break;
	}
	if (!Marker)
		Marker = Cast<UNGInteractionMarkerModel>(UUIMarkerModel::CreateRuntimeModel(
			MarkerSubsystem, GetMutableDefault<UNGInteractionMarkerModel>(), Target->GetActorLocation()));
	if (!Marker) return;
	Marker->SetViewer(LocalPlayer);
	Marker->SetFocus(Target, Action, FocusedPrompt, bFocusedAvailable);
	if (MarkerSubsystem->RegisterMarker(Marker)) FocusMarker = Marker;
	else Marker->SetViewer(nullptr);
}

void UNGInteractionComponent::EndPlay(EEndPlayReason::Type Reason) { EndInteraction(); BindAbilitySystem(nullptr); UpdateFocus(nullptr); Super::EndPlay(Reason); }
void UNGInteractionComponent::RequestInteract()
{
	auto* Pawn = Cast<APawn>(GetOwner());
	if (!Pawn || (!Pawn->HasAuthority() && !Pawn->IsLocallyControlled())) return;

	const auto* Controller = Cast<APlayerController>(Pawn->GetController());
	const auto* LocalPlayer = Controller ? Controller->GetLocalPlayer() : nullptr;
	const auto* Input = LocalPlayer ? LocalPlayer->GetSubsystem<UPlayerInputSubsystem>() : nullptr;
	if (Input && Input->IsGameplayInputBlocked()) return;

	AActor* Target = LocalPlayer ? FocusedActor.Get() : FindTarget();
	if (!IsValid(Target) || (LocalPlayer && (!bFocusedAvailable || !ValidateTarget(Target))))
	{
		OnInteractionResult.Broadcast(false);
		return;
	}

	if (Pawn->HasAuthority()) ServerRequestInteract_Implementation(Pawn, Target);
	else ServerRequestInteract(Pawn, Target);
}
void UNGInteractionComponent::ServerRequestInteract_Implementation(APawn* ExpectedPawn,AActor* ProposedTarget)
{
	auto* Pawn=Cast<APawn>(GetOwner());
	if (Pawn!=ExpectedPawn || !IsCurrentInteractor()) return;
	auto* ASC = NGPlayerContext::GetCurrentASC(Pawn, true);
	if (!ASC) return;
	FGameplayEventData Event; Event.EventTag=TAG_NGInteractEvent; Event.Instigator=Pawn; Event.Target=ProposedTarget;
	ASC->HandleGameplayEvent(TAG_NGInteractEvent,&Event);
}
void UNGInteractionComponent::ClientInteractionResult_Implementation(bool bSuccess) { OnInteractionResult.Broadcast(bSuccess); }
