# OnlineSubsystemSimplyStream Guide

This plugin started as a clone of OnlineSubsystemTencent. Our goal is to match Tencent’s UE `IOnline*` surface while speaking SimplyStream backend APIs.

## 1. Build, Runtime & Environment

- `USE_ONLINESUBSYSTEM` (env var + module define) gates all SimplyStream‑specific code inside MinimalNet. Guard game code with `#if USE_ONLINESUBSYSTEM`.
- The plugin `.uplugin` has `"EnabledByDefault": false`. Do **not** flip this until OSS parity + QA sign‑off.
- Feature toggles:
  - `SIMPLYSTREAM_ENABLE_EXTERNAL_UI` – overlay/portal UI.
  - `SIMPLYSTREAM_ENABLE_CHAT`, `SIMPLYSTREAM_ENABLE_LEADERBOARDS`, etc. – keep disabled unless actively working on the area.
- Backend base URL resolves in this order: config (`/Script/SimplyStreamTargetPlatformSettings` → `BackendURL`), env var `SIMPLYSTREAM_BACKEND_URL`, fallback `https://simplystream.com`.

## 2. Engine References (read‑only)

- UE install: `D:\UnrealEngines\5.6\Engine\` (read only, never modify). Useful folders:
  - `...\Plugins\Online\` – Epic reference implementations.
  - `...\Plugins\Online\OnlineSubsystemTencent` – original source for feature parity checks.
  - `...\Plugins\Online\Android\OnlineSubsystemGooglePlay`, `...\Plugins\Online\IOS\OnlineSubsystemIOS`, etc. – examples for optional interfaces.

## 3. Working Agreements

- **Backend/HTTP:** all calls run through `FHttpModule`, `Subsystem->GetAPIBaseURL()`, and `SimplyStreamIdentity` helpers for auth/local user lookups. Log failures with the relevant `UE_LOG_ONLINE_*` macro.
- **Generated SQL:** edit `backend/db/queries/*.sql` (e.g. `multiplayer_sessions.sql`) and run `bun run db:generate`. Never hand‑edit `backend/src/queries` outputs.
- **Code style:** include owning header first, use UE tabs/PascalCase, wrap unfinished code in obvious logs or `TODO`. All new identifiers use `FUniqueNetIdSimplyStream::Create(...)`.
- **Testing gate:** focus validation on `USE_ONLINESUBSYSTEM=1` (plugin enabled). Baseline/native builds without the subsystem have already been vetted.

## 4. Interface Matrix

| Interface                               | Status / Owner | Notes & Next Steps |
|-----------------------------------------|----------------|--------------------|
| Identity, Sessions, Presence, Friends, Recent Players | **Enabled** | Sessions now support create/find/join/destroy/update/end over HTTP. Presence+friends auto-refresh caches; presence push still TBD. |
| Chat                                    | **Enabled** (polling) | WebSocket delivery deferred; ensure `/api/v1/multiplayer/chat/*` stays stable. |
| Voice                                   | **Stubbed (logging only)** | Uses channel/token HTTP helpers; still needs backend voice service + audio bridge. |
| Store V2 / Purchase / Stats / Achievements | **Enabled** | Purchase interface piggybacks on shared identity helpers; achievements wrap `/api/v1/multiplayer/achievements`. |
| Leaderboards                            | **Enabled** | Full read/write implementation via `/api/v1/multiplayer/leaderboards/{id}` endpoints. |
| Entitlements                            | **Enabled** | Maps inventory items to UE entitlements format via `/api/v1/multiplayer/player/entitlements`. |
| Sharing                                 | **Enabled** | Full social sharing implementation via `/api/v1/multiplayer/player/sharing/*` endpoints; supports screenshots, videos, achievements, scores, match results, and social media platform configuration. |
| External UI                             | **Enabled** | Full overlay system via `/api/v1/multiplayer/player/external-ui/*` endpoints; supports login, friends, achievements, leaderboards, store, profile UIs. |
| User                                    | **Enabled** | User profile querying and caching via `/api/v1/multiplayer/player/users/bulk` endpoint; supports bulk user lookups for enriching friends/leaderboards with profile data. QueryUserIdMapping implemented via `/api/v1/multiplayer/player/users/search` with case-insensitive username/display name/email search. |
| Register/Unregister Players (sessions)  | **Stubs** | Logged only; backend join/leave already records members, but UE delegates should reflect state. |
| Events                                  | **Enabled** | Telemetry and analytics event tracking via `/api/v1/multiplayer/player/events` endpoint; supports TriggerEvent with FVariantData serialization and SetPlayerSessionId for session association. |
| Party, Groups, Cloud (shared/user), TitleFile, Message, Time | **Not instantiated** | Require new backend services plus UE wrappers (see Agent A/B sections). |

> Goal: minimize `nullptr` returns in `OnlineSubsystemSimplyStream.cpp`. Whenever an interface has a functional implementation (even behind a macro), prefer wiring it up so teams can opt‑in via config rather than touching engine code.

## 5. Agent Workstreams

### Agent A – Backend plumbing & service enablement
1. **Presence fan‑out / push** – define polling or WebSocket topic to keep friend presence fresh without manual queries.
2. **Session polish** – propagate backend error payloads to delegates, add invite/friends-only gating helpers, keep presence caches synced when ownership changes.
3. **Register/Unregister hooks** – wire UE registration delegates to backend join/leave so client sees accurate local membership state.
4. **Friends filters** – pass backend `status/search/includePresence/limit` parameters instead of client-side filtering, add pagination.
5. **New interfaces currently returning `nullptr`:**
   - `IOnlineParty`, `IOnlineGroups` – spec lobby/group APIs plus persistence, then enable once backend endpoints exist.
   - `IOnlineSharedCloud`, `IOnlineUserCloud`, `IOnlineTitleFile` – wrap SimplyStream storage service for title/user blobs.
6. **Backend coordination** – document every new route/environment variable and update `docs/` or this file.

**Upcoming auth refactor:** Dedicated servers currently fabricate their OSS user account from `SERVER_ID`/`PROJECT_ID`/`API_KEY` env vars. Once the backend issues short‑lived server tokens, we plan to switch `FOnlineIdentitySimplyStream::InitDedicatedServerLogin` back to requesting `/internal/v1/multiplayer/servers/{id}/token`, validate the response, and store that scoped credential on the `FUserOnlineAccount`. The backend/multiplayer-runtime will need matching changes before we flip that path back on.

### Agent B – Client UX, gating, and polish
1. ~~**Leaderboards**~~ – ✅ Enabled; full read/write implementation active.
2. ~~**Sharing**~~ – ✅ Enabled; full sharing implementation active with API paths corrected, supports multiple content types and platform configuration.
3. ~~**External UI & invites**~~ – ✅ Enabled; overlay system active with fixed endpoint paths, supports all major UI types.
4. ~~**Entitlements**~~ – ✅ Enabled; maps inventory to UE entitlements via `/api/v1/multiplayer/player/entitlements`.
5. ~~**IOnlineUser**~~ – ✅ Enabled; user profile querying, caching, and search active. QueryUserInfo supports bulk lookups, QueryUserIdMapping supports username/display name/email search with case-insensitive matching.
6. ~~**IOnlineEvents**~~ – ✅ Enabled; telemetry and analytics event tracking active. TriggerEvent supports all FVariantData types (int, float, string, bool, blob, JSON), SetPlayerSessionId associates events with session GUIDs, fire-and-forget HTTP delivery.
7. **IOnlineMessage / IOnlineTime** – design UE wrappers, hook up SimplyStream inbox/time APIs, and expose filters needed by UI.
8. **TitleFile consumer side** – once Agent A exposes storage APIs, build download/cache logic for MOTDs or config blobs.
9. **Voice improvements** – coordinate with backend voice team on token/channels; ensure current stub logs state transitions for easier QA.
10. **Automation** – keep the MinimalNetTests suite up to date (chat/presence/friends) and extend with failure cases (timeouts, malformed responses).

## 6. Testing & Validation (quick ref)

| Command | Purpose |
|---------|---------|
| `WasmServer\MinimalNet\build_server.bat` | Full build & package with `USE_ONLINESUBSYSTEM=1`. |
| `build_test.bat [chat|presence|friends]` | Build + run OSS automation for the selected area. |
| `run_tests.bat [chat|presence|friends]` | Run previously built automation. |
| `UnrealEditor-Cmd MinimalNet -ExecCmds="Automation RunTests OnlineSubsystem.*"` | Manual spot tests. |

Test prerequisites: backend reachable, guest auth permitted, async calls allow 10‑15 s timeouts. Log any stubbed code so QA knows the feature is incomplete.

## 7. Communication

- Update this guide when toggles, helpers, or workflows change.
- Describe new backend endpoints (method, path, auth scope, sample payload) in PRs and/or `docs/`.
- When removing Tencent code, state the SimplyStream replacement or rationale.
- Keep commit messages descriptive (e.g., `feat(sessions): add update endpoint`) so other agents can follow the trail.
