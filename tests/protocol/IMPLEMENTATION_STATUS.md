# Treeland Wayland Protocol Test Implementation Status

Current stage: PoC 3
State: implementing
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2]

Stage commits: pending

Automated validation: pending

Manual validation command: pending

Human validation: pending

Known issues:
- PoC 3 仅扩展 Wine window position 所需的最小 `new_id`、跨协议对象、
  xdg/shm fixture、serial 引用、`surface.geometry` probe 和 `server_condition`。
- 不扩展到多客户端、恶意 wire、通用 fd DSL、完整 array/fixed/event-new-id、
  frame presentation 或截图比较。
- PoC 3 expected 必须保持 `candidate`，直到人工审核。
- 受控沙箱不允许 Unix socket `bind()`，真实 wire 测试需要在获准的非沙箱环境运行。

Next authorized action: implement and validate PoC 3 only; preserve all PoC 0A through PoC 2 regressions
