#include "Online/NGConnectionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"

void UNGConnectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	FString SavedAddress;
	if (GConfig && (GConfig->GetString(*GetClass()->GetPathName(), TEXT("LastAddress"), SavedAddress, GGameUserSettingsIni)
		|| GConfig->GetString(TEXT("/Script/NEXTGENONE.NGConnectionSubsystem"), TEXT("LastAddress"), SavedAddress, GGameUserSettingsIni)))
	{
		LastAddress = MoveTemp(SavedAddress);
	}
	if (GEngine) { NetworkHandle=GEngine->OnNetworkFailure().AddUObject(this,&ThisClass::NetworkFailed); TravelHandle=GEngine->OnTravelFailure().AddUObject(this,&ThisClass::TravelFailed); }
}
void UNGConnectionSubsystem::Deinitialize()
{
	UnbindOnline();
	if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkHandle); GEngine->OnTravelFailure().Remove(TravelHandle); }
	bConnecting=false; Super::Deinitialize();
}
void UNGConnectionSubsystem::SetStatus(const FString& Text) { Status=FText::FromString(Text); OnStatusChanged.Broadcast(Status); }
bool UNGConnectionSubsystem::Connect(APlayerController* Player,const FString& InAddress)
{
	if (!Player || Player->GetGameInstance()!=GetGameInstance() || !Player->IsLocalController() || IsOnlineBusy() || Player->GetNetMode()==NM_DedicatedServer) return false;
	if (bInParty) { SetStatus(TEXT("Чтобы подключиться по адресу, сначала выйдите из группы.")); return false; }
	if (Player->GetNetMode()!=NM_Standalone) { SetStatus(TEXT("Сначала отключитесь от текущего сервера.")); return false; }
	const FString Address=InAddress.TrimStartAndEnd(); FString Host=Address,Port;
	if (Address.IsEmpty() || Address.Len()>259) { SetStatus(TEXT("Введите адрес сервера, например 127.0.0.1:7777.")); return false; }
	if (Address.Split(TEXT(":"),&Host,&Port))
	{
		bool bDigits=!Port.IsEmpty() && Port.Len()<=5; for (TCHAR C:Port) bDigits &= C>='0' && C<='9';
		const int32 Number=bDigits ? FCString::Atoi(*Port) : 0;
		if (Number<1 || Number>65535) { SetStatus(TEXT("Порт должен быть числом от 1 до 65535.")); return false; }
	}
	bool bValid=!Host.IsEmpty() && Host.Len()<=253 && !Host.Contains(TEXT(".."));
	for (TCHAR C:Host) bValid &= (C>='a' && C<='z') || (C>='A' && C<='Z') || (C>='0' && C<='9') || C=='.' || C=='-';
	if (!bValid) { SetStatus(TEXT("Нужен IPv4 или DNS-адрес без URL-параметров. IPv6 пока не поддержан.")); return false; }
	LastAddress=Address; SaveConfig(); bConnecting=true; ConnectStarted=FPlatformTime::Seconds(); SetStatus(TEXT("Подключение к ")+Address+TEXT("…"));
	Player->ClientTravel(Address,TRAVEL_Absolute); return true;
}
void UNGConnectionSubsystem::Disconnect()
{
	bConnecting=false;
	bConnectingToParty=false;
	if (!GetWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer) return;
	const FString Map = FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap());
	if (Map.IsEmpty())
	{
		SetStatus(TEXT("В проекте не задана игровая карта."));
		return;
	}
	SetStatus(TEXT("Отключено. Возврат в локальную игру."));
	UGameplayStatics::OpenLevel(this, FName(*Map), true);
}
void UNGConnectionSubsystem::Tick(float Delta)
{
	if (bInParty || bStartingPartyMatch) TickParty();
	if (!bConnecting) return;
	const auto* Driver=GetWorld() ? GetWorld()->GetNetDriver() : nullptr;
	if (GetWorld() && GetWorld()->GetNetMode()==NM_Client && Driver && Driver->ServerConnection && Driver->ServerConnection->GetConnectionState()==USOCK_Open)
	{ bConnecting=false; SetStatus(bConnectingToParty ? FString(TEXT("Вы в миссии группы.")) : TEXT("Подключено к ")+LastAddress); bConnectingToParty=false; }
	else if (FPlatformTime::Seconds()-ConnectStarted>30.)
	{
		const bool bPartyTravel = bConnectingToParty;
		Disconnect();
		SetStatus(bPartyTravel ? TEXT("Миссия не ответила. Попробуйте подключиться снова.") : TEXT("Время подключения истекло. Проверьте адрес и UDP-порт сервера."));
	}
}
void UNGConnectionSubsystem::NetworkFailed(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error)
{
	if (World && World->GetGameInstance()==GetGameInstance()) { bConnecting=false; bConnectingToParty=false; bStartingPartyMatch=false; SetStatus(TEXT("Ошибка сети: ")+Error); }
}
void UNGConnectionSubsystem::TravelFailed(UWorld* World,ETravelFailure::Type Type,const FString& Error)
{
	if (World && World->GetGameInstance()==GetGameInstance()) { bConnecting=false; bConnectingToParty=false; bStartingPartyMatch=false; SetStatus(TEXT("Ошибка подключения: ")+Error); }
}
