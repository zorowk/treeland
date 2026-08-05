# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4e (multi-arg event generalization)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3, MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d]

Stage commits (D4d and prior):
- `64dbceb65` test(protocol): accept MVP-D4d event new_id stage
- `52ee7a2a8` test(protocol): cover event new_id adapter values
- `2bdf40a30` test(protocol): accept MVP-D4c fd stage

Automated validation (D4e):
- scanner 为 treeland-test-multi-arg-v1 协议生成 per-event struct
  `tl_test_multi_arg_reply_event { uint32_t id; int32_t offset; char *name; }`；
  adapter 中生成 `reply_events[]` / `reply_event_count`：通过。
- event handler 按值复制 uint/int 字段，strdup string 字段；null string 存 NULL：通过。
- `clear_events` 释放所有 per-event string 字段后重置 counter：通过。
- scanner 检测 `needsPerEventStructs`（多参数或 int/string 类型）后启用 per-event 模式；
  单参数旧类型协议（fixed/array/fd/new_id）不受影响：42/42 全量回归通过。
- metadata 输出包含 interface.enums 数组，含 enum name 和 entry map：通过。
- 修复上游扫描器 `allow-null` XML 属性名（`allowNull` → `allow-null`），JSON metadata
  `allow_null` 字段现在正确反映 XML 声明：通过。
- null string (allow-null="true")、string ownership clear 后独立、capacity overflow、
  dead-target rejection 全部通过：8/8 自测通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d4e --repeat until-fail:20
  --output-on-failure`：连续 20 次通过。
- PoC 0A 至 MVP-D4d 加 D4e 完整协议回归：42/42 通过。
- ASan/LSan：1/1 通过，无 sanitizer error 或 leak。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target protocol-multi-arg-adapter-selftest \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4e \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4e \
  --repeat until-fail:20 \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd|new-id|multi-arg)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure
```

Human validation: pending

Known issues:
- D4e 已实现多参数 event 和 int/string/enum 类型支持；object 类型和 request new_id 仍未实现。
- 旧按类型分桶的数组（fixed_events, array_events 等）仍保留在 adapter struct 中，
  但 per-event handler 不填充它们；仅由前序协议的旧 handler 使用。
- string 字段使用 strdup，由 `clear_events` 统一释放；不跨线程共享。
- 修复了上游扫描器 `allow-null` 属性名 bug，影响范围仅限单字符属性名修正。
- D4e 仅覆盖 `protocol-wire` 层自测；未接入 JSON runner。

Next authorized action: start MVP-D4f object + request new_id when explicitly requested; preserve all PoC 0A through MVP-D4e regressions
