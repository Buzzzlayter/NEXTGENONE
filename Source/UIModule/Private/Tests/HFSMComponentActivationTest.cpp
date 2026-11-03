#include "HFSMState.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InputComponent.h"
#include "HFSMStateComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GCFSMRootState.h"
#include "LayoutTemplate/UILayoutTemplate.h"
#include "Misc/AssertionMacros.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHFSMComponentActivationTest, "UIModule.HFSM.ComponentActivation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FHFSMComponentActivationTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
	if (!TestNotNull(TEXT("Test world exists"), World.Get()))
	{
		return false;
	}
	APlayerController* Controller = World->SpawnActor<APlayerController>();
	if (!TestNotNull(TEXT("Test controller exists"), Controller))
	{
		World->DestroyWorld(false);
		return false;
	}
	Controller->InputComponent = NewObject<UInputComponent>(Controller);
	UInputComponent* Input = Controller->InputComponent;
	TStrongObjectPtr<ULocalPlayer> Player(NewObject<ULocalPlayer>(GEngine));
	Controller->Player = Player.Get();
	Player->PlayerController = Controller;
	TStrongObjectPtr<UGCFSMRootState> Root(UGCFSMRootState::MakeRootStateObject(Controller));
	Root->SetPaused(true);
	EGCFSMReplicatedExec Exec;
	UGCFSM* FSM = Root->MakeFSM(TEXT("ActivationTest"), EGCFSMReplicationOptions::NonReplicated, Exec);
	TStrongObjectPtr<UHFSMState> State(Cast<UHFSMState>(UGCFSMState::MakeStateObject(
		FSM, UHFSMState::StaticClass(), false, 0.f, TEXT("ActivationTest"))));
	UHFSMStateComponent* First = NewObject<UHFSMStateComponent>(State.Get());
	UUILayoutTemplate* FailingLayout = NewObject<UUILayoutTemplate>(State.Get());
	UHFSMStateComponent* Last = NewObject<UHFSMStateComponent>(State.Get());
	State->Components = { First, FailingLayout, Last };

	// Direct Controller bindings are outside the Blueprint input scopes and must remain untouched.
	Input->BindAction(TEXT("First"), IE_Pressed, First, &UHFSMStateComponent::OnEnter);
	Input->BindAction(TEXT("Last"), IE_Pressed, Last, &UHFSMStateComponent::OnEnter);
	TFunction<bool(const FEnsureHandlerArgs&)> PreviousHandler = SetEnsureHandler(
		[](const FEnsureHandlerArgs& Args)
		{
			return FCString::Strcmp(Args.Message, TEXT("Layout has no template class.")) == 0;
		});
	State->Enter();
	SetEnsureHandler(MoveTemp(PreviousHandler));

	TestEqual(TEXT("Earlier components keep their context"), First->GetState(), State.Get());
	TestEqual(TEXT("A component's local validation does not skip the next component"), Last->GetState(), State.Get());
	TestEqual(TEXT("Entry preserves direct Controller bindings"), Input->GetNumActionBindings(), 2);
	State->Tick(0.016f);
	State->Exit(NAME_None);
	TestEqual(TEXT("Exit preserves direct bindings"), Input->GetNumActionBindings(), 2);

	State->Components = { First, Last };
	State->Enter();
	TestEqual(TEXT("Configured components are unchanged by entry"), State->Components.Num(), 2);
	State->Exit(NAME_None);
	TestEqual(TEXT("Normal exit leaves direct Controller bindings alone"), Input->GetNumActionBindings(), 2);
	State->Exit(NAME_None);
	TestEqual(TEXT("Repeated exit leaves bindings unchanged"), Input->GetNumActionBindings(), 2);
	World->DestroyWorld(false);
	return true;
}

#endif
