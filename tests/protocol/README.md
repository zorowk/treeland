# Treeland Protocol Testing Guide

## Architecture

```
cases/*.json              (self-contained — declares xml_path + module)
  │
  ├── CMake globs JSONs → wayland-scanner stubs →
  │   gen-test-client C client → json-controller test binary
  │
  └── json-controller (QApplication + WServer + nested QEventLoop)
        reads {tests}, spawns gen-client, diffs EXPECT output
```

No per-protocol CMake rules. Adding a protocol test = one JSON file.

## File Layout

```
tests/protocol/
├── gen-test-client/          # Code generator (reads XML → C via template)
│   ├── main.cpp
│   ├── template.c.in
│   └── CMakeLists.txt
├── json-controller.cpp       # Per-protocol test runner (QApplication + WServer)
├── gencliente2etest.cpp      # Echo server E2E harness
├── CMakeLists.txt            # add_gen_client_test(), JSON-driven auto-generator
├── fixtures/                 # 6 self-contained test protocol XMLs
├── cases/                    # 21 JSON test cases (self-contained format)
├── *echoserver.c             # Echo servers for fixture protocols
└── lsan-suppressions.txt
```

## JSON Test Case Format (self-contained)

```json
{
  "protocol": "treeland_window_management_v1",
  "xml_path": "/usr/share/treeland-protocols/treeland-window-management-v1.xml",
  "module": {
    "class": "WindowManagementInterfaceV1",
    "header": "windowmanagementinterfacev1.h",
    "dir": "window-management"
  },
  "tests": [
    {
      "name": "set-desktop-show",
      "steps": [
        { "type": "request", "name": "set_desktop", "args": [1] },
        { "type": "roundtrip" },
        { "type": "checkpoint" }
      ],
      "expected": {
        "events": [
          { "event": "show_desktop", "args": [{ "type": "uint", "value": 1 }] }
        ]
      }
    }
  ]
}
```

- `xml_path`: absolute path to the protocol XML
- `module`: omitted for standard wayland protocols (WServer auto-registers them)
- `module.class`: Qt/Wayland C++ class name (must match `server->attach<ClassName>()` in `src/seat/helper.cpp`)
- `module.header`: header filename in `src/modules/<dir>/`
- `module.dir`: treeland module subdirectory

Step types:
- `"type": "request"` — call a request. `"name"` matches XML. `"args"`: numbers, strings, `"-"` for null.
- `"type": "roundtrip"` — `wl_display_roundtrip()`
- `"type": "checkpoint"` — end of steps, flush and collect events

## How to Add a Protocol Test

### 1. Create the JSON case file

```bash
# Write cases/treeland-<name>.json with xml_path, optional module, and tests
```

### 2. Build and run

```bash
# CMake auto-discovers all cases/*.json. No manual rules.
cmake --preset default -B build
cmake --build build -j$(nproc)

# Run treeland internal protocol tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build -L treeland --output-on-failure

# Run standard wayland protocol tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build -L std-wayland --output-on-failure

# Run a single protocol
cmake --build build --target treeland-wallpaper-color-v1-json-test -j8
QT_QPA_PLATFORM=offscreen ./build/tests/protocol/treeland-wallpaper-color-v1-json-test
```

## Protocol Status (21 total)

| Protocol | Module Class | JSON | CMake |
|----------|-------------|------|-------|
| window-management | WindowManagementInterfaceV1 | ✅ | ✅ auto |
| wallpaper-color | WallpaperColorInterfaceV1 | ✅ | ✅ auto |
| ddm | DDMInterfaceV1 | ✅ | ✅ auto |
| screensaver | ScreensaverInterfaceV1 | ✅ | ✅ auto |
| virtual-output | VirtualOutputManagerInterfaceV1 | ✅ | ✅ auto |
| keyboard-state-notify | TreelandKeyboardStateNotifyManagerInterfaceV1 | ✅ | ✅ auto |
| app-id-resolver | — (wlroots-managed) | ✅ | ✅ auto |
| capture | — | ✅ | ✅ auto |
| dde-shell | — | ✅ | ✅ auto |
| foreign-toplevel | — | ✅ | ✅ auto |
| input-manager | — | ✅ | ✅ auto |
| output-manager | — | ✅ | ✅ auto |
| personalization | — | ✅ | ✅ auto |
| prelaunch-splash-v1 | — | ✅ | ✅ auto |
| prelaunch-splash-v2 | — | ✅ | ✅ auto |
| shortcut-manager-v1 | — | ✅ | ✅ auto |
| shortcut-manager-v2 | — | ✅ | ✅ auto |
| wallpaper-manager | — | ✅ | ✅ auto |
| wallpaper-shell | — | ✅ | ✅ auto |
| wine-window-management | — | ✅ | ✅ auto |
| wine-window-state | — | ✅ | ✅ auto |

## Running All Tests

```bash
# Build everything
cmake --preset default -B build
cmake --build build -j$(nproc)

# All gen-client tests (echo server fixtures)
ctest --test-dir build -L gen-client --output-on-failure

# JSON-driven treeland protocol tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build -L treeland --output-on-failure

# JSON-driven standard wayland protocol tests
QT_QPA_PLATFORM=offscreen ctest --test-dir build -L std-wayland --output-on-failure
```

## Known Limitations

- Treeland protocol tests need proper server fixtures (outputs, seats, surfaces) 
  for events that depend on compositor state. Current tests are smoke-level 
  (verify client connects and doesn't crash).
- Standard wayland protocol tests are limited to protocols wlroots auto-registers.
  Protocols needing specific treeland infrastructure (xdg-shell surfaces, etc.)
  require the `NEED_SURFACE_FIXTURE` path in gen-test-client.
- `QT_QPA_PLATFORM=offscreen` is required because json-controller links Qt6::Widgets
  (for QApplication). No actual display is used.
