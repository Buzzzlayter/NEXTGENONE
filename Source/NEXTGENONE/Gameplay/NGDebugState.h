#pragma once
#include "CoreMinimal.h"
#include "Components/ALSDebugComponent.h"
#include "Components/UIWidgetMounterComponent.h"
#include "NGDebugState.generated.h"

bool NGTriggerUIEvent(UHFSMState* State,FName Event);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGOverlayRequested,bool,bOpen);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGOverlayCycle,bool,bUp);
// The project copy of the ALS DebugComponent keeps its coloring graphs; UI is owned by HFSM.
UCLASS(Blueprintable)
class NEXTGENONE_API UNGDebugComponent : public UALSDebugComponent
{
	GENERATED_BODY()
public:
	UPROPERTY(BlueprintAssignable) FNGOverlayRequested OnOverlayRequested;
	UPROPERTY(BlueprintAssignable) FNGOverlayCycle OnOverlayCycle;
	virtual void OpenOverlayMenu_Implementation(bool bOpen) override { OnOverlayRequested.Broadcast(bOpen); }
	virtual void OverlayMenuCycle_Implementation(bool bUp) override { OnOverlayCycle.Broadcast(bUp); }
};

UCLASS()
class NEXTGENONE_API UNGDebugWidgetMounter : public UUIWidgetMounterComponent
{
	GENERATED_BODY()
public:
	void Configure(TSubclassOf<UUserWidget> Class,int32 Order) { WidgetClass=Class; bAddToViewportIfNoLayout=true; FallbackZOrder=Order; }
};

UCLASS(EditInlineNew,DisplayName="Runtime Settings Navigation")
class NEXTGENONE_API UNGRuntimeUIStateComponent : public UHFSMPlayerStateComponent
{
	GENERATED_BODY()
public:
	UNGRuntimeUIStateComponent();
	// Serialized compatibility only. Runtime no longer creates the ALS demo menus.
	UPROPERTY(meta=(DeprecatedProperty)) TSoftClassPtr<UUserWidget> DebugHUDClass;
	UPROPERTY(meta=(DeprecatedProperty)) TSoftClassPtr<UUserWidget> OverlayClass;
	virtual bool Enter() override;
	virtual void Exit(FName Event) override;
	virtual void AbortEnter() override;
private:
	TWeakObjectPtr<APlayerController> Controller;
	void OpenSettings();
	void Release();
};
