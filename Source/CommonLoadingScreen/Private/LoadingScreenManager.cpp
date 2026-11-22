// Copyright Epic Games, Inc. All Rights Reserved.

#include "LoadingScreenManager.h"

#include "Blueprint/UserWidget.h"
#include "CommonLoadingScreenSettings.h"
#include "CommonLoadingScreenWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "LoadingProcessInterface.h"
#include "Misc/App.h"
#include "PreLoadScreenManager.h"
#include "UObject/UObjectGlobals.h"

namespace
{
class FLoadingScreenInputProcessor final : public IInputProcessor
{
public:
	void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
	bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent&) override { return true; }
	bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent&) override { return true; }
	bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent&) override { return true; }
	bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
	bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
	bool HandleMouseButtonUpEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
	bool HandleMouseButtonDoubleClickEvent(FSlateApplication&, const FPointerEvent&) override { return true; }
	bool HandleMouseWheelOrGestureEvent(FSlateApplication&, const FPointerEvent&, const FPointerEvent*) override { return true; }
};

bool TryGetLoadingReason(const UObject* Object, FString& OutReason)
{
	const auto* Processor = Cast<ILoadingProcessInterface>(Object);
	FString Reason;
	if (!Processor || !Processor->ShouldShowLoadingScreen(Reason))
	{
		return false;
	}
	OutReason = Reason.IsEmpty() ? GetNameSafe(Object) : MoveTemp(Reason);
	return true;
}
}

bool ULoadingScreenManager::ShouldCreateSubsystem(UObject* Outer) const
{
	const auto* Instance = Cast<UGameInstance>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && FApp::CanEverRender() && Instance && !Instance->IsDedicatedServerInstance();
}

void ULoadingScreenManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FCoreUObjectDelegates::PreLoadMapWithContext.AddUObject(this, &ThisClass::HandlePreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &ThisClass::HandlePostLoadMap);
	bInitialized = true;
}

void ULoadingScreenManager::Deinitialize()
{
	bInitialized = false;
	FCoreUObjectDelegates::PreLoadMapWithContext.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
	HideLoadingScreen();
	LoadingProcessors.Reset();
	PendingLoadingReason.Reset();
	Super::Deinitialize();
}

void ULoadingScreenManager::Tick(float DeltaTime)
{
	UpdateLoadingScreen();
}

ETickableTickType ULoadingScreenManager::GetTickableTickType() const
{
	return ETickableTickType::Conditional;
}

bool ULoadingScreenManager::IsTickable() const
{
	return bInitialized;
}

TStatId ULoadingScreenManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULoadingScreenManager, STATGROUP_Tickables);
}

UWorld* ULoadingScreenManager::GetTickableGameObjectWorld() const
{
	const auto* Instance = GetGameInstance();
	return Instance ? Instance->GetWorld() : nullptr;
}

void ULoadingScreenManager::RegisterLoadingProcessor(UObject* Processor)
{
	if (IsValid(Processor) && Cast<ILoadingProcessInterface>(Processor))
	{
		LoadingProcessors.AddUnique(Processor);
		if (!Screen.IsValid())
		{
			TryGetLoadingReason(Processor, PendingLoadingReason);
		}
	}
}

void ULoadingScreenManager::UnregisterLoadingProcessor(UObject* Processor)
{
	LoadingProcessors.Remove(Processor);
}

void ULoadingScreenManager::HandlePreLoadMap(const FWorldContext& WorldContext, const FString& MapName)
{
	if (WorldContext.OwningGameInstance == GetGameInstance())
	{
		bInLoadMap = true;
		if (GEngine && GEngine->IsInitialized())
		{
			UpdateLoadingScreen();
		}
	}
}

void ULoadingScreenManager::HandlePostLoadMap(UWorld* LoadedWorld)
{
	if (!LoadedWorld || LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		bInLoadMap = false;
	}
}

