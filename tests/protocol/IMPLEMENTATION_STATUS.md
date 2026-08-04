# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D1
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3]

Stage commits:
- `931cff4c0ee73da91acc413af1cf3a9e15fed3ca` test(protocol): support expected Wayland protocol errors
- `681bd57eb5bda81bbdb9545b603885ac6d13aef4` test(protocol): exercise Wine invalid-sibling protocol error

Automated validation:
- `strict` validation 在 marshal 前以 `adapter_validation_error` 拒绝
  `set_z_order(hwnd_top, sibling_id=1)`：通过。
- `wire` validation 保留对象、request、参数类型和 enum 范围校验，仅允许上述安全语义错误
  到达正式 `WineWindowControl::set_z_order()`：通过。
- 真实 Wayland protocol error 记录 `treeland_wine_window_control_v1` interface、动态
  object ID、error code `0`、symbolic object `control` 和 display error `EPROTO`：通过。
- expected matcher 验证 protocol error；错误 code 和错误 symbolic object self-test 均以
  `checkpoint_protocol_error_diff` 和目标 checkpoint 失败：通过。
- protocol error 后不发送 protocol destructor，本地 proxy、display、client 和 surface
  安全清理并恢复资源基线：通过。
- `ctest --test-dir build-feat-testprotocol -L protocol-error --output-on-failure`：4/4 通过。
- `ctest --test-dir build-feat-testprotocol -R
  'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner|protocol-wine-window-management'
  --output-on-failure`：28/28 通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target treeland-protocol-test-runner \
  -j8

./build-feat-testprotocol/tests/protocol/treeland-protocol-test-runner \
  --input tests/protocol/cases/wine-protocol-error-invalid-sibling.input.json \
  --expected tests/protocol/cases/wine-protocol-error-invalid-sibling.expected.json \
  --dump-actual build-feat-testprotocol/test-results/mvp-d1/invalid-sibling.actual.json \
  --report-dir build-feat-testprotocol/test-results/mvp-d1/invalid-sibling \
  --verbose

ctest --test-dir build-feat-testprotocol \
  -L protocol-error \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner|protocol-wine-window-management' \
  --output-on-failure
```

Human validation: pending

Known issues:
- MVP-D1 只覆盖可由合法生成 C API 表达的 Wine `invalid_sibling` protocol error；不发送
  畸形 opcode，不伪造 signature，也不实现 malicious client。
- `wire` 目前只绕过 `set_z_order` 的 sibling 语义约束；未知 request、错误对象、错误参数类型
  和越界 `z_order_op` 仍在 marshal 前拒绝。
- symbolic object name 是基于客户端对象表的 best-effort 映射；actual 始终保留动态 object ID，
  映射失败时名称允许为空。
- 正向 expected 仍为 `candidate`，需人工核对 actual 后才能更新为 `human-reviewed`。
- 受控沙箱不允许 Unix socket `bind()`，真实 wire 测试需要在获准的非沙箱环境运行。

Next authorized action: run and report MVP-D1 manual validation; do not start MVP-D2 until MVP-D1 is accepted
