// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
Unreal WebTransport network driver.
=============================================================================*/

#include "WebTransportNetDriver.h"
#include "HAL/IConsoleManager.h"


#include "Engine/NetworkDelegates.h"
#include "WebTransportConnection.h"
#include "WebTransportServer.h"
#include "WebTransport.h"

#include "Misc/CommandLine.h"
#include "Containers/Ticker.h"
#include "PacketHandler.h"
#include "SocketSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include <emscripten/emscripten.h>
#include <emscripten/em_asm.h>
#include <emscripten/threading.h>
#include "../../../../../../Source/ThirdParty/wasm_utils/wndr_probe.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(WebTransportNetDriver)

/*-----------------------------------------------------------------------------
Declarations.
-----------------------------------------------------------------------------*/

/** Size of the network recv buffer */
#define NETWORK_MAX_PACKET (576)

// WebTransport->WebSocket runtime fallback state (declared extern in WebTransportNetworkingPrivate.h so the
// plugin module's OnNetworkFailure handler can re-travel). Set on the client when a session fails; flips
// IsAvailable() to false so the re-travel selects the WebSocket fallback. Never set on the server.
bool GWebTransportSessionFailed = false;
FString GWebTransportRetryURL;
TWeakObjectPtr<UWorld> GWebTransportRetryWorld;

// How long to wait for the browser WebTransport session to open before giving up and falling back to WebSocket.
static constexpr double WEBTRANSPORT_CONNECT_TIMEOUT_SECONDS = 5.0;

// Feature-detect WebTransport availability for THIS runtime, covering both roles:
//  - browser client: the global `WebTransport` constructor exists.
//  - dedicated server (Node): the runner's `init_wt` bridge function exists.
// MAIN_THREAD_EM_ASM (not plain EM_ASM): init_wt is a main-thread-only global supplied by the Node runner
// (the bridge wraps it in MAIN_THREAD_EM_ASM too), and IsAvailable() may be evaluated off the main thread.
// (CDO-safe: no instance state touched.)
static bool WebTransportRuntimeSupported()
{
	return MAIN_THREAD_EM_ASM_INT({
		return ((typeof WebTransport !== 'undefined') || (typeof init_wt === 'function')) ? 1 : 0;
	}) != 0;
}

UWebTransportNetDriver::UWebTransportNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// 2026-07-05: WebTransport DISABLED BY DEFAULT (kill switch, opt back in with
// `wndr.WebTransportEnabled 1`). The mp runtime exposes no WT listener, so every join
// burned a failed WT connect + teardown + WS re-travel — an aborted-first-travel sequence
// implicated in the multiplayer join stalls. Straight-to-WebSocket until the server side
// exists and the fallback path is proven safe.
static TAutoConsoleVariable<int32> CVarWndrWebTransportEnabled(
	TEXT("wndr.WebTransportEnabled"),
	0,
	TEXT("0 (default): never select the WebTransport net driver (straight to WebSocket). 1: allow WebTransport with WebSocket fallback."),
	ECVF_Default);

bool UWebTransportNetDriver::IsAvailable() const
{
	// The engine resolves DriverClassNameFallback synchronously here (UnrealEngine.cpp CreateNetDriver_Local):
	// returning false makes it pick the WebSocket fallback driver. So this is BOTH the up-front browser-support
	// gate AND (via GWebTransportSessionFailed, set after an async WebTransport connect failure + a re-travel)
	// the runtime fallback.
	if (CVarWndrWebTransportEnabled.GetValueOnAnyThread() == 0)
	{
		return false;
	}
	return WebTransportRuntimeSupported() && !GWebTransportSessionFailed;
}

ISocketSubsystem* UWebTransportNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get();
}

bool UWebTransportNetDriver::InitBase(bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error)
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	return true;
}

