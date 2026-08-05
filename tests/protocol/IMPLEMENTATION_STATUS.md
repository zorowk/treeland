# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D6 (CI stabilization)
State: awaiting_human_validation
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3,
  MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d, MVP-D4e, MVP-D4f, MVP-D4g, MVP-D5]

Stage commits (D5 and prior):
- `b301ca0e1` test(protocol): accept MVP-D5 stage
- `1f0c3910d` docs(protocol): hand off MVP-D5 validation
- `2da14d33c` test(protocol): cover global lifecycle and version matrix

Automated validation (D6):
- 100x repeat on generated-adapter tests (array/fixed/fd/new-id/multi-arg/object-newid/
  global-version + window-management wire/high + scanner + missing-listener):
  连续 100 次全部通过 (140s)。
- 100x repeat on JSON runner tests (window-management-json + multi-arg-echo):
  连续 100 次全部通过 (20s)。
- CI workflow `.github/workflows/protocol-test.yml`: Debug/Release 构建 + ctest,
  ASan 构建 + ctest with LSan suppressions, 失败时上传 test-results artifact。
- CMakePresets.json: 新增 `ci-asan` 和 `ci-ubsan` configure/build presets。
- 所有测试无需 DISPLAY/WAYLAND_DISPLAY/GPU/DBus (QT_QPA_PLATFORM=offscreen)。
- 每 case 有明确超时 (TIMEOUT 10/30/40)。
- 无 `_Exit()` 掩盖析构问题；无固定时延同步。

Manual validation command:
```bash
# 100x repeat on generated adapter tests
QT_QPA_PLATFORM=offscreen \
ctest --test-dir build-feat-testprotocol \
  -L generated-adapter \
  --repeat until-fail:100 \
  --output-on-failure

# 100x repeat on JSON runner tests
QT_QPA_PLATFORM=offscreen \
ctest --test-dir build-feat-testprotocol \
  -L json-runner -R 'multi-arg-echo|window-management-json$' \
  --repeat until-fail:100 \
  --output-on-failure

# Full regression
ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd|new-id|multi-arg|object-newid|global-version)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure
```

Human validation: pending

Known issues:
- MVP-D7 (malicious client) 已放弃。
- CI workflow 仅在 GitHub Actions 可用；本地可通过 ctest 命令等价执行。
- UBSan 构建仅配置 preset，尚未独立运行完整 protocol 测试（UBSan 下 QtWayland 生成的
  xdg_popup 重复符号问题需评估）。
- 100x repeat 仅覆盖 wire 层和 JSON runner；full protocol-high Wine 场景因耗时较长
  (每个 case ~0.3s × 100 = 30s，共 10+ case = 300s+) 未纳入日常 smoke。

Next authorized action: 全部 MVP 阶段已完成。后续方向：D4h 批量协议 smoke、D4g 通用 runner、
或新协议能力扩展。
