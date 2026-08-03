# Treeland Wayland Protocol Test Implementation Status

Current stage: PoC 1
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B]

Stage commits:
- `a5f57dc8989ccefebfc4e956c383d52ae07a9afb` feat(protocol-scanner): feat(protocol): generate C test client adapters
- `958f759981f511542da4a5adf69a75fc9161a1ef` test(protocol): exercise generated protocol adapters

Automated validation:
- `qtwaylandscanner_treeland` 的 `test-client-header`、`test-client-code` 和
  `test-client-metadata` 模式构建并生成成功。
- 相同 XML 连续生成两次，header、code、metadata 均逐字一致；生成 C 通过语法检查。
- metadata 自动核对 interface/version/request/event/argument/destructor/allow_null：通过。
- handwritten/generated 分别通过 PoC 0A wire 和 PoC 0B Helper high 场景。
- missing-listener self-test 以 `checkpoint_event_diff` 正确失败。
- handwritten/generated wire 与 high 规范化契约对照：通过。
- `ctest --test-dir build-feat-testprotocol -R
  'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract)'
  --output-on-failure`：9/9 通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target generate-window-management-test-adapter \
           test-protocol-window-management-wire \
           test-protocol-window-management-wire-generated \
           test-protocol-window-management-wire-generated-missing-listener \
           test-protocol-window-management-wire-wrong-expected \
           test-protocol-window-management-helper \
           test-protocol-window-management-helper-generated \
           test-protocol-window-management-helper-wrong-expected \
           protocol-adapter-contract-diff \
  -j8

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract)' \
  --output-on-failure
```

Human validation: pending

Known issues:
- PoC 1 只为已接受的 window-management 单 interface 场景生成 adapter；复杂 event 参数、
  `new_id`、fd/array/fixed 和多协议对象仍属于后续阶段。
- handwritten adapter 继续作为 golden，不得在 PoC 1 人工验收前删除。
- 完整 `Helper` 仍会创建 DConfig 等进程级常驻设施；high 测试沿用已接受的进程边界退出策略。
- 受控沙箱不允许 Unix socket `bind()`，真实 wire 测试需要在获准的非沙箱环境运行。

Next authorized action: wait for human validation of PoC 1; do not start PoC 2 or remove the handwritten golden adapter
