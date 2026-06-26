// Copyright (c) 2024 Gamecentric, All rights reserved

#pragma once

#include "CoreMinimal.h"
#include <Subsystems/EngineSubsystem.h>
#include "GCFSMSubsystem.generated.h"

class UGCFSMRootState;

/**
 * 
 */
UCLASS()
class GCFSM_API UGCFSMSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UGCFSMRootState* GetOrMakeRootStateFromContext(UObject* context);
	static UGCFSMRootState* GetRootStateFromContext(UObject* context, bool verbose = false);

protected:
	virtual void Initialize(FSubsystemCollectionBase& collection) override;
	virtual void Deinitialize() override;
	
private:
	static UGCFSMSubsystem* instance;

	UPROPERTY(Transient)
	TMap<uint64, TObjectPtr<UGCFSMRootState>> activeContexts;
	
	void OnPreGarbageCollect();
	void OnWorldCleanup(UWorld* world, bool sessionEnded, bool cleanupResources);
};
