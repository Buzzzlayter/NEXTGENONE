// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "LoadingScreenManager.generated.h"

class IInputProcessor;
class SWidget;
class UGameViewportClient;
struct FWorldContext;

UCLASS()
class COMMONLOADINGSCREEN_API ULoadingScreenManager : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	DECLARE_EVENT_OneParam(ULoadingScreenManager, FOnLoadingScreenVisibilityChanged, bool);

	void Initialize(FSubsystemCollectionBase& Collection) override;
	void Deinitialize() override;
	bool ShouldCreateSubsystem(UObject* Outer) const override;

	void Tick(float DeltaTime) override;
	ETickableTickType GetTickableTickType() const override;
	bool IsTickable() const override;
	bool IsTickableWhenPaused() const override { return true; }
	TStatId GetStatId() const override;
	UWorld* GetTickableGameObjectWorld() const override;

	void RegisterLoadingProcessor(UObject* Processor);
	void UnregisterLoadingProcessor(UObject* Processor);
	bool GetLoadingScreenDisplayStatus() const { return Screen.IsValid(); }
	FOnLoadingScreenVisibilityChanged& OnLoadingScreenVisibilityChangedDelegate() { return VisibilityChanged; }

	UFUNCTION(BlueprintPure, Category="Loading Screen")
	const FString& GetDebugReasonForShowingOrHidingLoadingScreen() const { return LoadingReason; }

private:
	void HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName);
	void HandlePostLoadMap(UWorld* LoadedWorld);
	void UpdateLoadingScreen();
	bool NeedsLoadingScreen(const UGameViewportClient& Viewport);
	void ShowLoadingScreen(UGameViewportClient& Viewport);
	void HideLoadingScreen();

	TWeakObjectPtr<UGameViewportClient> ScreenViewport;
	TSharedPtr<SWidget> Screen;
	TSharedPtr<IInputProcessor> InputProcessor;
	TArray<TWeakObjectPtr<UObject>> LoadingProcessors;
	FOnLoadingScreenVisibilityChanged VisibilityChanged;
	FString LoadingReason;
	FString PendingLoadingReason;
	double ReadySince = -1.0;
	bool bInLoadMap = false;
	bool bInitialized = false;
};
