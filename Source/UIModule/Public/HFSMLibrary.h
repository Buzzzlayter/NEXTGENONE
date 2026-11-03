#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HFSMLibrary.generated.h"

class APlayerController;
class ULocalPlayer;
class UInputComponent;

UCLASS()
class UIMODULE_API UHFSMLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// Native lifecycle helpers: mappings/policies remain owned by PlayerInputSubsystem.
	static UInputComponent* BindBlueprintInput(UObject* Receiver, const UObject* Context);
	static void UnbindBlueprintInput(UInputComponent* Input);

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	static UObject* ResolveContextOwner(const UObject* StateOrContext);

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	static APlayerController* ResolvePlayerController(const UObject* StateOrContext);

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	static ULocalPlayer* ResolveLocalPlayer(const UObject* StateOrContext);

	/** Queues an event for the context's FSMs and their substates, without invoking internal event handlers. */
	UFUNCTION(BlueprintCallable, Category = "HFSM|Events")
	static void QueueEvent(UObject* StateOrContext, FGameplayTag Event);
	/** Same event queue, for FSM event names that do not require a GameplayTag. */
	UFUNCTION(BlueprintCallable, Category = "HFSM|Events")
	static void QueueNamedEvent(UObject* StateOrContext, FName Event);

	/** Queues events in array order using the same policy as QueueEvent. */
	UFUNCTION(BlueprintCallable, Category = "HFSM|Events")
	static void QueueEvents(UObject* StateOrContext, const TArray<FGameplayTag>& Events);
};
