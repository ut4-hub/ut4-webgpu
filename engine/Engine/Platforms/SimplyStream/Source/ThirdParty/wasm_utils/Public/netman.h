#pragma once
//move this outside html5
#include <functional>
#include "archive.h"

#include <chrono>
#include <filesystem>
#define ENABLE_WSS 0
#if ENABLE_WSS
#ifdef _WIN32
#define ASIO_STANDALONE 1
#include "RCS/client_ws.hpp"
#include "RCS/client_wss.hpp"
//using WssServer = SimpleWeb::SocketServer<SimpleWeb::WSS>;
using WssClient = SimpleWeb::SocketClient<SimpleWeb::WSS>;
#elif defined(__EMSCRIPTEN__)
#include <emscripten/websocket.h>
#endif
#endif

enum class ThreadStatus : uint8_t {
	IDLE,
	PRUNE,
	PRUNE_WAIT,
	DECOMPRESSING,
	FETCHING,
	READING,
	WAIT_ON_DECOMPRESS,
	WAIT_ON_RESETING,
	WAIT_ON_FETCHING,
	WAIT_ON_READING,
};

struct Session {
	struct Capabilities {
		bool webgl = false;
		int webgpu = 0; // 0 = no, >0 = version
		bool offscreenCanvas = false;
		bool sharedArrayBuffer = false;
		bool webRTC = false;
		bool h264 = false;
		bool vp9 = false;
		bool av1 = false;
		bool webTransport = false;
		bool webCodecs = false;
		uint64_t bench_cpu = 0;
		uint64_t bench_gpu = 0;
		uint64_t memory_cpu = 0;
		uint64_t memory_gpu = 0;
	};
	std::string session_id;
	std::string projectId;
	std::string rev;
	uint32_t RAM = 0;      // TODO - Get Max Memory Usage
	uint32_t CPU = 0;      // RawGameThreadTime
	uint32_t GPU_MEM = 0;  // TODO - Get Max GPU Memory Usage
	uint32_t GPU_SPD = 0;  // GAverageMS
	uint32_t STORAGE = 0;  //?
	uint64_t STARTUP_0 = 0;
	uint64_t STARTUP_1 = 0;
	uint64_t TOTAL_TIME = 0;
	uint64_t ASSET_C = 0;
	uint64_t ASSET_S = 0;
	Capabilities capabilities;
	void PushAsset(datahash asset);
	void SendSession();
};

enum Status : uint8_t {
	NOT_DOWNLOADED = 0,
	FETCHING = 1,
	DOWNLOADED = 2,
	DECOMPRESSING = 3,
	DECOMPRESSED = 4,
	DECOMPRESS_FAILED = 5,
};
void SetStatus(datahash hash, Status status);
Status GetStatus(datahash hash);

namespace fs = std::filesystem;
using namespace std::chrono_literals;
//using namespace concurrencpp;
extern struct FS _fs;

enum class NET_TYPE {
	ANY, WEBSOCKET, WEBTRANSPORT, DIRECT
};

enum MESSAGE_TYPE : uint8_t {
	UNKNOWN_ASSET,
	ASSET,
	GAME_INFO,
	GAME_DEBUG,
	TRACE
};

struct Message {
	bool wants_reply;
	MESSAGE_TYPE type;
	std::string msg;
};
struct Client {
	uint8_t id = 0;
	NET_TYPE conType = NET_TYPE::WEBSOCKET;
#if ENABLE_WSS
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_WEBSOCKET_T ws;
#else
	std::shared_ptr<WssClient::Connection> ws;
#endif
#endif
};

struct Server {
	uint8_t id = 0;
	NET_TYPE conType = NET_TYPE::WEBSOCKET;
#if ENABLE_WSS
#ifdef __EMSCRIPTEN__
	EMSCRIPTEN_WEBSOCKET_T ws;
#else
	std::shared_ptr<WssClient::Connection> ws;
#endif
#endif
};

struct NetManServer {
	uint8_t ids = 1;
#if ENABLE_WSS
	HashT<EMSCRIPTEN_WEBSOCKET_T, Client> ws_to_client;
#endif
	std::function<void(Client& client)> onConnect;
	std::function<void(Client& client, MESSAGE_TYPE type, uint8_t msg_id, Archive& msg)> onMsg;
	uint8_t send(Client& server, const std::string& msg);
	void send(Client& server, uint8_t id, const std::string& msg);
	void listen(NET_TYPE type, std::string_view addr, uint16_t port, bool blocking);
};
struct Message2 {
#if ENABLE_WSS
	EMSCRIPTEN_WEBSOCKET_T socket;
#endif
    std::string data;
};
enum class Endpoint {
	Cloudflare, AWS
};
struct NetManClient {
	const static uint8_t max_sockets = 8;
	const static uint8_t max_messages = 50;
	Endpoint endpoints[max_sockets];
#if ENABLE_WSS && defined(__EMSCRIPTEN__)
	std::atomic<uint8_t> selected = {0};
	EMSCRIPTEN_WEBSOCKET_T ws[max_sockets];
#endif
	std::atomic<uint8_t> sent[max_sockets] = {0};
	uint8_t ids = 1;
#if ENABLE_WSS
	HashT<EMSCRIPTEN_WEBSOCKET_T, Server> ws_to_server;
	HashT<Server*, EMSCRIPTEN_WEBSOCKET_T> server_to_ws;
#endif
	std::function<void(Server& server)> onConnect;
	std::function<void(Server& server, MESSAGE_TYPE type, uint8_t msg_id, Archive& msg)> onMsg;
	void send(Server& server, const std::string& msg);
	void sendText(Server& server, const std::string&& msg);
	void connect(NET_TYPE type, uint16_t port);
	void disconnect();
	void reset();
};

#include "cache_client.h"
enum class FetchType {
	None, Asyncify, AccessHandles
};
struct FS {
	bool dirty = false;
	FetchType fetchType = FetchType::AccessHandles;
	Cache cache;
	GameInfo info;
	GameInfoDebug debug;
	HashT<datahash, std::vector<datahash>> optmap;
	bool FetchGameInfo(const std::string& project_id, const std::string& rev);
	bool FetchGameDebug(const std::string& project_id, const std::string& rev);
	bool Exists(const std::string& fn);
	uint32_t Size(asset_index fn);
	uint32_t Size(const std::string& fn);
	bool Fetch(const fnhash hash, bool include_deps = false);
	fnhash GetFnHash(const std::string& fn);
	asset_index GetAssetIndex(const std::string& fn);
	bool Get(asset_index hash, View*& data, bool include_deps = false);
	bool Get(const std::string& fn, View*& data, bool include_deps = false);
	bool Put(const datahash hash, const std::string_view data);
	bool Put(const std::string& fn, const std::string_view data);
	bool Contains(const fnhash hash);
	bool Touch(asset_index hash);
	void SetGameInfo(Archive& msg, const std::string& id);
	void SetGameDebug(Archive& msg, const std::string& id);
	void IncreaseRef(datahash crc);
	void DecreaseRef(datahash crc);
	std::vector<datahash> GetDeps(const datahash hash);
	std::vector<int> GetStatus(const std::vector<datahash>& hashes);
};