// Exclusive publisher - Forward Gateway, LLC

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GamepadCursorWidget.generated.h"

class FViewport;

UCLASS(Blueprintable)
class UIMODULE_API UGamepadCursorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintImplementableEvent, Category = "Cursor")
	void OnViewportResized(float DPIScale);

	UFUNCTION(BlueprintImplementableEvent, Category = "Cursor")
	void OnFocusFound(bool bHasFocus);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void ViewportResized(FViewport* Viewport, uint32 Value);
	FDelegateHandle ViewportResizedHandle;
};
