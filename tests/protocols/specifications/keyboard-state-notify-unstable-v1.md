# `treeland-keyboard-state-notify-unstable-v1` 测试规范

## 范围

- XML / interface：`treeland_keyboard_state_notify_manager_v1` / `treeland_keyboard_state_watcher_v1`
- 测试源码：`tests/protocols/treeland-keyboard-state-notify-unstable-v1/`
- Fixture：desktop integration fixture（`protocol_test_desktop_setup` 创建 headless output）
- 覆盖等级：**P / E**。

## 必须观察到的结果

| 场景 | 客户端动作 | 必须观察到的结果 |
| --- | --- | --- |
| Null seat watcher | `get_keyboard_state_watcher(null)` | watcher 创建成功，无 crash |
| Apply with modifiers | `set_modifiers(CAPS_LOCK)` + `set_flags(LOCKED\|UNLOCKED)` + `apply` | 查询并返回当前 Caps Lock 状态；无匹配 modifier 时不发送事件 |
| Real seat watcher, no modifiers | 绑定真实 seat 的 watcher 后 `apply` 无 modifiers | 不发送任何事件，不崩溃 |
| Virtual keyboard 状态改变 | 为真实 seat 创建 virtual keyboard，依次写入和清除 Caps Lock locked modifier | 客户端依次收到 `state_changed(CAPS_LOCK, LOCKED)` 与 `state_changed(CAPS_LOCK, UNLOCKED)` |

## 已证明的生产链路

基础用例证明协议接口的绑定、双缓冲配置和空配置行为正常。

客户端绑定 `keyboard_state_notify_manager_v1` 后，`get_keyboard_state_watcher(null)`
创建 watcher。`set_modifiers` + `set_flags` + `apply` 触发 double-buffer 语义：先
累加 modifiers 和 flags，再在 `apply` 时一次性查询当前键盘状态并发送
`current_state`。headless 环境中 `getSeatKeyboard()` 返回 `nullopt`（无物理键盘设备），
因此不会发送 `current_state` 事件。测试断言 watcher 创建成功且 `apply` 不崩溃，证明
空 modifier 和空键盘路径不会触发空遍历。

E 级用例通过 `zwp_virtual_keyboard_manager_v1` 为同一真实 `wl_seat` 创建 virtual
keyboard，并先提交 XKB keymap。watcher 订阅 Caps Lock 的 locked/unlocked 状态后，客户端
调用 virtual keyboard 的 `modifiers(0, 0, caps_mask, caps_mask)`，再调用
`modifiers(0, 0, 0, 0)` 清除它。两次 Wayland round-trip 分别必须收到：

```text
virtual keyboard modifiers
  → WInputMethodHelper::handleNewVKV1 创建并 attach WInputDevice
  → WSeat::on_keyboard_modifiers
  → KeyboardStateNotifyManagerInterfaceV1 读取真实 seat keyboard state
  → treeland_keyboard_state_watcher_v1.state_changed
```

测试断言两条 event 的 modifier 都是 `CAPS_LOCK`，状态依次为 `LOCKED`、`UNLOCKED`。
这不是 fixture 手工发送 event，也不依赖固定延时或物理键盘，因此该路径为 E 级。

服务端实现位于
`src/modules/keyboard-state-notify/keyboardstatenotifymanagerinterfacev1.h`、
`keyboardstatenotifymanagerinterfacev1.cpp`。

## 已知边界 / 下一项结果

- 物理键盘产生的 key-event 路径：当前验证的是 virtual keyboard 的 modifier 更新路径，
  尚未比较物理按键与 virtual keyboard 的行为一致性。
- `current_state` 的初始值：未单独覆盖 watcher 在创建时已处于 Caps Lock locked 状态的
  初始同步。
- 多 watcher：多个 watcher 同时监听同一或不同 seat 的行为未验证。
- seat 销毁后 watcher 行为：seat 销毁时 watcher 是否正确清理或变为 inert 未验证。
- re-apply 语义：重复 `apply` 相同 modifier 集合是否去重或重发事件未验证。
