#include "Components/HFSMPlayerStateComponent.h"

#include "Components/InputComponent.h"
#include "Context/HFSMContextLibrary.h"
#include "Engine/InputDelegateBinding.h"
#include "GameFramework/PlayerController.h"

void UHFSMPlayerStateComponent::Init(UHFSMState* StateOwner)
{
	Super::Init(StateOwner);
	APlayerController* Controller = UHFSMContextLibrary::ResolvePlayerController(GetContextOwner());
	PlayerContext = IsValid(Controller) && Controller->GetLocalPlayer()
		? FLocalPlayerContext(Controller->GetLocalPlayer(), GetWorld()) : FLocalPlayerContext();
	bHasPlayerContext = true;
}

APlayerController* UHFSMPlayerStateComponent::GetPlayerController() const
{
	if (!IsValid(GetContextOwner()))
	{
		return nullptr;
	}
	if (bHasPlayerContext)
	{
		return PlayerContext.IsInitialized() ? PlayerContext.GetPlayerController() : nullptr;
	}
	return UHFSMContextLibrary::ResolvePlayerController(GetContextOwner());
}

ULocalPlayer* UHFSMPlayerStateComponent::GetLocalPlayer() const
{
	if (!IsValid(GetContextOwner()))
	{
		return nullptr;
	}
	if (bHasPlayerContext)
	{
		return PlayerContext.IsInitialized() ? PlayerContext.GetLocalPlayer() : nullptr;
	}
	return UHFSMContextLibrary::ResolveLocalPlayer(GetContextOwner());
}

bool UHFSMPlayerStateComponent::Enter()
{
	APlayerController* Controller = GetPlayerController();
	if (!ensureMsgf(IsValid(Controller) && Controller->GetLocalPlayer(),
		TEXT("Player state component requires an explicit local player context.")))
	{
		return false;
	}
	if (UInputComponent* InputComponent = Controller->InputComponent)
	{
		UInputDelegateBinding::BindInputDelegates(GetClass(), InputComponent, this);
		BoundInputComponent = InputComponent;
	}
	if (!Super::Enter())
	{
		ReleasePlayerContext();
		return false;
	}
	return true;
}

void UHFSMPlayerStateComponent::Exit(FName Event)
{
	Super::Exit(Event);
	ReleasePlayerContext();
}

void UHFSMPlayerStateComponent::ReleasePlayerContext()
{
	if (UInputComponent* InputComponent = BoundInputComponent.Get())
	{
		InputComponent->ClearBindingsForObject(this);
	}
	BoundInputComponent.Reset();
	PlayerContext = FLocalPlayerContext();
	bHasPlayerContext = false;
}
