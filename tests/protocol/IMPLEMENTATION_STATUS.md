# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D4f (object + request new_id)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3, MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d, MVP-D4e]

Stage commits (D4e and prior):
- `acffbdb2e` test(protocol): accept MVP-D4e multi-arg event generalization stage
- `34408968b` docs(protocol): hand off MVP-D4e validation
- `b05a1b885` test(protocol): cover multi-arg event generalization with int/string/enum

Automated validation (D4f):
- scanner 为所有协议 interface 生成 forward-declare（非仅 event new_id children）：通过。
- `selectTargetInterface` 选择有 events 的 interface（非第一个），修复 prelaunch-splash/
  screensaver 等多 interface 无 event 协议的 target 选择：通过。
- request wrapper 对 `new_id` 参数跳过参数列表并捕获 wayland-scanner 返回的 proxy，
  不将 new_id 传给底层函数：通过。
- 无 event 接口不生成 listener struct 和 add_listener 调用：通过。
- D4f 自测协议（treeland-test-object-newid-v1）覆盖 request new_id + event new_id +
  object 参数 metadata：5/5 通过。
- 全部 treeland 私有协议（21 个）scanner 生成 + C 语法编译通过率：19/21 (90%)。
  剩余 2 个 wine `unstable` 协议因 XML 协议名不含 `unstable` 后缀导致 include 路径
  不匹配——非扫描器问题，属于协议命名约定。
- PoC 0A 至 MVP-D4e 加 D4f 完整协议回归：43/43 通过。
- `ctest --test-dir build-feat-testprotocol -L mvp-d4f --repeat until-fail:20
  --output-on-failure`：连续 20 次通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target protocol-object-newid-adapter-selftest \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4f \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -L mvp-d4f \
  --repeat until-fail:20 \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd|new-id|multi-arg|object-newid)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure
```

Human validation: pending

Known issues:
- D4f 已实现 object 类型和 request new_id；但 request new_id 创建的 child proxy 未
  自动安装 listener（仅 event new_id 会安装）。
- 无 event 协议（prelaunch-splash、screensaver）跳过 listener 安装，可通过 bind 但
  无事件捕获。
- wine `unstable` 协议（2 个）因 XML 协议名与文件名不一致导致 include 路径错误，
  需单独修复命名约定。
- D4f 仅覆盖 `protocol-wire` 层自测；未接入 JSON runner。

Batch protocol compile results (19/21 = 90%):
  OK: app-id-resolver, capture, dde-shell, ddm, foreign-toplevel, input-manager,
      keyboard-state-notify, output-manager, personalization, prelaunch-splash-v1/v2,
      screensaver, shortcut-manager-v1/v2, virtual-output, wallpaper-color,
      wallpaper-manager, wallpaper-shell, window-management
  FAIL: wine-window-management-unstable, wine-window-state-unstable (naming)

Next authorized action: start MVP-D4g JSON runner full type integration when explicitly requested; preserve all PoC 0A through MVP-D4f regressions
