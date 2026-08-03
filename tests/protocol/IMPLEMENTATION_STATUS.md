# Treeland Wayland Protocol Test Implementation Status

Current stage: PoC 2
State: implementing
Accepted stages: [PoC 0A, PoC 0B, PoC 1]

Stage commits: []

Automated validation:
pending

Manual validation command:
pending

Human validation: pending

Known issues:
- PoC 2 只将已接受的 window-management 单 interface 场景迁移到 JSON；不增加新协议能力。
- handwritten 与 generated adapter 场景必须继续作为回归 golden。

Next authorized action: implement only PoC 2 schema, metadata validation, linear JSON runner, actual output, structured diff, and required self-tests
