# OnlineSubsystemSimplyStream Conversion Plan

This document outlines the steps required to convert the placeholder `OnlineSubsystemTencent` plugin into a functional `OnlineSubsystemSimplyStream`.

## 1. Rename Files and Classes

-   Rename all files from `...Tencent...` to `...SimplyStream...`.
-   Perform a find-and-replace within the files to change all class names, prefixes, and defines from `TENCENT` to `SIMPLYSTREAM`.

## 2. Update Core Subsystem Files

-   **`OnlineSubsystemSimplyStream.h` / `.cpp`**:
    -   Remove all Tencent-specific logic (RailSDK, AAS dialogs, etc.).
    -   Simplify the `Init`, `Shutdown`, and `Tick` functions.
    -   Keep the core interface pointers (`SessionInterface`, `IdentityInterface`, etc.).
-   **`OnlineSubsystemSimplyStream.Build.cs`**:
    -   Update module names and definitions.
    -   Remove dependencies on Tencent-specific third-party libraries.
-   **`OnlineSubsystemSimplyStream.uplugin`**:
    -   Update the plugin's `FriendlyName`, `Description`, and module name.

## 3. Implement `IOnlineIdentity`

-   **`OnlineIdentitySimplyStream.h` / `.cpp`**:
    -   This will be a lightweight implementation.
    -   The `Login` function will simply create a local `FUserOnlineAccount` with a hardcoded `FUniqueNetId`.
    -   `GetUniquePlayerId` will return this hardcoded ID. This is sufficient for a non-platform-backed identity.

## 4. Implement `IOnlineSession`

This is the most critical part, connecting Unreal's session management to the SimplyStream API.

-   **`OnlineSessionInterfaceSimplyStream.h` / `.cpp`**:
    -   **`CreateSession`**: Will call `POST /api/v1/multiplayer/matches` using the `ConfigId` from the session settings. It will store the returned `MatchId` in the `FNamedOnlineSession`.
    -   **`FindSessions`**: Will call `POST /api/v1/projects/{projectId}/multiplayer/find-server` using the `Region` from the search settings. It will parse the IP/Port from the response and create an `FOnlineSessionSearchResult` with a custom `FOnlineSessionInfoSimplyStream` that holds the connection string.
    -   **`JoinSession`**: Will retrieve the connection string from the search result and trigger the `OnJoinSessionComplete` delegate. The engine's `NetDriver` will then use this to connect.
    -   **`RegisterPlayer` / `UnregisterPlayer`**: Will call the corresponding `.../matches/{matchId}/players` endpoints.
    -   **`DestroySession`**: Will call `POST .../matches/{matchId}/end`.

## 5. Implement Social Features (`IOnlineFriends`, `IOnlinePresence`)

Instead of removing these, we will adapt them to use new SimplyStream API endpoints.

-   **Backend**:
    -   Create new tables: `friends`, `friend_invites`, and `user_presence`.
    -   Create new API endpoints for reading friends lists, sending/accepting invites, and querying/updating user presence.
-   **`OnlineFriendsSimplyStream.h` / `.cpp`**:
    -   Implement `ReadFriendsList` to call the new "list friends" endpoint.
    -   Implement `SendInvite`, `AcceptInvite`, and `RejectInvite` to manage friend requests through the API.
-   **`OnlinePresenceSimplyStream.h` / `.cpp`**:
    -   Implement `SetPresence` to update a user's status (e.g., "In Lobby", "In Match") via an API call. The presence data will be stored as a JSONB object.
    -   Implement `QueryPresence` to fetch the presence status for other users.

## 6. Implement Player Moderation and Safety (`IOnlineMessageSanitizer`, `IOnlineUser::QueryBlockedPlayers`)

-   **Backend**:
    -   Create a new table `blocked_users`.
    -   Implement API endpoints to block/unblock users and query the block list.
    -   Implement a "sanitize message" endpoint that can be used for text chat filtering (this could integrate with a third-party service).
-   **`OnlineMessageSanitizerSimplyStream.h` / `.cpp`**:
    -   Implement `SanitizeDisplayName` and `SanitizeDisplayNames` to call the new message sanitizer endpoint.
-   **`OnlineFriendsSimplyStream.cpp`**:
    -   Implement `QueryBlockedPlayers` to fetch the block list for a user.

## 7. Implement Play Time Limits (`IOnlinePlayTimeLimit`)

-   **Backend**:
    -   Add a `play_time_limits` table to store rules (e.g., daily limits for certain regions).
    -   Add `total_play_time_today` to the `users` table.
    -   Create an endpoint for the game server to periodically report a player's session time.
-   **`OnlinePlayTimeLimitSimplyStream.h` / `.cpp`**:
    -   This interface will periodically call the "report play time" endpoint.
    -   It will also implement logic to check if a player has exceeded their limit and trigger the appropriate delegates.

## 8. Update Helper Files

-   **`OnlineSubsystemSimplyStreamTypes.h`**:
    -   Rename `FUniqueNetIdRail` to `FUniqueNetIdSimplyStream`. This will be a simple string-based unique ID.
    -   Update `FOnlineSessionInfoSimplyStream` to hold the IP-based connection string.
-   **`OnlineAsyncTaskManagerSimplyStream.h` / `.cpp`**:
    -   Remove all RailSDK event handling. The manager will be simplified to just queue standard HTTP-based async tasks.

By following this plan, we will have a feature-rich Online Subsystem that correctly implements Unreal Engine's session management, social features, and player safety on top of the SimplyStream API.
