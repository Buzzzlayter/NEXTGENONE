#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "NativeGameplayTags.h"
#include "NGAbilitySystem.generated.h"

GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGDead);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputJump);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputDash);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputMantle);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputPickup);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGDashActive);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGMantleActive);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputPunch);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGPunchActive);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGMovementAbility);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGReloadActive);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGMagazineAmmo);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGReserveAmmo);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInputInteract);
GAMEPLAYFOUNDATION_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_NGInteractEvent);

UCLASS()
class GAMEPLAYFOUNDATION_API UNGAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
public:
	// Call once on first Triggered and on Completed/Canceled, not every Triggered tick.
	UFUNCTION(BlueprintCallable, Category="GAS|Input") virtual void AbilityInputTagPressed(FGameplayTag InputTag);
	UFUNCTION(BlueprintCallable, Category="GAS|Input") void AbilityInputTagReleased(FGameplayTag InputTag);
	void ClearAbilityInput();
	UFUNCTION(BlueprintCallable, Category="GAS|Input") virtual bool TryActivateAbilityByInputTag(FGameplayTag InputTag);
private:
	void NotifyAbilityInput(FGameplayAbilitySpec& Spec, bool bPressed);
};

UCLASS()
class GAMEPLAYFOUNDATION_API UNGHealthSet : public UAttributeSet
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
class GAMEPLAYFOUNDATION_API UNGInitHealthEffect : public UGameplayEffect
{
	GENERATED_BODY()
public: UNGInitHealthEffect();
};

UCLASS()
class GAMEPLAYFOUNDATION_API UNGDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()
public: UNGDamageEffect();
};

USTRUCT(BlueprintType)
struct GAMEPLAYFOUNDATION_API FNGAbilityGrant
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere) TSubclassOf<UGameplayAbility> Ability;
	UPROPERTY(EditAnywhere) FGameplayTag InputTag;
	UPROPERTY(EditAnywhere, meta=(ClampMin="1")) int32 Level = 1;
};

USTRUCT()
struct GAMEPLAYFOUNDATION_API FNGGrantedHandles
{
	GENERATED_BODY()
	UPROPERTY() TArray<FGameplayAbilitySpecHandle> Abilities;
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Effects;
	void Remove(UAbilitySystemComponent* ASC);
};

UCLASS(BlueprintType)
class GAMEPLAYFOUNDATION_API UNGAbilitySet : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditDefaultsOnly) TArray<FNGAbilityGrant> Abilities;
	UPROPERTY(EditDefaultsOnly) TArray<TSubclassOf<UGameplayEffect>> Effects;
	void Give(UAbilitySystemComponent* ASC, UObject* Source, FNGGrantedHandles& Out, bool bApplyInstantEffects = true) const;
};

