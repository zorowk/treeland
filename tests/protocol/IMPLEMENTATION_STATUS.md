# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4c (fd)
State: accepted
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3, MVP-D4a, MVP-D4b, MVP-D4c]

Stage commits (D4b and prior):
- `2dc4e99d710af993a0aed864a82f46e47e8a1fe0` test(protocol): cover array adapter values
- `428ee429f0ed01d1e4d3135b0b14f6c90c4cab05` test(protocol): cover fixed adapter values
- `772802c6c1baf9eb40cae5a28449e46bef950095` fix(protocol): isolate Wine controls by manager binding
- `d6792442d243106604950e585fde6d4970c64000` test(protocol): exercise Wine multi-client isolation

Automated validation (D4c):
- scanner test-client-header/code/metadata 模式从 `treeland-test-fd-v1.xml` 生成
  `struct tl_test_fd_fd_event { const char *name; int fd; }`、`fd_events[]`、`fd_event_count`、
  `int32_t` request wrapper 和 metadata：通过。
- 生成 adapter 使用 `F_DUPFD_CLOEXEC` + `fcntl()` 在 event callback 内 dup fd（独立所有权），
  `clear_events`/`adapter_fini` 关闭所有已拥有 fd，不保留原始 fd 数字：通过。
- 独立 socketpair 上的真实 Wayland request/event wire echo 覆盖有效 fd；echo 后关闭原始 fd，
  event snapshot 的 dup fd 可通过 `fcntl(F_GETFD)` 验证：通过。
- `normalizedProtocolFd` 返回 `{ "fd": "valid" }` — 不序列化原始 fd 数字；null fd 返回空对象：通过。
- JSON 输入接受 `{ "fd": true }`（分配管道 fd）和 `null`（fd = -1）；拒绝裸值、错误成员名、
  `false`、额外字段：通过。
- 非 nullable fd 的负值 request 自然导致服务端 `wl_client_post_implementation_error` 断开；
  event capacity 溢出产生 `event_snapshot_failed`，clear 后恢复：通过。
- protocol destructor 后再次调用 fd request 返回 adapter validation failure：通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d4c --repeat until-fail:20
  --output-on-failure`：连续 20 次通过。
- PoC 0A 至 MVP-D4b 加 D4c 完整协议回归：40/40 通过。
- ASan/LSan 下 `ctest --test-dir build-feat-testprotocol-asan -R protocol-fd-adapter-selftest
  --output-on-failure`：1/1 通过，无 sanitizer error 或 leak。
- scanner 两次生成 header 和 code：diff 为空，输出稳定。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target protocol-fd-adapter-selftest \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4c \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4c \
  --repeat until-fail:20 \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure

ASAN_OPTIONS=detect_leaks=0 \
cmake --build build-feat-testprotocol-asan \
  --target protocol-fd-adapter-selftest \
  -j8

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:detect_odr_violation=0 \
LSAN_OPTIONS=exitcode=23:suppressions="$PWD/tests/protocol/lsan-suppressions.txt" \
ctest --test-dir build-feat-testprotocol-asan \
  -R protocol-fd-adapter-selftest \
  --output-on-failure
```

Human validation: passed

Known issues:
- MVP-D4c 已实现 fd；event `new_id` 和 malicious wire 仍未实现。
- fd 由 `protocolFdFromJson` 通过 `pipe()` 分配，echo request 完成后调用方负责关闭。
  Event callback 使用 `fcntl(F_DUPFD_CLOEXEC)` dup，`clear_events` 关闭，所有权明确。
- `normalizedProtocolFd` 返回 `{ "fd": "valid" }`，不序列化原始 fd 数字，符合阶段规范。
- 当前 D4c 仅覆盖 `protocol-wire` 层自测；fd 尚未接入 JSON runner 或 Wine protocol-high 场景。
- 受控沙箱不允许 Unix socket `bind()`，真实 Wayland 测试需要在获准的非沙箱环境运行。
- ASan 构建阶段使用 `detect_leaks=0`，因为 instrumented QtWayland 代码生成器在受控环境下不能
  启动 LSan；最终测试阶段重新启用 LSan。
- ASan 运行禁用 `detect_odr_violation`，因为 waylib 与 treeland 都链接了生成的
  `xdg_popup_interface`。
- LSan suppressions 仅覆盖现有 Waylib/QML fixture wrapper 循环。

Next authorized action: start MVP-D4d event new_id only when explicitly requested; preserve all PoC 0A through MVP-D4c regressions