bool UWebTransportNetDriver::InitConnect(FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error)
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_WT_INIT_CONNECT);
	if (!InitBase(true, InNotify, ConnectURL, false, Error))
	{
		return false;
	}

	// Create new connection.
	ServerConnection = NewObject<UWebTransportConnection>(NetConnectionClass);

	TSharedRef<FInternetAddr> InternetAddr = GetSocketSubsystem()->CreateInternetAddr();
	bool Ok;
	InternetAddr->SetIp(*ConnectURL.Host, Ok);
	// Honor an explicit port on the travel URL (a session's advertised host:port endpoint). FURL can't flag
	// "explicit", so treat any port other than the engine's default-URL port as explicit; a bare-host travel
	// keeps the configured WebTransportPort (which matches the server's InitListen default).
	const int32 ExplicitPort = (ConnectURL.Port > 0 && ConnectURL.Port != FURL::UrlConfig.DefaultPort) ? ConnectURL.Port : WebTransportPort;
	InternetAddr->SetPort(ExplicitPort);

	// Pass the hostname too: a domain travel target can't SetIp into InternetAddr (stays 0.0.0.0) and the
	// WebTransport dial needs the cert-matched hostname anyway.
	FWebTransport* WebTransport = new FWebTransport(*InternetAddr, ConnectURL.Host);
	UWebTransportConnection* Connection = (UWebTransportConnection*)ServerConnection;
	Connection->SetWebTransport(WebTransport);

	FWebTransportPacketReceivedCallBack CallBack;
	CallBack.BindUObject(Connection, &UWebTransportConnection::ReceivedRawPacket);
	WebTransport->SetReceiveCallBack(CallBack);

	FWebTransportInfoCallBack  ConnectedCallBack;
	ConnectedCallBack.BindUObject(this, &UWebTransportNetDriver::OnWebTransportServerConnected);
	WebTransport->SetConnectedCallBack(ConnectedCallBack);

	ServerConnection->InitLocalConnection(this, NULL, ConnectURL, USOCK_Pending);

	// Create channel zero.
	CreateInitialClientChannels();

	// Arm the WebTransport connect deadline; TickDispatch falls back to WebSocket if the session hasn't
	// opened (or errored) by then. The browser WebTransport handshake is async, so InitConnect can't know
	// the result yet — and the engine's DriverClassNameFallback only fires at driver-creation, not here.
	WebTransportConnectDeadline = FPlatformTime::Seconds() + WEBTRANSPORT_CONNECT_TIMEOUT_SECONDS;
	bWebTransportFellBack = false;
	// Remember where to re-travel if WebTransport fails (the plugin module's OnNetworkFailure handler uses it).
	WebTransportPendingURL = ConnectURL.ToString();

	return true;
}

