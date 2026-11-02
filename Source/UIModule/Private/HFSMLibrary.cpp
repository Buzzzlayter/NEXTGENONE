#include "HFSMLibrary.h"

#include "Components/ActorComponent.h"
#include "Components/HFSMPlayerStateComponent.h"
#include "Components/Widget.h"
#include "Engine/LocalPlayer.h"
#include "GCFSMState.h"
#include "GCFSMUtilities.h"
#include "GameFramework/HUD.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HFSMStateComponent.h"
#include "Subsystems/LocalPlayerSubsystem.h"

UObject* UHFSMLibrary::ResolveContextOwner(const UObject* StateOrContext)
{
	if (!IsValid(StateOrContext))
	{
		return nullptr;
	}
	if (const UHFSMStateComponent* Component = Cast<UHFSMStateComponent>(StateOrContext))
	{
		return Component->GetContextOwner();
	}
	if (const UGCFSMState* State = Cast<UGCFSMState>(StateOrContext))
	{
		return State->GetContextObject();
	}
	return const_cast<UObject*>(StateOrContext);
}

APlayerController* UHFSMLibrary::ResolvePlayerController(const UObject* StateOrContext)
{
	if (!IsValid(StateOrContext))
	{
		return nullptr;
	}
	if (const UHFSMPlayerStateComponent* Component = Cast<UHFSMPlayerStateComponent>(StateOrContext))
	{
		return Component->GetPlayerController();
	}
	UObject* Context = ResolveContextOwner(StateOrContext);
	if (const UActorComponent* Component = Cast<UActorComponent>(Context))
	{
		Context = Component->GetOwner();
	}
	if (!IsValid(Context))
	{
		return nullptr;
	}
	if (APlayerController* Controller = Cast<APlayerController>(Context))
	{
		return Controller;
	}
	if (const APawn* Pawn = Cast<APawn>(Context))
	{
		return Cast<APlayerController>(Pawn->GetController());
	}
	if (const AHUD* HUD = Cast<AHUD>(Context))
	{
		return HUD->GetOwningPlayerController();
	}
	if (const APlayerState* PlayerState = Cast<APlayerState>(Context))
	{
		return PlayerState->GetPlayerController();
	}
	if (const UWidget* Widget = Cast<UWidget>(Context))
	{
		return Widget->GetOwningPlayer();
	}
	if (const ULocalPlayer* Player = Cast<ULocalPlayer>(Context))
	{
		return Player->GetPlayerController(Player->GetWorld());
	}
	if (const ULocalPlayerSubsystem* Subsystem = Cast<ULocalPlayerSubsystem>(Context))
	{
		const ULocalPlayer* Player = Subsystem->GetLocalPlayer();
		return Player ? Player->GetPlayerController(Player->GetWorld()) : nullptr;
	}
	return nullptr;
}

ULocalPlayer* UHFSMLibrary::ResolveLocalPlayer(const UObject* StateOrContext)
{
	if (!IsValid(StateOrContext))
	{
		return nullptr;
	}
	if (const UHFSMPlayerStateComponent* Component = Cast<UHFSMPlayerStateComponent>(StateOrContext))
	{
		return Component->GetLocalPlayer();
	}
	UObject* Context = ResolveContextOwner(StateOrContext);
	if (ULocalPlayer* Player = Cast<ULocalPlayer>(Context))
	{
		return Player;
	}
	if (const ULocalPlayerSubsystem* Subsystem = Cast<ULocalPlayerSubsystem>(Context))
	{
		return Subsystem->GetLocalPlayer();
	}
	if (const UWidget* Widget = Cast<UWidget>(Context))
	{
		return Widget->GetOwningLocalPlayer();
	}
	const APlayerController* Controller = ResolvePlayerController(Context);
	return IsValid(Controller) ? Controller->GetLocalPlayer() : nullptr;
}

void UHFSMLibrary::QueueEvent(UObject* StateOrContext, FGameplayTag Event)
{
	UGCFSMUtilities::TriggerEvent(
		StateOrContext,
		Event.GetTagName(),
		EGCFSMTriggerEventTargetPolicy::ContextObject,
		true,
		false,
		EGCFSMTriggerEventQueuePolicy::JustQueue);
}

void UHFSMLibrary::QueueEvents(UObject* StateOrContext, const TArray<FGameplayTag>& Events)
{
	for (const FGameplayTag& Event : Events)
	{
		QueueEvent(StateOrContext, Event);
	}
}
