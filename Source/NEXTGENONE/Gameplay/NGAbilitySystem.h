#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "AbilitySystemInterface.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "GameFramework/PlayerState.h"
#include "NativeGameplayTags.h"
#include "NGAbilitySystem.generated.h"

UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGDead);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputJump);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputDash);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputMantle);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputPickup);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGDashActive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGMantleActive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputPunch);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGPunchActive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGMovementAbility);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGReloadActive);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGMagazineAmmo);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGReserveAmmo);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputInteract);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInteractEvent);

class ANextgenoneCharacter;

UCLASS()
class NEXTGENONE_API UNGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
public:
	// Call on Started and Completed/Canceled, not every Triggered tick.
	UFUNCTION(BlueprintCallable, Category="GAS|Input") void AbilityInputTagPressed(FGameplayTag InputTag);
	UFUNCTION(BlueprintCallable, Category="GAS|Input") void AbilityInputTagReleased(FGameplayTag InputTag);
	void ClearAbilityInput();
	UFUNCTION(BlueprintCallable, Category="GAS|Input") bool TryActivateAbilityByInputTag(FGameplayTag InputTag);
private:
	void NotifyAbilityInput(FGameplayAbilitySpec& Spec, bool bPressed);
};

UCLASS()
class NEXTGENONE_API UNGHealthSet : public UAttributeSet
{
	GENERATED_BODY()
public:
	UNGHealthSet();
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Health, Category="Health") FGameplayAttributeData Health;
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_MaxHealth, Category="Health") FGameplayAttributeData MaxHealth;
	UPROPERTY() FGameplayAttributeData IncomingDamage;
	static FGameplayAttribute GetHealthAttribute();
	static FGameplayAttribute GetMaxHealthAttribute();
	static FGameplayAttribute GetIncomingDamageAttribute();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& Old);
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& Old);
};

UCLASS()
class NEXTGENONE_API UNGInitHealthEffect : public UGameplayEffect
{
	GENERATED_BODY()
public: UNGInitHealthEffect();
};

UCLASS()
class NEXTGENONE_API UNGDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public: UNGDamageEffect();
};

USTRUCT(BlueprintType)
struct FNGAbilityGrant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TSubclassOf<UGameplayAbility> Ability;
	UPROPERTY(EditAnywhere) FGameplayTag InputTag;
	UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 Level = 1;
};

USTRUCT()
struct FNGGrantedHandles
{
	GENERATED_BODY()
	UPROPERTY() TArray<FGameplayAbilitySpecHandle> Abilities;
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Effects;
	void Remove(UAbilitySystemComponent* ASC);
};

UCLASS(BlueprintType)
class NEXTGENONE_API UNGAbilitySet : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly) TArray<FNGAbilityGrant> Abilities;
	UPROPERTY(EditDefaultsOnly) TArray<TSubclassOf<UGameplayEffect>> Effects;
	void Give(UAbilitySystemComponent* ASC, UObject* Source, FNGGrantedHandles& Out) const;
};

