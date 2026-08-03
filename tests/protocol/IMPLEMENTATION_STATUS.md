# Treeland Wayland Protocol Test Implementation Status

Current stage: PoC 3
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2]

Stage commits:
- `2d38dd31f4561d3147ec57c47e419268fdb84479` refactor(protocol): expose focused shell protocol initialization
- `c79617591cffb6bd52904fa63fcd45e2c1b79d0d` fix(protocol): reject Wine positions outside active outputs
- `5953ee035d47d7a2bb96dcbe7f35de13d0946934` test(protocol): exercise Wine window position behavior

Automated validation:
- 使用官方 Wayland scanner 生成 Wine private protocol 与 stable xdg-shell client bindings，
  并使用 generated metadata 做接口和版本预检：通过。
- 真实 `Helper::initShellProtocols()` → `ShellHandler::init()` 生产接线、headless output、
  mapped xdg_toplevel、shm buffer、跨协议 `new_id` window control fixture：通过。
- `set_position(100, 120)` 更新 `surface.geometry`，client event 回显请求 serial：通过。
- 1280x720 output 的原点 `(0, 0)` 接受；负坐标、右边界 `x=1280`、下边界
  `y=720` 和远端 `(1400, 800)` 均拒绝并保持当前 geometry：通过。
- client 在 request flush 后、completion 前断开，client/surface 数量恢复基线且客户端线程退出：通过。
- 错误位置 expected self-test 以 `checkpoint_probe_diff` 和目标 checkpoint 失败：通过。
- `ctest --test-dir build-feat-testprotocol -R
  'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner|protocol-wine-window-management'
  --output-on-failure`：24/24 通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target treeland-protocol-test-runner \
  -j8

./build-feat-testprotocol/tests/protocol/treeland-protocol-test-runner \
  --input tests/protocol/cases/wine-set-position.input.json \
  --expected tests/protocol/cases/wine-set-position.expected.json \
  --dump-actual build-feat-testprotocol/test-results/poc-3/wine-set-position.actual.json \
  --report-dir build-feat-testprotocol/test-results/poc-3/wine-set-position \
  --verbose

./build-feat-testprotocol/tests/protocol/treeland-protocol-test-runner \
  --input tests/protocol/cases/wine-set-position-outside-output.input.json \
  --expected tests/protocol/cases/wine-set-position-outside-output.expected.json \
  --dump-actual build-feat-testprotocol/test-results/poc-3/wine-set-position-outside-output.actual.json \
  --report-dir build-feat-testprotocol/test-results/poc-3/wine-set-position-outside-output \
  --verbose

./build-feat-testprotocol/tests/protocol/treeland-protocol-test-runner \
  --input tests/protocol/cases/wine-disconnect-before-completion.input.json \
  --expected tests/protocol/cases/wine-disconnect-before-completion.expected.json \
  --dump-actual build-feat-testprotocol/test-results/poc-3/wine-disconnect-before-completion.actual.json \
  --report-dir build-feat-testprotocol/test-results/poc-3/wine-disconnect-before-completion \
  --verbose

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-(wire|high)-window-management|protocol-window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|protocol-json-runner|protocol-wine-window-management' \
  --output-on-failure
```

Human validation: pending; PoC 3 expected remains candidate

Known issues:
- PoC 3 仅扩展 Wine window position 所需的最小 `new_id`、跨协议对象、
  xdg/shm fixture、serial 引用、`surface.geometry` probe 和 `server_condition`。
- 不扩展到多客户端、恶意 wire、通用 fd DSL、完整 array/fixed/event-new-id、
  frame presentation 或截图比较。
- PoC 3 expected 必须保持 `candidate`，直到人工审核。
- 受控沙箱不允许 Unix socket `bind()`，真实 wire 测试需要在获准的非沙箱环境运行。

Next authorized action: human-validate PoC 3 only; do not accept PoC 3 or start a later stage automatically
