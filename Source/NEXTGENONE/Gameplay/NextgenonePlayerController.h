#pragma once
#include "CoreMinimal.h"
#include "Character/ALSPlayerController.h"
#include "NextgenonePlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
UCLASS()
class NEXTGENONE_API ANextgenonePlayerController : public AALSPlayerController
{
	GENERATED_BODY()
public:
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* ControlledPawn) override;
	virtual void OnRep_Pawn() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	UFUNCTION(Exec) void NGEquip();
	UFUNCTION(Exec) void NGUnequip();
	UFUNCTION(Exec) void NGFire();
	// Invoked only by the dedicated test GameMode when -NGCombatSmoke is present.
	UFUNCTION(Client, Reliable) void ClientSmoke(int32 Command);
	UFUNCTION(Server, Reliable) void ServerSmokeResult(int32 Command, bool bPassed);
private:
	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> CombatMapping;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> EquipAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> UnequipAction;
	void UpdateGameplayPawn();
	void RemoveCombatMapping();
};
