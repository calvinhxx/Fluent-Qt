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

## 绘制层级（z-order）与透明语义

整窗按「背后 → 前」分三层，每层只看 `fluentMicaBackdrop` 一个布尔决定「透明露背景 vs 自绘纯色」：

```
z-0  底层背景    Window 自身
z-1  chrome      标题栏 + 左侧导航栏（Top 模式为顶部导航条）
z-2  内容岛      内容宿主 StackContentHost + 页面（右下，左上圆角）
```

| 层 | Normal | Mica / Acrylic |
|---|---|---|
| z-0 背景 | 不透明 `themeBackdrop`（=`bgCanvas`；失焦洗向 `bgLayer`） | 窗口画**透明**，DWM / 系统合成出 Mica / Acrylic |
| z-1 chrome | 不透明 `bgCanvas` | 画**透明 → 透出 z-0** |
| z-2 内容岛 | 不透明 `bgLayer` / `bgLayerAlt` | **仍不透明** `bgLayer`（从不露背景） |

**关键非对称（最容易搞错）：**

- **z-1 chrome 会透**：只有 Normal 才有自己的纯色；Mica / Acrylic 下它画透明、露出 z-0。所以 Mica 下标题栏 / 导航栏看起来与背景「统一」，本质是二者都透明、露的是同一个 z-0，而非它们自带一层统一色。
- **z-2 内容岛永远不透**：三种模式都是实色岛，从不露背景。（Mica 下内容宿主会先 `CompositionMode_Source` 擦透明，仅为清掉切页残影，随后不透明页面盖上——肉眼始终是实色。）

一句话：**chrome 跟背景走（透明 / 纯色二选一），内容岛恒为实色。**

**由此推出的硬规则：** 任何「擦透明以露背景」的 `Source` 擦除，必须门控在 **`fluentMicaBackdrop`（真有系统背景）**，而**不是**裸的 `WA_TranslucentBackground`。因为 Windows 是「始终半透明」模型——Normal 模式窗口同样 `WA_TranslucentBackground=true`，若按它判定，Normal 下也会擦透明，但背后并无系统背景 → 露出**黑色**。踩坑实例：导航栏 `TreeView`（`!m_backgroundVisible`）在 Normal 把 viewport 擦成透明，渲染成 `#000000`，而标题栏自绘 `bgCanvas` 是 `#202020`，形成「标题栏 ≠ 导航栏」的缝（2026-06-23 修：门控改为 `fluentMicaBackdrop`，Normal 下不擦，让背后不透明 chrome 透出）。

**同理也作用于内容岛的圆角缺口（2026-06-24）。** 真正画这个圆角的是 `ContentFrameOverlay`（[NavigationView.cpp:89](../../src/components/navigation/NavigationView.cpp)）——一个 `WA_TransparentForMouseEvents`+`WA_NoSystemBackground` 的小 overlay，在带框侧边模式被 `raise()` 叠在内容宿主**之上**（半径 8 + 极淡的 strokeDivider 描边）。它的 `paintEvent` **无条件**用 `CompositionMode_Clear` 把左上圆角缺口挖透明来「露背景」。Mica 下正确；但 Normal 下 Clear 在不透明 chrome 背板上写入 (0,0,0,0)，于是在标题栏/导航栏/内容三交界处渲染出一小块**杂色块**（dark 背板上是黑、light 背板上是近白 #FFFFFF）。修：把这段 Clear 门控在 `fluentMicaBackdrop`——Normal 不挖，缺口保留背后的不透明内容/chrome 色（bgCanvas，无缝），只留细圆角描边；Mica 照挖露背景。**教训：凡是贴着「始终半透明」后备缓冲做 `Source`/`Clear` 擦除的层都必须门控在 `fluentMicaBackdrop`；而且别想当然认为圆角是背景控件画的——这里画它的是 `raise()` 在最上层的透明 overlay。**（曾误判为 `StackContentHost` 并改了它的 `paintEvent`，纯属空操作：宿主的 paint 根本到不了屏幕，被滚动区的不透明 bgCanvas 和这个 overlay 盖住。）

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

统一修法：每帧用 `QPainter::CompositionMode_Source` 擦除，**并门控在「真有系统背景」时**（`fluentMicaBackdrop`，见上文 z-order 硬规则）——不要只看裸的 `WA_TranslucentBackground`：Windows 始终半透明，Normal 下它也为真，按它判定会在 Normal 露黑（未门控的 Source 擦除写入 `RGBA(0,0,0,0)`，在不透明窗口上同样渲染成黑）。已应用于 `Window.cpp`、`StackContentHost.cpp`、`TitleBar.cpp`、`TreeView.cpp`（`!m_backgroundVisible` 时）、`NavigationView.cpp`（清 chrome rect）、Gallery 内容页，以及 `GalleryNavigationPane.cpp` 的页脚分隔线。页面栈切换时应在显示新透明页面前同步清除旧页面区域。另外：**子控件不要单独设 `WA_TranslucentBackground`**——macOS 上会把它提升为独立图层，容易在 vibrancy 合成期间产生白线或残影。
