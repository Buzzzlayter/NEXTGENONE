#include "Gameplay/Abilities/NGPingAbility.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Teams/NGTeamComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "Map/UIMarkerSubsystem.h"

UNGPingCooldownEffect::UNGPingCooldownEffect()
{
	DurationPolicy = EGameplayEffectDurationType::HasDuration;
	DurationMagnitude = FScalableFloat(0.5f);
	FInheritedTagContainer Tags;
	Tags.AddTag(TAG_NGPingCooldown);
	auto* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("PingCooldownTags"));
	GEComponents.Add(TargetTags);
	TargetTags->SetAndApplyTargetTagChanges(Tags);
}

UNGPingAbility::UNGPingAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	NetSecurityPolicy = EGameplayAbilityNetSecurityPolicy::ServerOnlyTermination;
	ActivationBlockedTags.AddTag(TAG_NGDead);
	CooldownGameplayEffectClass = UNGPingCooldownEffect::StaticClass();
	MarkerTemplate = CreateDefaultSubobject<UNGPingMarkerModel>(TEXT("MarkerModel"));
}

void UNGPingAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* Info,
	FGameplayAbilityActivationInfo Activation, const FGameplayEventData* Event)
{
	const auto* Pawn = Cast<APawn>(Info->AvatarActor.Get());
	const auto* Controller = Info->PlayerController.Get();
	auto* Source = Info->OwnerActor.Get();
	const auto* Team = UNGTeamComponent::Find(Source);
	const FName RecipientGroup = Team ? Team->GetTeamName() : NAME_None;
	const bool bPlaced = Info->IsNetAuthority() && Pawn && Controller && Controller->GetPawn() == Pawn
		&& !RecipientGroup.IsNone() && CommitAbility(Handle, Info, Activation)
		&& PlacePing(*Pawn, *Controller, Source, RecipientGroup);
	EndAbility(Handle, Info, Activation, true, !bPlaced);
}

bool UNGPingAbility::PlacePing(const APawn& Pawn, const APlayerController& Controller, AActor* Source, FName RecipientGroup) const
{
	auto* World = Pawn.GetWorld();
	auto* Markers = UWorld::GetSubsystem<UUIMarkerSubsystem>(World);
	if (!Markers || !MarkerTemplate || !FMath::IsFinite(PingRange) || PingRange <= 0.f)
		return false;

	FVector Origin = FVector::ZeroVector;
	FRotator EyeRotation = FRotator::ZeroRotator;
	Pawn.GetActorEyesViewPoint(Origin, EyeRotation);
	const FVector Destination = Origin + Controller.GetControlRotation().Vector() * PingRange;
	if (Origin.ContainsNaN() || Destination.ContainsNaN()) return false;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(NGPing), false, &Pawn);
	TArray<AActor*> AttachedActors;
	Pawn.GetAttachedActors(AttachedActors);
	Params.AddIgnoredActors(AttachedActors);
	FHitResult Hit;
	if (!World->LineTraceSingleByChannel(Hit, Origin, Destination, ECC_Visibility, Params)) return false;

	return Markers->PlaceMarkerForSource(Source, MarkerTemplate.Get(),
		Hit.ImpactPoint + Hit.ImpactNormal * 2.0, RecipientGroup);
}
