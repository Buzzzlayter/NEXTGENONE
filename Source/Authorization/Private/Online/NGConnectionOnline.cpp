#include "Online/NGConnectionSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/PlayerController.h"
#include "GameMapsSettings.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineUserInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Char.h"
#include "Misc/CommandLine.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Online/OnlineSessionNames.h"
#include "OnlineSessionSettings.h"
#include "OnlineSubsystem.h"

namespace
{
	const FName PartySession(TEXT("NGParty"));
	const FName PartyCodeKey(TEXT("NG_PARTY_CODE"));
	const FName MatchReadyKey(TEXT("NG_MATCH_READY"));
	const FName MemberNameKey(TEXT("NG_MEMBER_NAME"));

	IOnlineIdentityPtr GetIdentity(FName Provider = TEXT("EOS"))
	{
		auto* Subsystem = IOnlineSubsystem::Get(Provider);
		return Subsystem ? Subsystem->GetIdentityInterface() : nullptr;
	}

	IOnlineSessionPtr GetSessions()
	{
		auto* Subsystem = IOnlineSubsystem::Get(TEXT("EOS"));
		return Subsystem ? Subsystem->GetSessionInterface() : nullptr;
	}
}

bool UNGConnectionSubsystem::BindOnline()
{
	const auto Sessions = GetSessions();
	if (!Sessions) return false;
	if (CreateHandle.IsValid()) return true;

	CreateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(FOnCreateSessionCompleteDelegate::CreateWeakLambda(this,
		[this](FName SessionName, bool bSuccess)
		{
			if (SessionName != PartySession || OnlineAction != EOnlineAction::Create) return;
			if (bSuccess) SharePartyProfile(TEXT("Группа создана. Передайте код другому игроку."));
			else FinishOnlineAction(TEXT("EOS не удалось создать группу."));
		}));
	FindHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(FOnFindSessionsCompleteDelegate::CreateWeakLambda(this,
		[this](bool bSuccess)
		{
			if (OnlineAction != EOnlineAction::Search) return;
			const auto Search = MoveTemp(PartySearch);
			const auto ActiveSessions = GetSessions();
			const auto* Result = bSuccess && Search ? Search->SearchResults.FindByPredicate([this](const FOnlineSessionSearchResult& Entry)
			{
				FString FoundCode;
				return Entry.IsValid() && Entry.Session.SessionSettings.Get(PartyCodeKey, FoundCode) && FoundCode == PartyCode;
			}) : nullptr;
			if (!Result || !ActiveSessions)
			{
				FinishOnlineAction(TEXT("Группа с таким кодом не найдена или недоступна."));
				return;
			}
			OnlineAction = EOnlineAction::Join;
			if (!ActiveSessions->JoinSession(OnlineUser, PartySession, *Result) && OnlineAction == EOnlineAction::Join)
				FinishOnlineAction(TEXT("Подключение к группе не удалось запустить."));
		}));
	JoinHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(FOnJoinSessionCompleteDelegate::CreateWeakLambda(this,
		[this](FName SessionName, EOnJoinSessionCompleteResult::Type Result)
		{
			if (SessionName != PartySession || OnlineAction != EOnlineAction::Join) return;
			if (Result == EOnJoinSessionCompleteResult::Success) SharePartyProfile(TEXT("Вы в группе."));
			else FinishOnlineAction(TEXT("EOS не удалось подключить вас к группе."));
		}));
	LeaveHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(FOnDestroySessionCompleteDelegate::CreateWeakLambda(this,
		[this](FName SessionName, bool bSuccess)
		{
			if (SessionName != PartySession || OnlineAction != EOnlineAction::Leave) return;
			if (bSuccess && GetWorld() && GetWorld()->GetNetMode() != NM_Standalone) Disconnect();
			FinishOnlineAction(bSuccess ? TEXT("Вы вышли из группы.") : TEXT("EOS не удалось выполнить выход из группы."));
		}));
	UpdateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(FOnUpdateSessionCompleteDelegate::CreateWeakLambda(this,
		[this](FName SessionName, bool bSuccess)
		{
			if (SessionName != PartySession) return;
			if (OnlineAction == EOnlineAction::ShareProfile)
			{
				FinishOnlineAction(GetStatus().ToString());
				return;
			}
			if (OnlineAction != EOnlineAction::Publish) return;
			bStartingPartyMatch = false;
			if (!bSuccess)
			{
				const auto ActiveSessions = GetSessions();
				if (auto* Session = ActiveSessions ? ActiveSessions->GetNamedSession(PartySession) : nullptr)
					Session->SessionSettings.Set(MatchReadyKey, false, EOnlineDataAdvertisementType::ViaOnlineService);
			}
			FinishOnlineAction(bSuccess ? TEXT("Миссия открыта для группы.") : TEXT("Не удалось открыть миссию для группы. Попробуйте ещё раз."));
		}));
	return true;
}

