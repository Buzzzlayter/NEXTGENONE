#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "NGConnectionSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FNGConnectionStatus,FText,Message);

class FOnlineSessionSearch;
class FUniqueNetId;

struct FNGPartyMember
{
	FString Id;
	FString Name;
	bool bLeader = false;
	bool bLocal = false;
	bool operator==(const FNGPartyMember&) const = default;
};

UCLASS(Config=GameUserSettings)
class AUTHORIZATION_API UNGConnectionSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float Delta) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UNGConnectionSubsystem,STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate() && (bConnecting || bInParty || bStartingPartyMatch); }
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	UFUNCTION(BlueprintCallable,Category="Network") bool Connect(APlayerController* Player,const FString& Address);
	UFUNCTION(BlueprintCallable,Category="Network") void Disconnect();
	UFUNCTION(BlueprintPure,Category="Network") FText GetStatus() const { return Status; }
	UFUNCTION(BlueprintPure,Category="Network") bool IsConnecting() const { return bConnecting; }
	UPROPERTY(BlueprintAssignable,Category="Network") FNGConnectionStatus OnStatusChanged;
	UPROPERTY(Config,BlueprintReadOnly,Category="Network") FString LastAddress=TEXT("127.0.0.1:7777");
	void SignIn(APlayerController* Player, bool bSteam);
	void CreateParty();
	void JoinParty(const FString& Code);
	void LeaveParty();
	void PlayParty();
	bool IsOnlineSignedIn() const;
	bool IsInParty() const { return bInParty; }
	bool IsPartyLeader() const { return bPartyLeader; }
	bool IsPartyMatchReady() const { return bPartyMatchReady; }
	bool IsOnlineBusy() const { return OnlineAction != EOnlineAction::None || bStartingPartyMatch || bConnecting; }
	const FString& GetPartyCode() const { return PartyCode; }
	const TArray<FNGPartyMember>& GetPartyMembers() const { return PartyMembers; }
	FSimpleMulticastDelegate OnPartyChanged;
private:
	enum class EOnlineAction : uint8 { None, Login, Create, Search, Join, Leave, ShareProfile, Publish };
	EOnlineAction OnlineAction = EOnlineAction::None;
	int32 OnlineUser = INDEX_NONE;
	TWeakObjectPtr<class ULocalPlayer> OnlinePlayer;
	TSharedPtr<FOnlineSessionSearch> PartySearch;
	FDelegateHandle LoginHandle, CreateHandle, FindHandle, JoinHandle, LeaveHandle, UpdateHandle;
	FString PartyCode;
	FString OnlineNickname;
	TArray<FNGPartyMember> PartyMembers;
	double NextPartyRefresh = 0.0;
	double PartyMatchStarted = 0.0;
	bool bInParty = false;
	bool bPartyLeader = false;
	bool bPartyMatchReady = false;
	bool bStartingPartyMatch = false;
	bool bConnectingToParty = false;
	bool bSteamSignIn = false;
	bool BindOnline();
	void UnbindOnline();
	void RefreshParty();
	void TickParty();
	void SharePartyProfile(const FString& Message);
	void FinishOnlineAction(const FString& Message);
	void LoginComplete(int32 LocalUser, bool bSuccess, const FUniqueNetId& Id, const FString& Error);
	bool bConnecting=false;
	double ConnectStarted=0;
	FText Status=FText::FromString(TEXT("Войдите через Steam или Epic Games, чтобы создать группу."));
	FDelegateHandle NetworkHandle,TravelHandle;
	void SetStatus(const FString& Text);
	void NetworkFailed(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error);
	void TravelFailed(UWorld* World,ETravelFailure::Type Type,const FString& Error);
};
