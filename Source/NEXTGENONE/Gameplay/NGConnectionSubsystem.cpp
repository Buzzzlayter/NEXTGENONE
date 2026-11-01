#include "NGConnectionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

void UNGConnectionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (GEngine) { NetworkHandle=GEngine->OnNetworkFailure().AddUObject(this,&ThisClass::NetworkFailed); TravelHandle=GEngine->OnTravelFailure().AddUObject(this,&ThisClass::TravelFailed); }
}
void UNGConnectionSubsystem::Deinitialize()
{
	if (GEngine) { GEngine->OnNetworkFailure().Remove(NetworkHandle); GEngine->OnTravelFailure().Remove(TravelHandle); }
	bConnecting=false; Super::Deinitialize();
}
void UNGConnectionSubsystem::SetStatus(const FString& Text) { Status=FText::FromString(Text); OnStatusChanged.Broadcast(Status); }
bool UNGConnectionSubsystem::Connect(APlayerController* Player,const FString& InAddress)
{
	if (!Player || Player->GetGameInstance()!=GetGameInstance() || !Player->IsLocalController() || bConnecting || Player->GetNetMode()==NM_DedicatedServer) return false;
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
	if (!GetWorld() || GetWorld()->GetNetMode()==NM_DedicatedServer) return;
	SetStatus(TEXT("Отключено. Возврат в локальный Gameplay.")); UGameplayStatics::OpenLevel(this,TEXT("/Game/Maps/Gameplay"),true);
}
void UNGConnectionSubsystem::Tick(float Delta)
{
	if (!bConnecting) return;
	const auto* Driver=GetWorld() ? GetWorld()->GetNetDriver() : nullptr;
	if (GetWorld() && GetWorld()->GetNetMode()==NM_Client && Driver && Driver->ServerConnection && Driver->ServerConnection->GetConnectionState()==USOCK_Open)
	{ bConnecting=false; SetStatus(TEXT("Подключено к ")+LastAddress); }
	else if (FPlatformTime::Seconds()-ConnectStarted>30.) { Disconnect(); SetStatus(TEXT("Время подключения истекло. Проверьте адрес и UDP-порт сервера.")); }
}
void UNGConnectionSubsystem::NetworkFailed(UWorld* World,UNetDriver* Driver,ENetworkFailure::Type Type,const FString& Error)
{
	if (World && World->GetGameInstance()==GetGameInstance()) { bConnecting=false; SetStatus(TEXT("Ошибка сети: ")+Error); }
}
void UNGConnectionSubsystem::TravelFailed(UWorld* World,ETravelFailure::Type Type,const FString& Error)
{
	if (World && World->GetGameInstance()==GetGameInstance()) { bConnecting=false; SetStatus(TEXT("Ошибка подключения: ")+Error); }
}
