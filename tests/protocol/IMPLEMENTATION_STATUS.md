# Treeland Wayland Protocol Test Implementation Status

Current stage: MVP-D5 (global lifecycle and version matrix)
State: accepted
Accepted stages: [PoC 0A, PoC 0B, PoC 1, PoC 2, PoC 3, MVP-D1, MVP-D2, MVP-D3, MVP-D4a, MVP-D4b, MVP-D4c, MVP-D4d, MVP-D4e, MVP-D4f, MVP-D4g, MVP-D5]

Stage commits (D4g and prior):
- `0ed686ef5` test(protocol): accept MVP-D4g JSON runner stage
- `19b6780b4` docs(protocol): hand off MVP-D4g validation
- `31d1da462` test(protocol): wire JSON runner for generic multi-type protocol cases

Automated validation (D5):
- global/version self-test: advertised version 记录 (advertised_version=1)，bind version 1 和
  超出版本 clamp (requested 5 → bound 1) 全部正确：通过。
- `globalRemove()` 后已有 resource 继续可用，新 adapter 无法 bind (global_name=0)：通过。
- global 销毁重建后新版本重新广播，adapter 可重新 bind：通过。
- scanner 解析 XML `since` 属性（默认 1），纳入 metadata `since` 字段：通过。
- 生成 adapter 在 request wrapper 中增加 since 校验（`since > bound_version` 则返回 -1）：
  编译通过，自测验证 window-management 协议 `set_desktop` 的 since=1 不受影响。
- PoC 0A 至 MVP-D4g 加 D5 完整协议回归：44/44 通过。

Manual validation command:
```bash
cmake --build build-feat-testprotocol \
  --target protocol-global-version-selftest \
  -j8

ctest --test-dir build-feat-testprotocol \
  -L mvp-d5 \
  --output-on-failure

ctest --test-dir build-feat-testprotocol \
  -R 'protocol-((array|fixed|fd|new-id|multi-arg|object-newid|global-version)-adapter-selftest|(wire|high)-window-management|window-management-(generated-adapter-output|adapter-contract|json-contract|json-repeatable)|json-runner|wine-window-management)' \
  --output-on-failure
```

Human validation: passed

Known issues:
- D5 已实现 version clamping、global remove/rebind 和 since 校验。
- 当前 since 校验仅生成比较代码；尚无专门的高 since 值协议用于验证 since 边界行为
  （所有 treeland 协议 since 值均为 1）。
- version matrix 的数据驱动自动生成（如遍历所有协议的 version/bind 组合）未在本阶段实现。
- ASan/LSan 未单独验证（D5 无新增内存操作）。

Next authorized action: start MVP-D6 CI stabilization when explicitly requested; preserve all PoC 0A through MVP-D5 regressions
