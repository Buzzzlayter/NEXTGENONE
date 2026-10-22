#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "HFSMContextLibrary.generated.h"

class APlayerController;
class ULocalPlayer;

UCLASS()
class UIMODULE_API UHFSMContextLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	static UObject* ResolveContextOwner(const UObject* StateOrContext);

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	static APlayerController* ResolvePlayerController(const UObject* StateOrContext);

	UFUNCTION(BlueprintPure, Category = "HFSM|Context")
	static ULocalPlayer* ResolveLocalPlayer(const UObject* StateOrContext);
};
