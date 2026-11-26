#pragma once

#include "Map/UIWorldMarkerComponent.h"
#include "NGInteractionMarkerComponent.generated.h"

class APawn;
class UNGInteractionMarkerModel;

UCLASS(Blueprintable, ClassGroup = UI, PrioritizeCategories = ("Marker"), meta = (BlueprintSpawnableComponent))
class NEXTGENONE_API UNGInteractionMarkerComponent : public UUIWorldMarkerComponent
{
	GENERATED_BODY()

public:
	UNGInteractionMarkerComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type Reason) override;

private:
	void OnInteractionFocusChanged(APawn* Interactor, bool bFocused, const FText& Prompt, bool bAvailable);
	void RemovePresentation(APawn* Interactor);
	static void DestroyPresentation(UUIWorldMarkerComponent* Presentation);

	UPROPERTY(EditAnywhere, Category = "Marker")
	TSubclassOf<UNGInteractionMarkerModel> ModelClass;

	TMap<TWeakObjectPtr<APawn>, TWeakObjectPtr<UUIWorldMarkerComponent>> Presentations;
};
