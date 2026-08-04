# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D3
State: implementing
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2]

Stage commits:
- none yet

Automated validation:
- pending

Manual validation command: pending

Human validation: pending

Known issues:
- MVP-D3 只处理多客户端隔离，不实现 fd/array/fixed、event `new_id` 或 malicious wire。
- MVP-D2 已接受的生命周期 expected 继续保持 `human-reviewed`；MVP-D3 新增 expected
  在人工验收前保持 `candidate`。
- 受控沙箱不允许 Unix socket `bind()`，真实 Wayland 测试需要在获准的非沙箱环境运行。
- ASan 构建阶段使用 `detect_leaks=0`，因为 instrumented QtWayland 代码生成器在受控环境下不能
  启动 LSan；最终测试阶段重新启用 LSan。
- 最终 ASan 运行禁用 `detect_odr_violation`，因为 waylib 与 treeland 都链接了生成的
  `xdg_popup_interface`；否则测试在进入生命周期逻辑前终止。
- LSan suppressions 仅覆盖现有 Waylib/QML fixture wrapper 循环；WineWindowControl 和
  WineWindowManager 未被抑制，并由远端 control resource 的 `1 -> 0` 硬断言独立验证。

Next authorized action: implement MVP-D3 multi-client isolation only; preserve all PoC 0A through MVP-D2 regressions
