# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4g (JSON runner full type integration)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3, MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d, MVP-D4e, MVP-D4f]

Stage commits (D4f and prior):
- `4b328d7b4` test(protocol): accept MVP-D4f object + request new_id stage
- `848caa14b` docs(protocol): hand off MVP-D4f validation
- `09e5cfad7` test(protocol): cover object type and request new_id support

Automated validation (D4g):
- JSON runner 新增 generic protocol 路由：通过 input.json `protocol` 字段检测
  `treeland_test_multi_arg_v1` 协议，绕过 window-management 专用验证，直接加载并执行。
- `runGenericProtocolJsonCase` 创建 socketpair echo server，驱动
  tl_test_multi_arg adapter，按 JSON steps 执行 request/barrier/checkpoint/disconnect。
- checkpoint collector 将 per-event struct (`tl_test_multi_arg_reply_event`) 规范化为
  JSON：uint/int 类型输出 typed value、string 输出 owning copy 或 null。
- expected JSON 使用 `{ "type": "uint", "value": 42 }` 等 D4 类型表示，actual 与 expected
  逐字段精确比较。
- multi-arg-echo JSON case：request(42, -7, "hello") → reply event → checkpoint 验证
  uint=42, int=-7, string="hello"：PASS。
- PoC 0A 至 MVP-D4f 加 D4g 完整协议回归：44/44 通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target treeland-protocol-test-runner \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4g \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd|new-id|multi-arg|object-newid)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure
```

Human validation: pending

Known issues:
- D4g 实现了 generic JSON runner 路由和 multi-arg 协议的 checkpoint 收集与比较。
- 当前 generic runner 仅支持 `treeland_test_multi_arg_v1`（hardcoded），未实现完全
  metadata-driven 的通用 runner（留给后续阶段）。
- fixed/array/fd/new_id/object 类型的 JSON runner 集成尚未创建对应的测试 case
  （D4a-D4f 各类型的硬编码 self-test 已充分覆盖类型正确性）。
- runner 未接入 per-event struct 的 multi-arg JSON 固定 schema 验证（使用
  直接 JSON 比较代替）。

Next authorized action: start MVP-D5 Global lifecycle and version matrix when explicitly requested; preserve all PoC 0A through MVP-D4g regressions
