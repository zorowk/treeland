# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D2
State: accepted
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2]

Stage commits:
- `48e8e9b23bc8f288a97206d29be477f8785761ba` test(protocol): expose Wine control resource lifecycle
- `e96bf3bb8d70512a8aa392a0760de2b122756e38` test(protocol): model Wine lifecycle teardown modes
- `f8107de3a2f84d0c04de0d7ebe3fa87f4eb06e35` test(protocol): cover Wine lifecycle cleanup matrix

Automated validation:
- protocol destroy、proxy-only destroy、graceful disconnect、abrupt disconnect 和 server
  shutdown 五种模式均产生可区分的生命周期 checkpoint：通过。
- lifecycle report 分别记录客户端本地 proxy/display 状态与服务端
  client/surface/control-resource 状态：通过。
- proxy-only destroy 仅销毁本地 proxy，checkpoint 中远端 control resource 仍存活；连接断开后
  `destroy_resource` 计数增加且资源恢复基线：通过。
- abrupt disconnect 在关闭 transport 前保留远端 resource，不发送 protocol destructor；服务端通过
  条件等待观察正式最终清理：通过。
- server shutdown 在客户端 proxy 仍存活时观察 display failure，随后 runner 完成显式 teardown：通过。
- resource-not-restored self-test 使用故意错误的最终资源计数并在目标 checkpoint 失败：通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d2 --output-on-failure`：6/6 通过。
- `ctest --test-dir build-feat-testprotocol -R
  'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner|protocol-wine-window-management'
  --output-on-failure`：34/34 通过。
- ASan/LSan 构建运行
  `ctest --test-dir build-feat-testprotocol-asan -L abrupt-disconnect --output-on-failure`：
  6/6 通过，无未抑制的 lifecycle error 或 leak。

Manual validation command:
```bash
cmake --build build-feat-testprotocol -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d2-accepted \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner|protocol-wine-window-management' \
  --output-on-failure

cmake -S . -B build-feat-testprotocol-asan -G Ninja \
  -DADDRESS_SANITIZER=ON \
  -DWITH_SUBMODULE_WAYLIB=ON

ASAN_OPTIONS=detect_leaks=0 \
cmake --build build-feat-testprotocol-asan \
  --target treeland-protocol-test-runner \
  -j8

ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:detect_odr_violation=0 \
LSAN_OPTIONS=exitcode=23:suppressions="$PWD/tests/protocol/lsan-suppressions.txt" \
ctest --test-dir build-feat-testprotocol-asan \
  -L abrupt-disconnect \
  --output-on-failure
```

Human validation: passed

Known issues:
- MVP-D2 不实现多客户端隔离、malicious wire 或 MVP-D3 行为。
- MVP-D2 正向 expected provenance 已随人工验收更新为 `human-reviewed`；故障检测
  resource-not-restored self-test 继续保持 `candidate`。
- 受控沙箱不允许 Unix socket `bind()`，真实 Wayland 测试需要在获准的非沙箱环境运行。
- ASan 构建阶段使用 `detect_leaks=0`，因为 instrumented QtWayland 代码生成器在受控环境下不能
  启动 LSan；最终测试阶段重新启用 LSan。
- 最终 ASan 运行禁用 `detect_odr_violation`，因为 waylib 与 treeland 都链接了生成的
  `xdg_popup_interface`；否则测试在进入生命周期逻辑前终止。
- LSan suppressions 仅覆盖现有 Waylib/QML fixture wrapper 循环；WineWindowControl 和
  WineWindowManager 未被抑制，并由远端 control resource 的 `1 -> 0` 硬断言独立验证。

Next authorized action: start MVP-D3 only when explicitly requested; preserve all PoC 0A through MVP-D2 regressions