void UNGConnectionSubsystem::UnbindOnline()
{
	if (LoginHandle.IsValid())
		if (const auto Identity = GetIdentity()) Identity->ClearOnLoginCompleteDelegate_Handle(OnlineUser, LoginHandle);
	if (CreateHandle.IsValid())
	{
		if (const auto Sessions = GetSessions())
		{
			Sessions->ClearOnCreateSessionCompleteDelegate_Handle(CreateHandle);
			Sessions->ClearOnFindSessionsCompleteDelegate_Handle(FindHandle);
			Sessions->ClearOnJoinSessionCompleteDelegate_Handle(JoinHandle);
			Sessions->ClearOnDestroySessionCompleteDelegate_Handle(LeaveHandle);
			Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(UpdateHandle);
		}
	}
	PartySearch.Reset();
}

void UNGConnectionSubsystem::SignIn(APlayerController* Player, bool bSteam)
{
	if (IsOnlineBusy() || bInParty || !Player || Player->GetGameInstance() != GetGameInstance() || !Player->GetLocalPlayer()) return;
	const auto Identity = GetIdentity();
	if (!Identity || !BindOnline())
	{
		SetStatus(TEXT("EOS недоступен. Для онлайн-входа нужен настроенный продукт Epic Online Services."));
		return;
	}
	OnlinePlayer = Player->GetLocalPlayer();
	OnlineUser = Player->GetLocalPlayer()->GetControllerId();
	if (IsOnlineSignedIn())
	{
		SetStatus(TEXT("Вы уже вошли в систему."));
		return;
	}
	OnlineAction = EOnlineAction::Login;
	bSteamSignIn = bSteam;
	LoginHandle = Identity->AddOnLoginCompleteDelegate_Handle(OnlineUser, FOnLoginCompleteDelegate::CreateUObject(this, &ThisClass::LoginComplete));
	SetStatus(bSteam ? TEXT("Вход через Steam…") : TEXT("Вход через Epic Games…"));
	if (bSteam)
	{
		const auto SteamIdentity = GetIdentity(TEXT("STEAM"));
		if (!SteamIdentity || SteamIdentity->GetLoginStatus(OnlineUser) != ELoginStatus::LoggedIn)
		{
			Identity->ClearOnLoginCompleteDelegate_Handle(OnlineUser, LoginHandle);
			LoginHandle.Reset();
			FinishOnlineAction(TEXT("Steam недоступен. Запустите игру из Steam под своим аккаунтом."));
			return;
		}
		SteamIdentity->GetLinkedAccountAuthToken(OnlineUser, TEXT("WebAPI:epiconlineservices"), IOnlineIdentity::FOnGetLinkedAccountAuthTokenCompleteDelegate::CreateWeakLambda(this,
			[this, Identity](int32 LocalUser, bool bSuccess, const FExternalAuthToken& Token)
			{
				if (LocalUser != OnlineUser || OnlineAction != EOnlineAction::Login) return;
				if (bSuccess && Token.HasTokenString()
					&& Identity->Login(LocalUser, FOnlineAccountCredentials(TEXT("externalauth:SteamSessionTicket"), FString(), Token.TokenString))) return;
				if (OnlineAction != EOnlineAction::Login) return;
				Identity->ClearOnLoginCompleteDelegate_Handle(OnlineUser, LoginHandle);
				LoginHandle.Reset();
				FinishOnlineAction(TEXT("Не удалось передать вход Steam в EOS."));
			}));
		return;
	}

	FString LauncherAuthType;
	FParse::Value(FCommandLine::Get(), TEXT("AUTH_TYPE="), LauncherAuthType);
	const bool bStarted = LauncherAuthType.Equals(TEXT("exchangecode"), ESearchCase::IgnoreCase)
		? Identity->AutoLogin(OnlineUser)
		: Identity->Login(OnlineUser, FOnlineAccountCredentials(TEXT("accountportal"), FString(), FString()));
	if (!bStarted && OnlineAction == EOnlineAction::Login)
	{
		Identity->ClearOnLoginCompleteDelegate_Handle(OnlineUser, LoginHandle);
		LoginHandle.Reset();
		FinishOnlineAction(TEXT("Не удалось открыть вход Epic Games."));
	}
}

