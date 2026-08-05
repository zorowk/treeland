# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4h (batch protocol adaptation) + D4i foundation (registry-driven runner)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3,
  MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d, MVP-D4e, MVP-D4f, MVP-D4g, MVP-D5, MVP-D6]

Key achievements since D5:
- Scanner generates per-protocol registry struct (`tl_test_<adapter>_registry_type`)
  with uniform function pointer table (init/fini/bind/dispatch/destroy/listener).
- Runner uses registry interface via `ProtocolRegistry` — protocol-agnostic dispatch.
- 21/21 treeland protocols generate + compile.
- Dispatch function handles uint/int/string/enum via string parsing.
- D4h (batch compile) complete: 100% pass rate.

Manual validation command:
```bash
cmake --build build-feat-testprotocol --target treeland-protocol-test-runner -j8
ctest --test-dir build-feat-testprotocol -L mvp-d4g --output-on-failure
ctest --test-dir build-feat-testprotocol -R 'protocol-((array|fixed|fd|new-id|multi-arg|object-newid|global-version)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' --output-on-failure
```

Human validation: pending

Known issues:
- Registry-driven runner still uses hand-rolled echo server, not Helper::init() (D4i).
- Event normalization is still protocol-specific (needs per-event struct access).
- D4i (production server path) and D4j (AI JSON generation) remain unimplemented.

Next authorized action: D4i (Helper::init() production server path) or D4j (AI JSON generation).
