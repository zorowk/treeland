# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4d (event new_id)
State: accepted
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3, MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d]

Stage commits (D4c and prior):
- `2bdf40a30` test(protocol): accept MVP-D4c fd stage
- `6d11ffc75` docs(protocol): hand off MVP-D4c validation
- `50934cf03` test(protocol): cover fd adapter values
- `2dc4e99d7` test(protocol): cover array adapter values
- `428ee429f` test(protocol): cover fixed adapter values

Automated validation (D4d):
- scanner test-client-header/code/metadata 从双接口 XML (`treeland_test_new_id_v1` +
  `treeland_test_child_v1`) 生成 parent adapter struct 中的 child_proxy、
  child_listener_installed、child_events[] 和 child_event_count 字段：通过。
- scanner 为 child interface 生成 event handler 和 listener struct；parent 的
  `child_created` event handler 验证 proxy 非空且尚未安装，调用
  `treeland_test_child_v1_add_listener` 安装 child listener：通过。
- `clear_events` 通过 `wl_proxy_destroy` 销毁 child proxy，重置 listener 标志和
  event count：通过。
- 独立 socketpair 上的真实 Wayland wire：服务端 `wl_resource_create` 创建 child
  resource，发送 `child_created(new_id)` + `done(42)`；客户端 adapter 捕获 child proxy
  并正确接收 child event：通过。
- duplicate child detected（二次 create_child 未 clear 时 child_proxy 非空，
  event_snapshot_failed = true）：通过。
- protocol destructor 后 request 返回 -1：通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d4d --repeat until-fail:20
  --output-on-failure`：连续 20 次通过。
- PoC 0A 至 MVP-D4c 加 D4d 完整协议回归：41/41 通过。
- ASan/LSan 下 `ctest --test-dir build-feat-testprotocol-asan -R
  protocol-new-id-adapter-selftest --output-on-failure`：1/1 通过，无未抑制的
  sanitizer error 或 leak。
- scanner 两次生成 header 和 code：diff 为空，输出稳定。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target protocol-new-id-adapter-selftest \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4d \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4d \
  --repeat until-fail:20 \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd|new-id)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure

ASAN_OPTIONS=detect_leaks=0 \
cmake --build build-feat-testprotocol-asan \
  --target protocol-new-id-adapter-selftest \
  -j8

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:detect_odr_violation=0 \
LSAN_OPTIONS=exitcode=23:suppressions="$PWD/tests/protocol/lsan-suppressions.txt" \
ctest --test-dir build-feat-testprotocol-asan \
  -R protocol-new-id-adapter-selftest \
  --output-on-failure
```

Human validation: passed

Known issues:
- MVP-D4d 已实现 event new_id（单 child interface）；malicious wire 仍未实现。
- scanner 仅处理 parent 的第一个 event 中的 new_id 参数；不处理同一 parent 的多个不同
  child interface 或同一个 event 的多个 new_id 参数。
- child proxy 的 interface/version 未独立记录在 adapter struct 中（Wayland scanner
  生成的 proxy 已包含这些信息）。
- D4d 仅覆盖 `protocol-wire` 层自测；event new_id 尚未接入 JSON runner。
- 受控沙箱不允许 Unix socket `bind()`，真实 Wayland 测试需要在获准的非沙箱环境运行。
- LSan suppressions 已新增 `wl_display_read_events` 以覆盖 raw Wayland client 内部
  buffer 泄漏。

Next authorized action: MVP-D4 (剩余参数类型) 已完成；下一个阶段待用户指定。
