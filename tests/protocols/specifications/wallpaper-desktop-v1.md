# Wallpaper manager + shell 桌面联合路径测试规范

## 状态

- 测试源码：`tests/protocols/treeland-wallpaper-desktop-v1/`
- Fixture：完整生产 `Helper`、默认 headless output、生产 output QML 与
  `WallpaperSwitcher`。
- 覆盖等级：**E（已通过）**。

这不是新增 XML；它把 `treeland-wallpaper-manager-unstable-v1` 和
`treeland-wallpaper-shell-unstable-v1` 串成一条实际业务链。两份各自的基础测试仍保留其
资源、事件和错误覆盖。

## 输入与顺序

客户端只创建一个无 role 的 `wl_surface`，并按以下严格顺序使用它：

1. 绑定实际存在的 `wl_output`、`treeland_wallpaper_manager_v1` 和 version 2 的
   `treeland_wallpaper_shell_v1`。
2. 调用 `manager.get_treeland_wallpaper(output, surface)`，然后发送
   `wallpaper.set_image_source("/tmp/treeland-protocol-wallpaper-red", DESKTOP)`。
   当前实现把该 source 写进这个 output 当前 workspace 的生产
   `WallpaperManager` 配置。
3. 用**同一个** `wl_surface` 调用
   `shell.get_treeland_wallpaper_surface(surface, source)`，从而创建真正的
   `TreelandWallpaperSurfaceInterfaceV1`，而不是建立独立的测试 surface。
4. 创建 `64×64`、`wl_shm/ARGB8888`、每个像素为 `0xffff0000` 的 buffer，将其 attach/
   damage/commit 到该 wallpaper surface，最后发送 `wallpaper_surface.ready()`。
5. 通过 `wl_display_roundtrip()` 确认服务端已按请求顺序处理；测试不以时间延迟判断状态。

这里的路径标识符不是待解码 JPEG 文件。它用于验证当前生产实现的 manager 配置与 shell
surface 协作；视觉内容来自 wallpaper client 真正提交的红色 `wl_shm` buffer。

测试实施时发现 `TreelandWallpaperSurfaceInterfaceV1` 创建的 surface 没有 xdg/layer
shell role，wlroots 因而不会在 buffer commit 后自动 map。生产 `ready` 路径现会在确认已
提交有效 buffer 后调用 `WSurface::map()`；该测试已验证 `mapped=1` 与 `wallpaperReady=1`。

## 必须观察到的生产结果

服务端同步读取生产对象时，以下结果必须同时成立：

| 检查 | 生产对象/结果 |
| --- | --- |
| shell 注册 | `TreelandWallpaperSurfaceInterfaceV1::get(source)` 返回刚创建的对象 |
| surface 生命周期 | 该对象的 `WSurface` 已 mapped，且 `wallpaperReady()` 为真 |
| manager 关联 | `TreelandWallpaperInterfaceV1::getReferenceWallpaperInterfaceFromSurface(surface)` 返回对象，并能得到同一个真实 `WOutput` |
| 配置写入 | `Helper::currentWorkspaceWallpaper(output)` 等于客户端发送的 source |
| QML 应用 | 运行中的、输出匹配的 `WallpaperSwitcher::source()` 等于 source |
| 实际 surface 绑定 | 此 `WallpaperSwitcher` 的 `WSurfaceItemContent::surface()` 就是 shell surface 的 `WSurface` |
| 内容与 buffer | 该 content 可见；其绑定的 `WSurface` 已提交 `64×64` buffer。`WallpaperItem` 自身会铺满 output，因此不以 item 的 `width/height` 判断 buffer 尺寸 |

因此通过条件不是 `ready` 事件，也不是 `WallpaperManager` 内部 map 有值，而是 manager 写入的
配置已经使生产 QML 的 `WallpaperSwitcher → WallpaperItem → WSurfaceItemContent` 持有客户端
提交的同一张 wallpaper surface。

## 已知边界

- 尚未读取最终 output backing buffer，因此尚未达到 V 级；红色 buffer 的提交与 item 几何
  只能证明 content 已接入生产场景，不能证明最终合成像素。
- 未覆盖 JPEG/视频文件校验、解码失败、notifier 的真实来源，也未覆盖锁屏 role、跨 workspace
  切换和淡入淡出动画完成。