void UNGConnectionSubsystem::LoginComplete(int32 LocalUser, bool bSuccess, const FUniqueNetId&, const FString&)
{
	if (LocalUser != OnlineUser || OnlineAction != EOnlineAction::Login) return;
	const auto Identity = GetIdentity();
	if (Identity) Identity->ClearOnLoginCompleteDelegate_Handle(OnlineUser, LoginHandle);
	LoginHandle.Reset();
	const auto AccountId = Identity && bSuccess ? Identity->GetUniquePlayerId(OnlineUser) : nullptr;
	const auto ProfileIdentity = bSteamSignIn && AccountId.IsValid() ? GetIdentity(TEXT("STEAM")) : Identity;
	OnlineNickname = ProfileIdentity && AccountId.IsValid() ? ProfileIdentity->GetPlayerNickname(OnlineUser).Left(40) : FString();
	if (auto* LocalPlayer = OnlinePlayer.Get(); LocalPlayer && AccountId.IsValid())
		LocalPlayer->SetCachedUniqueNetId(FUniqueNetIdRepl(AccountId));
	FinishOnlineAction(AccountId.IsValid() ? TEXT("Вы вошли. Создайте группу или введите код.")
		: TEXT("Вход EOS не завершён. Проверьте аккаунт и настройки продукта."));
}

bool UNGConnectionSubsystem::IsOnlineSignedIn() const
{
	if (OnlineUser == INDEX_NONE) return false;
	const auto Identity = GetIdentity();
	return Identity && Identity->GetLoginStatus(OnlineUser) == ELoginStatus::LoggedIn;
}

void UNGConnectionSubsystem::CreateParty()
{
	if (IsOnlineBusy() || bInParty || !IsOnlineSignedIn() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone) return;
	const auto Sessions = GetSessions();
	if (!Sessions) return;
	PartyCode = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8).ToUpper();
	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = 4;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bUseLobbiesIfAvailable = true;
	Settings.bUsesPresence = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.Set(PartyCodeKey, PartyCode, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(MatchReadyKey, false, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(SETTING_HOST_MIGRATION, false, EOnlineDataAdvertisementType::DontAdvertise);
	OnlineAction = EOnlineAction::Create;
	SetStatus(TEXT("Создаём группу…"));
	if (!Sessions->CreateSession(OnlineUser, PartySession, Settings) && OnlineAction == EOnlineAction::Create)
		FinishOnlineAction(TEXT("Не удалось запустить создание группы."));
}

void UNGConnectionSubsystem::JoinParty(const FString& Code)
{
	if (IsOnlineBusy() || bInParty || !IsOnlineSignedIn() || !GetWorld() || GetWorld()->GetNetMode() != NM_Standalone) return;
	const FString CleanCode = Code.TrimStartAndEnd().ToUpper();
	bool bValidCode = CleanCode.Len() == 8;
	for (TCHAR Char : CleanCode) bValidCode &= FChar::IsHexDigit(Char);
	if (!bValidCode)
	{
		SetStatus(TEXT("Введите восьмизначный код группы."));
		return;
	}
	const auto Sessions = GetSessions();
	if (!Sessions) return;
	PartyCode = CleanCode;
	PartySearch = MakeShared<FOnlineSessionSearch>();
	PartySearch->MaxSearchResults = 4;
	PartySearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);
	PartySearch->QuerySettings.Set(PartyCodeKey, CleanCode, EOnlineComparisonOp::Equals);
	OnlineAction = EOnlineAction::Search;
	SetStatus(TEXT("Ищем группу…"));
	if (!Sessions->FindSessions(OnlineUser, PartySearch.ToSharedRef()) && OnlineAction == EOnlineAction::Search)
	{
		PartySearch.Reset();
		FinishOnlineAction(TEXT("Не удалось запустить поиск группы."));
	}
}

