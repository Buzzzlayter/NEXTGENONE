#include "Gameplay/Equipment/NGJetpackVisualActor.h"

#include "AbilitySystemComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayTags/NGGameplayTags.h"
#include "NiagaraComponent.h"

ANGJetpackVisualActor::ANGJetpackVisualActor()
{
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

void ANGJetpackVisualActor::UpdateExhaust()
{
	if (GetNetMode() == NM_DedicatedServer) return;
	const auto* ASC = GetAbilitySystemComponent();
	const bool bThrust = ASC && ASC->HasMatchingGameplayTag(TAG_NGJetpackActive) && !ASC->HasMatchingGameplayTag(TAG_NGDead);
	for (auto* Exhaust : {LeftExhaust.Get(), RightExhaust.Get()})
	{
		if (bThrust) Exhaust->Activate(true);
		else Exhaust->Deactivate();
	}
}
