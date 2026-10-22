#include "HFSMState.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/InputComponent.h"
#include "Components/HFSMPlayerStateComponent.h"
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
	UHFSMPlayerStateComponent* First = NewObject<UHFSMPlayerStateComponent>(State.Get());
	UUILayoutTemplate* FailingLayout = NewObject<UUILayoutTemplate>(State.Get());
	UHFSMPlayerStateComponent* Last = NewObject<UHFSMPlayerStateComponent>(State.Get());
	State->Components = { First, FailingLayout, Last };

	// Existing callbacks let us observe that rollback unbinds only the entered component.
	Input->BindAction(TEXT("First"), IE_Pressed, First, &UHFSMStateComponent::OnEnter);
	Input->BindAction(TEXT("Last"), IE_Pressed, Last, &UHFSMStateComponent::OnEnter);
	TFunction<bool(const FEnsureHandlerArgs&)> PreviousHandler = SetEnsureHandler(
		[](const FEnsureHandlerArgs& Args)
		{
			return FCString::Strcmp(Args.Message, TEXT("Layout has no template class.")) == 0;
		});
	State->Enter();
	SetEnsureHandler(MoveTemp(PreviousHandler));

	TestFalse(TEXT("A missing layout template fails state entry"), State->HasEnteredSuccessfully());
	TestEqual(TEXT("Previously entered components are rolled back"), State->ActiveComponents.Num(), 0);
	TestNull(TEXT("Components after the failure are not initialized"), Last->GetState());
	TestEqual(TEXT("Only the entered component's binding is removed"), Input->GetNumActionBindings(), 1);
	State->Tick(0.016f);
	State->Exit(NAME_None);
	TestEqual(TEXT("Exiting a failed state does not exit unentered components"), Input->GetNumActionBindings(), 1);

	State->Components = { First, Last };
	State->Enter();
	TestTrue(TEXT("Valid components enter successfully"), State->HasEnteredSuccessfully());
	TestEqual(TEXT("Both successful components are tracked"), State->ActiveComponents.Num(), 2);
	State->Exit(NAME_None);
	TestFalse(TEXT("Normal exit clears state activation"), State->HasEnteredSuccessfully());
	TestEqual(TEXT("Normal exit releases all active components"), State->ActiveComponents.Num(), 0);
	TestEqual(TEXT("Normal exit unbinds the remaining component"), Input->GetNumActionBindings(), 0);
	State->Exit(NAME_None);
	TestEqual(TEXT("Repeated exit leaves bindings unchanged"), Input->GetNumActionBindings(), 0);
	World->DestroyWorld(false);
	return true;
}

#endif
