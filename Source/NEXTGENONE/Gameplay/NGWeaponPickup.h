#pragma once
#include "CoreMinimal.h"
#include "NGItemPickup.h"
#include "NGWeaponPickup.generated.h"

class UNGWeaponDefinition;
// Compatibility for existing assets; new content uses ANGItemPickup / BP_ItemPickup.
UCLASS(Blueprintable)
class NEXTGENONE_API ANGWeaponPickup : public ANGItemPickup
{
    GENERATED_BODY()
public:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing=OnRep_Definition, Category="Weapon") TObjectPtr<UNGWeaponDefinition> Definition;
    bool TryTakeOnServer(ANextgenoneCharacter* Pawn, float MaxDistance);
private:
    UFUNCTION() void OnRep_Definition();
};
// Legacy Blueprint parent delegates to the same interaction implementation.
UCLASS(Blueprintable)
class NEXTGENONE_API UNGPickupWeaponAbility : public UNGInteractionAbility
{
    GENERATED_BODY()
};