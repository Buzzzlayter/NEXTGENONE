#pragma once

#include "GameplayCueNotify_Static.h"
#include "NGMedkitHealCue.generated.h"

UCLASS()
class NEXTGENONE_API UNGMedkitHealCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	UNGMedkitHealCue(const FObjectInitializer& ObjectInitializer);
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
};