void UNGConnectionSubsystem::LeaveParty()
{
	if (IsOnlineBusy() || !bInParty) return;
	const auto Sessions = GetSessions();
	if (!Sessions) return;
	OnlineAction = EOnlineAction::Leave;
	SetStatus(TEXT("Выходим из группы…"));
	if (!Sessions->DestroySession(PartySession) && OnlineAction == EOnlineAction::Leave)
		FinishOnlineAction(TEXT("Не удалось запустить выход из группы."));
}

void UNGConnectionSubsystem::PlayParty()
{
	if (!bInParty || IsOnlineBusy()) return;
	auto* World = GetWorld();
	const auto Sessions = GetSessions();
	if (!World || !Sessions) return;
	if (bPartyLeader)
	{
		if (World->GetNetMode() == NM_Client || bPartyMatchReady) return;
		const FString Map = FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap());
		if (Map.IsEmpty()) { SetStatus(TEXT("В проекте не задана игровая карта.")); return; }
		bStartingPartyMatch = true;
		PartyMatchStarted = FPlatformTime::Seconds();
		SetStatus(TEXT("Запускаем миссию…"));
		if (World->GetNetMode() != NM_ListenServer) UGameplayStatics::OpenLevel(this, FName(*Map), true, TEXT("listen"));
		return;
	}
	if (!bPartyMatchReady || World->GetNetMode() != NM_Standalone) return;
	FString ConnectAddress;
	auto* LocalPlayer = OnlinePlayer.Get();
	auto* Controller = LocalPlayer ? LocalPlayer->GetPlayerController(World) : nullptr;
	if (!Controller || !Sessions->GetResolvedConnectString(PartySession, ConnectAddress) || ConnectAddress.IsEmpty())
	{
		SetStatus(TEXT("Адрес миссии пока недоступен."));
		return;
	}
	bConnecting = true;
	bConnectingToParty = true;
	ConnectStarted = FPlatformTime::Seconds();
	SetStatus(TEXT("Подключаемся к миссии группы…"));
	Controller->ClientTravel(ConnectAddress, TRAVEL_Absolute);
}

void UNGConnectionSubsystem::FinishOnlineAction(const FString& Message)
{
	OnlineAction = EOnlineAction::None;
	RefreshParty();
	SetStatus(Message);
}

void UNGConnectionSubsystem::SharePartyProfile(const FString& Message)
{
	const auto Sessions = GetSessions();
	const auto* Session = Sessions ? Sessions->GetNamedSession(PartySession) : nullptr;
	const auto Identity = GetIdentity();
	const auto Id = Identity ? Identity->GetUniquePlayerId(OnlineUser) : nullptr;
	if (Session && Id.IsValid() && !OnlineNickname.IsEmpty())
	{
		FOnlineSessionSettings Settings = Session->SessionSettings;
		Settings.MemberSettings.FindOrAdd(Id.ToSharedRef()).Add(MemberNameKey,
			FOnlineSessionSetting(OnlineNickname, EOnlineDataAdvertisementType::ViaOnlineService));
		OnlineAction = EOnlineAction::ShareProfile;
		SetStatus(Message);
		if (Sessions->UpdateSession(PartySession, Settings) || OnlineAction != EOnlineAction::ShareProfile) return;
	}
	FinishOnlineAction(Message);
}

