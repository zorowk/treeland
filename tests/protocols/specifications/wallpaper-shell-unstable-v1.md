# `treeland-wallpaper-shell-unstable-v1` 测试规范

## 范围

- 测试源码：`tests/protocols/treeland-wallpaper-shell-unstable-v1/`
- Fixture：协议 fixture。
- 覆盖等级：**I/P**。

## 实际请求与预期结果

| 场景 | 客户端发送 | 生产业务逻辑与断言 |
| --- | --- | --- |
| shell surface | 用 `wl_surface` 创建 wallpaper surface | production shell 跟踪资源创建与销毁 |
| 生命周期事件 | 驱动 failed/ready/play/pause/slow-down 路径 | 客户端收到相应资源事件 |
| notifier | 订阅并驱动 add/remove | 收到 notifier 的 payload 与生命周期事件 |

## 已证明的生产链路

覆盖 wallpaper shell 与 notifier 的生产资源实现。

## 未覆盖

基础测试没有 output 上的 mapped wallpaper surface 或渲染读回；事件不代表壁纸已经可见或
播放。与 manager 共同驱动生产 `WallpaperSwitcher` 的路径已由
[`wallpaper-desktop-v1.md`](wallpaper-desktop-v1.md) 通过；仍未覆盖最终 output 像素与媒体
播放。
