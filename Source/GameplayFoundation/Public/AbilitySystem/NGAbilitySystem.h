#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "Abilities/GameplayAbility.h"
#include "Engine/DataAsset.h"
#include "NGAbilitySystem.generated.h"


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
	/** Run after all state entry resources have been acquired. */
	UPROPERTY(EditAnywhere) bool bActivateOnEnter = false;
	FGameplayAbilitySpecHandle Give(UAbilitySystemComponent* ASC, UObject* Source) const;
};

USTRUCT()
struct GAMEPLAYFOUNDATION_API FNGGrantedHandles
{
	GENERATED_BODY()
	UPROPERTY() TArray<FGameplayAbilitySpecHandle> Abilities;
	UPROPERTY() TArray<FActiveGameplayEffectHandle> Effects;
	void Remove(UAbilitySystemComponent* ASC);
};
