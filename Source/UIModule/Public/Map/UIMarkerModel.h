#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "Map/UIMarkerWidget.h"
#include "Network/UIReplicatedObject.h"
#include "UIMarkerModel.generated.h"

class APlayerController;
class UTexture2D;
class UUIMarkerModel;

DECLARE_MULTICAST_DELEGATE_TwoParams(FUIMarkerChanged, UUIMarkerModel*, bool);

USTRUCT()
struct FUIMarkerVisibilityRules
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, Category = "Marker", meta = (ClampMin = "0", Units = "cm", EditCondition = "!bAlwaysVisible", EditConditionHides))
	float VisibilityRadius = 1000.f;
	UPROPERTY(EditAnywhere, Category = "Marker", meta = (EditCondition = "!bAlwaysVisible", EditConditionHides))
	bool bRequireLineOfSight = false;

	UPROPERTY(EditAnywhere, Category = "Marker")
	bool bAlwaysVisible = false;

	UPROPERTY(EditAnywhere, Category = "Marker")
	bool bKeepAtEdge = false;

	bool RequiresViewer() const { return !bAlwaysVisible && (VisibilityRadius > 0.f || bRequireLineOfSight); }
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, PrioritizeCategories = ("Marker"))
class UIMODULE_API UUIMarkerModel : public UUIReplicatedObject
{
	GENERATED_BODY()

public:
	static UUIMarkerModel* CreateRuntimeModel(UObject* Outer, UUIMarkerModel* Template, const FVector& Position, FName RecipientGroup = NAME_None, AActor* Source = nullptr);
	AActor* GetSourceActor() const { return SourceActor.Get(); }
	FGuid GetMarkerId() const { return MarkerId; }
	virtual bool ShouldReplicateMarker() const { return bReplicateMarker; }
	virtual bool ShouldRegisterOnBeginPlay() const { return true; }
	FName GetRecipientGroup() const { return RecipientGroup; }
	virtual bool IsVisibleTo(const APlayerController& Viewer) const;
	virtual FVector GetPosition() const { return WorldPosition; }
	void SetPosition(const FVector& Position);
	virtual const FText& GetLabel() const { return Label; }
	virtual UTexture2D* GetIcon() const;
	virtual UClass* GetWidgetClass(EUIMarkerSurface Surface) const;
	virtual const FUIMarkerVisibilityRules* GetVisibility(EUIMarkerSurface Surface) const;
	FUIMarkerChanged& OnMarkerChanged() { return MarkerChanged; }

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = "Marker", meta = (BlueprintProtected))
	void NotifyMarkerChanged();

	UPROPERTY(EditAnywhere, ReplicatedUsing = NotifyMarkerChanged, Category = "Marker")
	FUIMarkerVisibilityRules MapVisibility{0.f};

	UPROPERTY(EditAnywhere, ReplicatedUsing = NotifyMarkerChanged, Category = "Marker")
	FUIMarkerVisibilityRules WorldVisibility;

	UPROPERTY(EditAnywhere, ReplicatedUsing = NotifyMarkerChanged, Category = "Marker")
	TSubclassOf<UUIMarkerWidget> WidgetClass;

	UPROPERTY(EditAnywhere, ReplicatedUsing = NotifyMarkerChanged, Category = "Marker")
	TSubclassOf<UUIMarkerWidget> WorldWidgetClass;

private:
	UFUNCTION()
	void OnRep_WorldPosition();

	UPROPERTY(ReplicatedUsing = NotifyMarkerChanged)
	FGuid MarkerId;

	UPROPERTY(ReplicatedUsing = OnRep_WorldPosition)
	FVector_NetQuantize10 WorldPosition = FVector::ZeroVector;

	UPROPERTY(Replicated)
	FName RecipientGroup;

	UPROPERTY(EditAnywhere, Category = "Marker")
	bool bReplicateMarker = false;

	UPROPERTY(EditAnywhere, ReplicatedUsing = NotifyMarkerChanged, Category = "Marker")
	FText Label;

	UPROPERTY(EditAnywhere, ReplicatedUsing = NotifyMarkerChanged, Category = "Marker")
	TObjectPtr<UTexture2D> Icon;

	FUIMarkerChanged MarkerChanged;
	TWeakObjectPtr<AActor> SourceActor;
};
