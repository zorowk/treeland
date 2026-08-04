# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4a (fixed)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3]

Stage commits:
- `428ee429f0ed01d1e4d3135b0b14f6c90c4cab05` test(protocol): cover fixed adapter values
- `772802c6c1baf9eb40cae5a28449e46bef950095` fix(protocol): isolate Wine controls by manager binding
- `d6792442d243106604950e585fde6d4970c64000` test(protocol): exercise Wine multi-client isolation

Automated validation:
- D4a 使用 `{ "raw": int32 }` 作为 Wayland signed 24.8 fixed 的唯一 JSON 表示；整数、负数、
  `INT32_MIN` 和 `INT32_MAX` 可精确往返，null、浮点 raw、越界值和额外字段均被拒绝：通过。
- scanner 从测试 XML 生成 `wl_fixed_t` request wrapper、fixed metadata 和 event listener；listener
  在 callback 返回前按值复制 raw fixed 和静态 event name，clear 后不保留借用内存：通过。
- 独立 socketpair 上的真实 Wayland request/event wire echo 覆盖 `0`、`-0.5`、`1.5`、
  `INT32_MIN` 和 `INT32_MAX`，actual 统一规范化为 `{ "raw": ... }`：通过。
- protocol destructor 后再次调用 fixed request 返回 adapter validation failure：通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d4a --repeat until-fail:20
  --output-on-failure`：连续 20 次通过。
- PoC 0A 至 MVP-D3 加 D4a 完整协议回归：38/38 通过。
- ASan/LSan 下 `ctest --test-dir build-feat-testprotocol-asan -L mvp-d4a
  --output-on-failure`：1/1 通过，无 sanitizer error 或 leak。
- 两个独立线程分别拥有自己的 `wl_display`、proxy/object table、event collector 和连接错误快照：通过。
- 两个 manager binding 均获得各自 session-local `window_id=1`；client-2 的
  `hwnd_insert_after(sibling_id=1)` 不解析到 client-1，stack order 保持不变：通过。
- client-1 `set_position` 只改变自身 geometry/event；client-2 roundtrip 无污染事件：通过。
- client-1 graceful disconnect 后 client-2 control、surface 和连接继续存活并可处理 request：通过。
- client-1 protocol error 只归属 client-1；client-2 保持无 display/protocol error 并可继续操作：通过。
- 两个客户端 teardown 后 client/surface/control resource 恢复基线 `0/0/0`：通过。
- wrong-attribution self-test 将 client-1 event 故意写入 client-2 expected，并以
  `checkpoint_client_isolation_diff` 在 `client-1-operated` checkpoint 失败：通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d3 --output-on-failure`：3/3 通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d3 --repeat until-fail:20
  --output-on-failure`：三个测试各连续 20 次通过。
- PoC 0A 至 MVP-D3 完整协议回归：37/37 通过。
- ASan/LSan 下 `ctest --test-dir build-feat-testprotocol-asan -L mvp-d3
  --output-on-failure`：3/3 通过，无未抑制的 sanitizer error 或 leak。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target protocol-fixed-adapter-selftest \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4a \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4a \
  --repeat until-fail:20 \
  --output-on-failure

cmake --build build-feat-testprotocol \
  --target treeland-protocol-test-runner \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d3-accepted \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -L mvp-d3-accepted \
  --repeat until-fail:20 \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-(fixed-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure

ASAN_OPTIONS=detect_leaks=0 \
cmake --build build-feat-testprotocol-asan \
  --target protocol-fixed-adapter-selftest \
  -j8

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:detect_odr_violation=0 \
LSAN_OPTIONS=exitcode=23:suppressions="$PWD/tests/protocol/lsan-suppressions.txt" \
ctest --test-dir build-feat-testprotocol-asan \
  -L mvp-d4a \
  --output-on-failure
```

Human validation: pending for MVP-D4a; MVP-D3 passed

Known issues:
- MVP-D4a 只实现 fixed；array、fd、event `new_id` 和 malicious wire 仍未实现。
- fixed 是无动态资源的 scalar owning snapshot；释放规则为按值销毁，不分配、不借用 callback
  临时内存。array 和 fd 必须在各自子阶段另行定义深拷贝/关闭规则。
- MVP-D3 正向 expected 已随人工验收更新为 `human-reviewed`；wrong-attribution self-test 继续保持
  `candidate`。
- 受控沙箱不允许 Unix socket `bind()`，真实 Wayland 测试需要在获准的非沙箱环境运行。
- ASan 构建阶段使用 `detect_leaks=0`，因为 instrumented QtWayland 代码生成器在受控环境下不能
  启动 LSan；最终测试阶段重新启用 LSan。
- 最终 ASan 运行禁用 `detect_odr_violation`，因为 waylib 与 treeland 都链接了生成的
  `xdg_popup_interface`；否则测试在进入生命周期逻辑前终止。
- LSan suppressions 仅覆盖现有 Waylib/QML fixture wrapper 循环；WineWindowControl 和
  WineWindowManager 未被抑制，并由远端 control resource 的 `1 -> 0` 硬断言独立验证。

Next authorized action: after explicit human validation, accept MVP-D4a; do not start MVP-D4b array before that acceptance