void UNGConnectionSubsystem::RefreshParty()
{
	const auto Sessions = CreateHandle.IsValid() ? GetSessions() : nullptr;
	const auto* Session = Sessions ? Sessions->GetNamedSession(PartySession) : nullptr;
	const bool bWasInParty = bInParty;
	const bool bWasLeader = bPartyLeader;
	const bool bWasReady = bPartyMatchReady;
	const FString PreviousCode = PartyCode;
	bInParty = Session && Session->SessionInfo.IsValid();
	bPartyLeader = false;
	bPartyMatchReady = false;
	TArray<FNGPartyMember> Members;
	if (bInParty)
	{
		const auto Identity = GetIdentity();
		const auto LocalId = Identity ? Identity->GetUniquePlayerId(OnlineUser) : nullptr;
		const auto* EOS = IOnlineSubsystem::Get(TEXT("EOS"));
		const auto Users = EOS ? EOS->GetUserInterface() : nullptr;
		bPartyLeader = LocalId.IsValid() && Session->OwningUserId.IsValid() && *LocalId == *Session->OwningUserId;
		Session->SessionSettings.Get(PartyCodeKey, PartyCode);
		Session->SessionSettings.Get(MatchReadyKey, bPartyMatchReady);
		for (const auto& Member : Session->SessionSettings.MemberSettings)
		{
			const auto& Id = *Member.Key;
			const bool bLocal = LocalId.IsValid() && *LocalId == Id;
			const bool bLeader = Session->OwningUserId.IsValid() && *Session->OwningUserId == Id;
			FString Name;
			if (const auto* Setting = Member.Value.Find(MemberNameKey)) Setting->Data.GetValue(Name);
			if (Name.IsEmpty() && Users)
				if (const auto Profile = Users->GetUserInfo(OnlineUser, Id)) Name = Profile->GetDisplayName();
			if (Name.IsEmpty() && bLocal) Name = OnlineNickname;
			if (Name.IsEmpty() && bLocal && Identity) Name = Identity->GetPlayerNickname(OnlineUser);
			if (Name.IsEmpty() && bLeader) Name = Session->OwningUserName;
			if (Name.IsEmpty() || Name == Id.ToString()) Name = bLocal ? TEXT("Вы") : TEXT("Участник");
			Members.Add({Id.ToString(), Name.Left(40), bLeader, bLocal});
		}
		Members.Sort([](const FNGPartyMember& A, const FNGPartyMember& B)
		{
			if (A.bLeader != B.bLeader) return A.bLeader;
			return A.Id < B.Id;
		});
	}
	else if (OnlineAction == EOnlineAction::None) PartyCode.Reset();
	if (bInParty != bWasInParty || bPartyLeader != bWasLeader || bPartyMatchReady != bWasReady || PartyCode != PreviousCode || PartyMembers != Members)
	{
		PartyMembers = MoveTemp(Members);
		OnPartyChanged.Broadcast();
	}
}

void UNGConnectionSubsystem::TickParty()
{
	const double Now = FPlatformTime::Seconds();
	if (Now >= NextPartyRefresh)
	{
		NextPartyRefresh = Now + 0.5;
		RefreshParty();
	}
	if (!bStartingPartyMatch || OnlineAction == EOnlineAction::Publish) return;
	if (bPartyLeader && GetWorld() && GetWorld()->GetNetMode() == NM_ListenServer && GetWorld()->GetNetDriver())
	{
		const auto Sessions = GetSessions();
		auto* Session = Sessions ? Sessions->GetNamedSession(PartySession) : nullptr;
		if (!Session) return;
		FOnlineSessionSettings Settings = Session->SessionSettings;
		Settings.Set(MatchReadyKey, true, EOnlineDataAdvertisementType::ViaOnlineService);
		OnlineAction = EOnlineAction::Publish;
		if (!Sessions->UpdateSession(PartySession, Settings) && OnlineAction == EOnlineAction::Publish)
		{
			bStartingPartyMatch = false;
			Session->SessionSettings.Set(MatchReadyKey, false, EOnlineDataAdvertisementType::ViaOnlineService);
			FinishOnlineAction(TEXT("Не удалось опубликовать миссию в группе."));
		}
	}
	else if (Now - PartyMatchStarted > 30.0)
	{
		bStartingPartyMatch = false;
		SetStatus(TEXT("Миссия не запустилась. Проверьте подключение и повторите попытку."));
	}
}
