#pragma once

#include "Components/ActorComponent.h"
#include "GenericTeamAgentInterface.h"
#include "NGTeamComponent.generated.h"

/** Persistent membership on PlayerState, also usable on other team-owned Actors. */
UCLASS(ClassGroup=Gameplay, meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGTeamComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGTeamComponent();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	FGenericTeamId GetTeamId() const { return FGenericTeamId(TeamId); }
	void SetTeamId(FGenericTeamId NewTeamId);
	static UNGTeamComponent* Find(const AActor* Actor);
	ETeamAttitude::Type GetAttitudeTowards(const AActor* Other) const;
private:
	UPROPERTY(Replicated, EditDefaultsOnly, Category="Team") uint8 TeamId = FGenericTeamId::NoTeam.GetId();
};
