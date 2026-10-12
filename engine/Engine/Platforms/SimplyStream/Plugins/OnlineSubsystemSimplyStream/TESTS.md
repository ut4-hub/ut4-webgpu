### Running Tests

#### Using Test Scripts (Recommended)

**Build and run all tests:**
```bash
cd WasmServer\MinimalNet
build_test.bat
```

**Build and run specific test category:**
```bash
build_test.bat chat      # Run only chat tests
build_test.bat presence  # Run only presence tests
build_test.bat friends   # Run only friends tests
```

**Run tests without rebuilding:**
```bash
run_tests.bat           # All tests
run_tests.bat chat      # Chat tests only
run_tests.bat presence  # Presence tests only
run_tests.bat friends   # Friends tests only
```

#### Manual Testing Commands

```bash
# Run all OnlineSubsystem tests
UnrealEditor-Cmd MinimalNet -ExecCmds="Automation RunTests OnlineSubsystem"

# Run specific test category
UnrealEditor-Cmd MinimalNet -ExecCmds="Automation RunTests OnlineSubsystem.Chat"
UnrealEditor-Cmd MinimalNet -ExecCmds="Automation RunTests OnlineSubsystem.Presence"
UnrealEditor-Cmd MinimalNet -ExecCmds="Automation RunTests OnlineSubsystem.Friends"

# Run individual test
UnrealEditor-Cmd MinimalNet -ExecCmds="Automation RunTests OnlineSubsystem.Chat.CreateRoom"
```