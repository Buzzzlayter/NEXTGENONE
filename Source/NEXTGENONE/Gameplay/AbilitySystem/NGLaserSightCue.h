#pragma once

#include "GameplayCueNotify_Static.h"
#include "NGLaserSightCue.generated.h"

UCLASS()
class NEXTGENONE_API UNGLaserSightCue : public UGameplayCueNotify_Static
{
	GENERATED_BODY()
public:
	UNGLaserSightCue(const FObjectInitializer& Init);
	virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
	virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
};
