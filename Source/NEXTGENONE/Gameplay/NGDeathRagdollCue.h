#pragma once

#include "GameplayCueNotify_Static.h"
#include "NGDeathRagdollCue.generated.h"

/** Cosmetic ALS ragdoll only. No body-state replication or health decisions. */
UCLASS(Blueprintable)
class NEXTGENONE_API UNGDeathRagdollCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
public:
	UNGDeathRagdollCue(const FObjectInitializer& Init);
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
};
