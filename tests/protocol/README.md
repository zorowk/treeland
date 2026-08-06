# Treeland Protocol Testing Guide

## Architecture

```
/usr/share/treeland-protocols/*.xml    (also /usr/share/wayland-protocols/)
  │
  ├── wayland-scanner → wayland-<proto>-client-protocol.h/.c  (wire stubs)
  └── gen-test-client  → tl-test-<proto>.c                    (standalone C client)
                            │
                            └── json-controller               (reads JSON, starts WServer,
                                                               spawns client, checks output)
```

## File Layout

```
tests/protocol/
├── gen-test-client/          # Code generator (reads XML, outputs C)
│   ├── main.cpp              # Generator source
│   ├── template.c.in         # C template with {{PLACEHOLDERS}}
│   └── CMakeLists.txt
├── json-controller.cpp       # Reads per-protocol JSON, drives test
├── treeland-controller.cpp   # Starts WServer + protocol module
├── gencliente2etest.cpp      # Generic E2E harness for echo server tests
├── CMakeLists.txt            # add_gen_client_test(), add_json_protocol_test()
├── fixtures/                 # 6 self-contained test protocol XMLs
├── cases/                    # Per-protocol JSON test cases
├── *echoserver.c             # Echo servers for fixture protocols
├── lsan-suppressions.txt
└── .github/workflows/protocol-test.yml
```

## Protocol Sources

| Path | Contents |
|------|----------|
| `/usr/share/treeland-protocols/treeland-*.xml` | 21 Treeland private protocols |
| `/usr/share/wayland-protocols/` | Standard Wayland protocols (xdg-shell, etc.) |

All 21 treeland protocols compile via gen-test-client.

## How to Add a Protocol Test

### Step 1: Verify the client compiles

```bash
cd build-feat-testprotocol
./tests/protocol/gen-test-client/gen-test-client \
    /usr/share/treeland-protocols/treeland-<name>.xml \
    /tmp/test.c

# Compile check (needs wayland-scanner stubs):
WAYLAND_SCANNER=$(pkg-config --variable=wayland_scanner wayland-scanner)
$WAYLAND_SCANNER client-header <xml> /tmp/wayland-<proto>-client-protocol.h
$WAYLAND_SCANNER private-code <xml> /tmp/wayland-<proto>-client-protocol.c
gcc -fsyntax-only -I/tmp -I/usr/include /tmp/test.c
```

### Step 2: Write the JSON test case

Create `tests/protocol/cases/<module>.json`:

```json
{
  "protocol": "treeland_window_management_v1",
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

Step types:
- `"type": "request"` — call a protocol request. `"name"` matches the XML request name. `"args"` are values (numbers as-is, strings as-is, `"-"` for null).
- `"type": "roundtrip"` — `wl_display_roundtrip()`.
- `"type": "checkpoint"` — end of steps, collect events.

### Step 3: Add CMake rule

In `tests/protocol/CMakeLists.txt`, add:

```cmake
add_json_protocol_test(test-<module> <xml_var> <gen_dir> <client_code_var>
    "<module>interfacev1.h" <ModuleClass> <module-dir>)
```

Example for window-management:
```cmake
add_json_protocol_test(test-wm window_management_xml wm_generated_directory wm_client_code
    "windowmanagementinterfacev1.h" WindowManagementInterfaceV1 window-management)
```

### Step 4: Build and run

```bash
cmake --build build-feat-testprotocol --target test-<module>-json-test -j8
ctest --test-dir build-feat-testprotocol -R test-<module>-json --output-on-failure
```

## Deciding What to Test

**Not all protocols need JSON cases.** Decision flow:

1. Does the protocol have events? → If no, smoke test only (request + no crash).
2. Do events fire immediately on request? → If yes, full JSON test case.
3. Do events depend on treeland state (outputs, surfaces, seats)? → If yes, may need server fixture setup before testing.

**Priority order for writing JSON cases:**

| Priority | Protocols | Reason |
|----------|-----------|--------|
| 1 | window-management | Already done; simplest request→event |
| 2 | wallpaper-color, virtual-output-manager | Simple request args, events fire immediately |
| 3 | capture, keyboard-state-notify | Events fire on state subscription |
| 4 | ddm, screensaver, prelaunch-splash | Complex state dependencies or no events |

## AI-Assisted JSON Generation

The `gen-test-client` generates a C client. To generate the JSON test case:

1. Read the protocol XML to understand requests and events
2. Read the treeland module source (`src/modules/<module>/`) to understand server behavior
3. For each request that triggers an immediate event, write a test case
4. The event format in `expected.events` matches the gen-test-client output format:
   `EVENT <event_name> <arg_name>=<value> ...`

Example: If gen-test-client outputs `EVENT show_desktop state=1`, the expected JSON is:
```json
{ "event": "show_desktop", "args": [{ "type": "uint", "value": 1 }] }
```

## Running All Tests

```bash
# All gen-client tests (echo server fixtures)
ctest --test-dir build-feat-testprotocol -L gen-client --output-on-failure

# JSON-driven treeland protocol tests
ctest --test-dir build-feat-testprotocol -L json-driven --output-on-failure

# CI stability (100x repeat)
QT_QPA_PLATFORM=offscreen ctest --test-dir build-feat-testprotocol -L gen-client --repeat until-fail:100
```

## Current Coverage

| Layer | Count | Status |
|-------|-------|--------|
| Protocols compiling | 21/21 | ✅ |
| gen-client E2E tests | 6 | ✅ (fixture protocols) |
| JSON treeland tests | 1 | ⚠️ (window-management only) |
| CI workflow | 1 | ✅ |

## Known Gaps

- Only window-management has a JSON test case; 20 protocols need cases written
- json-controller currently hardcodes WindowManagementInterfaceV1; needs registry for other modules
- Fixture-based E2E tests use hand-rolled echo servers; treeland integration tests use WServer
- No standard wayland protocol tests yet (xdg-shell, wl_output, etc.)

## Full Protocol Coverage Strategy

Two protocol sources, two testing approaches:

### Treeland private protocols (21 in /usr/share/treeland-protocols/)

Compiled from XML by treeland. Each has a src/modules/<name>/ implementation.
Testing requires json-controller to attach the specific module.

Status: all 21 compile via gen-test-client. Only window-management has a
JSON test case. Remaining 20 need:
1. JSON case file in tests/protocol/cases/<module>.json
2. CMake rule: add_json_protocol_test(...)

### Standard Wayland protocols (55 in /usr/share/wayland-protocols/)

Implemented by wlroots. WServer auto-registers them - no attach<>() needed.
gen-test-client generates clients from any wayland XML.

Testing a standard protocol:
1. gen-test-client /usr/share/wayland-protocols/.../xxx.xml → client.c
2. Start treeland headless (WServer auto-provides all wlroots protocols)
3. Spawn client, check EVENT output

No per-protocol CMake module rules needed. Share a generic controller.

### What's practical to test

| Category | Count | Testable | Notes |
|----------|-------|----------|-------|
| Treeland private | 21 | All 21 | Need JSON case per protocol |
| Wayland core (xdg-shell, etc.) | 5 | Yes | Basic request→event |
| Wayland staging | 30 | Some | Pick simple API ones |
| Wayland hardware (drm, dmabuf) | 10 | No | Need GPU/hardware |
| Wayland unstable | 10 | Some | Legacy, low priority |

### Priority

1. Write JSON cases for remaining 20 treeland protocols
2. Add generic standard-protocol controller (WServer only)
3. Test xdg-shell, wl_output, xdg-activation (most used)
4. Extend to other staging protocols as needed
