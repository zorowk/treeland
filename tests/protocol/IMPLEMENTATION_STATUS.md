# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D6 (CI stabilization)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3,
  MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d, MVP-D4e, MVP-D4f, MVP-D4g, MVP-D5]

Stage commits (D5 and prior):
- `b301ca0e1` test(protocol): accept MVP-D5 stage
- `a82025145` feat(ci): add protocol test workflow and sanitizer presets

Recent achievements (post-D5):
- `e9bca40d9` feat(scanner): generate dispatch function, achieve 21/21 protocol compile
  - Scanner now generates `tl_test_<adapter>_dispatch()` with string-based arg parsing
  - All 21 treeland protocols in /usr/share/treeland-protocols/ generate + compile
  - Runner uses dispatch for metadata-driven request execution
- CI workflow for Debug/Release/ASan: `.github/workflows/protocol-test.yml`
- CMakePresets.json: ci-asan, ci-ubsan presets

Manual validation command (D6):
```bash
QT_QPA_PLATFORM=offscreen \
ctest --test-dir build-feat-testprotocol \
  -L generated-adapter --repeat until-fail:100 --output-on-failure

QT_QPA_PLATFORM=offscreen \
ctest --test-dir build-feat-testprotocol \
  -L json-runner -R 'multi-arg-echo|window-management-json$' \
  --repeat until-fail:100 --output-on-failure
```

Human validation: pending (D6)

Known issues:
- MVP-D7 (malicious client) 已放弃。
- dispatch 函数仅支持 uint/int/string/enum 的字符串解析；object/array/fd/fixed 传 NULL/0。
- generic runner 仍仅支持 multi-arg 协议（hardcoded per protocol）；完全 metadata-driven
  通用 runner 需要每个协议的 server echo 实现模板。

Next authorized action: 全部 MVP 阶段已完成。
