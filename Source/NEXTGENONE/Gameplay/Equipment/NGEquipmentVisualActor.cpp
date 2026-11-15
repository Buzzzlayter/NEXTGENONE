#include "Gameplay/Equipment/NGEquipmentVisualActor.h"

#include "AbilitySystemComponent.h"
#include "Components/SceneComponent.h"
#include "Gameplay/Equipment/NGEquipmentInstance.h"
#include "Gameplay/Equipment/NGEquipmentComponent.h"
#include "Net/UnrealNetwork.h"

ANGEquipmentVisualActor::ANGEquipmentVisualActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bNetUseOwnerRelevancy = true;
	SetActorEnableCollision(false);
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void ANGEquipmentVisualActor::Initialize(UNGEquipmentInstance* InSource, UAbilitySystemComponent* InASC)
{
	if (Source == InSource && AbilitySystem.Get() == InASC && TagChangedHandle.IsValid()) return;
	if (auto* Previous = AbilitySystem.Get()) Previous->RegisterGenericGameplayTagEvent().Remove(TagChangedHandle);
	Source = InSource;
	AbilitySystem = InASC;
	TagChangedHandle.Reset();
	if (!HasActorBegunPlay()) return;
	if (auto* ASC = GetAbilitySystemComponent())
		TagChangedHandle = ASC->RegisterGenericGameplayTagEvent().AddUObject(this, &ThisClass::GameplayTagChanged);
	AbilitySystemReady();
}

void ANGEquipmentVisualActor::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Source();
}

void ANGEquipmentVisualActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ThisClass, Source);
}

void ANGEquipmentVisualActor::OnRep_Source()
{
	if (auto* Equipment = Source ? Source->GetEquipment() : nullptr) Equipment->RefreshReplicatedState();
}

UAbilitySystemComponent* ANGEquipmentVisualActor::GetAbilitySystemComponent() const
{
	auto* ASC = AbilitySystem.Get();
	return ASC && ASC->GetAvatarActor() == GetOwner() ? ASC : nullptr;
}

UNGEquipmentInstance* ANGEquipmentVisualActor::GetEquipmentSource() const { return Source.Get(); }

void ANGEquipmentVisualActor::AbilitySystemReady() { if (GetAbilitySystemComponent()) OnVisualReady(); }

void ANGEquipmentVisualActor::GameplayTagChanged(FGameplayTag Tag, int32 Count)
{
	if (GetAbilitySystemComponent()) OnGameplayTagChanged(Tag, Count);
}

void ANGEquipmentVisualActor::EndPlay(EEndPlayReason::Type Reason)
{
	if (auto* ASC = AbilitySystem.Get()) ASC->RegisterGenericGameplayTagEvent().Remove(TagChangedHandle);
	TagChangedHandle.Reset(); AbilitySystem.Reset(); Source = nullptr;
	Super::EndPlay(Reason);
}