bool UWebTransportNetDriver::InitListen(FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error)
{
	if (!InitBase(false, InNotify, LocalURL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	InitConnectionlessHandler();

	WebTransportServer = new class FWebTransportServer();

	FWebTransportClientConnectedCallBack CallBack;
	CallBack.BindUObject(this, &UWebTransportNetDriver::OnWebTransportClientConnected);

	if(!WebTransportServer->Init(WebTransportPort, CallBack))
		return false;

	WebTransportServer->Tick();
	LocalURL.Port = WebTransportPort;
	LocalAddr = GetSocketSubsystem()->GetLocalBindAddr(*GLog);
	LocalAddr->SetPort(WebTransportPort);
	UE_LOGF(LogWebTransportNetworking, Log, "%ls WebTransportNetDriver listening on port %i", *GetDescription(), LocalURL.Port);

	// server has no server connection.
	ServerConnection = NULL;
	return true;
}

void UWebTransportNetDriver::TickDispatch(float DeltaTime)
{
	Super::TickDispatch(DeltaTime);

	if (WebTransportServer)
		WebTransportServer->Tick();

	// Client-side WebTransport->WebSocket fallback: if the browser session errors or doesn't open before the
	// deadline, flag WebTransport unavailable and schedule a re-travel (now over WebSocket —
	// IsAvailable() will return false on the retry). Keep checking HasErrored() after the session opens too:
	// the JS reader/callback path can fail after ready, and otherwise Lyra sits in PendingNetGame until the
	// generic 120s net timeout.
	if (!WebTransportServer && ServerConnection && !bWebTransportFellBack)
	{
		UWebTransportConnection* Conn = GetServerConnection();
		FWebTransport* WT = Conn ? static_cast<FWebTransport*>(Conn->GetWebTransport()) : nullptr;
		const bool bErrored = WT && WT->HasErrored();
		const bool bTimedOut = WT && WebTransportConnectDeadline > 0.0 && !WT->IsConnected() && FPlatformTime::Seconds() > WebTransportConnectDeadline;
		if (WT && WT->IsConnected() && WebTransportConnectDeadline > 0.0)
		{
			WebTransportConnectDeadline = 0.0; // opened — stop checking
		}
		if (WT && (bErrored || bTimedOut))
		{
			WndrMarkNetTravelStage(WNDR_NET_STAGE_WT_FALLBACK_SCHEDULED);
			bWebTransportFellBack = true;
			GWebTransportSessionFailed = true;            // -> IsAvailable() false on the re-travel
#if defined(__EMSCRIPTEN__) // symbol lives in wasm_utils (client wasm only)
			{
				extern double GWndrNetFallbackGraceUntil; // SimplyStreamPlatform.h — session-layer leave suppression
				GWndrNetFallbackGraceUntil = FPlatformTime::Seconds() + 30.0;
			}
#endif
			UE_LOGF(LogWebTransportNetworking, Warning,
				"WebTransport connect %ls; flagging unavailable and falling back to WebSocket (%ls)",
				bErrored ? TEXT("failed") : TEXT("timed out"), *WebTransportPendingURL);
			// The driver OWNS the WebSocket re-travel: schedule it here, slightly deferred. Rationale:
			// (a) an OnNetworkFailure-handler re-travel proved unreliable — the module's PostEngineInit
			// binding never fired on the wasm client, so no fallback ever ran; (b) an immediate
			// SetClientTravel can be clobbered by synchronous teardown of this driver.
			// A short-delay one-shot ticker (captures only the URL string) outlives this driver's teardown;
			// IsAvailable() is already false so the re-travel selects WebSocket, and its Browse cancels
			// this pending net game.
			{
				const FString RetryUrl = WebTransportPendingURL;
				FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([RetryUrl](float) -> bool
				{
					if (GEngine)
					{
						for (const FWorldContext& Context : GEngine->GetWorldContexts())
						{
							if (Context.WorldType == EWorldType::Game && Context.World())
							{
								UE_LOG(LogWebTransportNetworking, Warning, TEXT("Re-traveling over WebSocket to %s"), *RetryUrl);
								GEngine->SetClientTravel(Context.World(), *RetryUrl, TRAVEL_Absolute);
								break;
							}
						}
					}
					return false; // one-shot
				}), 0.5f);
			}
			// Deliberately NO BroadcastNetworkFailure here: the fallback is recoverable, and the
			// broadcast triggers the game's failure handling (Lyra: return-to-entry + session
			// destroy) which races the deferred WS re-travel. The re-travel's Browse tears down
			// this pending connection; genuinely unrecoverable failures still broadcast via the
			// normal connection-timeout path.
			UE_LOGF(LogWebTransportNetworking, Warning,
				"WebTransport fallback scheduled; suppressing network-failure broadcast (re-travel will supersede this pending connection)");
		}
	}
}

void UWebTransportNetDriver::LowLevelSend(TSharedPtr<const FInternetAddr> Address, void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	if (Address.IsValid() && Address->IsValid())
	{
		const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

		if (ConnectionlessHandler.IsValid())
		{
			const ProcessedPacket ProcessedData =
					ConnectionlessHandler->OutgoingConnectionless(Address, (uint8*)DataToSend, CountBits, Traits);

			if (!ProcessedData.bError)
			{
				DataToSend = ProcessedData.Data;
				CountBits = ProcessedData.CountBits;
			}
			else
			{
				CountBits = 0;
			}
		}


		// connectionless webtransports do not exist (yet)
		// scan though existing connections
		if (CountBits > 0)
		{
			for (int32 i = 0; i<ClientConnections.Num(); ++i)
			{
				UWebTransportConnection* Connection = (UWebTransportConnection*)ClientConnections[i];
				if (Connection && ( Connection->LowLevelGetRemoteAddress(true) == Address->ToString(true) ) )
				{
					Connection->GetWebTransport()->Send((uint8*)DataToSend, FMath::DivideAndRoundUp(CountBits, 8));
					break;
				}
			}
		}
	}
	else
	{
		UE_LOGF(LogNet, Warning, "UWebTransportNetDriver::LowLevelSend: Invalid send address '%ls'", *Address->ToString(false));
	}
}

FString UWebTransportNetDriver::LowLevelGetNetworkNumber()
{
	return WebTransportServer != nullptr ? WebTransportServer->Info() : FString(TEXT(""));
}

void UWebTransportNetDriver::LowLevelDestroy()
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_WT_DESTROY);
	Super::LowLevelDestroy();
	delete WebTransportServer;
	WebTransportServer = nullptr;
}