bool ULoadingScreenManager::NeedsLoadingScreen(const UGameViewportClient& Viewport)
{
	const auto* Instance = GetGameInstance();
	const FWorldContext* Context = Instance->GetWorldContext();
	if (bInLoadMap || !Context || !Context->World())
	{
		LoadingReason = TEXT("Loading map.");
		return true;
	}
	if (Context->PendingNetGame)
	{
		LoadingReason = TEXT("Connecting to server.");
		return true;
	}

	const UWorld* LoadingWorld = Context->World();
	if (!Context->TravelURL.IsEmpty() || LoadingWorld->IsInSeamlessTravel())
	{
		LoadingReason = TEXT("Travel is pending.");
		return true;
	}
	auto* GameState = LoadingWorld->GetGameState();
	if (!GameState || !LoadingWorld->HasBegunPlay())
	{
		LoadingReason = TEXT("Waiting for gameplay to start.");
		return true;
	}

	const auto CheckActor = [this](AActor* Actor)
	{
		if (TryGetLoadingReason(Actor, LoadingReason))
		{
			return true;
		}
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (const UActorComponent* Component : Components)
		{
			if (TryGetLoadingReason(Component, LoadingReason))
			{
				return true;
			}
		}
		return false;
	};
	if (CheckActor(GameState))
	{
		return true;
	}

	LoadingProcessors.RemoveAll([](const TWeakObjectPtr<UObject>& Processor) { return !Processor.IsValid(); });
	const auto Processors = LoadingProcessors;
	for (const auto& Processor : Processors)
	{
		if (TryGetLoadingReason(Processor.Get(), LoadingReason))
		{
			return true;
		}
	}

	bool bHasController = false;
	bool bMissingController = false;
	for (const ULocalPlayer* Player : Instance->GetLocalPlayers())
	{
		auto* Controller = Player ? Player->UPlayer::GetPlayerController(LoadingWorld) : nullptr;
		if (!Controller)
		{
			bMissingController = true;
			continue;
		}
		bHasController = true;
		if (CheckActor(Controller))
		{
			return true;
		}
	}

	if (!bHasController || (bMissingController && Viewport.GetCurrentSplitscreenConfiguration() != ESplitScreenType::None))
	{
		LoadingReason = TEXT("Waiting for a local player controller.");
		return true;
	}
	LoadingReason.Reset();
	return false;
}

void ULoadingScreenManager::UpdateLoadingScreen()
{
	auto* Viewport = GetGameInstance()->GetGameViewportClient();
	const auto* PreLoad = FPreLoadScreenManager::Get();
	if (!Viewport || !FSlateApplication::IsInitialized() || (PreLoad && PreLoad->HasValidActivePreLoadScreen()))
	{
		HideLoadingScreen();
		return;
	}
	if (Screen.IsValid() && ScreenViewport.Get() != Viewport)
	{
		HideLoadingScreen();
	}

	const bool bNeedsLoadingScreen = NeedsLoadingScreen(*Viewport);
	if (bNeedsLoadingScreen || !PendingLoadingReason.IsEmpty())
	{
		if (!bNeedsLoadingScreen)
		{
			LoadingReason = MoveTemp(PendingLoadingReason);
		}
		PendingLoadingReason.Reset();
		ReadySince = -1.0;
		ShowLoadingScreen(*Viewport);
		return;
	}
	if (!Screen.IsValid())
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (ReadySince < 0.0)
	{
		ReadySince = Now;
	}
	if (Now - ReadySince >= GetDefault<UCommonLoadingScreenSettings>()->HoldAfterLoad)
	{
		HideLoadingScreen();
	}
	else
	{
		LoadingReason = TEXT("Finishing loading.");
	}
}

void ULoadingScreenManager::ShowLoadingScreen(UGameViewportClient& Viewport)
{
	if (Screen.IsValid())
	{
		return;
	}

	const auto* Settings = GetDefault<UCommonLoadingScreenSettings>();
	TSubclassOf<UUserWidget> WidgetClass = Settings->LoadingScreenWidget.LoadSynchronous();
	UUserWidget* Widget = WidgetClass && !WidgetClass->HasAnyClassFlags(CLASS_Abstract)
		? CreateWidget<UUserWidget>(GetGameInstance(), WidgetClass) : nullptr;
	if (!Widget)
	{
		Widget = CreateWidget<UCommonLoadingScreenWidget>(GetGameInstance());
	}
	if (!Widget)
	{
		return;
	}

	Screen = Widget->TakeWidget();
	ScreenViewport = &Viewport;
	Viewport.AddViewportWidgetContent(Screen.ToSharedRef(), Settings->LoadingScreenZOrder);
	if (!GIsEditor)
	{
		InputProcessor = MakeShared<FLoadingScreenInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor, 0);
	}
	VisibilityChanged.Broadcast(true);

	if (bInLoadMap && !GIsEditor && Screen.IsValid())
	{
		FSlateApplication::Get().Tick();
	}
}

void ULoadingScreenManager::HideLoadingScreen()
{
	if (InputProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
		}
		InputProcessor.Reset();
	}
	if (!Screen.IsValid())
	{
		return;
	}

	if (auto* Viewport = ScreenViewport.Get())
	{
		Viewport->RemoveViewportWidgetContent(Screen.ToSharedRef());
	}
	ScreenViewport.Reset();
	Screen.Reset();
	ReadySince = -1.0;
	VisibilityChanged.Broadcast(false);
}
