# Window Chrome Architecture

窗口 chrome（标题栏、导航栏、窗口背景）按操作系统采用不同的原生方案，逻辑结构就是：

```
if (Windows)      -> 原生 DWM Mica + 自绘 caption
else if (macOS)   -> 原生 NSVisualEffectView vibrancy + unified title bar + traffic lights
else              -> 降级：原生窗口装饰，纯色背景
```

但这个 `if/else` **不是运行时分支**，而是拆成了两个维度，这是阅读代码时最容易困惑的地方。

## 两个维度

chrome 工作被刻意拆成两层，让可复用控件保持 platform-agnostic（不含 `#ifdef`）：

1. **原生窗口集成**（window flags、resize hit-test、traffic lights、安装 Mica/vibrancy）
   → `WindowChromeCompat` + 三个平台文件。
2. **绘制 / 颜色**（标题栏、导航栏透明 vs 纯色、tint）
   → 在可复用控件 `Window.cpp` / `TitleBar.cpp` / `NavigationView.cpp` 里，只看一个布尔标记 `fluentMicaBackdrop`。

两层之间靠这一个 window property 握手：

```
WindowChromeCompat 判断「本 OS 有系统背景」并安装它 -> 设置 fluentMicaBackdrop=true
   -> TitleBar / NavigationView 看到 true -> 透明绘制（露出背景）
   -> 看到 false -> 绘制纯色降级背景
```

## 分层结构

```
┌─ 可复用控件（不含 #ifdef，只看 fluentMicaBackdrop 一个标记） ─┐
│   Window.cpp · TitleBar.cpp · NavigationView.cpp              │
└───────────────▲──────────────────────────┬──────────────────┘
   sets fluentMicaBackdrop                  │ 调用原生操作
                │                           ▼
        ┌─────────────────────────────────────────────┐
        │  WindowChromeCompat  (路由 / façade)          │
        │  只做转发：每个方法 -> detail::xxx(...)         │
        │  + 平台中立 helper：classifyHitTest（几何）、    │
        │    canBeginSystemOperation（守卫）、            │
        │    usesCustomWindowChrome / prefersNativeMacControls（策略布尔）│
        └───────────────────────┬─────────────────────┘
                  编译期选一个文件（detail:: 的实现体）
        ┌───────────────┬───────────────┬───────────────┐
        ▼               ▼               ▼
  _win.cpp         _mac.cpp         _fallback.cpp
  if Windows       #ifdef Q_OS_MAC  #if !win && !mac
  DWM Mica         NSVisualEffectView   no-ops
  + 自绘 caption    + unified titlebar   原生装饰
```

`WindowChromeCompat.cpp` 本身几乎没有平台逻辑——它是 switchboard：每个 public 方法都是一行转发到 `detail::xxx`，而 `detail::xxx` 的函数体分别实现在三个平台文件里。`usesCustomWindowChrome()` / `prefersNativeMacControls()` 这两个布尔，就是 `if win` / `else if mac` 本身。

## 想读哪部分，就打开哪个文件

| 想看的东西 | 去哪 |
|---|---|
| 某个 OS 的原生实现（`if/else` 三个分支） | `WindowChromeCompat_win.cpp` / `_mac.cpp` / `_fallback.cpp`，一个分支一个文件 |
| 颜色 / 透明 vs 纯色绘制 | 不在 `WindowChromeCompat`——在 `Window.cpp` / `TitleBar.cpp` / `NavigationView.cpp`，都是 `if (fluentMicaBackdrop) …` |
| 路由 + 平台中立 helper | `WindowChromeCompat.cpp`（转发、`classifyHitTest`、守卫、策略布尔） |
| 公共接口与契约 | `WindowChromeCompat.h` |

## 各 OS 的方案

| 分支 | 文件 | title bar / nav | 背景 |
|---|---|---|---|
| Windows | `_win.cpp` | 自绘 caption（去掉 `WS_CAPTION`），自定义 hit-test | DWM Mica 系统背景 |
| macOS | `_mac.cpp` | 原生 traffic lights + full-size content + 透明 titlebar | 两层兄弟 `NSView`：Mica 使用 window-background vibrancy + 较强 tint，Acrylic 使用 sidebar vibrancy + 较弱 tint |
| 其它 | `_fallback.cpp` | 原生窗口装饰（无自绘 caption） | 纯色 `themeBackdrop` |

`platformSupportsSystemBackdrop()` 决定 `fluentMicaBackdrop` 是否为真：Windows / macOS 为真（走透明 + 系统背景），其它为假（走纯色降级）。

## macOS 半透明清除注意事项（踩过的坑）

macOS 半透明顶层（vibrancy）下，系统**不会自动清除后备缓冲**。任何在 mica 下透明/半透明绘制以露出背景的控件，若重绘/动画/滚动，会出现：

- **重影**（不透明内容叠加）、
- **累加发黑**（半透明描边每帧叠加 alpha）、
- **启动亮缝**（vibrancy 合成前的那一帧露出满强度 tint ≈243）。

统一修法：每帧用 `QPainter::CompositionMode_Source` 擦除，**并门控在顶层确实半透明时**（`WA_TranslucentBackground` / `fluentMicaBackdrop`）——未门控的 Source 擦除会写入 `RGBA(0,0,0,0)`，在不透明窗口上渲染成黑。已应用于 `Window.cpp`、`StackContentHost.cpp`、`TitleBar.cpp`、`TreeView.cpp`（`!m_backgroundVisible` 时）、`NavigationView.cpp`（清 chrome rect）、Gallery 内容页，以及 `GalleryNavigationPane.cpp` 的页脚分隔线。页面栈切换时应在显示新透明页面前同步清除旧页面区域。另外：**子控件不要单独设 `WA_TranslucentBackground`**——macOS 上会把它提升为独立图层，容易在 vibrancy 合成期间产生白线或残影。
