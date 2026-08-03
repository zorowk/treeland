# Treeland Wayland Protocol Test Implementation Status

Current stage: PoC 2
State: accepted
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2]

Stage commits:
- `40354f12fd755bdcc7102e5009a32be0dc74ed76` feat(protocol): add JSON-driven wire test runner
- `fc567ca0a9e2d46b526d2c7c22c1811acc64bfaf` test(protocol): verify JSON runner contracts

Automated validation:
- 标准 JSON Schema、input/expected loader、case/provenance 校验和 generated metadata
  预验证：通过。
- 正确 JSON 完成 bind、initial checkpoint、set_desktop request、client_roundtrip、
  request checkpoint、protocol destroy、client_roundtrip 和 disconnect：通过。
- owning/规范化 `actual.json` 包含协议版本、XML SHA-256、两个 checkpoint、服务端状态、
  connection 和生命周期证据；相同 case 连续运行逐字一致：通过。
- hardcoded generated adapter golden 与 JSON runner 规范化契约对照：通过。
- invalid schema、unknown request、wrong argument type、metadata mismatch、wrong expected
  event、wrong server state、case ID mismatch、malformed expected self-test 均以预期类别失败：通过。
- `ctest --test-dir build-feat-testprotocol -R
  'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner'
  --output-on-failure`：20/20 通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target generate-window-management-test-adapter \
           treeland-protocol-test-runner \
           protocol-json-contract-diff \
           test-protocol-window-management-wire \
           test-protocol-window-management-wire-generated \
           test-protocol-window-management-wire-generated-missing-listener \
           test-protocol-window-management-wire-wrong-expected \
           test-protocol-window-management-helper \
           test-protocol-window-management-helper-generated \
           test-protocol-window-management-helper-wrong-expected \
           protocol-adapter-contract-diff \
  -j8

QT_QPA_PLATFORM=offscreen \
./build-feat-testprotocol/tests/protocol/treeland-protocol-test-runner \
  --input tests/protocol/cases/window-management-show.input.json \
  --expected tests/protocol/cases/window-management-show.expected.json \
  --dump-actual build-feat-testprotocol/test-results/poc-2/window-management-show.actual.json \
  --report-dir build-feat-testprotocol/test-results/poc-2 \
  --verbose

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner' \
  --output-on-failure
```

Human validation: passed

Known issues:
- PoC 2 只将已接受的 window-management 单 interface 场景迁移到 JSON；不增加新协议能力。
- handwritten 与 generated adapter 场景必须继续作为回归 golden。
- 当前 expected 已随本阶段人工验收更新为 `human-reviewed`。
- 仍不支持 `new_id`、跨协议对象、xdg/shm fixture、多客户端及 fd/array/fixed；这些不属于 PoC 2。
- 受控沙箱不允许 Unix socket `bind()`，真实 wire 测试需要在获准的非沙箱环境运行。

Next authorized action: start PoC 3 window behavior implementation; preserve all PoC 0A through PoC 2 regressions
