#include "HFSMLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/ActorComponent.h"
#include "HFSMStateComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GCFSMRootState.h"
#include "HFSMState.h"
#include "Input/PlayerInputSubsystem.h"
#include "LayoutTemplate/LayoutWidget.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHFSMContextOwnershipTest, "UIModule.HFSM.ContextOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHFSMContextOwnershipTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
	if (!TestNotNull(TEXT("Test world exists"), World.Get()))
	{
		return false;
	}
	APlayerController* FirstController = World->SpawnActor<APlayerController>();
	APlayerController* OwnerController = World->SpawnActor<APlayerController>();
	AHUD* HUD = World->SpawnActor<AHUD>();
	APawn* UnpossessedPawn = World->SpawnActor<APawn>();
	if (!FirstController || !OwnerController || !HUD || !UnpossessedPawn)
	{
		AddError(TEXT("Could not create context ownership test actors."));
		World->DestroyWorld(false);
		return false;
	}
	TStrongObjectPtr<ULocalPlayer> FirstPlayer(NewObject<ULocalPlayer>(GEngine));
	TStrongObjectPtr<ULocalPlayer> OwnerPlayer(NewObject<ULocalPlayer>(GEngine));
	FirstController->Player = FirstPlayer.Get();
	OwnerController->Player = OwnerPlayer.Get();
	FirstPlayer->PlayerController = FirstController;
	OwnerPlayer->PlayerController = OwnerController;
	FirstController->InputComponent = NewObject<UInputComponent>(FirstController);
	OwnerController->InputComponent = NewObject<UInputComponent>(OwnerController);
	HUD->PlayerOwner = OwnerController;
	TStrongObjectPtr<ULayoutWidget> Widget(NewObject<ULayoutWidget>());
	Widget->SetOwningPlayer(OwnerController);
	UActorComponent* ActorComponent = NewObject<UActorComponent>(OwnerController);

	TestEqual(TEXT("Controller resolves its own player"),
		UHFSMLibrary::ResolveLocalPlayer(OwnerController), OwnerPlayer.Get());
	TestEqual(TEXT("Local player resolves its own controller"),
		UHFSMLibrary::ResolvePlayerController(OwnerPlayer.Get()), OwnerController);
	TestEqual(TEXT("HUD resolves its owning controller"),
		UHFSMLibrary::ResolvePlayerController(HUD), OwnerController);
	TestEqual(TEXT("Widget resolves its owning player"),
		UHFSMLibrary::ResolveLocalPlayer(Widget.Get()), OwnerPlayer.Get());
	TestEqual(TEXT("Actor component resolves its owning actor's player"),
		UHFSMLibrary::ResolveLocalPlayer(ActorComponent), OwnerPlayer.Get());
	TestNull(TEXT("World does not select a player"), UHFSMLibrary::ResolveLocalPlayer(World.Get()));
	TestNull(TEXT("World does not select an input subsystem"), UPlayerInputSubsystem::GetInputSubsystem(World.Get()));
	TestNull(TEXT("Unpossessed pawn does not fall back to another player"),
		UHFSMLibrary::ResolveLocalPlayer(UnpossessedPawn));

	TStrongObjectPtr<UGCFSMRootState> Root(UGCFSMRootState::MakeRootStateObject(HUD));
	Root->SetPaused(true);
	EGCFSMReplicatedExec Exec;
	UGCFSM* FSM = Root->MakeFSM(TEXT("OwnershipTest"), EGCFSMReplicationOptions::NonReplicated, Exec);
	TStrongObjectPtr<UHFSMState> State(Cast<UHFSMState>(UGCFSMState::MakeStateObject(
		FSM, UHFSMState::StaticClass(), false, 0.f, TEXT("OwnershipTest"))));
	UHFSMStateComponent* PlayerComponent = NewObject<UHFSMStateComponent>(State.Get());
	PlayerComponent->Init(State.Get());
	TestEqual(TEXT("State keeps the original GCFSM context"), State->GetContextOwner(), static_cast<UObject*>(HUD));
	TestEqual(TEXT("Component keeps the state context"), PlayerComponent->GetContextOwner(), static_cast<UObject*>(HUD));
	TestEqual(TEXT("Component resolves the owner instead of the first controller"),
		PlayerComponent->GetPlayerController(), OwnerController);

	UInputComponent* FirstInput = FirstController->InputComponent;
	UInputComponent* OwnerInput = OwnerController->InputComponent;
	FirstInput->BindAction(TEXT("Unrelated"), IE_Pressed, PlayerComponent, &UHFSMStateComponent::OnEnter);
	OwnerInput->BindAction(TEXT("Owned"), IE_Pressed, PlayerComponent, &UHFSMStateComponent::OnEnter);
	PlayerComponent->Enter();
	{
		HUD->PlayerOwner = FirstController;
		TestEqual(TEXT("State resolves the current context owner"), State->GetPlayerController(), FirstController);
		TestEqual(TEXT("Component follows its state's current player"), PlayerComponent->GetLocalPlayer(), FirstPlayer.Get());
		TestEqual(TEXT("Context library resolves the same player"),
			UHFSMLibrary::ResolveLocalPlayer(PlayerComponent), FirstPlayer.Get());
		PlayerComponent->Exit(NAME_None);
		TestEqual(TEXT("Exit preserves direct Controller bindings"), OwnerInput->GetNumActionBindings(), 1);
		TestEqual(TEXT("Exit leaves another controller untouched"), FirstInput->GetNumActionBindings(), 1);
	}

	TStrongObjectPtr<UGCFSMRootState> WorldRoot(UGCFSMRootState::MakeRootStateObject(World.Get()));
	WorldRoot->SetPaused(true);
	UGCFSM* WorldFSM = WorldRoot->MakeFSM(TEXT("WorldTest"), EGCFSMReplicationOptions::NonReplicated, Exec);
	TStrongObjectPtr<UHFSMState> WorldState(Cast<UHFSMState>(UGCFSMState::MakeStateObject(
		WorldFSM, UHFSMState::StaticClass(), false, 0.f, TEXT("WorldTest"))));
	UHFSMStateComponent* GenericComponent = NewObject<UHFSMStateComponent>(WorldState.Get());
	GenericComponent->Init(WorldState.Get());
	TestEqual(TEXT("Generic component supports a world owner"), GenericComponent->GetContextOwner(),
		static_cast<UObject*>(World.Get()));
	FirstInput->BindAction(TEXT("Generic"), IE_Pressed, GenericComponent, &UHFSMStateComponent::OnEnter);
	GenericComponent->Enter();
	{
		GenericComponent->Exit(NAME_None);
		TestEqual(TEXT("Generic component does not manage player bindings"), FirstInput->GetNumActionBindings(), 2);
	}
	World->DestroyWorld(false);
	return true;
}

#endif
