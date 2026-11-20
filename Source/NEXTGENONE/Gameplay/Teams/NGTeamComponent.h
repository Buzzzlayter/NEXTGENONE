#pragma once

#include "Components/ActorComponent.h"
#include "GenericTeamAgentInterface.h"
#include "NGTeamComponent.generated.h"

class APlayerController;

/** Persistent membership on PlayerState, also usable on other team-owned Actors. */
UCLASS(ClassGroup=Gameplay, PrioritizeCategories = ("Team"), meta=(BlueprintSpawnableComponent))
class NEXTGENONE_API UNGTeamComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UNGTeamComponent();

	FName GetTeamName() const { return TeamName; }
	void SetTeamName(FName NewTeamName);
	static UNGTeamComponent* Find(const AActor* Actor);
	ETeamAttitude::Type GetAttitudeTowards(const AActor* Other) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	APlayerController* GetOwningPlayerController() const;
	UPROPERTY(Replicated, EditDefaultsOnly, Category = "Team")
	FName TeamName;
};
