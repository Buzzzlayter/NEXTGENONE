#include "Gameplay/Equipment/NGJetpackVisualActor.h"

#include "AbilitySystemComponent.h"
#include "Character/ALSBaseCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace
{
	const FName EngineAudioTag(TEXT("JetpackEngineAudio"));
	const FName ThrusterSpawnRateParameter(TEXT("User.Thrusters_SpawnRate"));
	const FName EnergyCoreSpawnRateParameter(TEXT("User.EnergyCore_SpawnRate"));
	constexpr float FullAttenuationYawRate = 60.f;
	constexpr float ExhaustInterpSpeed = 10.f;

	TOptional<float> ReadSpawnRate(const UNiagaraComponent& Exhaust, FName ParameterName)
	{
		bool bHasComponentValue = false;
		const float Rate = Exhaust.GetVariableFloat(ParameterName, bHasComponentValue);
		if (bHasComponentValue) return Rate;

		const UNiagaraSystem* System = Exhaust.GetAsset();
		if (!System) return {};
		const FNiagaraVariableBase Parameter(FNiagaraTypeDefinition::GetFloatDef(), ParameterName);
		return System->GetExposedParameters().GetParameterOptionalValue<float>(Parameter);
	}
}

void ANGJetpackVisualActor::FExhaustSpawnRates::Capture(const UNiagaraComponent& Exhaust)
{
	Thrusters = ReadSpawnRate(Exhaust, ThrusterSpawnRateParameter);
	EnergyCore = ReadSpawnRate(Exhaust, EnergyCoreSpawnRateParameter);
}

void ANGJetpackVisualActor::FExhaustSpawnRates::Apply(UNiagaraComponent& Exhaust, float RateScale) const
{
	if (Thrusters.IsSet()) Exhaust.SetVariableFloat(ThrusterSpawnRateParameter, Thrusters.GetValue() * RateScale);
	if (EnergyCore.IsSet()) Exhaust.SetVariableFloat(EnergyCoreSpawnRateParameter, EnergyCore.GetValue() * RateScale);
}

void ANGJetpackVisualActor::UpdateExhaustSpawnRates()
{
	LeftSpawnRates.Apply(*LeftExhaust, 1.f - SmoothedTurn);
	RightSpawnRates.Apply(*RightExhaust, 1.f + SmoothedTurn);
}

ANGJetpackVisualActor::ANGJetpackVisualActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	PrimaryActorTick.TickGroup = TG_PostPhysics;

	LeftThruster = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftThruster"));
	RightThruster = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightThruster"));
	LeftExhaust = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LeftExhaust"));
	RightExhaust = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RightExhaust"));
	LeftThruster->SetupAttachment(GetRootComponent());
	RightThruster->SetupAttachment(GetRootComponent());
	LeftThruster->SetRelativeLocation(FVector(0.f, -18.f, 0.f));
	RightThruster->SetRelativeLocation(FVector(0.f, 18.f, 0.f));
	LeftThruster->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RightThruster->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeftExhaust->SetupAttachment(LeftThruster);
	RightExhaust->SetupAttachment(RightThruster);
	LeftExhaust->SetAutoActivate(false);
	RightExhaust->SetAutoActivate(false);
}

void ANGJetpackVisualActor::AbilitySystemReady()
{
	Super::AbilitySystemReady();
	UpdateExhaust();
}

void ANGJetpackVisualActor::GameplayTagChanged(FGameplayTag Tag, int32 Count)
{
	Super::GameplayTagChanged(Tag, Count);
	if (Tag == TAG_NGJetpackActive || Tag == TAG_NGDead) UpdateExhaust();
}

void ANGJetpackVisualActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const auto* Character = Cast<AALSBaseCharacter>(GetOwner());
	if (!bExhaustActive || !Character || DeltaSeconds <= 0.f) return;

	const float CurrentYaw = Character->GetActorRotation().Yaw;
	const float YawDelta = FMath::FindDeltaAngleDegrees(PreviousOwnerYaw, CurrentYaw);
	PreviousOwnerYaw = CurrentYaw;
	const float TargetTurn = Character->IsPoweredFlightActive()
		? FMath::Clamp(YawDelta / (DeltaSeconds * FullAttenuationYawRate), -1.f, 1.f)
		: 0.f;
	const float NewTurn = FMath::FInterpTo(SmoothedTurn, TargetTurn, DeltaSeconds, ExhaustInterpSpeed);
	if (NewTurn == SmoothedTurn) return;

	SmoothedTurn = NewTurn;
	UpdateExhaustSpawnRates();
}

void ANGJetpackVisualActor::UpdateExhaust()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const auto* ASC = GetAbilitySystemComponent();
	const bool bThrust = ASC && ASC->HasMatchingGameplayTag(TAG_NGJetpackActive) && !ASC->HasMatchingGameplayTag(TAG_NGDead);
	if (bThrust && !bExhaustActive)
	{
		LeftSpawnRates.Capture(*LeftExhaust);
		RightSpawnRates.Capture(*RightExhaust);
		if (const AActor* JetpackOwner = GetOwner())
		{
			PreviousOwnerYaw = JetpackOwner->GetActorRotation().Yaw;
		}
		LeftExhaust->Activate(true);
		RightExhaust->Activate(true);
		SetActorTickEnabled(true);
	}
	else if (!bThrust)
	{
		SetActorTickEnabled(false);
		if (SmoothedTurn != 0.f)
		{
			SmoothedTurn = 0.f;
			UpdateExhaustSpawnRates();
		}
		LeftExhaust->Deactivate();
		RightExhaust->Deactivate();
	}
	bExhaustActive = bThrust;
	if (auto* EngineAudio = FindComponentByTag<UAudioComponent>(EngineAudioTag))
	{
		if (bThrust && !EngineAudio->IsPlaying()) EngineAudio->FadeIn(0.08f);
		else if (!bThrust && EngineAudio->IsPlaying()) EngineAudio->Stop();
	}
}