bool UWebTransportNetDriver::HandleSocketsCommand(const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld)
{
	Ar.Logf(TEXT(""));
	if (WebTransportServer != NULL)
	{
		Ar.Logf(TEXT("Running WebTransport Server %s"), *WebTransportServer->Info());
	}
	else
	{
		check(GetServerConnection());
		Ar.Logf(TEXT("WebTransport client's EndPoint %s"), *(GetServerConnection()->WebTransport->RemoteEndPoint(true)));
	}
	return UNetDriver::Exec(InWorld, TEXT("SOCKETS"), Ar);
}

#if UE_ALLOW_EXEC_COMMANDS
bool UWebTransportNetDriver::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("SOCKETS")))
	{
		return HandleSocketsCommand(Cmd, Ar, InWorld);
	}
	return UNetDriver::Exec(InWorld, Cmd, Ar);
}
#endif // UE_ALLOW_EXEC_COMMANDS

UWebTransportConnection* UWebTransportNetDriver::GetServerConnection()
{
	return (UWebTransportConnection*)ServerConnection;
}

void UWebTransportNetDriver::OnWebTransportClientConnected(INetworkingWebTransport* ClientWebTransport)
{
	// Determine if allowing for client/server connections
	const bool bAcceptingConnection = Notify != nullptr && Notify->NotifyAcceptingConnection() == EAcceptConnection::Accept;
	if (bAcceptingConnection)
	{

		UWebTransportConnection* Connection = NewObject<UWebTransportConnection>(NetConnectionClass);
		check(Connection);

		TSharedRef<FInternetAddr> InternetAddr = GetSocketSubsystem()->CreateInternetAddr();
		int32 AddressPort = 0;

		InternetAddr->SetRawIp(ClientWebTransport->GetRawRemoteAddr(AddressPort));
		InternetAddr->SetPort(0);
		Connection->SetWebTransport(ClientWebTransport);
		Connection->InitRemoteConnection(this, NULL, FURL(), *InternetAddr, USOCK_Open);
		
		Notify->NotifyAcceptedConnection(Connection);

		AddClientConnection(Connection);

		FWebTransportPacketReceivedCallBack CallBack;
		CallBack.BindUObject(Connection, &UWebTransportConnection::ReceivedRawPacket);
		if (ConnectionlessHandler.IsValid() && StatelessConnectComponent.IsValid())
		{
			Connection->bChallengeHandshake = true;
		}
#if !UE_BUILD_SHIPPING
		else if (FParse::Param(FCommandLine::Get(), TEXT("NoPacketHandler")))
		{
			UE_LOGF(LogNet, Log, "Accepting connection without handshake, due to '-NoPacketHandler'.")
		}
#endif
		else
		{
			UE_LOGF(LogNet, Log,
					"Invalid ConnectionlessHandler (%i) or StatelessConnectComponent (%i); can't accept connections.",
					(int32)(ConnectionlessHandler.IsValid()), (int32)(StatelessConnectComponent.IsValid()));
		}
		ClientWebTransport->SetReceiveCallBack(CallBack);

		UE_LOGF(LogWebTransportNetworking, Log, " WebTransport server running on %ls Accepted Connection from %ls ", *WebTransportServer->Info(),*ClientWebTransport->RemoteEndPoint(true));
	}
}

bool UWebTransportNetDriver::IsNetResourceValid(void)
{
	if (	(WebTransportServer && !ServerConnection)//  Server
		||	(!WebTransportServer && ServerConnection) // client
		)
	{
		return true;
	}
		
	return false;
}

// Just logging, not yet attached to html5 clients.
void UWebTransportNetDriver::OnWebTransportServerConnected()
{
	WndrMarkNetTravelStage(WNDR_NET_STAGE_WT_CLIENT_OPEN);
	check(GetServerConnection());
	UE_LOGF(LogWebTransportNetworking, Log, " %ls WebTransport Client %ls connected to server %ls ", *GetDescription(),
		*GetServerConnection()->WebTransport->LocalEndPoint(true),
		*GetServerConnection()->WebTransport->RemoteEndPoint(true));
}
