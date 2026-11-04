#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "NGConnectionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGConnectionStatus,FText,Message);

/** Dedicated-server direct connect. Steam/session discovery intentionally remains a stub. */
UCLASS(Config=GameUserSettings)
class NEXTGENONE_API UNGConnectionSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float Delta) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UNGConnectionSubsystem,STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return bConnecting && !IsTemplate(); }
	UFUNCTION(BlueprintCallable,Category="Network") bool Connect(APlayerController* Player,const FString& Address);
	UFUNCTION(BlueprintCallable,Category="Network") void Disconnect();
	UFUNCTION(BlueprintPure,Category="Network") FText GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure,Category="Network") bool IsConnecting() const { return bConnecting; }
	UPROPERTY(BlueprintAssignable,Category="Network") FNGConnectionStatus OnStatusChanged;
	UPROPERTY(Config,BlueprintReadOnly,Category="Network") FString LastAddress=TEXT("127.0.0.1:7777");
private:
	bool bConnecting=false;
	double ConnectStarted=0;
	FText Status=FText::FromString(TEXT("Выделенный сервер · подключение по адресу. Steam пока не настроен."));
	FDelegateHandle NetworkHandle,TravelHandle;
	void SetStatus(const FString& Text);
	void NetworkFailed(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error);
	void TravelFailed(UWorld* World,ETravelFailure::Type Type,const FString& Error);
};
