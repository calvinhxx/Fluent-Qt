import { normalizeStudioLang } from './studio-locale.mjs';
import { isSafeStudioSvg, isStudioPhotoUrl } from './studio-media.mjs';
import {
  ANCHOR_CHILD_SCHEMA,
  ANCHOR_PROP_KEYS,
  MAX_NODES,
  STUDIO_CATALOG,
  STUDIO_CATEGORIES,
  STUDIO_TEMPLATES,
  buildDragPreviewDocument,
  canAcceptChild,
  cloneDoc,
  countNodes,
  createHistory,
  createNode,
  decodeShareState,
  duplicateNode,
  findNode,
  findParent,
  generateCpp,
  generateDesignerUi,
  generatePython,
  insertNode,
  mapInteractionEventToProps,
  moveNode,
  nodeDepth,
  removeNode,
  resetNodeProps,
  resolveDropTarget,
  updateNodeProps,
} from './studio-core.mjs';
import {
  AI_PROVIDER_PRESETS,
  changeAiEndpoint,
  providerOrigin,
  generateStudioPageWithAi,
} from './studio-ai.mjs';

const shortcuts = /^(mac|ip(?:hone|ad|od)|ios)/i.test(navigator.userAgentData?.platform || navigator.platform)
  ? { ai: '⌘K', undo: '⌘Z', redo: '⇧⌘Z', duplicate: '⌘D' }
  : { ai: 'Ctrl+K', undo: 'Ctrl+Z', redo: 'Ctrl+Shift+Z', duplicate: 'Ctrl+D' };

const UI_STRINGS = {
  zh: {
    'toolbar.code': '导出代码',
    'toolbar.ai': 'AI 生成',
    'toolbar.templates': '模板',
    'templates.title': '场景预设模板',
    'templates.subtitle': '选择一套页面原型直接编辑与试玩，一键导出 C++17、PySide6 或 Qt Designer .ui',
    'templates.loadedToast': '已加载「{name}」· {undo} 可撤销',
    'ai.title': 'AI 新建页面',
    'ai.run': '生成界面',
    'ai.running': '生成中…',
    'ai.promptPlaceholder': '一句话描述你想新建的 Fluent-Qt 桌面界面（Enter 生成）',
    'ai.settingsSummary': 'AI 配置',
    'ai.providerLabel': '生成引擎',
    'ai.providerAuto': '免费云端',
    'ai.providerCustom': '自定义 API',
    'ai.workerUrlLabel': 'Cloudflare Worker 地址',
    'ai.workerHint': '填入已部署的免费 Worker 地址；没有 Worker 时请切换「自定义 API」填写你自己的 Key 或 Ollama。',
    'ai.presetLabel': '快捷填充',
    'ai.baseUrlLabel': 'Base URL',
    'ai.modelLabel': '模型名称',
    'ai.apiKeyLabel': 'API Key（仅存当前浏览器，本地 Ollama 可留空）',
    'ai.statusGenerating': '正在构建 Fluent-Qt 桌面布局…',
    'ai.statusThinking': '模型正在构思桌面布局…',
    'ai.statusStreaming': '正在生成界面（已构思约 {widgets} 个组件）…',
    'ai.statusEmptyPrompt': '请先输入你想生成的页面描述。',
    'ai.generatedToast': '已生成 {count} 个组件（{source}）· {undo} 可撤销',
    'ai.modeCreate': '新建整页',
    'ai.modeIterate': '迭代当前页',
    'ai.modeRefine': '精修选中',
    'ai.promptIteratePlaceholder': '描述你想如何修改当前页面（例如：给左侧加搜索框、把按钮改成图标按钮）',
    'ai.promptRefinePlaceholder': '描述如何局部优化当前选中组件（不影响页面其余区域）',
    'ai.targetLabel': '目标：',
    'ai.styleAuto': '智能匹配',
    'ai.styleWinui': 'WinUI 浅蓝',
    'ai.styleCyber': '暗青监控',
    'ai.styleEditorial': '珊瑚画廊',
    'ai.styleCreative': '暗紫创意',
    'ai.styleEmerald': '翡翠商务',
    'artboard.light': '浅色',
    'artboard.dark': '暗色',
    'artboard.reset': '还原',
    'mode.edit': '编辑',
    'mode.play': '试玩',
    'viewport.desktop': '桌面',
    'viewport.tablet': '平板',
    'viewport.compact': '紧凑',
    'pane.palette': '组件',
    'pane.outline': '图层',
    'pane.inspector': '属性',
    'stage.retryWasm': '重试 WASM',
    'drawer.copy': '复制代码',
    'drawer.download': '下载文件',
    undoTitle: '撤销 ({undo})',
    redoTitle: '重做 ({redo})',
    duplicateTitle: '复制选中节点 ({duplicate})',
    searchPlaceholder: '搜索 103 个组件与布局…',
    anchorSectionTitle: '锚点定位 (AnchorLayout)',
    containerTag: '容器',
    qtTag: 'Qt',
    qtTagHint: '标准 Qt 布局容器（非 uilib 控件）',
    pluginTag: '插件',
    pluginTagHint: '可选扩展目标 {target}（非核心 uilib）',
    pageRoot: '页面',
    inspectorRootTip: '点击画布控件自由拖拽定位或修改属性，也可从左侧拖入新控件。',
    clearCanvasBtn: '清空画布',
    restoreSampleBtn: '恢复默认示例',
    wasmLoading: 'WASM 加载中…',
    wasmReady: 'WASM 就绪',
    schematicMode: '示意图模式',
    'schematicBanner.text': '当前处于 HTML/CSS 示意图预览，部分控件仅为结构近似，非真实 C++ uilib 渲染。',
    'schematicBanner.localHint': '本地启用真实 WASM：',
    'schematicBanner.dismiss': '关闭提示',
    'loader.title': '正在启动 C++ Qt 实时画布',
    'loader.fetching': '正在加载 WebAssembly 运行时…',
    'loader.mounting': '正在初始化控件树与排版…',
    rootProtected: '根页面容器不可删除或移动',
    copiedCode: '代码已复制到剪贴板',
    emptyContainer: '拖拽控件到此容器中，或点击左侧 + 添加',
    noSelectionProps: '当前控件无可配置属性',
    playHint: '试玩模式 · 直接点击画布控件触发真实事件',
    catBasic: '基础属性',
    catIconLayout: '图标与布局',
    catIcons: '图标',
    catLayout: '布局',
    catAppearance: '交互与外观',
    resetProps: '重置为默认属性',
    propsResetToast: '已恢复默认属性',
    followTheme: '跟随主题',
    mainIcon: '主图标',
    icon: '图标',
    dropdownIndicator: '下拉箭头',
    queryIcon: '查询图标',
    notSet: '未设置',
    propertyCategories: '属性分类',
    checkableHint: '开启后变为开关按钮，通过下方“已选中”属性查看效果',
    pageTemplate: '第 {page} 页',
    aiTokens: '{in}入/{out}出',
    aiCached: '{base} · 缓存 {rate}% · {tok}',
    addWidget: '添加 {type}',
    defaultGridHeaders: ['项目', '详情', '状态'],
    dragInto: '放入',
    dragMoveTo: '移动到',
    dragFreeAnchor: '自由定位',
    dragNoop: '保持当前位置',
    dragCancelHint: 'Esc 取消',
    'intro.guideTitle': '使用引导',
    'intro.title': '欢迎使用 Fluent-Qt Studio',
    'intro.subtitle': '真实 C++ Qt Widgets 可视化工作台 · 拖拽组装、AI 生成、三端代码导出',
    'intro.step1': '拖入 103 个真实 Qt 控件，或按 {ai} 一句话生成整页',
    'intro.step2': '画板独立切换明暗与主题色，切「试玩」直测真实交互',
    'intro.step3': '一键导出 C++17、PySide6 或共享 Qt Designer .ui',
    'intro.startTour': '开始 30 秒交互导览',
    'intro.tryAi': '✦ 试试 AI 生成',
    'intro.loadSample': '加载示例仪表盘',
    'intro.sampleLoadedToast': '已加载监控仪表盘示例 · {undo} 可返回初始画布',
    'tour.prev': '上一步',
    'tour.next': '下一步',
    'tour.done': '开始创作',
    'tour.step1Title': '103 个真实 C++ Fluent-Qt 控件',
    'tour.step1Desc': '从左侧分类搜索或直接拖拽任意布局、图表与控件到画布，所有渲染均由真实 Qt Widgets WebAssembly 驱动。',
    'tour.step2Title': '一句话生成整页或局部精修 ({ai})',
    'tour.step2Desc': '输入自然语言即可生成完整桌面布局，支持「新建整页」「迭代当前页」和选中控件后点 ✦ 局部精修。',
    'tour.step3Title': '独立画板调色与实时试玩',
    'tour.step3Desc': '在画板顶部零 Token 切换明暗与 6 组强调色（不影响外层网站主题），切到顶部「试玩」即可直接点击交互控件。',
    'tour.step4Title': '属性检查与三端代码导出',
    'tour.step4Desc': '在右侧微调真实 Qt 属性后，一键导出可直接编译的 C++17、PySide6 或共享 Qt Designer .ui 表单。',
    optionLabels: {
      ChevronDown: '向下', ChevronUp: '向上', ChevronRight: '向右',
      Standard: '标准', Accent: '强调', Subtle: '轻量',
      Small: '小', StandardSize: '默认', Medium: '中', Large: '大', ExtraLarge: '特大',
      None: '无', Add: '加号', CheckMark: '对勾', Save: '保存', FavoriteStar: '星标', Settings: '设置',
      Search: '搜索', Mail: '邮件', Home: '主页', Document: '文档', Info: '信息', Warning: '警告', ErrorBadge12: '错误',
      TextOnly: '仅文字', IconBefore: '图标在前', IconAfter: '图标在后', IconOnly: '仅图标', IconAbove: '图标在上', IconBelow: '图标在下',
      Rest: '默认', Hover: '悬停', Pressed: '按下', Disabled: '禁用',
      Unchecked: '未选中', PartiallyChecked: '部分选中', Checked: '已选中',
      Horizontal: '水平', Vertical: '垂直',
      NoTicks: '无刻度', TicksAbove: '上方刻度', TicksBelow: '下方刻度', TicksLeft: '左侧刻度', TicksRight: '右侧刻度', TicksBothSides: '双侧刻度',
      Peek: '点击预览', Hidden: '隐藏', Visible: '始终显示',
      Left: '左侧', Right: '右侧', Leading: '首端', Trailing: '尾端',
      Single: '单选', Multiple: '多选', Extended: '扩展多选',
      Compact: '紧凑', Inline: '行内',
      Caption: '辅助说明', Body: '正文', BodyStrong: '正文加粗', BodyLarge: '大号正文', BodyLargeStrong: '大号正文加粗',
      Subtitle: '副标题', Title: '标题', TitleLarge: '大标题', Display: '展示标题',
      ElideLeft: '左侧省略', ElideRight: '右侧省略', ElideMiddle: '中间省略', ElideNone: '不省略',
      Auto: '自动', Automatic: '自动', Stacked: '上下堆叠', Circular: '圆形', Square: '方形',
      Available: '在线', Away: '离开', Busy: '忙碌', DoNotDisturb: '请勿打扰', Offline: '离线',
      Dot: '圆点', Icon: '图标', Value: '数值',
      Informational: '通知', Attention: '关注', Caution: '警告', Success: '成功', Critical: '严重', Error: '错误', Validating: '校验中',
      Running: '运行中', Paused: '已暂停', Branded: '品牌样式', Simple: '简洁样式', Custom: '自定义',
      Layer: '默认层', LayerAlt: '交替层', Canvas: '画布底色', Beginning: '开头省略', Middle: '中间省略',
      List: '列表', Detail: '详情', LeftCompact: '左侧紧凑', LeftMinimal: '左侧极简',
      ScrollButtons: '滚动按钮', MoreButton: '更多按钮', Equal: '等宽', SizeToContent: '自适应内容',
      OnHover: '悬停显示', Always: '始终显示', Manual: '手动', Contain: '适应内容', ScaleDown: '缩小适应',
      Zoom: '缩放', Pan: '平移', Collapsed: '折叠', VisibleOnPointerOver: '悬停显示',
      FastTransformation: '快速采样', SmoothTransformation: '平滑采样',
      Line: '折线图', Area: '面积图', Bar: '柱状图', HorizontalBar: '条形图', Pie: '饼图', Donut: '环形图', Scatter: '散点图', Sparkline: '迷你图',
      Alternate: '交替', SlideFade: '滑动淡入', ScaleFade: '缩放淡入', Immediate: '立即切换', Transient: '轻量模式',
      Solid: '纯色', Transparent: '透明背景', FlowingRibbons: '流动光带', FloatingDots: '漂浮光点', Starfield: '星空穿梭',
      Raster: 'CPU 光栅', OpenGL: 'OpenGL 硬件', Mica: '云母 (Mica)', Acrylic: '亚克力 (Acrylic)',
      Stretch: '拉伸填充', Start: '顶部对齐', Center: '居中', End: '底部对齐',
      TopLeft: '左上角', TopCenter: '顶部居中', TopRight: '右上角', BottomLeft: '左下角', BottomCenter: '底部居中', BottomRight: '右下角',
      TopBar: '顶部通栏', BottomBar: '底部通栏', LeftDock: '左侧停靠', RightDock: '右侧停靠', Fill: '全幅填充',
      Top: '顶部', Bottom: '底部', Day: '日', Month: '月', Year: '年',
      Yes: '是', No: '否',
    },
  },
  en: {
    'toolbar.code': 'Export Code',
    'toolbar.ai': 'AI Build',
    'toolbar.templates': 'Templates',
    'templates.title': 'Starter & Industry Presets',
    'templates.subtitle': 'Load a runnable Fluent-Qt page preset to edit, play, or export as C++17, PySide6, or .ui',
    'templates.loadedToast': 'Loaded "{name}" · {undo} to undo',
    'ai.title': 'AI New Page',
    'ai.run': 'Generate',
    'ai.running': 'Generating…',
    'ai.promptPlaceholder': 'Describe the Fluent-Qt desktop UI you want to create (Enter to run)',
    'ai.settingsSummary': 'AI Config',
    'ai.providerLabel': 'Provider',
    'ai.providerAuto': 'Free Cloud',
    'ai.providerCustom': 'Custom API',
    'ai.workerUrlLabel': 'Cloudflare Worker URL',
    'ai.workerHint': 'Enter a deployed free Worker URL, or switch to Custom API to use your own key or Ollama.',
    'ai.presetLabel': 'Presets',
    'ai.baseUrlLabel': 'Base URL',
    'ai.modelLabel': 'Model',
    'ai.apiKeyLabel': 'API Key (stored locally in browser; optional for localhost Ollama)',
    'ai.statusGenerating': 'Architecting Fluent-Qt desktop layout…',
    'ai.statusThinking': 'Thinking through desktop layout…',
    'ai.statusStreaming': 'Generating UI (~{widgets} widgets)…',
    'ai.statusEmptyPrompt': 'Enter a description of the page you want to create.',
    'ai.generatedToast': 'Generated {count} widgets ({source}) · {undo} to undo',
    'ai.modeCreate': 'New Page',
    'ai.modeIterate': 'Iterate Page',
    'ai.modeRefine': 'Refine Selected',
    'ai.promptIteratePlaceholder': 'Describe edits to apply on top of the current page (Enter to run)',
    'ai.promptRefinePlaceholder': 'Describe how to refine ONLY the selected component/container',
    'ai.targetLabel': 'Target: ',
    'ai.styleAuto': 'Auto Match',
    'ai.styleWinui': 'WinUI Light',
    'ai.styleCyber': 'Cyber Dark',
    'ai.styleEditorial': 'Coral Light',
    'ai.styleCreative': 'Violet Dark',
    'ai.styleEmerald': 'Emerald Light',
    'artboard.light': 'Light',
    'artboard.dark': 'Dark',
    'artboard.reset': 'Reset',
    'mode.edit': 'Edit',
    'mode.play': 'Play',
    'viewport.desktop': 'Desktop',
    'viewport.tablet': 'Tablet',
    'viewport.compact': 'Compact',
    'pane.palette': 'Components',
    'pane.outline': 'Layers',
    'pane.inspector': 'Properties',
    'stage.retryWasm': 'Retry WASM',
    'drawer.copy': 'Copy',
    'drawer.download': 'Download',
    undoTitle: 'Undo ({undo})',
    redoTitle: 'Redo ({redo})',
    duplicateTitle: 'Duplicate selected node ({duplicate})',
    searchPlaceholder: 'Search 103 widgets & layouts…',
    anchorSectionTitle: 'Anchor Placement (AnchorLayout)',
    containerTag: 'Container',
    qtTag: 'Qt',
    qtTagHint: 'Standard Qt layout container (not a uilib widget)',
    pluginTag: 'Plugin',
    pluginTagHint: 'Optional extension target {target} (outside core uilib)',
    pageRoot: 'Page',
    inspectorRootTip: 'Click or drag widgets freely on canvas to edit layout and properties, or drag controls from the left.',
    clearCanvasBtn: 'Clear Canvas',
    restoreSampleBtn: 'Restore Sample',
    wasmLoading: 'Starting WASM…',
    wasmReady: 'C++ WASM Ready',
    schematicMode: 'Schematic Preview',
    'schematicBanner.text': 'HTML/CSS schematic preview active — some controls are simplified approximations rather than live C++ uilib widgets.',
    'schematicBanner.localHint': 'Run live WASM locally:',
    'schematicBanner.dismiss': 'Dismiss notice',
    'loader.title': 'Starting C++ Qt Live Canvas',
    'loader.fetching': 'Loading WebAssembly runtime…',
    'loader.mounting': 'Initializing widget tree & layout…',
    rootProtected: 'Root page container cannot be deleted or moved',
    copiedCode: 'Code copied to clipboard',
    emptyContainer: 'Drop widgets here or click + in the palette',
    noSelectionProps: 'No configurable properties',
    playHint: 'Play mode · Interact with live widgets on canvas',
    catBasic: 'Basic',
    catIconLayout: 'Icon & Layout',
    catIcons: 'Icons',
    catLayout: 'Layout',
    catAppearance: 'Style & State',
    resetProps: 'Reset properties',
    propsResetToast: 'Properties reset to defaults',
    followTheme: 'Follow theme',
    mainIcon: 'Main icon',
    icon: 'Icon',
    dropdownIndicator: 'Dropdown indicator',
    queryIcon: 'Query icon',
    notSet: 'Not set',
    propertyCategories: 'Property categories',
    checkableHint: 'Enables toggle button behavior. Toggle "Checked" below to see the active state.',
    pageTemplate: 'Page {page}',
    aiTokens: '{in}in/{out}out',
    aiCached: '{base} · {rate}% cached · {tok}',
    addWidget: 'Add {type}',
    defaultGridHeaders: ['Item', 'Details', 'Status'],
    dragInto: 'Drop into',
    dragMoveTo: 'Move to',
    dragFreeAnchor: 'Free anchor',
    dragNoop: 'Keep position',
    dragCancelHint: 'Esc to cancel',
    'intro.guideTitle': 'Studio Guide',
    'intro.title': 'Welcome to Fluent-Qt Studio',
    'intro.subtitle': 'Real C++ Qt Widgets visual workbench · drag & drop, AI build, and 3-target code export',
    'intro.step1': 'Drag 103 real Qt widgets onto the stage, or press {ai} to build with AI',
    'intro.step2': 'Switch isolated artboard themes & accents, or toggle Play for live interaction',
    'intro.step3': 'Export production C++17, PySide6, or shared Qt Designer .ui files',
    'intro.startTour': 'Start 30s Tour',
    'intro.tryAi': '✦ Try AI Build',
    'intro.loadSample': 'Load Sample Dashboard',
    'intro.sampleLoadedToast': 'Loaded Analytics Dashboard sample · {undo} to undo',
    'tour.prev': 'Back',
    'tour.next': 'Next',
    'tour.done': 'Start Building',
    'tour.step1Title': '103 Real C++ Fluent-Qt Widgets',
    'tour.step1Desc': 'Search categories or drag any layout, chart, or widget onto the canvas — powered by real Qt Widgets in WebAssembly.',
    'tour.step2Title': 'Generate Pages or Refine Selection ({ai})',
    'tour.step2Desc': 'Describe any desktop UI to generate full layouts, iterate on the current page, or select a widget and click ✦ to refine just that part.',
    'tour.step3Title': 'Isolated Artboard & Live Play Mode',
    'tour.step3Desc': 'Switch artboard light/dark themes and 6 accent colors without changing the outer site theme, or toggle Play mode to interact live.',
    'tour.step4Title': 'Export C++, PySide6 & Qt Designer .ui',
    'tour.step4Desc': 'Fine-tune real Qt properties on the right, then export ready-to-build C++17, PySide6, or shared Qt Designer .ui files.',
    optionLabels: {
      ChevronDown: 'Down', ChevronUp: 'Up', ChevronRight: 'Right',
      Standard: 'Standard', Accent: 'Accent', Subtle: 'Subtle',
      Small: 'Small', StandardSize: 'Default', Medium: 'Medium', Large: 'Large', ExtraLarge: 'Extra large',
      None: 'None', Add: 'Add', CheckMark: 'CheckMark', Save: 'Save', FavoriteStar: 'Favorite star', Settings: 'Settings',
      Search: 'Search', Mail: 'Mail', Home: 'Home', Document: 'Document', Info: 'Info', Warning: 'Warning', ErrorBadge12: 'Error badge',
      TextOnly: 'Text only', IconBefore: 'Icon before text', IconAfter: 'Icon after text', IconOnly: 'Icon only', IconAbove: 'Icon above text', IconBelow: 'Icon below text',
      Rest: 'Rest', Hover: 'Hover', Pressed: 'Pressed', Disabled: 'Disabled',
      Unchecked: 'Unchecked', PartiallyChecked: 'Partially checked', Checked: 'Checked',
      Horizontal: 'Horizontal', Vertical: 'Vertical',
      NoTicks: 'No ticks', TicksAbove: 'Ticks above', TicksBelow: 'Ticks below', TicksLeft: 'Ticks left', TicksRight: 'Ticks right', TicksBothSides: 'Both sides',
      ElideLeft: 'Elide left', ElideRight: 'Elide right', ElideMiddle: 'Elide middle', ElideNone: 'No elide',
      VisibleOnPointerOver: 'On hover', FastTransformation: 'Fast', SmoothTransformation: 'Smooth',
      Raster: 'CPU raster', FlowingRibbons: 'Flowing ribbons', FloatingDots: 'Floating dots', Starfield: 'Starfield',
      Stretch: 'Stretch', Start: 'Start', Center: 'Center', End: 'End',
      TopLeft: 'Top left', TopCenter: 'Top center', TopRight: 'Top right', BottomLeft: 'Bottom left', BottomCenter: 'Bottom center', BottomRight: 'Bottom right',
      TopBar: 'Top bar', BottomBar: 'Bottom bar', LeftDock: 'Left dock', RightDock: 'Right dock', Fill: 'Fill',
      Yes: 'Yes', No: 'No',
    },
  },
};

const qs = (sel, root = document) => root.querySelector(sel);
const els = {
  modeEdit: qs('#mode-edit'),
  modePlay: qs('#mode-play'),
  widthButtons: Array.from(document.querySelectorAll('.viewport-group .seg-btn')),
  btnUndo: qs('#btn-undo'),
  btnRedo: qs('#btn-redo'),
  btnResetProps: qs('#btn-reset-props'),
  btnDuplicate: qs('#btn-duplicate'),
  btnDelete: qs('#btn-delete'),
  btnCode: qs('#btn-code'),
  paletteSearch: qs('#palette-search'),
  btnClearSearch: qs('#btn-clear-search'),
  paletteList: qs('#palette-list'),
  catalogCount: qs('#catalog-count'),
  navHome: qs('#nav-home-link'),
  tabProps: qs('#tab-props'),
  tabLayers: qs('#tab-layers'),
  propsPane: qs('#props-pane'),
  layersPane: qs('#layers-pane'),
  runtimeBadge: qs('#runtime-badge'),
  btnRetryWasm: qs('#btn-retry-wasm'),
  schematicBanner: qs('#schematic-banner'),
  schematicBannerText: qs('#schematic-banner-text'),
  schematicBannerCmd: qs('#schematic-banner-cmd'),
  btnDismissSchematicBanner: qs('#btn-dismiss-schematic-banner'),
  eventPill: qs('#event-pill'),
  nodeCrumb: qs('#node-crumb'),
  stageFrame: qs('#stage-frame'),
  stageScroll: qs('#stage-scroll'),
  artboardBar: qs('#artboard-bar'),
  artboardTitle: qs('#artboard-title'),
  artboardThemeButtons: Array.from(document.querySelectorAll('[data-artboard-theme]')),
  artboardSwatches: Array.from(document.querySelectorAll('[data-artboard-accent]')),
  btnArtboardReset: qs('#btn-artboard-reset'),
  canvasShell: qs('#canvas-shell'),
  wasmStage: qs('#wasm-stage'),
  schematicStage: qs('#schematic-stage'),
  wasmLoader: qs('#wasm-loader'),
  wasmLoaderStage: qs('#wasm-loader-stage'),
  editOverlay: qs('#edit-overlay'),
  hitboxLayer: qs('#hitbox-layer'),
  hoverBox: qs('#hover-box'),
  dropContainerBox: qs('#drop-container-box'),
  dropContainerTag: qs('#drop-container-tag'),
  dropGuideX: qs('#drop-guide-x'),
  dropGuideY: qs('#drop-guide-y'),
  dropLine: qs('#drop-line'),
  dragPreviewBox: qs('#drag-preview-box'),
  dragPreviewTag: qs('#drag-preview-tag'),
  dragGhost: qs('#drag-ghost'),
  dragGhostLabel: qs('#drag-ghost-label'),
  dragGhostBody: qs('#drag-ghost-body'),
  dragGhostPill: qs('#drag-ghost-pill'),
  selectionBox: qs('#selection-box'),
  selectionBar: qs('.selection-bar'),
  selectionTag: qs('#selection-tag'),
  selectionOps: qs('.selection-ops'),
  selAiRefine: qs('#sel-ai-refine'),
  selUp: qs('#sel-up'),
  selDown: qs('#sel-down'),
  selDup: qs('#sel-dup'),
  selDel: qs('#sel-del'),
  nodeCount: qs('#node-count'),
  outlineTree: qs('#outline-tree'),
  selectedTypeBadge: qs('#selected-type-badge'),
  selectedIdPill: qs('#selected-id-pill'),
  selectedApiHint: qs('#selected-api-hint'),
  selectedDescHint: qs('#selected-desc-hint'),
  inspectorBody: qs('#inspector-body'),
  codeDrawer: qs('#code-drawer'),
  drawerTabs: Array.from(document.querySelectorAll('.drawer-tab')),
  exportStatus: qs('#export-status'),
  btnCopyCode: qs('#btn-copy-code'),
  btnDownloadCode: qs('#btn-download-code'),
  btnCloseDrawer: qs('#btn-close-drawer'),
  codeOutput: qs('#code-output code'),
  btnAi: qs('#btn-ai'),
  aiModal: qs('#ai-modal'),
  btnCloseAi: qs('#btn-close-ai'),
  btnTemplates: qs('#btn-templates'),
  templatesModal: qs('#templates-modal'),
  btnCloseTemplates: qs('#btn-close-templates'),
  templatesGrid: qs('#templates-grid'),
  aiModeButtons: Array.from(document.querySelectorAll('.ai-mode-group [data-ai-mode]')),
  aiTargetPill: qs('#ai-target-pill'),
  aiStyleButtons: Array.from(document.querySelectorAll('#ai-style-row [data-ai-style]')),
  aiPromptInput: qs('#ai-prompt-input'),
  btnAiRun: qs('#btn-ai-run'),
  aiStatus: qs('#ai-status'),
  aiSettingsDetails: qs('#ai-settings-details'),
  aiProviderButtons: Array.from(document.querySelectorAll('.ai-provider-group .seg-btn')),
  aiWorkerFields: qs('#ai-worker-fields'),
  aiCustomFields: qs('#ai-custom-fields'),
  aiWorkerUrl: qs('#ai-worker-url'),
  aiBaseUrl: qs('#ai-base-url'),
  aiModel: qs('#ai-model'),
  aiApiKey: qs('#ai-api-key'),
  aiPresetButtons: Array.from(document.querySelectorAll('[data-ai-preset]')),
  btnIntro: qs('#btn-intro'),
  introCard: qs('#studio-intro-card'),
  btnIntroDismiss: qs('#btn-intro-dismiss'),
  btnIntroTour: qs('#btn-intro-tour'),
  btnIntroAi: qs('#btn-intro-ai'),
  btnIntroSample: qs('#btn-intro-sample'),
  tourLayer: qs('#tour-layer'),
  tourBackdrop: qs('#tour-backdrop'),
  tourSpotlight: qs('#tour-spotlight'),
  tourPopover: qs('#tour-popover'),
  tourStepBadge: qs('#tour-step-badge'),
  tourTitle: qs('#tour-title'),
  tourDesc: qs('#tour-desc'),
  tourDots: Array.from(document.querySelectorAll('[data-tour-dot]')),
  btnTourClose: qs('#btn-tour-close'),
  btnTourPrev: qs('#btn-tour-prev'),
  btnTourNext: qs('#btn-tour-next'),
  toast: qs('#toast'),
};

const params = new URLSearchParams(window.location.search);
const referrer = document.referrer ? new URL(document.referrer) : null;
const siteReferrer = referrer?.origin === location.origin && !referrer.pathname.includes('/studio/');
const refLocale = siteReferrer && referrer.pathname.split('/').map(normalizeStudioLang).find(Boolean);
const requestedLang = normalizeStudioLang(params.get('lang'))
  || (siteReferrer ? refLocale || 'en' : normalizeStudioLang(navigator.language)) || 'en';
const wasmProbe = params.get('renderer') === 'schematic' ? null : probeWasm();
let localeBundle = null;
let displayLang = requestedLang;
if (requestedLang !== 'en' && requestedLang !== 'zh') {
  document.body.inert = true;
  localeBundle = await loadLocaleBundle(requestedLang);
  if (!localeBundle) displayLang = 'en';
}
const initialTemplate = Object.hasOwn(STUDIO_TEMPLATES, params.get('template'))
  ? params.get('template')
  : 'starter';
const decodedHashResult = window.location.hash ? decodeShareState(window.location.hash) : null;
const decodedHashDoc = decodedHashResult?.ok ? decodedHashResult.document : null;
const initialDoc = decodedHashDoc || cloneDoc(STUDIO_TEMPLATES[initialTemplate].document);

const state = {
  lang: displayLang,
  localeBundle,
  theme: document.documentElement.dataset.theme || 'light',
  canvasTheme: document.documentElement.dataset.theme || 'light',
  mode: 'edit',
  openCategories: new Set(['layout']),
  searchExpandedSnapshot: null,
  viewportWidth: 860,
  viewportTimerId: null,
  inspectorTab: 'props',
  inspectorActiveTabs: new Map(),
  inspectorController: null,
  nodeInspection: null,
  schemaCache: {},
  history: createHistory(initialDoc),
  liveDraftDoc: null,
  selectedId: 'root',
  hasUserSelection: false,
  collapsedIds: new Set(),
  codeFormat: 'cpp',
  drawerOpen: false,
  wasmSession: `studio-${Math.random().toString(36).slice(2, 10)}`,
  wasmReady: false,
  wasmPainted: false,
  wasmFallback: false,
  wasmShowRetry: false,
  schematicBannerDismissed: false,
  wasmRequestId: 0,
  wasmLayout: null,
  wasmTimeoutId: null,
  wasmRevealTimerId: null,
  dragSession: null,
  suppressNextClick: false,
  hoveredNodeId: null,
  introEvaluated: false,
  introCardVisible: false,
  tourOpen: false,
  tourStep: 0,
  eventText: '',
  toastTimerId: null,
  propSyncTimerId: null,
  pendingPatch: null,
  playGesture: { nodeId: null, eventName: null, at: 0 },
  aiOpen: false,
  aiBusy: false,
  aiMode: 'create',
  aiStylePreset: 'auto',
  aiAbortController: null,
};

// History owns the snapshots. Undo/redo restores the matching preset without adding it to shared documents.
const presetForSnapshot = new WeakMap();
if (!decodedHashDoc) presetForSnapshot.set(state.history.current(), initialTemplate);

async function loadLocaleBundle(lang) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 4000);
  try {
    const response = await fetch(`./locales/${lang}.json`, { signal: controller.signal });
    if (response.ok) {
      const bundle = await response.json();
      if (bundle?.ui && bundle.components) return bundle;
    }
  } catch {
    // Fall back to the built-in English UI when the bundle is unavailable.
  } finally {
    clearTimeout(timer);
  }
  return null;
}

function formatUiTemplate(str) {
  if (typeof str !== 'string') return str;
  return str
    .replaceAll('{ai}', shortcuts.ai)
    .replaceAll('{undo}', shortcuts.undo)
    .replaceAll('{redo}', shortcuts.redo)
    .replaceAll('{duplicate}', shortcuts.duplicate);
}

function t(key) {
  const bundleUi = state.localeBundle?.ui;
  const dict = UI_STRINGS[state.lang] || UI_STRINGS.en;
  const raw = bundleUi?.[key] ?? dict[key] ?? UI_STRINGS.en[key] ?? key;
  return formatUiTemplate(raw);
}

function labelOf(obj) {
  if (!obj) return '';
  if (typeof obj === 'string') return state.localeBundle?.strings?.[obj] || obj;
  if (obj[state.lang]) return obj[state.lang];
  if (obj.en && state.localeBundle?.strings?.[obj.en]) return state.localeBundle.strings[obj.en];
  return obj.en || obj.zh || '';
}

function categoryLabel(cat) {
  return state.localeBundle?.categories?.[cat?.id] || labelOf(cat?.label);
}

function componentLabel(meta) {
  if (!meta) return '';
  return state.localeBundle?.components?.[meta.type] || labelOf(meta.label);
}

function templateLabel(tpl) {
  if (!tpl) return '';
  return state.localeBundle?.templates?.[tpl.id]?.label || labelOf(tpl.label);
}

function templateDescription(tpl) {
  if (!tpl) return '';
  return state.localeBundle?.templates?.[tpl.id]?.description || labelOf(tpl.description || tpl.label);
}

function currentDoc() {
  return state.liveDraftDoc || state.history.current();
}

function showToast(msg) {
  els.toast.textContent = msg;
  els.toast.hidden = false;
  clearTimeout(state.toastTimerId);
  state.toastTimerId = setTimeout(() => {
    els.toast.hidden = true;
  }, 2400);
}

function nodeHumanLabel(node) {
  if (!node) return '';
  if (node.id === 'root') return t('pageRoot');
  const meta = STUDIO_CATALOG[node.type];
  return meta ? componentLabel(meta) : node.type;
}

function nodeHintSnippet(node) {
  if (!node || node.id === 'root') return '';
  const raw = node.props?.title || node.props?.label || node.props?.text || '';
  if (!raw || typeof raw !== 'string') return '';
  const trimmed = raw.trim();
  return trimmed.length > 18 ? `${trimmed.slice(0, 17)}…` : trimmed;
}

function setInspectorTab(nextTab) {
  state.inspectorTab = nextTab === 'layers' ? 'layers' : 'props';
  const showLayers = state.inspectorTab === 'layers';
  els.tabProps?.classList.toggle('is-active', !showLayers);
  els.tabLayers?.classList.toggle('is-active', showLayers);
  els.tabProps?.setAttribute('aria-selected', String(!showLayers));
  els.tabLayers?.setAttribute('aria-selected', String(showLayers));
  if (els.propsPane) els.propsPane.hidden = showLayers;
  if (els.layersPane) els.layersPane.hidden = !showLayers;
}

/* Fluent 2 ComboBox (replaces native OS <select> popups across Studio) */
let activeComboMenu = null;

function closeActiveComboMenu(restoreFocus = false) {
  if (!activeComboMenu) return;
  const prev = activeComboMenu;
  activeComboMenu = null;
  prev.close(restoreFocus);
}

function createFluentComboBox({
  host,
  id,
  ariaLabel,
  triggerId = '',
  propKey = '',
  disabled = false,
  minMenuWidth = 0,
  options = [],
  value = '',
  formatTriggerLabel = null,
  onChange = null,
}) {
  if (!host) return null;
  host.innerHTML = '';
  let currentOptions = options;
  let currentValue = value;
  let openState = null;

  const trigger = document.createElement('button');
  trigger.type = 'button';
  trigger.className = 'fluent-combo-trigger';
  trigger.disabled = Boolean(disabled);
  if (triggerId) trigger.id = triggerId;
  if (propKey) trigger.dataset.propKey = propKey;
  trigger.setAttribute('role', 'combobox');
  trigger.setAttribute('aria-haspopup', 'listbox');
  trigger.setAttribute('aria-expanded', 'false');
  if (ariaLabel) trigger.setAttribute('aria-label', ariaLabel);

  const labelEl = document.createElement('span');
  labelEl.className = 'fluent-combo-label';
  const chevronEl = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  chevronEl.setAttribute('class', 'fluent-combo-chevron');
  chevronEl.setAttribute('viewBox', '0 0 12 12');
  chevronEl.setAttribute('width', '11');
  chevronEl.setAttribute('height', '11');
  chevronEl.setAttribute('aria-hidden', 'true');
  chevronEl.innerHTML = '<path fill="none" stroke="currentColor" stroke-width="1.4" stroke-linecap="round" stroke-linejoin="round" d="M2.5 4.25L6 7.75l3.5-3.5"/>';
  trigger.append(labelEl, chevronEl);
  host.appendChild(trigger);

  function syncTriggerDisplay() {
    const matched = currentOptions.find((opt) => String(opt.value) === String(currentValue));
    const display = formatTriggerLabel
      ? formatTriggerLabel(currentValue, matched)
      : (matched ? matched.label : String(currentValue ?? ''));
    labelEl.textContent = display;
    trigger.title = display;
    trigger.dataset.value = String(currentValue ?? '');
  }

  function closeMenu(restoreFocus = false) {
    if (!openState) return;
    const { menu, cleanup } = openState;
    openState = null;
    if (activeComboMenu && activeComboMenu.close === closeMenu) activeComboMenu = null;
    cleanup();
    menu.remove();
    trigger.setAttribute('aria-expanded', 'false');
    if (restoreFocus) trigger.focus();
  }

  function selectValue(nextVal) {
    const prev = currentValue;
    currentValue = String(nextVal);
    syncTriggerDisplay();
    closeMenu(false);
    if (onChange && currentValue !== String(prev)) onChange(currentValue);
  }

  function openMenu() {
    if (trigger.disabled) return;
    if (openState) {
      closeMenu(false);
      return;
    }
    closeActiveComboMenu(false);

    const menu = document.createElement('div');
    menu.className = 'fluent-combo-menu';
    menu.setAttribute('role', 'listbox');
    menu.dataset.comboFor = id || '';
    if (ariaLabel) menu.setAttribute('aria-label', ariaLabel);

    const optionButtons = [];
    let focusedIdx = Math.max(0, currentOptions.findIndex((o) => String(o.value) === String(currentValue)));

    currentOptions.forEach((opt) => {
      if (opt.dividerBefore) {
        const sep = document.createElement('div');
        sep.className = 'fluent-combo-divider';
        sep.setAttribute('role', 'separator');
        menu.appendChild(sep);
      }
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'fluent-combo-option';
      const selected = String(opt.value) === String(currentValue);
      if (selected) btn.classList.add('is-selected');
      btn.dataset.value = String(opt.value);
      btn.setAttribute('role', 'option');
      btn.setAttribute('aria-selected', String(selected));

      const txt = document.createElement('span');
      txt.className = 'fluent-combo-option-text';
      txt.textContent = opt.label;
      btn.appendChild(txt);

      if (opt.badge !== undefined && opt.badge !== '') {
        const badge = document.createElement('span');
        badge.className = 'fluent-combo-option-badge';
        badge.textContent = String(opt.badge);
        btn.appendChild(badge);
      }

      const idx = optionButtons.length;
      btn.addEventListener('mouseenter', () => setFocusedIndex(idx));
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        selectValue(opt.value);
      });
      optionButtons.push(btn);
      menu.appendChild(btn);
    });

    function setFocusedIndex(nextIdx) {
      if (!optionButtons.length) return;
      const clamped = ((nextIdx % optionButtons.length) + optionButtons.length) % optionButtons.length;
      focusedIdx = clamped;
      optionButtons.forEach((b, i) => b.classList.toggle('is-focused', i === clamped));
      optionButtons[clamped].scrollIntoView({ block: 'nearest' });
    }

    document.body.appendChild(menu);
    const rect = trigger.getBoundingClientRect();
    const width = Math.max(rect.width, minMenuWidth);
    const left = Math.max(8, Math.min(rect.left, window.innerWidth - width - 8));
    menu.style.width = `${width}px`;
    menu.style.left = `${left}px`;
    const naturalH = Math.min(360, menu.scrollHeight + 8);
    const spaceBelow = window.innerHeight - rect.bottom - 10;
    const spaceAbove = rect.top - 10;
    if (spaceBelow < Math.min(naturalH, 210) && spaceAbove > spaceBelow) {
      menu.style.maxHeight = `${Math.min(360, spaceAbove)}px`;
      menu.style.bottom = `${window.innerHeight - rect.top + 4}px`;
      menu.style.top = 'auto';
    } else {
      menu.style.maxHeight = `${Math.min(360, Math.max(140, spaceBelow))}px`;
      menu.style.top = `${rect.bottom + 4}px`;
      menu.style.bottom = 'auto';
    }

    trigger.setAttribute('aria-expanded', 'true');
    setFocusedIndex(focusedIdx);

    const onPointerDown = (e) => {
      if (host.contains(e.target) || menu.contains(e.target)) return;
      closeMenu(false);
    };
    const onKeyDown = (e) => {
      if (e.key === 'Escape') {
        e.preventDefault();
        closeMenu(true);
      } else if (e.key === 'ArrowDown') {
        e.preventDefault();
        setFocusedIndex(focusedIdx + 1);
      } else if (e.key === 'ArrowUp') {
        e.preventDefault();
        setFocusedIndex(focusedIdx - 1);
      } else if (e.key === 'Home') {
        e.preventDefault();
        setFocusedIndex(0);
      } else if (e.key === 'End') {
        e.preventDefault();
        setFocusedIndex(optionButtons.length - 1);
      } else if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        const opt = currentOptions[focusedIdx];
        if (opt) selectValue(opt.value);
      } else if (e.key === 'Tab') {
        closeMenu(false);
      }
    };
    const onScroll = (e) => {
      if (menu.contains(e.target)) return;
      closeMenu(false);
    };
    // Clicking inside the WASM iframe blurs the outer window; dismiss stray flyouts cleanly.
    const onWindowBlur = () => closeMenu(false);
    const onResize = () => closeMenu(false);

    document.addEventListener('pointerdown', onPointerDown, true);
    document.addEventListener('keydown', onKeyDown, true);
    window.addEventListener('scroll', onScroll, true);
    window.addEventListener('resize', onResize);
    window.addEventListener('blur', onWindowBlur);

    openState = {
      menu,
      cleanup: () => {
        document.removeEventListener('pointerdown', onPointerDown, true);
        document.removeEventListener('keydown', onKeyDown, true);
        window.removeEventListener('scroll', onScroll, true);
        window.removeEventListener('resize', onResize);
        window.removeEventListener('blur', onWindowBlur);
      },
    };
    activeComboMenu = { close: closeMenu };
  }

  trigger.addEventListener('click', (e) => {
    e.stopPropagation();
    openMenu();
  });
  trigger.addEventListener('keydown', (e) => {
    if (!openState && (e.key === 'ArrowDown' || e.key === 'ArrowUp' || e.key === 'Enter' || e.key === ' ')) {
      e.preventDefault();
      openMenu();
    }
  });

  syncTriggerDisplay();

  return {
    trigger,
    setOptions(nextOpts) {
      currentOptions = nextOpts;
      syncTriggerDisplay();
    },
    setValue(nextVal) {
      currentValue = String(nextVal ?? '');
      syncTriggerDisplay();
    },
    setFiltered(isFiltered) {
      host.classList.toggle('is-filtered', Boolean(isFiltered));
    },
    setDisabled(isDisabled) {
      trigger.disabled = Boolean(isDisabled);
      if (trigger.disabled) closeMenu(false);
    },
    getValue: () => currentValue,
    open: openMenu,
    close: closeMenu,
  };
}

function applyLanguage() {
  document.documentElement.dataset.lang = state.lang;
  document.documentElement.lang = state.lang === 'zh' ? 'zh-CN' : state.lang;

  document.querySelectorAll('[data-i18n]').forEach((el) => {
    el.textContent = t(el.dataset.i18n);
  });
  els.paletteSearch.placeholder = t('searchPlaceholder');
  updateAiModalModeUi();
  updateSchematicBanner();
  if (els.navHome) els.navHome.href = requestedLang === 'en' ? '../' : `../${requestedLang === 'zh' ? 'zh-CN' : requestedLang}/`;
  if (els.btnUndo) els.btnUndo.title = t('undoTitle');
  if (els.btnRedo) els.btnRedo.title = t('redoTitle');
  els.btnDuplicate.title = t('duplicateTitle');
  els.btnAi.querySelector('kbd').textContent = shortcuts.ai;
  if (els.btnIntro) {
    els.btnIntro.title = t('intro.guideTitle');
    els.btnIntro.setAttribute('aria-label', t('intro.guideTitle'));
  }
  if (els.btnResetProps) {
    els.btnResetProps.title = t('resetProps');
    els.btnResetProps.setAttribute('aria-label', t('resetProps'));
  }
}

function syncSelectionToWasm() {
  postToWasm({ type: 'studio-select', session: state.wasmSession, nodeId: state.selectedId });
}

function flushLiveDraft({ syncWasm = true } = {}) {
  const pending = state.pendingPatch;
  if (state.propSyncTimerId) {
    clearTimeout(state.propSyncTimerId);
    state.propSyncTimerId = null;
  }
  state.pendingPatch = null;
  if (state.liveDraftDoc) {
    state.history.push(state.liveDraftDoc);
    state.liveDraftDoc = null;
  }
  if (syncWasm && pending) {
    syncPatchToWasm(pending.nodeId, pending.patch, currentDoc());
  }
}

function commitDocument(nextDoc, nextSelectedId = state.selectedId, pushHistory = true, userInitiated = true) {
  closeActiveComboMenu(false);
  if (state.propSyncTimerId) {
    clearTimeout(state.propSyncTimerId);
    state.propSyncTimerId = null;
  }
  state.pendingPatch = null;
  state.liveDraftDoc = null;
  if (pushHistory) {
    state.history.push(nextDoc);
  } else {
    state.history.reset(nextDoc);
  }
  if (!findNode(nextDoc.root, nextSelectedId)) {
    state.selectedId = nextDoc.root.id;
  } else {
    state.selectedId = nextSelectedId;
  }
  state.hasUserSelection = state.selectedId !== 'root' ? true : Boolean(userInitiated && state.hasUserSelection);
  renderAll();
  syncToWasm();
  syncSelectionToWasm();
}

function selectNode(nodeId, userInitiated = true) {
  flushLiveDraft({ syncWasm: true });
  const doc = currentDoc();
  if (!findNode(doc.root, nodeId)) return;
  closeActiveComboMenu(false);
  state.selectedId = nodeId;
  if (userInitiated) state.hasUserSelection = true;
  renderBreadcrumbs();
  renderOutline();
  renderInspector();
  syncOverlay();
  syncSelectionToWasm();
}

function setMode(nextMode) {
  if (state.introCardVisible) dismissIntroCard({ remember: true });
  flushLiveDraft({ syncWasm: true });
  state.mode = nextMode === 'play' ? 'play' : 'edit';
  const isPlay = state.mode === 'play';
  els.modeEdit.classList.toggle('is-active', !isPlay);
  els.modePlay.classList.toggle('is-active', isPlay);
  els.modeEdit.setAttribute('aria-pressed', String(!isPlay));
  els.modePlay.setAttribute('aria-pressed', String(isPlay));
  els.editOverlay.classList.toggle('is-play', isPlay);
  clearDropVisuals();
  if (isPlay && !state.eventText) {
    setEventPill(t('playHint'));
  }
  if (!isPlay) {
    postToWasm({ type: 'studio-refresh', session: state.wasmSession });
    requestAnimationFrame(syncOverlay);
  }
}

function setEventPill(text) {
  state.eventText = text || '';
  els.eventPill.textContent = state.eventText;
}

const systemThemeQuery = window.matchMedia('(prefers-color-scheme: dark)');
const forcedColorsQuery = window.matchMedia('(forced-colors: active)');

function resolveSiteTheme() {
  try {
    const saved = localStorage.getItem('fluent-qt-theme');
    if (['light', 'dark', 'high-contrast'].includes(saved)) return saved;
  } catch (_) {}
  return systemThemeQuery.matches ? 'dark' : 'light';
}

function inheritTheme() {
  const nextTheme = forcedColorsQuery.matches ? 'high-contrast' : resolveSiteTheme();
  state.theme = nextTheme;
  document.documentElement.dataset.theme = nextTheme;
  if (typeof currentDoc === 'function' && els.canvasShell) {
    applyDocumentDesign(currentDoc());
  } else {
    state.canvasTheme = nextTheme;
    postToWasm({ type: 'theme', theme: nextTheme });
  }
}

function resolveViewportTier(width) {
  const w = Number(width) || 860;
  if (w < 500) return 'compact';
  if (w < 720) return 'medium';
  return 'expanded';
}

function syncViewportTier(width = state.viewportWidth) {
  if (!els.canvasShell) return;
  els.canvasShell.dataset.viewportTier = resolveViewportTier(width);
}

const QT_LAYOUT_TYPES = new Set(['VBox', 'HBox', 'Grid']);
const PLUGIN_TARGET_BY_TYPE = {
  ParticleBackdrop: 'FluentQt::Particles',
  SpatialView: 'FluentQt::Spatial',
  SpatialItem: 'FluentQt::Spatial',
  QmlView: 'FluentQt::QmlBridge',
};

function createPaletteItemCard(meta) {
  const card = document.createElement('div');
  card.className = 'palette-item';
  card.dataset.type = meta.type;
  card.dataset.container = meta.container ? '1' : '0';
  const isQtLayout = QT_LAYOUT_TYPES.has(meta.type);
  const pluginTarget = PLUGIN_TARGET_BY_TYPE[meta.type];
  card.title = isQtLayout
    ? `${meta.type} · ${meta.cppClass} (${t('qtTagHint')})`
    : (pluginTarget
      ? `${meta.type} · ${meta.cppClass} (${t('pluginTagHint').replace('{target}', pluginTarget)})`
      : `${meta.type} · ${meta.cppClass}`);

  const info = document.createElement('div');
  info.className = 'palette-item-info';
  const nameSpan = document.createElement('span');
  nameSpan.className = 'palette-item-name';
  nameSpan.textContent = componentLabel(meta);
  info.appendChild(nameSpan);

  if (isQtLayout) {
    const srcTag = document.createElement('span');
    srcTag.className = 'palette-item-tag is-source-qt';
    srcTag.textContent = t('qtTag');
    srcTag.title = t('qtTagHint');
    info.appendChild(srcTag);
  } else if (pluginTarget) {
    const srcTag = document.createElement('span');
    srcTag.className = 'palette-item-tag is-source-plugin';
    srcTag.textContent = t('pluginTag');
    srcTag.title = t('pluginTagHint').replace('{target}', pluginTarget);
    info.appendChild(srcTag);
  }

  if (meta.container) {
    const tagSpan = document.createElement('span');
    tagSpan.className = 'palette-item-tag';
    tagSpan.textContent = t('containerTag');
    info.appendChild(tagSpan);
  }

  const addBtn = document.createElement('button');
  addBtn.type = 'button';
  addBtn.className = 'palette-add-btn';
  addBtn.textContent = '+';
  const addLabel = t('addWidget').replace('{type}', meta.type);
  addBtn.title = addLabel;
  addBtn.setAttribute('aria-label', addLabel);
  addBtn.addEventListener('click', (e) => {
    e.stopPropagation();
    quickInsertType(meta.type);
  });

  card.addEventListener('dblclick', () => quickInsertType(meta.type));
  attachPointerDrag(card, () => ({ kind: 'palette', type: meta.type }), { ignoreSelector: '.palette-add-btn' });

  card.append(info, addBtn);
  return card;
}

function renderPalette() {
  const query = els.paletteSearch.value.trim().toLowerCase();
  if (els.btnClearSearch) els.btnClearSearch.hidden = els.paletteSearch.value.length === 0;
  if (!query && state.searchExpandedSnapshot !== null) {
    state.openCategories = state.searchExpandedSnapshot;
    state.searchExpandedSnapshot = null;
  } else if (query && state.searchExpandedSnapshot === null) {
    const existingCats = els.paletteList.querySelectorAll('.palette-cat');
    if (existingCats.length) {
      const domOpen = new Set();
      existingCats.forEach((el) => {
        if (el.open && el.dataset.categoryId) domOpen.add(el.dataset.categoryId);
      });
      state.openCategories = domOpen;
    }
    state.searchExpandedSnapshot = new Set(state.openCategories);
  }

  els.paletteList.innerHTML = '';
  let totalVisible = 0;

  for (const cat of STUDIO_CATEGORIES) {
    const items = Object.values(STUDIO_CATALOG).filter((meta) => {
      if (meta.category !== cat.id) return false;
      if (!query) return true;
      return (
        meta.type.toLowerCase().includes(query)
        || meta.cppClass.toLowerCase().includes(query)
        || meta.pyClass.toLowerCase().includes(query)
        || meta.label.zh.toLowerCase().includes(query)
        || meta.label.en.toLowerCase().includes(query)
        || componentLabel(meta).toLowerCase().includes(query)
      );
    });
    if (!items.length) continue;

    const section = document.createElement('details');
    section.className = 'palette-cat';
    section.dataset.categoryId = cat.id;
    section.open = query ? true : state.openCategories.has(cat.id);
    if (!query) {
      section.addEventListener('toggle', () => {
        if (els.paletteSearch.value.trim()) return;
        if (section.open) state.openCategories.add(cat.id);
        else state.openCategories.delete(cat.id);
      });
    }

    const heading = document.createElement('summary');
    heading.className = 'palette-cat-title';

    const lead = document.createElement('span');
    lead.className = 'palette-cat-lead';
    const chev = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    chev.setAttribute('class', 'palette-cat-chevron');
    chev.setAttribute('viewBox', '0 0 12 12');
    chev.setAttribute('width', '12');
    chev.setAttribute('height', '12');
    chev.setAttribute('aria-hidden', 'true');
    chev.innerHTML = '<path fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" d="M4.25 2.5L7.75 6l-3.5 3.5"/>';
    const titleSpan = document.createElement('span');
    titleSpan.className = 'palette-cat-label';
    titleSpan.textContent = categoryLabel(cat);
    lead.append(chev, titleSpan);

    const countSpan = document.createElement('span');
    countSpan.className = 'palette-cat-count';
    countSpan.textContent = String(items.length);
    heading.append(lead, countSpan);
    section.appendChild(heading);

    const grid = document.createElement('div');
    grid.className = 'palette-grid';
    for (const meta of items) {
      totalVisible++;
      grid.appendChild(createPaletteItemCard(meta));
    }
    section.appendChild(grid);
    els.paletteList.appendChild(section);
  }
  els.catalogCount.textContent = String(totalVisible);
}

function quickInsertType(type) {
  const doc = currentDoc();
  const newNode = createNode(type, doc);
  const selected = findNode(doc.root, state.selectedId) || doc.root;

  let targetParentId = doc.root.id;
  let targetIndex = doc.root.children.length;

  if (canAcceptChild(selected, 1)) {
    targetParentId = selected.id;
    targetIndex = (selected.children || []).length;
  } else {
    const parentInfo = findParent(doc.root, selected.id);
    if (parentInfo && parentInfo.parent && canAcceptChild(parentInfo.parent, 1)) {
      targetParentId = parentInfo.parent.id;
      targetIndex = parentInfo.index + 1;
    }
  }

  const targetParent = findNode(doc.root, targetParentId);
  if (targetParent?.type === 'AnchorBox') {
    const pad = targetParent.props?.padding ?? 16;
    const siblingCount = (targetParent.children || []).length;
    if (siblingCount === 0) {
      newNode.props = type === 'ParticleBackdrop'
        ? { ...newNode.props, anchorPosition: 'Fill', anchorMargin: 0, anchorOffsetX: 0, anchorOffsetY: 0 }
        : { ...newNode.props, anchorPosition: 'Center', anchorMargin: pad, anchorOffsetX: 0, anchorOffsetY: 0 };
    } else {
      newNode.props = {
        ...newNode.props,
        anchorPosition: 'TopLeft',
        anchorMargin: pad,
        anchorOffsetX: (siblingCount % 5) * 32,
        anchorOffsetY: (siblingCount % 7) * 48,
      };
    }
  }

  const res = insertNode(doc, targetParentId, targetIndex, newNode);
  if (!res.ok) {
    showToast(res.error);
    return;
  }
  state.collapsedIds.delete(targetParentId);
  commitDocument(res.document, res.nodeId, true);
}

function renderBreadcrumbs() {
  const doc = currentDoc();
  const path = [];
  function walk(node, acc) {
    acc.push(node);
    if (node.id === state.selectedId) return true;
    for (const child of node.children || []) {
      if (walk(child, acc)) return true;
    }
    acc.pop();
    return false;
  }
  walk(doc.root, path);

  els.nodeCrumb.innerHTML = '';
  path.forEach((node, idx) => {
    if (idx > 0) {
      const sep = document.createElement('span');
      sep.className = 'crumb-sep';
      sep.textContent = '›';
      els.nodeCrumb.appendChild(sep);
    }
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = `crumb-btn${node.id === state.selectedId ? ' is-current' : ''}`;
    const snippet = nodeHintSnippet(node);
    const label = nodeHumanLabel(node);
    btn.textContent = snippet ? `${label} · ${snippet}` : label;
    btn.title = `${node.type}#${node.id}`;
    btn.addEventListener('click', () => selectNode(node.id, true));
    els.nodeCrumb.appendChild(btn);
  });
}

function renderOutline() {
  const doc = currentDoc();
  els.nodeCount.textContent = `${countNodes(doc.root)} / ${MAX_NODES}`;
  els.outlineTree.innerHTML = '';

  function buildRows(node, depth) {
    const meta = STUDIO_CATALOG[node.type];
    const hasChildren = (node.children || []).length > 0;
    const isCollapsed = state.collapsedIds.has(node.id);

    const row = document.createElement('div');
    row.className = `tree-row${node.id === state.selectedId ? ' is-selected' : ''}`;
    row.dataset.nodeId = node.id;
    row.title = `${node.type}#${node.id}`;
    row.setAttribute('role', 'treeitem');
    row.setAttribute('aria-selected', String(node.id === state.selectedId));
    row.dataset.draggable = node.id !== 'root' ? '1' : '0';

    const indent = document.createElement('span');
    indent.className = 'tree-indent';
    indent.style.width = `${depth * 14}px`;
    row.appendChild(indent);

    const toggle = document.createElement('button');
    toggle.type = 'button';
    toggle.className = 'tree-toggle';
    toggle.textContent = meta?.container ? (isCollapsed ? '▸' : '▾') : '·';
    toggle.tabIndex = -1;
    if (meta?.container && hasChildren) {
      toggle.addEventListener('click', (e) => {
        e.stopPropagation();
        if (state.collapsedIds.has(node.id)) state.collapsedIds.delete(node.id);
        else state.collapsedIds.add(node.id);
        renderOutline();
      });
    }
    row.appendChild(toggle);

    const typeEl = document.createElement('span');
    typeEl.className = 'tree-type';
    typeEl.textContent = nodeHumanLabel(node);
    row.appendChild(typeEl);

    const hint = nodeHintSnippet(node);
    if (hint) {
      const idEl = document.createElement('span');
      idEl.className = 'tree-id';
      idEl.textContent = hint;
      row.appendChild(idEl);
    }

    if (meta?.maxChildren === 1) {
      const slot = document.createElement('span');
      slot.className = 'tree-slot';
      slot.textContent = `${(node.children || []).length}/1`;
      row.appendChild(slot);
    }

    row.addEventListener('click', () => selectNode(node.id, true));
    row.addEventListener('dblclick', () => {
      selectNode(node.id, true);
      setInspectorTab('props');
    });
    if (node.id !== 'root') {
      attachPointerDrag(row, () => ({ kind: 'move', nodeId: node.id }), { ignoreSelector: '.tree-toggle' });
    }

    els.outlineTree.appendChild(row);

    if (!isCollapsed && hasChildren) {
      for (const child of node.children) buildRows(child, depth + 1);
    }
  }

  buildRows(doc.root, 0);
}

function clearTreeRowDropClasses(row) {
  row.classList.remove('is-drop-before', 'is-drop-inside', 'is-drop-after');
}

function computeTreeDropPlacement(clientY, node, row) {
  const rect = row.getBoundingClientRect();
  const relY = (clientY - rect.top) / Math.max(1, rect.height);
  const meta = STUDIO_CATALOG[node.type];
  if (node.id === 'root') {
    return { targetParentId: 'root', targetIndex: (node.children || []).length, visual: 'inside' };
  }
  if (meta?.container && relY > 0.25 && relY < 0.75) {
    return { targetParentId: node.id, targetIndex: (node.children || []).length, visual: 'inside' };
  }
  const parentInfo = findParent(currentDoc().root, node.id);
  if (!parentInfo || !parentInfo.parent) return null;
  if (relY < 0.5) {
    return { targetParentId: parentInfo.parent.id, targetIndex: parentInfo.index, visual: 'before' };
  }
  return { targetParentId: parentInfo.parent.id, targetIndex: parentInfo.index + 1, visual: 'after' };
}

function canExecuteDrop(payload, targetParentId) {
  const doc = currentDoc();
  const targetParent = findNode(doc.root, targetParentId);
  if (!targetParent) return false;

  if (payload.kind === 'palette') {
    return canAcceptChild(targetParent, 1) && nodeDepth(doc.root, targetParentId) + 1 <= 12;
  }
  if (payload.kind === 'move') {
    const srcInfo = findParent(doc.root, payload.nodeId);
    if (!srcInfo) return false;
    if (payload.nodeId === targetParentId || findNode(srcInfo.node, targetParentId)) return false;
    const sameParent = srcInfo.parent?.id === targetParentId;
    if (!sameParent && !canAcceptChild(targetParent, 1)) return false;
    return true;
  }
  return false;
}

const LEGACY_STUDIO_ALIAS_KEYS = new Set(['typography', 'checked', 'pages', 'posX', 'posY', 'posZ', 'rotX', 'rotY', 'rotZ']);
const STRUCTURAL_LAYOUT_KEYS = new Set(['spacing', 'padding', 'minHeight', 'columns', 'align', 'listPaneWidth']);

function localizedText(value) {
  if (!value) return '';
  if (typeof value === 'string') return state.localeBundle?.strings?.[value] || value;
  if (value[state.lang]) return value[state.lang];
  if (value.en && state.localeBundle?.strings?.[value.en]) return state.localeBundle.strings[value.en];
  return value.en || value.zh || '';
}

function optionLabel(field, value) {
  const dict = UI_STRINGS[state.lang] || UI_STRINGS.en;
  const raw = field?.optionLabels?.[value] ?? field?.valueLabels?.[String(value)];
  if (raw !== undefined) {
    const str = typeof raw === 'string' ? raw : localizedText(raw);
    if (str) return str;
  }
  const key = String(value ?? '');
  return state.localeBundle?.options?.[key]
    || dict.optionLabels?.[key]
    || UI_STRINGS.en.optionLabels?.[key]
    || key;
}

function choiceDisplayLabel(field, choice) {
  const raw = choice?.label;
  if (raw && typeof raw === 'object' && 'page' in raw) {
    return t('pageTemplate').replace('{page}', String(raw.page));
  }
  if (raw && typeof raw === 'object') return localizedText(raw);
  return optionLabel(field, raw ?? choice?.value);
}

function evalConditionExpr(expr, values) {
  if (!expr) return true;
  const v = values || {};
  if (expr === 'hasIcon' || expr === 'icon') return Boolean(v.icon && v.icon !== 'None');
  if (expr === 'stackedIcon') return Boolean(v.icon && v.icon !== 'None' && ['IconAbove', 'IconBelow'].includes(v.fluentLayout));
  if (expr === 'hasVisibleIcon') return Boolean(v.icon && v.icon !== 'None') && v.fluentLayout !== 'TextOnly';

  const tokens = [];
  for (let i = 0; i < expr.length;) {
    const ch = expr[i];
    if (/\s/.test(ch)) { i += 1; continue; }
    const two = expr.slice(i, i + 2);
    if (['&&', '||', '==', '!=', '<=', '>='].includes(two)) {
      tokens.push({ t: two }); i += 2; continue;
    }
    if ('!()<>'.includes(ch)) { tokens.push({ t: ch }); i += 1; continue; }
    if (ch === '\'' || ch === '"') {
      let j = i + 1;
      let s = '';
      while (j < expr.length && expr[j] !== ch) {
        if (expr[j] === '\\' && j + 1 < expr.length) j += 1;
        s += expr[j];
        j += 1;
      }
      tokens.push({ t: 'lit', v: s });
      i = j + 1;
      continue;
    }
    if (/[-0-9.]/.test(ch) && (ch !== '-' || /[0-9]/.test(expr[i + 1] || ''))) {
      let j = i + (ch === '-' ? 1 : 0);
      while (j < expr.length && /[0-9.]/.test(expr[j])) j += 1;
      tokens.push({ t: 'lit', v: Number(expr.slice(i, j)) });
      i = j;
      continue;
    }
    let j = i;
    while (j < expr.length && /[A-Za-z0-9_.]/.test(expr[j])) j += 1;
    if (j === i) return Boolean(v[expr]);
    const word = expr.slice(i, j);
    if (word === 'true') tokens.push({ t: 'lit', v: true });
    else if (word === 'false') tokens.push({ t: 'lit', v: false });
    else if (word === 'null') tokens.push({ t: 'lit', v: null });
    else tokens.push({ t: 'id', v: word });
    i = j;
  }

  let pos = 0;
  const peek = () => tokens[pos]?.t;
  const resolveId = (name) => {
    if (name === 'hasIcon' || name === 'icon') return Boolean(v.icon && v.icon !== 'None');
    if (name === 'stackedIcon') return Boolean(v.icon && v.icon !== 'None' && ['IconAbove', 'IconBelow'].includes(v.fluentLayout));
    if (name === 'hasVisibleIcon') return Boolean(v.icon && v.icon !== 'None') && v.fluentLayout !== 'TextOnly';
    if (name.endsWith('.length')) {
      const base = v[name.slice(0, -7)];
      return base == null ? 0 : (base.length ?? 0);
    }
    return v[name];
  };
  const parsePrimary = () => {
    const tok = tokens[pos];
    if (!tok) return undefined;
    if (tok.t === '(') { pos += 1; const val = parseOr(); if (peek() === ')') pos += 1; return val; }
    if (tok.t === 'lit') { pos += 1; return tok.v; }
    if (tok.t === 'id') { pos += 1; return resolveId(tok.v); }
    return undefined;
  };
  const parseUnary = () => (peek() === '!' ? (pos += 1, !parseUnary()) : parsePrimary());
  const parseCmp = () => {
    const left = parseUnary();
    const op = peek();
    if (['==', '!=', '<', '<=', '>', '>='].includes(op)) {
      pos += 1;
      const right = parseUnary();
      if (op === '==') return left === right;
      if (op === '!=') return left !== right;
      if (op === '<') return Number(left) < Number(right);
      if (op === '<=') return Number(left) <= Number(right);
      if (op === '>') return Number(left) > Number(right);
      return Number(left) >= Number(right);
    }
    return left;
  };
  const parseAnd = () => {
    let left = parseCmp();
    while (peek() === '&&') { pos += 1; const right = parseCmp(); left = Boolean(left) && Boolean(right); }
    return left;
  };
  const parseOr = () => {
    let left = parseAnd();
    while (peek() === '||') { pos += 1; const right = parseAnd(); left = Boolean(left) || Boolean(right); }
    return left;
  };
  return Boolean(parseOr());
}

function propsEqualShallow(a, b) {
  if (a === b) return true;
  if (Array.isArray(a) && Array.isArray(b)) {
    return a.length === b.length && a.every((item, i) => item === b[i]);
  }
  return false;
}

function isNodePropsModified(node, meta, inAnchorBox) {
  if (!node || !meta) return false;
  const props = node.props || {};
  const defaults = meta.defaultProps || {};
  for (const [key, val] of Object.entries(props)) {
    if (val === undefined) continue;
    if (inAnchorBox && ANCHOR_PROP_KEYS.includes(key)) continue;
    if (!Object.prototype.hasOwnProperty.call(defaults, key)) return true;
    if (!propsEqualShallow(val, defaults[key])) return true;
  }
  return false;
}

function convertStudioCatalogField(sf, defaultProps, category = 'basic', group = '') {
  const min = sf.min ?? 0;
  const max = sf.max ?? 100;
  const base = {
    id: sf.key,
    property: sf.key,
    label: sf.label,
    category,
    group: group || undefined,
    default: defaultProps?.[sf.key],
    source: 'studio',
  };
  if (sf.kind === 'boolean') return { ...base, type: 'boolean' };
  if (sf.kind === 'select') return { ...base, type: 'enum', options: sf.options || [] };
  if (sf.kind === 'number') {
    return {
      ...base,
      type: 'number',
      minimum: min,
      maximum: max,
      step: 1,
      integer: true,
      control: (max - min <= 250) ? 'range' : 'number',
    };
  }
  if (sf.kind === 'textarea') return { ...base, type: 'text', control: 'textarea' };
  if (sf.kind === 'list') return { ...base, type: 'string-list' };
  if (sf.kind === 'numlist') return { ...base, type: 'numlist' };
  return { ...base, type: 'text' };
}

function buildInspectorModel(node, doc, meta) {
  const parentInfo = findParent(doc.root, node.id);
  const inAnchorBox = parentInfo?.parent?.type === 'AnchorBox';
  const anchorDefaults = {
    anchorPosition: 'TopLeft',
    anchorMargin: parentInfo?.parent?.props?.padding ?? 12,
    anchorOffsetX: 0,
    anchorOffsetY: 0,
  };

  const inspected = (state.nodeInspection && state.nodeInspection.nodeId === node.id && state.nodeInspection.supported)
    ? state.nodeInspection.schema
    : null;
  const hasPlayground = Boolean(inspected && Array.isArray(inspected.properties) && inspected.properties.length > 0);
  const fields = [];
  const coveredIds = new Set();

  if (hasPlayground) {
    for (const f of inspected.properties) {
      coveredIds.add(f.id);
      fields.push({ ...f, source: 'playground' });
    }
  }

  for (const sf of meta?.propSchema || []) {
    if (coveredIds.has(sf.key)) continue;
    if (hasPlayground) {
      if (sf.key === 'typography' && coveredIds.has('fluentTypography')) continue;
      if (node.type === 'CheckBox' && sf.key === 'checked' && coveredIds.has('checkState')) continue;
      if (node.type === 'PipsPager' && sf.key === 'pages' && coveredIds.has('numberOfPages')) continue;
      if (node.type === 'TabView' && sf.key === 'items' && coveredIds.has('tabs')) continue;
      if (node.type === 'SpatialItem' && LEGACY_STUDIO_ALIAS_KEYS.has(sf.key) && coveredIds.has('positionX')) continue;
    }
    const category = hasPlayground && STRUCTURAL_LAYOUT_KEYS.has(sf.key) ? 'icon' : 'basic';
    fields.push(convertStudioCatalogField(sf, meta?.defaultProps, category));
    coveredIds.add(sf.key);
  }

  if (inAnchorBox) {
    for (const af of ANCHOR_CHILD_SCHEMA) {
      fields.push(convertStudioCatalogField(af, anchorDefaults, 'basic', 'anchor-placement'));
    }
  }

  const values = {};
  for (const f of fields) {
    if (f.default !== undefined) values[f.id] = f.default;
  }
  const np = node.props || {};
  for (const [k, v] of Object.entries(np)) {
    if (v !== undefined) values[k] = v;
  }
  if (hasPlayground && inspected.values) {
    for (const [k, v] of Object.entries(inspected.values)) {
      values[k] = v;
    }
  }

  const effective = { ...(hasPlayground ? (inspected.effective || {}) : {}) };
  const categoryLabels = {};
  if (hasPlayground && Array.isArray(inspected.categories)) {
    for (const c of inspected.categories) {
      if (c?.id) categoryLabels[c.id] = localizedText(c.label);
    }
  }

  const schemaKey = hasPlayground
    ? `pg:${node.type}:${inAnchorBox ? '1' : '0'}:${fields.map((f) => f.id).join(',')}`
    : `cat:${node.type}:${inAnchorBox ? '1' : '0'}`;

  return {
    node,
    meta,
    hasPlayground,
    inspected,
    fields,
    values,
    effective,
    categoryLabels,
    inAnchorBox,
    schemaKey,
    description: hasPlayground && inspected.description ? localizedText(inspected.description) : '',
  };
}

function formatRangeValue(field, val) {
  if (val === null || val === undefined || val === '') return t('notSet');
  if (field.valueLabels && Object.prototype.hasOwnProperty.call(field.valueLabels, String(val))) {
    return optionLabel(field, String(val));
  }
  if (field.id === 'iconRotation' || field.id.startsWith('rot')) return `${val}°`;
  if (field.id === 'iconScale') return `${val}x`;
  if (['iconOffsetX', 'iconOffsetY', 'chevronOffsetX', 'chevronOffsetY', 'cornerRadius', 'handleSize', 'trackHeight', 'spacing', 'padding', 'minHeight', 'listPaneWidth', 'anchorMargin', 'anchorOffsetX', 'anchorOffsetY'].includes(field.id)) {
    return `${val}px`;
  }
  return String(val);
}

function toCssSwatchColor(raw) {
  const s = String(raw || '').trim();
  const m8 = /^#([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})$/.exec(s);
  if (m8) return `rgba(${parseInt(m8[2], 16)}, ${parseInt(m8[3], 16)}, ${parseInt(m8[4], 16)}, ${(parseInt(m8[1], 16) / 255).toFixed(3)})`;
  if (/^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$/.test(s)) return s;
  return 'transparent';
}

function toHexPickerColor(raw) {
  const s = String(raw || '').trim();
  const m6 = /^#([0-9a-fA-F]{6})$/.exec(s);
  if (m6) return `#${m6[1]}`;
  const m8 = /^#[0-9a-fA-F]{2}([0-9a-fA-F]{6})$/.exec(s);
  if (m8) return `#${m8[1]}`;
  const m3 = /^#([0-9a-fA-F])([0-9a-fA-F])([0-9a-fA-F])$/.exec(s);
  if (m3) return `#${m3[1]}${m3[1]}${m3[2]}${m3[2]}${m3[3]}${m3[3]}`;
  return '#0062b0';
}

function updateInspectorSummaryHeader(node, meta, model) {
  els.selectedTypeBadge.textContent = nodeHumanLabel(node);
  if (els.selectedIdPill) els.selectedIdPill.textContent = `#${node.id}`;
  if (els.selectedApiHint) els.selectedApiHint.textContent = meta ? `${meta.cppClass} · ${meta.pyClass}` : '';
  if (els.selectedDescHint) {
    els.selectedDescHint.textContent = model.description || '';
    els.selectedDescHint.hidden = !model.description;
  }
  if (els.btnResetProps) {
    els.btnResetProps.disabled = !isNodePropsModified(node, meta, model.inAnchorBox);
  }
}

function renderInspector() {
  const doc = currentDoc();
  const node = findNode(doc.root, state.selectedId) || doc.root;
  const meta = STUDIO_CATALOG[node.type];
  const model = buildInspectorModel(node, doc, meta);
  updateInspectorSummaryHeader(node, meta, model);
  els.inspectorBody.innerHTML = '';

  if (node.id === 'root') {
    const tip = document.createElement('div');
    tip.className = 'insp-tip-card';
    tip.textContent = t('inspectorRootTip');
    els.inspectorBody.appendChild(tip);
  }

  const orderedCatIds = ['basic', 'icon', 'appearance'];
  const fieldsByCat = new Map(orderedCatIds.map((id) => [id, []]));
  for (const field of model.fields) {
    const cat = fieldsByCat.has(field.category) ? field.category : 'basic';
    fieldsByCat.get(cat).push(field);
  }
  const visibleCatIds = orderedCatIds.filter((id) => fieldsByCat.get(id).length > 0);

  if (!visibleCatIds.length) {
    state.inspectorController = null;
    const empty = document.createElement('p');
    empty.className = 'prop-label';
    empty.textContent = t('noSelectionProps');
    els.inspectorBody.appendChild(empty);
  } else {
    const iconFields = fieldsByCat.get('icon');
    const hasIconContent = iconFields.some((f) => /icon|glyph|chevron/i.test(f.id));
    const hasLayoutField = iconFields.some((f) => f.id === 'fluentLayout' || !/icon|glyph|chevron|rotation|scale|offset/i.test(f.id));
    const iconTabTitle = hasIconContent && hasLayoutField
      ? t('catIconLayout')
      : (hasIconContent ? t('catIcons') : t('catLayout'));

    const catTitleMap = {
      basic: model.categoryLabels.basic || t('catBasic'),
      icon: model.categoryLabels.icon || iconTabTitle,
      appearance: model.categoryLabels.appearance || t('catAppearance'),
    };

    const groupTitles = {
      'primary-icon': model.fields.some((f) => f.group === 'dropdown-indicator') ? t('mainIcon') : t('icon'),
      'dropdown-indicator': t('dropdownIndicator'),
      'query-icon': t('queryIcon'),
      'anchor-placement': t('anchorSectionTitle'),
    };

    let activeCat = state.inspectorActiveTabs.get(node.type) || 'basic';
    if (!visibleCatIds.includes(activeCat)) activeCat = visibleCatIds[0];

    const tabButtons = new Map();
    const tabPanels = new Map();
    const rowEntries = new Map();

    const switchCategoryTab = (catId, moveFocus = false) => {
      activeCat = catId;
      state.inspectorActiveTabs.set(node.type, catId);
      tabButtons.forEach((btn, id) => {
        const selected = id === catId;
        btn.setAttribute('aria-selected', String(selected));
        btn.tabIndex = selected ? 0 : -1;
        if (selected && moveFocus) btn.focus();
      });
      tabPanels.forEach((panel, id) => {
        panel.hidden = id !== catId;
      });
    };

    if (visibleCatIds.length > 1) {
      const tabsBar = document.createElement('div');
      tabsBar.className = 'insp-category-tabs';
      tabsBar.setAttribute('role', 'tablist');
      tabsBar.setAttribute('aria-label', t('propertyCategories'));

      for (const catId of visibleCatIds) {
        const selected = catId === activeCat;
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'insp-category-tab';
        btn.setAttribute('role', 'tab');
        btn.dataset.inspTab = catId;
        btn.id = `insp-tab-${catId}`;
        btn.setAttribute('aria-controls', `insp-panel-${catId}`);
        btn.setAttribute('aria-selected', String(selected));
        btn.tabIndex = selected ? 0 : -1;
        btn.textContent = catTitleMap[catId];
        btn.addEventListener('click', () => switchCategoryTab(catId));
        btn.addEventListener('keydown', (e) => {
          if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(e.key)) return;
          e.preventDefault();
          const idx = visibleCatIds.indexOf(catId);
          let nextIdx = idx;
          if (e.key === 'ArrowLeft') nextIdx = (idx - 1 + visibleCatIds.length) % visibleCatIds.length;
          else if (e.key === 'ArrowRight') nextIdx = (idx + 1) % visibleCatIds.length;
          else if (e.key === 'Home') nextIdx = 0;
          else if (e.key === 'End') nextIdx = visibleCatIds.length - 1;
          switchCategoryTab(visibleCatIds[nextIdx], true);
        });
        tabsBar.appendChild(btn);
        tabButtons.set(catId, btn);
      }
      els.inspectorBody.appendChild(tabsBar);
    }

    for (const catId of visibleCatIds) {
      const panel = document.createElement('div');
      panel.className = 'insp-tabpanel';
      panel.id = `insp-panel-${catId}`;
      panel.dataset.inspTab = catId;
      panel.setAttribute('role', 'tabpanel');
      panel.hidden = catId !== activeCat;

      const catFields = fieldsByCat.get(catId);
      const distinctGroups = new Set(catFields.map((f) => f.group).filter(Boolean));
      const groupBoxes = new Map();

      for (const field of catFields) {
        let hostContainer = panel;
        if (field.group) {
          if (!groupBoxes.has(field.group)) {
            const needLegend = distinctGroups.size > 1 || field.group === 'anchor-placement';
            const groupEl = document.createElement(needLegend ? 'fieldset' : 'div');
            groupEl.className = 'insp-property-group';
            if (panel.children.length > 0) groupEl.classList.add('has-divider');
            groupEl.dataset.group = field.group;
            if (needLegend) {
              const legend = document.createElement('legend');
              legend.textContent = groupTitles[field.group] || field.group;
              groupEl.appendChild(legend);
            }
            panel.appendChild(groupEl);
            groupBoxes.set(field.group, groupEl);
          }
          hostContainer = groupBoxes.get(field.group);
        }

        const rowEl = document.createElement('div');
        rowEl.className = 'insp-field';
        rowEl.dataset.property = field.id;
        const inputId = `prop-${node.id}-${field.id}`;
        const label = document.createElement('label');
        label.htmlFor = inputId;
        label.textContent = localizedText(field.label);
        if (field.id === 'checkable') label.title = t('checkableHint');
        else if (field.description) label.title = localizedText(field.description);

        const controlWrap = document.createElement('div');
        controlWrap.className = 'insp-field-control';
        let entry = null;

        const isStacked = Boolean(field.nullable && field.type !== 'date')
          || field.type === 'string-list'
          || field.type === 'multi-enum'
          || field.control === 'textarea';
        if (isStacked) rowEl.classList.add('insp-field-stacked');

        const commitVal = (nextVal, commitHistory = true) => applyPropPatch(node.id, { [field.id]: nextVal }, commitHistory);

        if (field.type === 'multi-enum') {
          const box = document.createElement('div');
          box.className = 'insp-multi-options';
          box.dataset.propKey = field.id;
          box.setAttribute('role', 'group');
          box.setAttribute('aria-label', localizedText(field.label));
          const checks = [];
          for (const opt of field.options || []) {
            const optLbl = document.createElement('label');
            optLbl.className = 'insp-multi-item';
            const cb = document.createElement('input');
            cb.type = 'checkbox';
            cb.value = opt;
            cb.addEventListener('change', () => {
              if (field.singleSelection && cb.checked) {
                for (const peer of checks) peer.checked = peer === cb;
              }
              commitVal(checks.filter((c) => c.checked).map((c) => c.value), true);
            });
            checks.push(cb);
            optLbl.append(cb, document.createTextNode(optionLabel(field, opt)));
            box.appendChild(optLbl);
          }
          controlWrap.appendChild(box);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { for (const c of checks) c.disabled = dis; },
            syncValue(v) {
              const selected = Array.isArray(v) ? v : [];
              for (const c of checks) c.checked = selected.includes(c.value);
            },
          };
        } else if (field.control === 'segments') {
          const segs = document.createElement('div');
          segs.className = 'insp-segments';
          segs.dataset.propKey = field.id;
          segs.setAttribute('role', 'radiogroup');
          segs.setAttribute('aria-label', localizedText(field.label));
          const choices = field.type === 'boolean' ? [false, true] : (field.options || []);
          const segBtns = choices.map((choice) => {
            const b = document.createElement('button');
            b.type = 'button';
            b.className = 'insp-seg-option';
            b.setAttribute('role', 'radio');
            b.dataset.value = String(choice);
            if (field.type === 'boolean' && field.valueLabels?.[String(choice)] === undefined) {
              b.textContent = optionLabel(field, choice ? 'Yes' : 'No');
            } else {
              b.textContent = optionLabel(field, choice);
            }
            b.addEventListener('click', () => commitVal(choice, true));
            segs.appendChild(b);
            return { choice, b };
          });
          controlWrap.appendChild(segs);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { for (const item of segBtns) item.b.disabled = dis; },
            syncValue(v) {
              for (const item of segBtns) {
                item.b.setAttribute('aria-checked', String(item.choice === v));
              }
            },
          };
        } else if (field.type === 'enum' || field.control === 'select' || field.control === 'model-select') {
          const comboHost = document.createElement('div');
          comboHost.className = 'fluent-combo';
          const comboId = `prop-combo-${node.id}-${field.id}`;
          comboHost.dataset.comboId = comboId;
          const makeComboOptions = (f) => {
            if (f.control === 'model-select') {
              return (f.choices || []).map((c) => ({ value: String(c.value), label: choiceDisplayLabel(f, c) }));
            }
            return (f.options || []).map((optVal) => ({ value: optVal, label: optionLabel(f, optVal) }));
          };
          const combo = createFluentComboBox({
            host: comboHost,
            id: comboId,
            triggerId: inputId,
            propKey: field.id,
            ariaLabel: localizedText(field.label),
            options: makeComboOptions(field),
            value: String(model.values[field.id] ?? ''),
            onChange: (nextVal) => commitVal(field.control === 'model-select' ? Number(nextVal) : nextVal, true),
          });
          controlWrap.appendChild(comboHost);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { combo.setDisabled(dis); },
            syncFieldMeta(nextField) { combo.setOptions(makeComboOptions(nextField)); },
            syncValue(v) { combo.setValue(String(v ?? '')); },
          };
        } else if (field.type === 'boolean') {
          const sw = document.createElement('label');
          sw.className = 'fluent-switch';
          const input = document.createElement('input');
          input.id = inputId;
          input.type = 'checkbox';
          input.className = 'studio-switch';
          input.dataset.propKey = field.id;
          input.addEventListener('change', () => commitVal(input.checked, true));
          const track = document.createElement('span');
          track.className = 'fluent-switch-track';
          track.setAttribute('aria-hidden', 'true');
          sw.append(input, track);
          controlWrap.appendChild(sw);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { input.disabled = dis; },
            syncValue(v) { input.checked = Boolean(v); },
          };
        } else if (field.type === 'number') {
          const min = field.minimum ?? 0;
          const max = field.maximum ?? 100;
          const step = field.step ?? (field.integer ? 1 : 0.05);
          const isRange = field.control === 'range'
            || ['iconRotation', 'iconScale', 'iconOffsetX', 'iconOffsetY'].includes(field.id)
            || (max - min <= 250);
          const numInput = document.createElement('input');
          numInput.id = inputId;
          numInput.className = isRange ? 'insp-range-slider' : 'studio-input';
          numInput.type = isRange ? 'range' : 'number';
          numInput.dataset.propKey = field.id;
          numInput.min = String(min);
          numInput.max = String(max);
          numInput.step = String(step);
          let outEl = null;
          if (isRange) {
            outEl = document.createElement('output');
            outEl.className = 'insp-range-value';
          }
          const readNum = () => {
            const n = Number(numInput.value);
            return Number.isFinite(n) ? (field.integer ? Math.round(n) : n) : min;
          };
          numInput.addEventListener('input', () => {
            const val = readNum();
            if (outEl) outEl.value = formatRangeValue(field, val);
            if (isRange) commitVal(val, false);
          });
          numInput.addEventListener('change', () => commitVal(readNum(), true));
          controlWrap.appendChild(numInput);
          if (outEl) controlWrap.appendChild(outEl);
          entry = {
            field,
            rowEl,
            readCurrent() { return readNum(); },
            setDisabled(dis) { numInput.disabled = dis; },
            syncFieldMeta(nextField, allValues) {
              const effMin = nextField.minimumFrom && Number.isFinite(Number(allValues?.[nextField.minimumFrom]))
                ? Number(allValues[nextField.minimumFrom])
                : (nextField.minimum ?? 0);
              let effMax = nextField.maximumFrom && Number.isFinite(Number(allValues?.[nextField.maximumFrom]))
                ? Number(allValues[nextField.maximumFrom])
                : (nextField.maximum ?? 100);
              if (node.type === 'RangeSlider') {
                const lo = Number(allValues?.lowerValue);
                const hi = Number(allValues?.upperValue);
                if (nextField.id === 'lowerValue' && Number.isFinite(hi)) effMax = Math.max(effMin, hi - 1);
                if (nextField.id === 'upperValue' && Number.isFinite(lo)) numInput.min = String(Math.min(effMax, lo + 1));
                else numInput.min = String(effMin);
              } else {
                numInput.min = String(effMin);
              }
              numInput.max = String(effMax);
              if (nextField.step !== undefined) numInput.step = String(nextField.step);
            },
            syncValue(v, effVal) {
              const shown = v === null || v === undefined ? effVal : v;
              if (document.activeElement !== numInput && shown !== undefined && shown !== null) {
                numInput.value = String(shown);
              }
              if (outEl) outEl.value = formatRangeValue(entry.field, v === null ? (effVal ?? '') : (numInput.value || shown));
            },
          };
        } else if (field.type === 'string-list' || field.control === 'textarea') {
          const area = document.createElement('textarea');
          area.id = inputId;
          area.className = 'studio-textarea';
          area.rows = 3;
          area.dataset.propKey = field.id;
          let composing = false;
          const extract = () => (field.type === 'string-list'
            ? area.value.split(/\r?\n/).map((s) => s.trim()).filter(Boolean)
            : area.value);
          area.addEventListener('compositionstart', () => { composing = true; });
          area.addEventListener('compositionend', () => { composing = false; commitVal(extract(), false); });
          area.addEventListener('input', (e) => { if (!composing && !e?.isComposing) commitVal(extract(), false); });
          area.addEventListener('change', () => commitVal(extract(), true));
          controlWrap.appendChild(area);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { area.disabled = dis; },
            syncValue(v) {
              if (document.activeElement === area) return;
              area.value = field.type === 'string-list'
                ? (Array.isArray(v) ? v.join('\n') : '')
                : String(v ?? '');
            },
          };
        } else if (field.type === 'color') {
          const swatchWrap = document.createElement('span');
          swatchWrap.className = 'insp-color-swatch';
          const picker = document.createElement('input');
          picker.type = 'color';
          picker.ariaLabel = localizedText(field.label);
          const hexInput = document.createElement('input');
          hexInput.id = inputId;
          hexInput.type = 'text';
          hexInput.className = 'studio-input';
          hexInput.dataset.propKey = field.id;
          picker.addEventListener('input', () => {
            hexInput.value = picker.value;
            swatchWrap.style.setProperty('--swatch-color', picker.value);
            commitVal(picker.value, false);
          });
          picker.addEventListener('change', () => commitVal(picker.value, true));
          hexInput.addEventListener('change', () => {
            swatchWrap.style.setProperty('--swatch-color', toCssSwatchColor(hexInput.value));
            commitVal(hexInput.value, true);
          });
          swatchWrap.appendChild(picker);
          controlWrap.append(swatchWrap, hexInput);
          entry = {
            field,
            rowEl,
            readCurrent() { return hexInput.value || picker.value; },
            setDisabled(dis) { hexInput.disabled = dis; picker.disabled = dis; },
            syncValue(v, effVal) {
              const shown = v === null || v === undefined ? (effVal || '#0062B0') : String(v);
              swatchWrap.style.setProperty('--swatch-color', toCssSwatchColor(shown));
              picker.value = toHexPickerColor(shown);
              if (document.activeElement !== hexInput) hexInput.value = shown;
            },
          };
        } else if (field.type === 'date' || field.type === 'time' || field.type === 'datetime') {
          const input = document.createElement('input');
          input.id = inputId;
          input.className = 'studio-input';
          input.dataset.propKey = field.id;
          input.type = field.type === 'datetime' ? 'datetime-local' : field.type;
          input.addEventListener('change', () => {
            if (field.nullable && !input.value) { commitVal(null, true); return; }
            const raw = field.type === 'datetime' && input.value.length === 16 ? `${input.value}:00` : input.value;
            commitVal(raw, true);
          });
          controlWrap.appendChild(input);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { input.disabled = dis; },
            syncValue(v) {
              if (document.activeElement === input) return;
              if (!v) { input.value = ''; return; }
              const str = String(v);
              input.value = field.type === 'datetime' ? str.slice(0, 16) : (field.type === 'time' ? str.slice(0, 5) : str.slice(0, 10));
            },
          };
        } else if (field.type === 'numlist') {
          const input = document.createElement('input');
          input.id = inputId;
          input.type = 'text';
          input.className = 'studio-input';
          input.dataset.propKey = field.id;
          const parse = () => input.value.split(',').map((s) => Number(s.trim())).filter((n) => Number.isFinite(n));
          input.addEventListener('input', () => commitVal(parse(), false));
          input.addEventListener('change', () => commitVal(parse(), true));
          controlWrap.appendChild(input);
          entry = {
            field,
            rowEl,
            setDisabled(dis) { input.disabled = dis; },
            syncValue(v) {
              if (document.activeElement !== input) input.value = Array.isArray(v) ? v.join(', ') : '';
            },
          };
        } else {
          const input = document.createElement('input');
          input.id = inputId;
          input.type = 'text';
          input.className = 'studio-input';
          input.dataset.propKey = field.id;
          if (field.maximumLength) input.maxLength = field.maximumLength;
          let composing = false;
          input.addEventListener('compositionstart', () => { composing = true; });
          input.addEventListener('compositionend', () => { composing = false; commitVal(input.value, false); });
          input.addEventListener('input', (e) => { if (!composing && !e?.isComposing) commitVal(input.value, false); });
          input.addEventListener('change', () => commitVal(input.value, true));
          controlWrap.appendChild(input);
          entry = {
            field,
            rowEl,
            readCurrent() { return input.value; },
            setDisabled(dis) { input.disabled = dis; },
            syncValue(v) {
              if (document.activeElement !== input) input.value = String(v ?? '');
            },
          };
        }

        if (field.nullable && field.type !== 'date') {
          const hdr = document.createElement('div');
          hdr.className = 'insp-nullable-header';
          const followLbl = document.createElement('label');
          followLbl.className = 'insp-follow';
          const followCb = document.createElement('input');
          followCb.type = 'checkbox';
          followCb.dataset.followTheme = field.id;
          followCb.setAttribute('aria-label', `${localizedText(field.label)}: ${t('followTheme')}`);
          followCb.addEventListener('change', () => {
            const fallback = entry.readCurrent ? entry.readCurrent() : (field.minimum ?? 0);
            commitVal(followCb.checked ? null : fallback, true);
          });
          followLbl.append(followCb, document.createTextNode(t('followTheme')));
          hdr.append(label, followLbl);
          rowEl.append(hdr, controlWrap);
          entry.followCb = followCb;
        } else {
          rowEl.append(label, controlWrap);
        }

        let noteEl = null;
        if (field.effectHint || field.disabledHint) {
          noteEl = document.createElement('p');
          noteEl.className = 'insp-field-note';
          noteEl.hidden = true;
          rowEl.appendChild(noteEl);
        }
        entry.noteEl = noteEl;

        rowEntries.set(field.id, entry);
        hostContainer.appendChild(rowEl);
      }

      tabPanels.set(catId, panel);
      els.inspectorBody.appendChild(panel);
    }

    const syncController = (nextValues, nextEffective, nextSchema = null) => {
      const latestDoc = currentDoc();
      const latestNode = findNode(latestDoc.root, node.id) || node;
      updateInspectorSummaryHeader(latestNode, meta, model);
      if (nextSchema?.properties) {
        const schemaById = new Map(nextSchema.properties.map((f) => [f.id, f]));
        rowEntries.forEach((entry, id) => {
          if (schemaById.has(id)) {
            entry.field = { ...entry.field, ...schemaById.get(id) };
            entry.syncFieldMeta?.(entry.field, nextValues);
          }
        });
      } else {
        rowEntries.forEach((entry) => entry.syncFieldMeta?.(entry.field, nextValues));
      }

      rowEntries.forEach((entry, id) => {
        const f = entry.field;
        const val = nextValues[id];
        const eff = nextEffective?.[id];
        const visible = evalConditionExpr(f.visibleWhen, nextValues);
        const enabledByExpr = evalConditionExpr(f.enabledWhen, nextValues);
        const nullFollowed = Boolean(f.nullable && f.type !== 'date' && val === null);

        entry.rowEl.hidden = !visible;
        if (entry.followCb) entry.followCb.checked = nullFollowed;
        const disabled = !enabledByExpr || nullFollowed;
        entry.rowEl.classList.toggle('is-disabled', disabled);
        entry.setDisabled(disabled);
        entry.syncValue(val, eff);

        if (entry.noteEl) {
          let noteText = '';
          if (!enabledByExpr && f.disabledHint) noteText = localizedText(f.disabledHint);
          else if (f.effectHint && (!f.effectWhen || !evalConditionExpr(f.effectWhen, nextValues))) noteText = localizedText(f.effectHint);
          entry.noteEl.textContent = noteText;
          entry.noteEl.hidden = !noteText;
        }
      });

      tabPanels.forEach((panel) => {
        panel.querySelectorAll('.insp-property-group').forEach((grp) => {
          grp.hidden = ![...grp.querySelectorAll('.insp-field')].some((el) => !el.hidden);
        });
      });
    };

    syncController(model.values, model.effective, model.inspected);
    state.inspectorController = {
      nodeId: node.id,
      schemaKey: model.schemaKey,
      values: { ...model.values },
      effective: { ...model.effective },
      sync: syncController,
    };
  }

  if (node.id === 'root') {
    const actions = document.createElement('div');
    actions.className = 'insp-root-actions';
    const hasChildren = (node.children || []).length > 0;
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.id = hasChildren ? 'btn-clear-canvas' : 'btn-restore-sample';
    btn.className = 'insp-secondary-btn';
    btn.textContent = t(hasChildren ? 'clearCanvasBtn' : 'restoreSampleBtn');
    btn.addEventListener('click', () => {
      if (hasChildren) {
        const fallbackProps = node.type === 'AnchorBox'
          ? { minHeight: 540, padding: 16 }
          : { spacing: 14, padding: 20 };
        commitDocument(
          {
            schemaVersion: 1,
            root: {
              id: 'root',
              type: node.type || 'AnchorBox',
              props: { ...(node.props || fallbackProps) },
              children: [],
            },
          },
          'root',
          true,
          false,
        );
      } else {
        loadTemplate(STUDIO_TEMPLATES.starter);
      }
    });
    actions.appendChild(btn);
    els.inspectorBody.appendChild(actions);
  }
}

function normalizePropPatch(doc, nodeId, patch) {
  const node = findNode(doc.root, nodeId);
  if (node?.type === 'CheckBox' && Object.prototype.hasOwnProperty.call(patch, 'checkState')) {
    return { ...patch, checked: patch.checkState === 'Checked' };
  }
  return patch;
}

function applyPropPatch(nodeId, patch, commitHistoryEntry) {
  const curDoc = currentDoc();
  const normalizedPatch = normalizePropPatch(curDoc, nodeId, patch);
  const res = updateNodeProps(curDoc, nodeId, normalizedPatch);
  if (!res.ok) return;

  if (state.pendingPatch && state.pendingPatch.nodeId !== nodeId) {
    const stale = state.pendingPatch;
    state.pendingPatch = null;
    clearTimeout(state.propSyncTimerId);
    syncPatchToWasm(stale.nodeId, stale.patch, curDoc);
  }

  if (commitHistoryEntry) {
    state.liveDraftDoc = null;
    state.history.push(res.document);
  } else {
    state.liveDraftDoc = res.document;
  }
  if (state.inspectorController && state.inspectorController.nodeId === nodeId) {
    state.inspectorController.values = { ...state.inspectorController.values, ...normalizedPatch };
    state.inspectorController.sync(state.inspectorController.values, state.inspectorController.effective, null);
  }
  updateToolbarButtons();
  renderSchematic();
  requestAnimationFrame(syncOverlay);
  renderCodeDrawer();

  state.pendingPatch = {
    nodeId,
    patch: { ...(state.pendingPatch?.patch || {}), ...normalizedPatch },
  };
  clearTimeout(state.propSyncTimerId);
  state.propSyncTimerId = setTimeout(() => {
    const queued = state.pendingPatch;
    state.pendingPatch = null;
    state.propSyncTimerId = null;
    if (queued) syncPatchToWasm(queued.nodeId, queued.patch, currentDoc());
  }, commitHistoryEntry ? 0 : 90);
}

function syncPlayInteractionToDoc(nodeId, eventName, value) {
  const doc = currentDoc();
  const node = findNode(doc.root, nodeId);
  const patch = mapInteractionEventToProps(node, eventName, value);
  if (!patch) return;
  const res = updateNodeProps(doc, nodeId, patch);
  if (!res.ok) return;

  const now = Date.now();
  const sameGesture = state.playGesture.nodeId === nodeId
    && state.playGesture.eventName === eventName
    && (now - state.playGesture.at) < 400
    && state.history.canUndo();
  state.playGesture = { nodeId, eventName, at: now };
  state.liveDraftDoc = null;
  if (sameGesture) state.history.replaceCurrent(res.document);
  else state.history.push(res.document);

  if (state.inspectorController && state.inspectorController.nodeId === nodeId) {
    state.inspectorController.values = { ...state.inspectorController.values, ...patch };
    state.inspectorController.sync(state.inspectorController.values, state.inspectorController.effective, null);
  }
  updateToolbarButtons();
  renderCodeDrawer();
  if (!state.wasmReady) {
    syncSchematicShellHeight();
    requestAnimationFrame(syncOverlay);
  }
}

function resetSelectedProps() {
  flushLiveDraft({ syncWasm: false });
  const doc = currentDoc();
  const node = findNode(doc.root, state.selectedId);
  if (!node) return;
  const parentInfo = findParent(doc.root, node.id);
  const preserve = parentInfo?.parent?.type === 'AnchorBox' ? ANCHOR_PROP_KEYS : [];
  const res = resetNodeProps(doc, node.id, preserve);
  if (!res.ok) return;
  commitDocument(res.document, node.id, true);
  showToast(t('propsResetToast'));
}

function updateToolbarButtons() {
  els.btnUndo.disabled = !state.history.canUndo();
  els.btnRedo.disabled = !state.history.canRedo();
  const isRoot = state.selectedId === 'root';
  els.btnDelete.disabled = isRoot;
  els.btnDuplicate.disabled = isRoot;
}

function renderCodeDrawer() {
  if (els.codeDrawer.hidden) return;
  const doc = currentDoc();
  let code = '';
  if (state.codeFormat === 'cpp') code = generateCpp(doc, state.schemaCache);
  else if (state.codeFormat === 'python') code = generatePython(doc, state.schemaCache);
  else if (state.codeFormat === 'ui') code = generateDesignerUi(doc);
  else code = JSON.stringify(doc, null, 2);
  els.codeOutput.textContent = code;
}

const MEDIA_THEME_GRADIENTS = {
  Landscape: 'linear-gradient(135deg, #2e6d92 0%, #6eb5b5 55%, #f0b88c 100%)',
  Coffee: 'linear-gradient(135deg, #5c3a21 0%, #a46b3e 55%, #e5b887 100%)',
  Architecture: 'linear-gradient(135deg, #2c3e50 0%, #5b7896 55%, #c8d6e5 100%)',
  City: 'linear-gradient(135deg, #1b2845 0%, #48639c 55%, #f3a683 100%)',
  Portrait: 'linear-gradient(135deg, #7f4448 0%, #c8746c 55%, #f5cdb6 100%)',
  Technology: 'linear-gradient(135deg, #0b2538 0%, #008c99 55%, #7de2ec 100%)',
  Abstract: 'linear-gradient(135deg, #4b2d73 0%, #8764b8 55%, #e6b8ff 100%)',
  Music: 'linear-gradient(135deg, #28194a 0%, #8764b8 55%, #f283b6 100%)',
};
let activeDocThemeOverride = false;

function contrastForegroundHex(hex) {
  const r = parseInt(hex.slice(1, 3), 16);
  const g = parseInt(hex.slice(3, 5), 16);
  const b = parseInt(hex.slice(5, 7), 16);
  return ((0.299 * r + 0.587 * g + 0.114 * b) / 255) > 0.62 ? '#0b141c' : '#ffffff';
}

function syncArtboardBar(doc, effectiveCanvasTheme) {
  const design = doc?.design && typeof doc.design === 'object' ? doc.design : null;
  if (els.artboardTitle) {
    els.artboardTitle.textContent = `Artboard · ${state.viewportWidth}px`;
  }
  const hasExplicitTheme = design?.theme === 'light' || design?.theme === 'dark';
  const activeBarTheme = hasExplicitTheme ? design.theme : (effectiveCanvasTheme === 'dark' ? 'dark' : 'light');
  els.artboardThemeButtons.forEach((btn) => {
    const active = btn.dataset.artboardTheme === activeBarTheme;
    btn.classList.toggle('is-active', active);
    btn.setAttribute('aria-pressed', String(active));
  });
  const accUpper = typeof design?.accentColor === 'string' && /^#[0-9a-fA-F]{6}$/.test(design.accentColor)
    ? design.accentColor.toUpperCase()
    : '';
  els.artboardSwatches.forEach((btn) => {
    const active = (btn.dataset.artboardAccent || '').toUpperCase() === accUpper;
    btn.classList.toggle('is-active', active);
    btn.setAttribute('aria-pressed', String(active));
  });
  if (els.btnArtboardReset) {
    els.btnArtboardReset.hidden = !(hasExplicitTheme || Boolean(accUpper));
  }
}

function applyDocumentDesign(doc) {
  const design = doc?.design && typeof doc.design === 'object' ? doc.design : null;
  const nextCanvasTheme = forcedColorsQuery.matches
    ? 'high-contrast'
    : ((design?.theme === 'light' || design?.theme === 'dark') ? design.theme : state.theme);
  const themeChanged = state.canvasTheme !== nextCanvasTheme;
  state.canvasTheme = nextCanvasTheme;

  if (els.canvasShell) {
    els.canvasShell.dataset.canvasTheme = nextCanvasTheme;
    const acc = typeof design?.accentColor === 'string' && /^#[0-9a-fA-F]{6}$/.test(design.accentColor)
      ? design.accentColor
      : null;
    if (acc) {
      els.canvasShell.style.setProperty('--accent', acc);
      els.canvasShell.style.setProperty('--accent-fg', contrastForegroundHex(acc));
      els.canvasShell.style.setProperty('--active', `color-mix(in srgb, ${acc} 14%, transparent)`);
      els.canvasShell.style.setProperty('--studio-doc-accent', acc);
    } else {
      els.canvasShell.style.removeProperty('--accent');
      els.canvasShell.style.removeProperty('--accent-fg');
      els.canvasShell.style.removeProperty('--active');
      els.canvasShell.style.removeProperty('--studio-doc-accent');
    }
  }

  if (themeChanged) {
    postToWasm({ type: 'theme', theme: nextCanvasTheme });
    if (state.wasmReady) {
      setTimeout(() => {
        postToWasm({ type: 'studio-refresh', session: state.wasmSession });
      }, 90);
    }
  }
  syncArtboardBar(doc, nextCanvasTheme);
}

function renderAll() {
  applyDocumentDesign(currentDoc());
  updateToolbarButtons();
  renderBreadcrumbs();
  renderOutline();
  renderInspector();
  renderSchematic();
  renderCodeDrawer();
  requestAnimationFrame(syncOverlay);
}

/* Schematic Preview Renderer (Active during drag ghost, boot & offline fallback) */
function renderSchematic(doc = currentDoc()) {
  els.schematicStage.innerHTML = '';
  els.schematicStage.appendChild(renderSchematicNode(doc.root));
}

function fireSchematicEvent(nodeId, eventName, value = '') {
  const shown = eventName === 'textChanged' && String(value).length > 36
    ? `${String(value).length} chars`
    : value;
  setEventPill(`${nodeId} · ${eventName}${shown !== '' ? ` (${shown})` : ''}`);
  syncPlayInteractionToDoc(nodeId, eventName, value);
}

const SCHEMATIC_GLYPHS = {
  Settings: 'M8 10.2a2.2 2.2 0 1 0 0-4.4 2.2 2.2 0 0 0 0 4.4Zm0-7.7v1.3m0 8.4v1.3M3.6 4.6l1 1m6.8 6.8 1 1M2.5 8h1.3m8.4 0h1.3M3.6 11.4l1-1m6.8-6.8 1-1',
  Home: 'M2.5 7.5 8 3l5.5 4.5V13a1 1 0 0 1-1 1h-9a1 1 0 0 1-1-1V7.5ZM6.5 14V9.5h3V14',
  Search: 'M7 11.2a4.2 4.2 0 1 0 0-8.4 4.2 4.2 0 0 0 0 8.4Zm3.2-1 3.3 3.3',
  Save: 'M3 3.5h8.2L13 5.3v7.2a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V3.5Zm2.5 0v3h4.5v-3M5.5 13.5v-4h5v4',
  Edit: 'M11.2 2.6a1.4 1.4 0 0 1 2 2L6 11.8l-3 .75.75-3 7.45-6.95Z',
  Add: 'M8 3.2v9.6M3.2 8h9.6',
  Share: 'M8 9.8V2.5M5.2 5.2 8 2.4l2.8 2.8M3.5 9.5V13a1 1 0 0 0 1 1h7a1 1 0 0 0 1-1V9.5',
  Export: 'M8 9.8V2.5M5.2 5.2 8 2.4l2.8 2.8M3.5 9.5V13a1 1 0 0 0 1 1h7a1 1 0 0 0 1-1V9.5',
  Info: 'M8 14a6 6 0 1 0 0-12 6 6 0 0 0 0 12Zm0-6.5V11M8 5.2h.01',
  Document: 'M4 2.5h5.2L12.5 5.8V13a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V3.5a1 1 0 0 1 1-1Zm5 0v3.5h3.5',
  File: 'M4 2.5h5.2L12.5 5.8V13a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1V3.5a1 1 0 0 1 1-1Zm5 0v3.5h3.5',
  Folder: 'M2.5 4.5a1 1 0 0 1 1-1h3l1.2 1.4h4.8a1 1 0 0 1 1 1v6.6a1 1 0 0 1-1 1h-9a1 1 0 0 1-1-1v-8Z',
  Refresh: 'M13 6.6A5 5 0 1 0 12 11M13 3.5v3.1h-3.1',
  Play: 'M5 3.4 12.5 8 5 12.6V3.4Z',
  Pause: 'M5 3.5h2v9H5Zm4.5 0h2v9h-2Z',
  Delete: 'M3.5 4.5h9M6 4.5V3h4v1.5M5 4.5l.6 8.5h4.8l.6-8.5',
  Grid: 'M2.5 2.5h4.5v4.5H2.5Zm6.5 0h4.5v4.5H9Zm-6.5 6.5h4.5v4.5H2.5Zm6.5 0h4.5v4.5H9Z',
  Camera: 'M2.5 5.2h2.2l1-1.7h4.6l1 1.7h2.2a1 1 0 0 1 1 1v6.3a1 1 0 0 1-1 1h-11a1 1 0 0 1-1-1V6.2a1 1 0 0 1 1-1ZM8 11.3a2.3 2.3 0 1 0 0-4.6 2.3 2.3 0 0 0 0 4.6Z',
  Music: 'M6 12V4.5l7-1.5v7.5M6 12a1.7 1.7 0 1 1-3.4 0A1.7 1.7 0 0 1 6 12Zm7-1.5a1.7 1.7 0 1 1-3.4 0 1.7 1.7 0 0 1 3.4 0Z',
  Heart: 'M8 13.3 3.2 8.7a3 3 0 0 1 4.2-4.2L8 5.1l.6-.6a3 3 0 1 1 4.2 4.2L8 13.3Z',
  FavoriteStar: 'M8 2.4l1.7 3.5 3.8.6-2.7 2.7.6 3.8L8 11.2 4.6 13l.6-3.8L2.5 6.5l3.8-.6L8 2.4Z',
  Cloud: 'M4.8 12.5h6.8a2.5 2.5 0 0 0 .4-5 3.6 3.6 0 0 0-6.9-.9A2.6 2.6 0 0 0 4.8 12.5Z',
  Upload: 'M8 10.5V2.8M5 5.6 8 2.6l3 3M3 10.5V13h10v-2.5',
  Download: 'M8 2.8v7.7M5 7.7l3 3 3-3M3 10.5V13h10v-2.5',
  Filter: 'M2.5 3.5h11L9.2 8.4v3.8L6.8 13.5V8.4L2.5 3.5Z',
  Sync: 'M12.8 6.5A5 5 0 0 0 4 4.8M12.8 3.2v3.3H9.5M3.2 9.5a5 5 0 0 0 8.8 1.7M3.2 12.8V9.5h3.3',
  Brush: 'M11.6 2.5 13.5 4.4 7.2 10.7l-1.9-1.9 6.3-6.3ZM5.3 8.8C3.5 9.5 3.8 11.8 2.5 13c1.9.4 3.7-.2 4.4-2.1',
  Color: 'M8 2.4a5.6 5.6 0 1 0 0 11.2c1.1 0 1.7-.7 1.7-1.4 0-.5-.2-.9-.2-1.2 0-.6.5-1 1.1-1H12a1.6 1.6 0 0 0 1.6-1.6A5.6 5.6 0 0 0 8 2.4Z',
  Shield: 'M8 2.3 13 4.3v3.5c0 3.1-2.1 5.2-5 6-2.9-.8-5-2.9-5-6V4.3l5-2Z',
  Lock: 'M4 7.5h8v5.5H4Zm1.5 0V5.3a2.5 2.5 0 0 1 5 0v2.2',
  Mail: 'M2.5 4.2h11v7.6h-11Zm0 .4 5.5 3.9 5.5-3.9',
  Calendar: 'M2.5 4.5h11v9h-11Zm2.8-2v2.4m5.4-2.4v2.4M2.5 7.5h11',
  Clock: 'M8 13.5a5.5 5.5 0 1 0 0-11 5.5 5.5 0 0 0 0 11Zm0-8V8l2 1.4',
  Video: 'M2.5 4.8h7.4v6.4H2.5Zm7.4 2.2 3.6-1.8v5.6L9.9 9',
  Headphones: 'M3 10V8a5 5 0 0 1 10 0v2M3 9.5h2V13H3Zm8 0h2V13h-2Z',
  List: 'M5.5 4.2h8M5.5 8h8M5.5 11.8h8M2.5 4.2h.5M2.5 8h.5M2.5 11.8h.5',
  Warning: 'M8 2.6 13.6 12.8H2.4L8 2.6Zm0 4v2.8m0 1.8h.01',
  CheckMark: 'M3 8.4 6.4 11.8 13 4.6',
  Dismiss: 'M4 4l8 8m0-8-8 8',
  People: 'M6 7.5a2.2 2.2 0 1 0 0-4.4 2.2 2.2 0 0 0 0 4.4ZM2.5 13a3.5 3.5 0 0 1 7 0M11 8.5a1.8 1.8 0 1 0 0-3.6M13.5 13a2.8 2.8 0 0 0-2.5-2.7',
};

const SCHEMATIC_GLYPH_ALIASES = {
  palette: 'Color',
  paint: 'Brush',
  design: 'Brush',
  dashboard: 'Grid',
  analytics: 'Grid',
  metrics: 'Grid',
  bell: 'Info',
  notification: 'Info',
  photo: 'Camera',
  gallery: 'Camera',
  image: 'Camera',
  sparkle: 'FavoriteStar',
  ai: 'FavoriteStar',
  code: 'Document',
  terminal: 'Document',
  trash: 'Delete',
  close: 'Dismiss',
  user: 'People',
  rocket: 'Upload',
  globe: 'Cloud',
};

function resolveSchematicGlyph(name) {
  const clean = String(name || '').trim();
  if (!clean) return 'Document';
  if (SCHEMATIC_GLYPHS[clean]) return clean;
  const low = clean.toLowerCase();
  const alias = SCHEMATIC_GLYPH_ALIASES[low];
  if (alias) return alias;
  const direct = Object.keys(SCHEMATIC_GLYPHS).find((k) => k.toLowerCase() === low);
  return direct || 'Document';
}

function createGlyphSvg(name, size = 14) {
  const resolvedName = resolveSchematicGlyph(name);
  const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
  svg.setAttribute('viewBox', '0 0 16 16');
  svg.setAttribute('width', String(size));
  svg.setAttribute('height', String(size));
  svg.setAttribute('fill', 'none');
  svg.setAttribute('aria-hidden', 'true');
  const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
  path.setAttribute('d', SCHEMATIC_GLYPHS[resolvedName] || SCHEMATIC_GLYPHS.Document);
  path.setAttribute('stroke', 'currentColor');
  path.setAttribute('stroke-width', '1.45');
  path.setAttribute('stroke-linecap', 'round');
  path.setAttribute('stroke-linejoin', 'round');
  svg.appendChild(path);
  return svg;
}

function parseItemIconAndLabel(raw) {
  const str = String(raw || '');
  const sep = str.indexOf('|');
  if (sep > 0) {
    const left = str.slice(0, sep).trim();
    if (/^[A-Za-z_][A-Za-z0-9_]*$/.test(left)) {
      const right = str.slice(sep + 1).trim();
      return { glyph: resolveSchematicGlyph(left), label: right || left };
    }
  }
  return { glyph: inferCommandGlyph(str), label: str.trim() };
}

function inferCommandGlyph(label) {
  const s = String(label || '');
  if (/new|add|create|新建|添加|新增/i.test(s)) return 'Add';
  if (/save|commit|apply|保存|提交/i.test(s)) return 'Save';
  if (/edit|rename|format|编辑|修改|格式/i.test(s)) return 'Edit';
  if (/search|find|filter|搜索|筛选|查找/i.test(s)) return 'Search';
  if (/refresh|sync|reload|刷新|同步/i.test(s)) return 'Refresh';
  if (/share|export|download|导出|分享/i.test(s)) return 'Export';
  if (/play|run|start|运行|执行/i.test(s)) return 'Play';
  if (/delete|remove|close|删除|移除|关闭/i.test(s)) return 'Delete';
  if (/setting|config|设置|配置/i.test(s)) return 'Settings';
  if (/home|overview|概览|首页/i.test(s)) return 'Home';
  return 'Document';
}

function isHeaderLeadingChild(node) {
  if (!node) return false;
  if (node.type === 'Breadcrumb' || node.type === 'FontIcon') return true;
  if (node.type === 'Label') {
    const typo = node.props?.typography || 'Body';
    return /Title|Subtitle|Display|Strong/.test(typo);
  }
  if (node.type === 'HBox' && Array.isArray(node.children)) {
    return node.children.some((c) => isHeaderLeadingChild(c));
  }
  return false;
}

function hasSidebarDescendant(node, depth = 0) {
  if (!node || depth > 2) return false;
  if (['ListView', 'TreeView', 'FileListView', 'NavigationView'].includes(node.type)) return true;
  return (node.children || []).some((c) => hasSidebarDescendant(c, depth + 1));
}

function classifyStatusTone(text) {
  const low = String(text || '').trim().toLowerCase();
  if (!low || low.length > 24) return '';
  if (/^(healthy|ready|online|active|done|success|verified|normal|ok|saved|utf-8|正常|在线|已完成|就绪|已保存)$/.test(low)) {
    return 'is-ok';
  }
  if (/^(warning|pending|degraded|syncing|running|draft|警告|待处理|同步中)$/.test(low)) {
    return 'is-warn';
  }
  if (/^(error|failed|critical|offline|down|timeout|异常|失败|离线|错误)$/.test(low)) {
    return 'is-err';
  }
  return '';
}

function textElement(tag, value) {
  const element = document.createElement(tag);
  element.textContent = value;
  return element;
}

// Calendar cells have no per-node handlers. Reuse the month's markup across identical previews.
let schematicMonth;
function calendarPreview(props) {
  const today = new Date();
  const selected = typeof props.selectedDate === 'string'
    ? new Date(props.selectedDate.slice(0, 10) + 'T12:00:00') : today;
  const date = Number.isNaN(selected.getTime()) ? today : selected;
  const locale = document.documentElement.lang;
  const key = [locale, date.toDateString(), today.toDateString(), props.firstDayOfWeek].join('|');
  if (schematicMonth?.key !== key) {
    const calendar = document.createElement('div');
    calendar.className = 'sch-calendar';
    const hd = textElement('div', new Intl.DateTimeFormat(locale, { month: 'long', year: 'numeric' }).format(date));
    hd.className = 'sch-calendar-hd';
    const dow = document.createElement('div');
    dow.className = 'sch-calendar-dow';
    const weekdays = ['Sunday','Monday','Tuesday','Wednesday','Thursday','Friday','Saturday'];
    const explicit = weekdays.indexOf(props.firstDayOfWeek);
    const region = new Intl.Locale(locale);
    const first = explicit >= 0 ? explicit : (region.getWeekInfo?.().firstDay ?? region.weekInfo?.firstDay ?? 7) % 7;
    const weekday = new Intl.DateTimeFormat(locale, { weekday: 'short' });
    for (let i = 0; i < 7; ++i) dow.appendChild(textElement('span', weekday.format(new Date(2023, 0, 1 + first + i))));
    const grid = document.createElement('div');
    grid.className = 'sch-calendar-grid';
    const cursor = new Date(date.getFullYear(), date.getMonth(), 1, 12);
    cursor.setDate(1 - (cursor.getDay() - first + 7) % 7);
    do {
      const cell = textElement('span', String(cursor.getDate()));
      cell.className = 'sch-calendar-day';
      cell.classList.toggle('is-out', cursor.getMonth() !== date.getMonth());
      cell.classList.toggle('is-today', cursor.toDateString() === date.toDateString());
      grid.appendChild(cell);
      cursor.setDate(cursor.getDate() + 1);
    } while (cursor.getMonth() === date.getMonth() || cursor.getDay() !== first);
    calendar.append(hd, dow, grid);
    schematicMonth = { key, calendar };
  }
  const copy = schematicMonth.calendar.cloneNode(true);
  copy.classList.toggle('is-disabled', props.enabled === false);
  return copy;
}

function renderSchematicNode(node, { interactive = true } = {}) {
  const p = node.props || {};
  const wrap = document.createElement('div');
  wrap.className = 'sch-node';
  wrap.dataset.schId = node.id;

  function temporalInput(type, value, event, label) {
    const input = document.createElement('input');
    input.type = type;
    input.lang = document.documentElement.lang;
    input.className = 'sch-temporal-input';
    input.setAttribute('aria-label', label);
    input.disabled = p.enabled === false;
    if (type !== 'date') input.step = 'any';
    const raw = value || '';
    const rangeEndpoint = node.type === 'DateTimeRangePicker';
    // Browser datetime-local inputs have no zone field; keep a document's original offset on edits.
    const zone = rangeEndpoint ? raw.match(/T.*?(Z|[+-]\d{2}:\d{2})$/)?.[1] || '' : '';
    input.value = type === 'date' ? raw.slice(0, 10) : zone ? raw.slice(0, -zone.length) : raw;
    if (interactive) input.addEventListener('change', () => {
      if (!input.validity.valid) return;
      let selected = input.value;
      if (selected && rangeEndpoint) {
        if (type === 'date') selected += 'T00:00:00';
        selected += zone;
      }
      fireSchematicEvent(node.id, event, selected);
    });
    return input;
  }
  const renderChildrenInto = (containerEl) => {
    const children = node.children || [];
    if (!children.length) {
      const hint = document.createElement('div');
      hint.className = 'sch-empty-slot';
      hint.textContent = t('emptyContainer');
      containerEl.appendChild(hint);
      return;
    }
    for (const child of children) {
      containerEl.appendChild(renderSchematicNode(child, { interactive }));
    }
  };

  switch (node.type) {
    case 'VBox': {
      wrap.classList.add('sch-vbox');
      wrap.style.gap = `${p.spacing ?? 12}px`;
      wrap.style.padding = `${p.padding ?? 16}px`;
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'HBox': {
      wrap.classList.add('sch-hbox');
      const children = node.children || [];
      const leadingTypes = new Set(['FontIcon', 'Label', 'InfoBadge']);
      const trailingTypes = new Set([
        'AutoSuggestBox', 'LineEdit', 'ComboBox', 'MultiSelectComboBox',
        'Button', 'DropDownButton', 'SplitButton', 'ToggleButton',
        'ToggleSwitch', 'SelectorBar', 'Pivot', 'Avatar', 'DatePicker', 'CalendarDatePicker',
      ]);
      let splitIdx = -1;
      if (children.length >= 2) {
        let prefix = 0;
        while (prefix < children.length && leadingTypes.has(children[prefix].type)) prefix++;
        if (prefix >= 1 && prefix < children.length && trailingTypes.has(children[prefix].type)) {
          splitIdx = prefix;
        }
      }
      wrap.style.gap = `${p.spacing ?? 12}px`;
      wrap.style.padding = `${p.padding ?? 0}px`;
      if (p.align === 'Center' || splitIdx >= 0) wrap.style.alignItems = 'center';
      else if (p.align === 'Start') wrap.style.alignItems = 'flex-start';
      else if (p.align === 'End') wrap.style.alignItems = 'flex-end';
      renderChildrenInto(wrap);
      if (splitIdx >= 1 && wrap.children[splitIdx - 1]) {
        wrap.classList.add('is-header-row');
        wrap.children[splitIdx - 1].classList.add('sch-hbox-push');
      }
      return wrap;
    }
    case 'Grid': {
      wrap.classList.add('sch-grid');
      const cols = Math.max(1, Math.min(4, Number(p.columns ?? 2)));
      wrap.dataset.cols = String(cols);
      wrap.style.gridTemplateColumns = `repeat(${cols}, minmax(0, 1fr))`;
      wrap.style.gap = `${p.spacing ?? 12}px`;
      wrap.style.padding = `${p.padding ?? 0}px`;
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'Card': {
      wrap.classList.add('sch-card');
      if (p.borderVisible === false) wrap.classList.add('is-borderless');
      if (p.appearance === 'LayerAlt') wrap.classList.add('is-alt');
      else if (p.appearance === 'Canvas') wrap.classList.add('is-canvas');
      wrap.style.gap = `${p.spacing ?? 10}px`;
      wrap.style.padding = `${p.padding ?? 16}px`;
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'Expander': {
      wrap.classList.add('sch-expander');
      const hd = document.createElement('button');
      hd.type = 'button';
      hd.className = 'sch-expander-hd';
      const span = document.createElement('span');
      span.textContent = p.headerText || 'Expander';
      const chev = document.createElement('span');
      let expanded = p.expanded !== false;
      chev.textContent = expanded ? '▾' : '▸';
      hd.append(span, chev);
      const body = document.createElement('div');
      body.className = 'sch-expander-body';
      body.hidden = !expanded;
      renderChildrenInto(body);
      hd.addEventListener('click', () => {
        expanded = !expanded;
        body.hidden = !expanded;
        chev.textContent = expanded ? '▾' : '▸';
        fireSchematicEvent(node.id, 'expandedChanged', expanded);
      });
      wrap.append(hd, body);
      return wrap;
    }
    case 'Field': {
      wrap.classList.add('sch-field');
      if (p.labelPosition === 'Leading') wrap.classList.add('is-leading');
      const lbl = document.createElement('div');
      lbl.className = 'sch-field-label';
      lbl.textContent = p.labelText || 'Field';
      if (p.required) {
        const star = document.createElement('span');
        star.className = 'sch-field-req';
        star.textContent = '*';
        lbl.appendChild(star);
      }
      wrap.appendChild(lbl);
      const child = (node.children || [])[0];
      if (child) {
        wrap.appendChild(renderSchematicNode(child, { interactive }));
      } else {
        const empty = document.createElement('div');
        empty.className = 'sch-empty-slot';
        empty.textContent = t('emptyContainer');
        wrap.appendChild(empty);
      }
      if (p.helperText) {
        const h = document.createElement('div');
        h.className = 'sch-field-help';
        h.textContent = p.helperText;
        wrap.appendChild(h);
      }
      if (p.validationMessage) {
        const m = document.createElement('div');
        m.className = `sch-field-msg ${p.validationState || ''}`;
        m.textContent = p.validationMessage;
        wrap.appendChild(m);
      }
      return wrap;
    }
    case 'Divider': {
      wrap.className = p.orientation === 'Vertical' ? 'sch-node sch-divider-v' : 'sch-node sch-divider-h';
      return wrap;
    }
    case 'Button':
    case 'ToggleButton':
    case 'RepeatButton': {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = `sch-btn ${p.fluentStyle || 'Standard'} ${p.fluentSize || 'StandardSize'}`;
      let checked = Boolean(p.checked);
      if (node.type === 'ToggleButton' && checked) btn.classList.add('is-checked');
      btn.disabled = p.enabled === false;
      const labelText = p.text || node.type;
      if (node.type === 'Button' && typeof p.icon === 'string' && p.icon && p.icon !== 'None') {
        btn.append(createGlyphSvg(p.icon, 13), textElement('span', labelText));
      } else {
        btn.textContent = labelText;
      }
      if (interactive) {
        btn.addEventListener('click', () => {
          if (node.type === 'ToggleButton') {
            checked = !checked;
            btn.classList.toggle('is-checked', checked);
            fireSchematicEvent(node.id, 'toggled', checked);
          } else {
            fireSchematicEvent(node.id, 'clicked', labelText);
          }
        });
      }
      wrap.appendChild(btn);
      return wrap;
    }
    case 'CompoundButton': {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = `sch-btn sch-compound-btn ${p.fluentStyle || 'Standard'} ${p.fluentSize || 'StandardSize'}`;
      btn.disabled = p.enabled === false;
      const titleEl = document.createElement('span');
      titleEl.textContent = p.text || 'Compound Action';
      const subEl = document.createElement('small');
      subEl.textContent = p.secondaryText || 'Secondary description';
      btn.append(titleEl, subEl);
      if (interactive) {
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'clicked', titleEl.textContent));
      }
      wrap.appendChild(btn);
      return wrap;
    }
    case 'DropDownButton':
    case 'SplitButton':
    case 'ToggleSplitButton': {
      const box = document.createElement('div');
      box.className = 'sch-split-btn';
      const main = document.createElement('span');
      main.className = 'sch-split-main';
      main.textContent = p.text || node.type;
      const chev = document.createElement('span');
      chev.className = 'sch-split-chev';
      chev.textContent = '▾';
      box.append(main, chev);
      wrap.appendChild(box);
      return wrap;
    }
    case 'HyperlinkButton': {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'sch-link';
      btn.disabled = p.enabled === false;
      btn.textContent = p.text || 'Learn more';
      if (interactive) {
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'clicked', btn.textContent));
      }
      wrap.appendChild(btn);
      return wrap;
    }
    case 'CheckBox':
    case 'RadioButton': {
      const row = document.createElement('label');
      row.className = 'sch-check-row';
      const input = document.createElement('input');
      input.type = node.type === 'RadioButton' ? 'radio' : 'checkbox';
      input.disabled = p.enabled === false;
      input.checked = Boolean(p.checked || p.checkState === 'Checked');
      const span = document.createElement('span');
      span.textContent = p.text || node.type;
      if (interactive) {
        input.addEventListener('change', () => fireSchematicEvent(node.id, 'toggled', input.checked));
      }
      row.append(input, span);
      wrap.appendChild(row);
      return wrap;
    }
    case 'ToggleSwitch': {
      const row = document.createElement('label');
      row.className = 'sch-check-row';
      const sw = document.createElement('span');
      sw.className = 'fluent-switch';
      const input = document.createElement('input');
      input.type = 'checkbox';
      input.className = 'studio-switch';
      input.disabled = p.enabled === false;
      input.checked = Boolean(p.isOn);
      const track = document.createElement('span');
      track.className = 'fluent-switch-track';
      track.setAttribute('aria-hidden', 'true');
      sw.append(input, track);
      const span = document.createElement('span');
      const syncText = () => {
        span.textContent = input.checked ? (p.onContent || 'On') : (p.offContent || 'Off');
      };
      syncText();
      if (interactive) {
        input.addEventListener('change', () => {
          syncText();
          fireSchematicEvent(node.id, 'toggled', input.checked);
        });
      }
      row.append(sw, span);
      wrap.appendChild(row);
      return wrap;
    }
    case 'ComboBox':
    case 'MultiSelectComboBox':
    case 'TreeSelect': {
      const items = p.items || ['Option A', 'Option B'];
      const host = document.createElement('div');
      host.className = 'fluent-combo';
      const idx = Math.max(0, Math.min(items.length - 1, Number(p.currentIndex ?? 0)));
      const triggerVal = node.type === 'ComboBox' ? (items[idx] || node.type) : (p.placeholderText || node.type);
      if (!interactive) {
        host.innerHTML = `<button type="button" class="fluent-combo-trigger" disabled><span class="fluent-combo-label"></span><span class="fluent-combo-chevron">▾</span></button>`;
        host.querySelector('.fluent-combo-label').textContent = triggerVal;
      } else {
        createFluentComboBox({
          host,
          id: `sch-combo-${node.id}`,
          ariaLabel: node.id,
          disabled: p.enabled === false,
          options: items.map((item, i) => ({ value: String(i), label: item })),
          value: String(idx),
          onChange: (nextIdx) => fireSchematicEvent(node.id, 'currentIndexChanged', Number(nextIdx)),
        });
      }
      wrap.appendChild(host);
      return wrap;
    }
    case 'Slider':
    case 'RangeSlider': {
      const input = document.createElement('input');
      input.type = 'range';
      input.className = 'sch-slider';
      input.min = String(p.minimum ?? 0);
      input.max = String(p.maximum ?? 100);
      input.value = String(p.value ?? (node.type === 'RangeSlider' ? (p.upperValue ?? 75) : 50));
      input.disabled = p.enabled === false;
      if (interactive) {
        input.addEventListener('change', () => fireSchematicEvent(node.id, 'valueChanged', Number(input.value)));
      }
      wrap.appendChild(input);
      return wrap;
    }
    case 'TagInput': {
      const row = document.createElement('div');
      row.className = 'sch-tag-row';
      for (const item of p.tags || ['Qt 6.9', 'Fluent 2']) {
        const chip = document.createElement('span');
        chip.className = 'sch-tag';
        chip.textContent = item;
        row.appendChild(chip);
      }
      wrap.appendChild(row);
      return wrap;
    }
    case 'PipsPager': {
      const pips = document.createElement('div');
      pips.className = 'sch-pips';
      const count = Math.max(3, Math.min(8, Number(p.numberOfPages ?? p.pages ?? 5)));
      const cur = Number(p.selectedPageIndex ?? 0);
      for (let i = 0; i < count; i++) {
        const dot = document.createElement('span');
        dot.className = `sch-pip${i === cur ? ' is-active' : ''}`;
        pips.appendChild(dot);
      }
      wrap.appendChild(pips);
      return wrap;
    }
    case 'CalendarView': {
      wrap.appendChild(calendarPreview(p));
      return wrap;
    }
    case 'RatingControl': {
      const maxStars = Math.max(1, Math.min(10, Number(p.maxRating ?? 5)));
      let current = Math.round(Number(p.value ?? 4));
      const box = document.createElement('div');
      box.style.display = 'inline-flex';
      box.style.gap = '4px';
      const drawStars = () => {
        box.innerHTML = '';
        for (let i = 1; i <= maxStars; i++) {
          const b = document.createElement('button');
          b.type = 'button';
          b.className = 'mini-btn';
          b.textContent = i <= current ? '★' : '☆';
          b.addEventListener('click', () => {
            current = i;
            drawStars();
            fireSchematicEvent(node.id, 'valueChanged', String(current));
          });
          box.appendChild(b);
        }
      };
      drawStars();
      wrap.appendChild(box);
      return wrap;
    }
    case 'Label': {
      const lbl = document.createElement('div');
      const typo = p.typography || 'Body';
      const sizeMap = {
        Caption: '12px',
        Body: '14px',
        BodyStrong: '14px',
        BodyLarge: '16px',
        BodyLargeStrong: '16px',
        Subtitle: '18px',
        Title: '22px',
        TitleLarge: '28px',
        Display: '34px',
      };
      lbl.style.fontSize = sizeMap[typo] || '14px';
      lbl.style.lineHeight = '1.32';
      lbl.style.fontWeight = /Strong|Title|Subtitle|Display/.test(typo) ? '700' : '400';
      if (/Title|Display/.test(typo)) lbl.style.letterSpacing = '-0.02em';
      if (typo === 'TitleLarge') lbl.style.fontVariantNumeric = 'tabular-nums';
      if (p.colorRole === 'Secondary' || p.colorRole === 'Tertiary') lbl.style.color = 'var(--muted)';
      if (p.colorRole === 'Accent') lbl.style.color = 'var(--accent)';
      if (p.colorRole === 'Critical') lbl.style.color = 'var(--danger)';
      lbl.textContent = p.text || 'Text label';
      wrap.appendChild(lbl);
      return wrap;
    }
    case 'DatePicker':
    case 'TimePicker':
    case 'CalendarDatePicker': {
      const time = node.type === 'TimePicker';
      const input = temporalInput(time ? 'time' : 'date', time ? p.time : p.date,
        time ? 'timeChanged' : 'dateChanged', p.header || p.placeholderText || componentLabel(STUDIO_CATALOG[node.type]));
      if (p.header) wrap.appendChild(textElement('span', p.header));
      if (node.type === 'CalendarDatePicker' && p.placeholderText) {
        const hint = textElement('span', p.placeholderText);
        hint.className = 'sch-field-label';
        hint.hidden = Boolean(input.value);
        input.addEventListener('input', () => { hint.hidden = Boolean(input.value); });
        wrap.appendChild(hint);
      }
      wrap.appendChild(input);
      return wrap;
    }
    case 'DateTimeRangePicker': {
      const range = document.createElement('div');
      range.className = 'sch-date-range';
      const withTime = p.timeEnabled !== false;
      const inputType = withTime ? 'datetime-local' : 'date';
      const start = temporalInput(inputType, p.startDateTime, 'startDateTimeChanged',
        p.startLabelText || localizedText({en: 'Start', zh: '开始'}));
      const end = temporalInput(inputType, p.endDateTime, 'endDateTimeChanged',
        p.endLabelText || localizedText({en: 'End', zh: '结束'}));
      const limits = () => { start.max = end.value; end.min = start.value; };
      start.addEventListener('input', limits);
      end.addEventListener('input', limits);
      limits();
      range.append(start, textElement('span', '–'), end);
      wrap.appendChild(range);
      return wrap;
    }
    case 'LineEdit':
    case 'PasswordBox':
    case 'NumberBox':
    case 'AutoSuggestBox': {
      if (p.header) {
        const hd = document.createElement('div');
        hd.className = 'sch-field-label';
        hd.style.marginBottom = '4px';
        hd.textContent = p.header;
        wrap.appendChild(hd);
      }
      const input = document.createElement('input');
      input.className = 'studio-input';
      input.style.width = '100%';
      input.disabled = p.enabled === false;
      if (node.type === 'PasswordBox') input.type = 'password';
      else if (node.type === 'NumberBox') {
        input.type = 'number';
        input.min = String(p.minimum ?? 0);
        input.max = String(p.maximum ?? 100);
        input.value = String(p.value ?? 1);
      } else {
        input.type = 'text';
        input.value = p.text || '';
        input.placeholder = p.placeholderText || '';
      }
      input.addEventListener('change', () => {
        if (node.type === 'NumberBox') fireSchematicEvent(node.id, 'valueChanged', Number(input.value));
        else if (node.type === 'LineEdit' || node.type === 'AutoSuggestBox') fireSchematicEvent(node.id, 'textChanged', input.value);
        else fireSchematicEvent(node.id, 'changed', input.value);
      });
      wrap.appendChild(input);
      return wrap;
    }
    case 'TextEdit': {
      const area = document.createElement('textarea');
      area.className = 'studio-textarea sch-textedit';
      area.style.width = '100%';
      area.rows = Math.max(5, Number(p.minVisibleLines ?? 8));
      area.value = p.plainText || '';
      area.placeholder = p.placeholderText || '';
      area.disabled = p.enabled === false;
      area.addEventListener('change', () => fireSchematicEvent(node.id, 'textChanged', area.value));
      wrap.appendChild(area);
      return wrap;
    }
    case 'CodeBlock': {
      wrap.classList.add('sch-codeblock');
      const hd = document.createElement('div');
      hd.className = 'sch-codeblock-hd';
      const titleEl = textElement('span', p.headerText || 'Code');
      const langPill = textElement('span', String(p.language || 'cpp').toUpperCase());
      langPill.className = 'sch-lang-pill';
      hd.append(titleEl, langPill);
      const pre = document.createElement('pre');
      pre.textContent = p.code || '';
      pre.hidden = p.expanded === false;
      wrap.append(hd, pre);
      return wrap;
    }
    case 'InfoBar': {
      wrap.classList.add('sch-infobar', p.severity || 'Informational');
      const tt = document.createElement('strong');
      tt.textContent = p.title || 'Notice';
      const msg = document.createElement('span');
      msg.textContent = p.message || '';
      wrap.append(tt, msg);
      return wrap;
    }
    case 'InfoBadge': {
      const b = document.createElement('span');
      b.className = `sch-badge ${p.status || 'Informational'}${p.displayMode === 'Dot' ? ' is-dot' : ''}`;
      b.textContent = p.displayMode === 'Dot' ? '' : String(p.value ?? 5);
      wrap.appendChild(b);
      return wrap;
    }
    case 'Avatar': {
      const av = document.createElement('span');
      av.className = 'sch-avatar';
      const initials = String(p.name || 'FQ').split(/\s+/).map((s) => s[0]).join('').slice(0, 2).toUpperCase();
      av.textContent = initials || 'FQ';
      wrap.appendChild(av);
      return wrap;
    }
    case 'ProgressBar': {
      const bar = document.createElement('div');
      bar.className = 'sch-progress';
      const min = Number(p.minimum ?? 0);
      const max = Math.max(min + 1, Number(p.maximum ?? 100));
      const pct = Math.max(0, Math.min(100, ((Number(p.value ?? 64) - min) / (max - min)) * 100));
      const fill = document.createElement('span');
      fill.style.width = `${pct}%`;
      bar.appendChild(fill);
      wrap.appendChild(bar);
      return wrap;
    }
    case 'ProgressRing': {
      const ring = document.createElement('div');
      ring.className = 'sch-ring';
      ring.textContent = `${Math.round(Number(p.value ?? 42))}%`;
      wrap.appendChild(ring);
      return wrap;
    }
    case 'Breadcrumb': {
      wrap.classList.add('sch-crumb');
      (p.items || ['Home', 'Page']).forEach((item, i) => {
        if (i > 0) {
          const s = document.createElement('span');
          s.className = 'crumb-sep';
          s.textContent = '›';
          wrap.appendChild(s);
        }
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'crumb-btn';
        btn.textContent = item;
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'itemClicked', item));
        wrap.appendChild(btn);
      });
      return wrap;
    }
    case 'Stepper': {
      wrap.classList.add('sch-stepper');
      const activeIdx = Number(p.currentIndex ?? 0);
      (p.items || ['Step 1', 'Step 2']).forEach((item, i) => {
        const chip = document.createElement('button');
        chip.type = 'button';
        chip.className = `sch-step-chip${i === activeIdx ? ' is-active' : ''}`;
        chip.textContent = `${i + 1}. ${item}`;
        chip.addEventListener('click', () => fireSchematicEvent(node.id, 'stepRequested', i));
        wrap.appendChild(chip);
      });
      return wrap;
    }
    case 'ListView': {
      wrap.classList.add('sch-list');
      (p.items || ['Item 1', 'Item 2']).forEach((item, i) => {
        const row = document.createElement('div');
        row.className = `sch-list-item${i === 0 ? ' is-active' : ''}`;
        row.textContent = item;
        row.addEventListener('click', () => {
          wrap.querySelectorAll('.sch-list-item').forEach((el) => el.classList.remove('is-active'));
          row.classList.add('is-active');
          fireSchematicEvent(node.id, 'activated', String(i));
        });
        wrap.appendChild(row);
      });
      return wrap;
    }
    case 'TreeView': {
      wrap.classList.add('sch-tree');
      const items = p.items || ['src/', '  main.cpp', 'app/', 'README.md'];
      items.forEach((rawItem, i) => {
        const str = String(rawItem || '');
        const indentMatch = str.match(/^(\s+)/);
        const depth = indentMatch ? Math.min(3, Math.floor(indentMatch[1].length / 2)) : 0;
        const label = str.trim() || `Item ${i + 1}`;
        const isFolder = label.endsWith('/');
        const row = document.createElement('div');
        row.className = `sch-tree-row${i === 0 ? ' is-active' : ''}`;
        row.style.paddingLeft = `${10 + depth * 14}px`;
        const chev = textElement('span', isFolder ? '▾' : '');
        chev.className = 'sch-tree-chev';
        const icon = createGlyphSvg(isFolder ? 'Folder' : 'File', 13);
        const txt = textElement('span', label);
        txt.className = 'sch-tree-label';
        row.append(chev, icon, txt);
        row.addEventListener('click', () => {
          wrap.querySelectorAll('.sch-tree-row').forEach((el) => el.classList.remove('is-active'));
          row.classList.add('is-active');
          fireSchematicEvent(node.id, 'activated', String(i));
        });
        wrap.appendChild(row);
      });
      return wrap;
    }
    case 'FileListView': {
      wrap.classList.add('sch-filelist');
      (p.items || ['FluentQt.wasm', 'design-tokens.json']).forEach((item, i) => {
        const str = String(item || '');
        const dotIdx = str.lastIndexOf('.');
        const ext = dotIdx > 0 ? str.slice(dotIdx + 1).toUpperCase().slice(0, 6) : '';
        const row = document.createElement('div');
        row.className = `sch-list-item sch-file-row${i === 0 ? ' is-active' : ''}`;
        row.append(createGlyphSvg('Document', 13), textElement('span', str));
        if (ext) {
          const badge = textElement('span', ext);
          badge.className = 'sch-ext-pill';
          row.appendChild(badge);
        }
        row.addEventListener('click', () => {
          wrap.querySelectorAll('.sch-list-item').forEach((el) => el.classList.remove('is-active'));
          row.classList.add('is-active');
          fireSchematicEvent(node.id, 'activated', String(i));
        });
        wrap.appendChild(row);
      });
      return wrap;
    }
    case 'Timeline': {
      wrap.classList.add('sch-timeline');
      const items = p.items || ['Queued', 'Building UI', 'Ready'];
      items.forEach((item, i) => {
        const row = document.createElement('div');
        row.className = `sch-timeline-row${i === items.length - 1 ? ' is-current' : ''}`;
        const rail = document.createElement('div');
        rail.className = 'sch-timeline-rail';
        rail.appendChild(document.createElement('span'));
        row.append(rail, textElement('span', String(item)));
        wrap.appendChild(row);
      });
      return wrap;
    }
    case 'GridView':
    case 'FlowView': {
      wrap.classList.add('sch-tile-wrap');
      if (p.headerText) {
        const hd = textElement('div', p.headerText);
        hd.className = 'sch-chart-title';
        wrap.appendChild(hd);
      }
      const grid = document.createElement('div');
      const isGrid = node.type === 'GridView';
      grid.className = isGrid ? 'sch-tile-grid' : 'sch-flow-grid';
      const gradBg = MEDIA_THEME_GRADIENTS[p.mediaTheme] || MEDIA_THEME_GRADIENTS.Landscape;
      (p.items || ['Alpha', 'Beta', 'Gamma', 'Delta']).forEach((item, idx) => {
        const raw = String(item);
        if (!isGrid) {
          const tile = textElement('div', raw);
          tile.className = 'sch-tile-card';
          grid.appendChild(tile);
          return;
        }
        const sep = raw.indexOf('|');
        const titleStr = sep >= 0 ? raw.slice(0, sep).trim() : raw;
        const subStr = sep >= 0 ? raw.slice(sep + 1).trim() : '';
        const card = document.createElement('div');
        card.className = 'sch-tile-card is-media';
        const thumb = document.createElement('div');
        thumb.className = 'sch-tile-thumb';
        thumb.style.background = gradBg;
        thumb.style.filter = `hue-rotate(${(idx % 5) * 14 - 18}deg)`;
        const body = document.createElement('div');
        body.className = 'sch-tile-body';
        body.appendChild(textElement('strong', titleStr));
        if (subStr) body.appendChild(textElement('small', subStr));
        card.append(thumb, body);
        grid.appendChild(card);
      });
      wrap.appendChild(grid);
      return wrap;
    }
    case 'PropertyPanel': {
      wrap.classList.add('sch-prop-panel');
      const rows = [
        ['Layout Mode', p.layoutMode || 'Automatic'],
        ['Theme', 'Fluent Light / Dark'],
        ['Access', p.readOnly ? 'Read Only' : 'Editable'],
      ];
      rows.forEach(([k, v]) => {
        const r = document.createElement('div');
        r.className = 'sch-prop-row';
        r.append(textElement('span', k), textElement('strong', v));
        wrap.appendChild(r);
      });
      return wrap;
    }
    case 'LineChart':
    case 'AreaChart':
    case 'BarChart':
    case 'HorizontalBarChart':
    case 'PieChart':
    case 'DonutChart':
    case 'ScatterChart':
    case 'Sparkline':
    case 'ChartView': {
      wrap.classList.add('sch-chart');
      const head = document.createElement('div');
      head.className = 'sch-chart-head';
      const title = textElement('div', p.title || node.type);
      title.className = 'sch-chart-title';
      head.appendChild(title);
      if (p.seriesName) {
        const series = textElement('span', p.seriesName);
        series.className = 'sch-chart-series';
        head.appendChild(series);
      }
      const svg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
      svg.setAttribute('class', 'sch-chart-svg');
      svg.setAttribute('viewBox', '0 0 240 100');
      svg.setAttribute('preserveAspectRatio', 'none');
      const vals = Array.isArray(p.values) && p.values.length ? p.values : [20, 40, 35, 60];
      const maxV = Math.max(1, ...vals);
      if (node.type === 'PieChart' || node.type === 'DonutChart') {
        const track = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        track.setAttribute('cx', '120');
        track.setAttribute('cy', '50');
        track.setAttribute('r', '34');
        track.setAttribute('fill', 'none');
        track.setAttribute('stroke', 'var(--line)');
        track.setAttribute('stroke-width', '14');
        const ring = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
        ring.setAttribute('cx', '120');
        ring.setAttribute('cy', '50');
        ring.setAttribute('r', '34');
        ring.setAttribute('fill', node.type === 'PieChart' ? 'var(--active)' : 'none');
        ring.setAttribute('stroke', 'var(--accent)');
        ring.setAttribute('stroke-width', node.type === 'PieChart' ? '8' : '14');
        ring.setAttribute('stroke-dasharray', '155 220');
        ring.setAttribute('stroke-linecap', 'round');
        svg.append(track, ring);
      } else {
        for (const gy of [24, 52, 80]) {
          const gl = document.createElementNS('http://www.w3.org/2000/svg', 'line');
          gl.setAttribute('x1', '10');
          gl.setAttribute('x2', '230');
          gl.setAttribute('y1', String(gy));
          gl.setAttribute('y2', String(gy));
          gl.setAttribute('stroke', 'var(--line)');
          gl.setAttribute('stroke-width', '1');
          gl.setAttribute('stroke-dasharray', '3 3');
          svg.appendChild(gl);
        }
        if (node.type === 'LineChart' || node.type === 'AreaChart' || node.type === 'Sparkline' || node.type === 'ChartView' || node.type === 'ScatterChart') {
          const step = vals.length > 1 ? 220 / (vals.length - 1) : 220;
          const coords = vals.map((v, i) => [10 + i * step, 88 - (v / maxV) * 70]);
          const pointList = coords.map(([x, y]) => `${x},${y}`);
          if (node.type === 'AreaChart') {
            const area = document.createElementNS('http://www.w3.org/2000/svg', 'polygon');
            area.setAttribute('points', `10,92 ${pointList.join(' ')} ${coords[coords.length - 1][0]},92`);
            area.style.fill = 'var(--accent)';
            area.style.fillOpacity = '0.16';
            svg.appendChild(area);
          }
          const poly = document.createElementNS('http://www.w3.org/2000/svg', 'polyline');
          poly.setAttribute('fill', 'none');
          poly.setAttribute('stroke', 'currentColor');
          poly.setAttribute('stroke-width', '2.5');
          poly.setAttribute('stroke-linecap', 'round');
          poly.setAttribute('stroke-linejoin', 'round');
          poly.style.color = 'var(--accent)';
          poly.setAttribute('points', pointList.join(' '));
          svg.appendChild(poly);
          if (vals.length <= 14) {
            for (const [cx, cy] of coords) {
              const dot = document.createElementNS('http://www.w3.org/2000/svg', 'circle');
              dot.setAttribute('cx', String(cx));
              dot.setAttribute('cy', String(cy));
              dot.setAttribute('r', '2.6');
              dot.style.fill = 'var(--accent)';
              svg.appendChild(dot);
            }
          }
        } else {
          const barW = Math.max(10, 176 / vals.length);
          const gap = 220 / vals.length;
          vals.forEach((v, i) => {
            const rect = document.createElementNS('http://www.w3.org/2000/svg', 'rect');
            const h = Math.max(4, (v / maxV) * 76);
            rect.setAttribute('x', String(14 + i * gap));
            rect.setAttribute('y', String(92 - h));
            rect.setAttribute('width', String(barW));
            rect.setAttribute('height', String(h));
            rect.setAttribute('rx', '4');
            rect.style.fill = 'var(--accent)';
            svg.appendChild(rect);
          });
        }
      }
      wrap.append(head, svg);
      return wrap;
    }
    case 'SplitView': {
      wrap.classList.add('sch-split');
      const paneCount = (node.children || []).length;
      if (p.orientation === 'Vertical') wrap.classList.add('is-vertical');
      else if (paneCount >= 3) wrap.classList.add('is-three-pane-split');
      else if (paneCount === 2 && hasSidebarDescendant(node.children[0])) {
        wrap.classList.add('is-sidebar-split');
      }
      if (p.minHeight) wrap.style.minHeight = `${Number(p.minHeight)}px`;
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'ListDetailView': {
      wrap.classList.add('sch-listdetail');
      const paneW = Math.max(140, Math.min(420, Number(p.listPaneWidth ?? 216)));
      wrap.style.gridTemplateColumns = `${paneW}px minmax(0, 1fr)`;
      if (p.minHeight) wrap.style.minHeight = `${Number(p.minHeight)}px`;
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'ScrollView':
    case 'StackView': {
      wrap.classList.add('sch-vbox', node.type === 'ScrollView' ? 'sch-scrollview' : 'sch-stackview');
      wrap.style.gap = '10px';
      if (p.minHeight) wrap.style.minHeight = `${Number(p.minHeight)}px`;
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'Accordion':
    case 'FlipView': {
      wrap.classList.add('sch-card');
      wrap.style.gap = '10px';
      wrap.style.padding = '12px';
      const cap = document.createElement('div');
      cap.className = 'sch-chart-title';
      cap.textContent = node.type;
      wrap.appendChild(cap);
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'NavigationView': {
      wrap.classList.add('sch-navview');
      const navItems = p.items || ['Home', 'Analytics', 'Settings'];
      const pageNodes = node.children || [];
      let activeIdx = 0;
      const pane = document.createElement('div');
      pane.className = 'sch-navpane';
      const content = document.createElement('div');
      content.className = 'sch-vbox sch-navcontent';
      content.style.gap = '12px';

      const renderActivePage = () => {
        content.innerHTML = '';
        const activeNode = pageNodes[activeIdx];
        if (activeNode) {
          content.appendChild(renderSchematicNode(activeNode, activeIdx));
        } else {
          const fallback = document.createElement('div');
          fallback.className = 'sch-card';
          fallback.style.padding = '14px';
          const fallbackLabel = navItems[activeIdx] ? parseItemIconAndLabel(navItems[activeIdx]).label : `Page ${activeIdx + 1}`;
          fallback.appendChild(textElement('div', fallbackLabel, 'sch-title'));
          content.appendChild(fallback);
        }
      };

      const navButtons = [];
      navItems.forEach((item, i) => {
        const spec = parseItemIconAndLabel(item);
        const b = document.createElement('button');
        b.type = 'button';
        b.className = `sch-nav-item${i === activeIdx ? ' is-active' : ''}`;
        b.append(createGlyphSvg(spec.glyph, 14), textElement('span', spec.label));
        b.addEventListener('click', () => {
          activeIdx = i;
          navButtons.forEach((btn, idx) => btn.classList.toggle('is-active', idx === activeIdx));
          renderActivePage();
          fireSchematicEvent(node.id, 'navItemClicked', String(i));
        });
        navButtons.push(b);
        pane.appendChild(b);
      });
      renderActivePage();
      wrap.append(pane, content);
      return wrap;
    }
    case 'TabView': {
      wrap.classList.add('sch-tabbar');
      const activeIdx = Number(p.selectedIndex ?? 0);
      (p.items || ['Tab 1', 'Tab 2']).forEach((item, i) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `sch-tabbar-item${i === activeIdx ? ' is-active' : ''}`;
        btn.appendChild(textElement('span', item));
        if (p.tabsClosable !== false) {
          const close = textElement('span', '×');
          close.className = 'sch-tabbar-close';
          btn.appendChild(close);
        }
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'selectedIndexChanged', String(i)));
        wrap.appendChild(btn);
      });
      return wrap;
    }
    case 'Pivot':
    case 'SelectorBar': {
      wrap.classList.add('sch-pivot');
      const activeIdx = Number(p.selectedIndex ?? 0);
      (p.items || ['Tab 1', 'Tab 2']).forEach((item, i) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `sch-pivot-item${i === activeIdx ? ' is-active' : ''}`;
        btn.textContent = item;
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'selectedIndexChanged', String(i)));
        wrap.appendChild(btn);
      });
      return wrap;
    }
    case 'MenuBar': {
      wrap.classList.add('sch-menubar');
      (p.items || ['File', 'Edit', 'View', 'Help']).forEach((item) => {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'sch-menu-item';
        btn.textContent = item;
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'triggered', item));
        wrap.appendChild(btn);
      });
      return wrap;
    }
    case 'CommandBar':
    case 'DataGridToolBar': {
      wrap.classList.add('sch-commandbar');
      (p.items || ['New', 'Save', 'Share']).forEach((item) => {
        const spec = parseItemIconAndLabel(item);
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'sch-cmd-btn';
        btn.append(createGlyphSvg(spec.glyph, 13), textElement('span', spec.label));
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'triggered', spec.label));
        wrap.appendChild(btn);
      });
      return wrap;
    }
    case 'DataGrid': {
      const holder = document.createElement('div');
      holder.className = 'sch-table-wrap';
      const tbl = document.createElement('table');
      tbl.className = 'sch-table';
      const defaultRows = [
        ['BasicInput', 'Qt 6.9', 'Ready'],
        ['Charts', 'Qt 6.9', 'Verified'],
      ];
      const rows = Array.isArray(p.modelData) && p.modelData.length > 0 ? p.modelData : defaultRows;
      const customHeaders = Array.isArray(p.headers) && p.headers.length > 0 ? p.headers.map((h) => String(h ?? '')) : null;
      const colCount = Math.max(1, customHeaders ? customHeaders.length : 0, ...rows.map((r) => (Array.isArray(r) ? r.length : 1)));
      const defaultHeaders = state.localeBundle?.ui?.defaultGridHeaders
        || UI_STRINGS[state.lang]?.defaultGridHeaders
        || UI_STRINGS.en.defaultGridHeaders;
      const headerLabels = customHeaders || defaultHeaders;
      const thead = document.createElement('thead');
      const htr = document.createElement('tr');
      for (let c = 0; c < colCount; c++) {
        const th = document.createElement('th');
        th.textContent = headerLabels[c] || `Col ${c + 1}`;
        htr.appendChild(th);
      }
      thead.appendChild(htr);
      const tbody = document.createElement('tbody');
      for (const row of rows) {
        const tr = document.createElement('tr');
        const cells = Array.isArray(row) ? row : [row];
        for (let c = 0; c < colCount; c++) {
          const td = document.createElement('td');
          const cellVal = cells[c] === null || cells[c] === undefined ? '' : String(cells[c]);
          const tone = classifyStatusTone(cellVal);
          if (tone) {
            const pill = textElement('span', cellVal);
            pill.className = `sch-status-pill ${tone}`;
            td.appendChild(pill);
          } else {
            td.textContent = cellVal;
          }
          tr.appendChild(td);
        }
        tbody.appendChild(tr);
      }
      tbl.append(thead, tbody);
      holder.appendChild(tbl);
      wrap.appendChild(holder);
      return wrap;
    }
    case 'Shimmer': {
      wrap.classList.add('sch-shimmer');
      return wrap;
    }
    case 'ColorPicker': {
      wrap.classList.add('sch-color-row');
      const sw = document.createElement('span');
      sw.className = 'sch-color-swatch';
      sw.style.backgroundColor = p.color || '#0062B0';
      const txt = document.createElement('span');
      txt.textContent = `ColorPicker · ${p.color || '#0062B0'}`;
      wrap.append(sw, txt);
      return wrap;
    }
    case 'TitleBar': {
      wrap.classList.add('sch-titlebar');
      const left = document.createElement('div');
      left.className = 'sch-window-title';
      const dot = document.createElement('span');
      dot.className = 'sch-app-mark';
      left.append(dot, textElement('span', p.title || 'Fluent Window'));
      const ctrls = textElement('span', '─  ☐  ✕');
      ctrls.className = 'sch-window-controls';
      wrap.append(left, ctrls);
      return wrap;
    }
    case 'FontIcon': {
      const ic = document.createElement('span');
      ic.className = 'sch-font-icon';
      ic.appendChild(createGlyphSvg(p.glyph || 'Settings', 16));
      wrap.appendChild(ic);
      return wrap;
    }
    case 'AnchorBox': {
      wrap.classList.add('sch-anchor-box');
      wrap.style.minHeight = `${p.minHeight ?? 220}px`;
      const pad = p.padding ?? 12;
      if (!(node.children || []).length) {
        const hint = document.createElement('div');
        hint.className = 'sch-empty-slot';
        hint.textContent = t('emptyContainer');
        wrap.appendChild(hint);
      } else {
        (node.children || []).forEach((child, idx) => {
          const cp = child.props || {};
          const el = renderSchematicNode(child, { interactive });
          el.classList.add('sch-anchor-child');
          const m = cp.anchorMargin ?? pad;
          const ox = cp.anchorOffsetX ?? 0;
          const pos = cp.anchorPosition || ((idx === 0 && (child.type === 'ParticleBackdrop' || child.type === 'Card')) ? 'Fill' : 'TopLeft');
          const oy = cp.anchorOffsetY ?? ((pos === 'TopLeft' && !cp.anchorPosition) ? idx * 44 : 0);
          if (pos === 'Fill') Object.assign(el.style, { inset: `${m}px` });
          else if (pos === 'TopCenter') Object.assign(el.style, { top: `${m + oy}px`, left: '50%', transform: `translateX(calc(-50% + ${ox}px))` });
          else if (pos === 'TopRight') Object.assign(el.style, { top: `${m + oy}px`, right: `${m - ox}px` });
          else if (pos === 'Center') Object.assign(el.style, { top: '50%', left: '50%', transform: `translate(calc(-50% + ${ox}px), calc(-50% + ${oy}px))` });
          else if (pos === 'BottomLeft') Object.assign(el.style, { bottom: `${m - oy}px`, left: `${m + ox}px` });
          else if (pos === 'BottomCenter') Object.assign(el.style, { bottom: `${m - oy}px`, left: '50%', transform: `translateX(calc(-50% + ${ox}px))` });
          else if (pos === 'BottomRight') Object.assign(el.style, { bottom: `${m - oy}px`, right: `${m - ox}px` });
          else if (pos === 'TopBar') Object.assign(el.style, { top: `${m + oy}px`, left: `${m}px`, right: `${m}px` });
          else if (pos === 'BottomBar') Object.assign(el.style, { bottom: `${m - oy}px`, left: `${m}px`, right: `${m}px` });
          else if (pos === 'LeftDock') Object.assign(el.style, { left: `${m + ox}px`, top: `${m}px`, bottom: `${m}px` });
          else if (pos === 'RightDock') Object.assign(el.style, { right: `${m - ox}px`, top: `${m}px`, bottom: `${m}px` });
          else Object.assign(el.style, { top: `${m + oy}px`, left: `${m + ox}px` });
          wrap.appendChild(el);
        });
      }
      return wrap;
    }
    case 'Window': {
      wrap.classList.add('sch-window');
      const bar = document.createElement('div');
      bar.className = 'sch-window-bar';
      const left = document.createElement('div');
      left.className = 'sch-window-title';
      const dot = document.createElement('span');
      dot.className = 'sch-app-mark';
      left.append(dot, textElement('span', p.title || 'Fluent Window'));
      const ctrls = textElement('span', '─  ☐  ✕');
      ctrls.className = 'sch-window-controls';
      bar.append(left, ctrls);
      const body = document.createElement('div');
      body.className = 'sch-vbox';
      body.style.padding = `${p.padding ?? 16}px`;
      body.style.gap = `${p.spacing ?? 12}px`;
      renderChildrenInto(body);
      wrap.append(bar, body);
      return wrap;
    }
    case 'ParticleBackdrop': {
      wrap.classList.add('sch-particles');
      wrap.style.minHeight = `${p.minHeight ?? 200}px`;
      wrap.style.padding = `${p.padding ?? 16}px`;
      wrap.style.gap = `${p.spacing ?? 10}px`;
      const tag = document.createElement('div');
      tag.className = 'sch-field-help';
      tag.textContent = `ParticleBackdrop · ${p.effect || 'FlowingRibbons'}`;
      wrap.appendChild(tag);
      renderChildrenInto(wrap);
      return wrap;
    }
    case 'SpatialView':
    case 'SpatialItem': {
      wrap.classList.add('sch-spatial');
      wrap.style.minHeight = `${p.minHeight ?? 240}px`;
      const tag = document.createElement('div');
      tag.className = 'sch-field-help';
      tag.textContent = node.type === 'SpatialItem'
        ? `SpatialItem · rot(${p.rotX ?? 6}°, ${p.rotY ?? -16}°)`
        : `SpatialView · ${p.renderMode || 'Raster'}`;
      const stage = document.createElement('div');
      stage.className = 'sch-spatial-card';
      stage.style.transform = `perspective(900px) rotateX(${p.rotX ?? 6}deg) rotateY(${p.rotY ?? -16}deg)`;
      renderChildrenInto(stage);
      wrap.append(tag, stage);
      return wrap;
    }
    case 'ImageViewer': {
      wrap.classList.add('sch-image-viewer');
      wrap.style.minHeight = `${p.minHeight ?? 200}px`;
      const svgRaw = typeof p.svg === 'string' ? p.svg.trim() : '';
      if (isSafeStudioSvg(svgRaw)) {
        const img = document.createElement('img');
        img.className = 'sch-image-svg';
        img.alt = '';
        img.src = `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svgRaw)}`;
        wrap.appendChild(img);
        return wrap;
      }
      const photoUrl = typeof p.imageUrl === 'string' ? p.imageUrl.trim() : '';
      if (isStudioPhotoUrl(photoUrl)) {
        const img = document.createElement('img');
        img.className = 'sch-image-photo';
        img.alt = '';
        img.src = photoUrl;
        wrap.appendChild(img);
        return wrap;
      }
      const surface = document.createElement('div');
      surface.className = 'sch-image-placeholder';
      surface.style.background = MEDIA_THEME_GRADIENTS[p.mediaTheme] || MEDIA_THEME_GRADIENTS.Landscape;
      const badge = document.createElement('div');
      badge.className = 'sch-image-badge';
      badge.append(createGlyphSvg('Camera', 16), textElement('span', p.mediaTheme || 'ImageViewer'));
      surface.appendChild(badge);
      wrap.appendChild(surface);
      return wrap;
    }
    case 'FileDropZone': {
      const dz = document.createElement('div');
      dz.className = 'sch-dropzone';
      if (p.enabled === false) dz.classList.add('is-disabled');
      const iconWrap = document.createElement('div');
      iconWrap.className = 'sch-dropzone-icon';
      iconWrap.appendChild(createGlyphSvg('Upload', 18));
      const title = textElement('div', p.title || 'Drop files here');
      title.className = 'sch-dropzone-title';
      const desc = textElement('div', p.description || 'PNG, SVG, or .ui design files');
      desc.className = 'sch-dropzone-desc';
      dz.append(iconWrap, title, desc);
      if (p.browseButtonVisible !== false && p.browseText !== '') {
        const browse = textElement('button', p.browseText || 'Browse files');
        browse.type = 'button';
        browse.className = 'sch-btn Accent sch-dropzone-btn';
        browse.disabled = p.enabled === false;
        if (interactive) {
          browse.addEventListener('click', () => fireSchematicEvent(node.id, 'browseRequested', true));
        }
        dz.appendChild(browse);
      }
      if (p.hintText) {
        const hint = textElement('div', p.hintText);
        hint.className = 'sch-dropzone-hint';
        dz.appendChild(hint);
      }
      wrap.appendChild(dz);
      return wrap;
    }
    case 'SplashScreen': {
      const sp = document.createElement('div');
      sp.className = 'sch-splash';
      sp.style.minHeight = `${Math.max(140, Number(p.minHeight ?? 190))}px`;
      const title = textElement('div', p.title || 'Fluent-Qt');
      title.className = 'sch-splash-title';
      const sub = textElement('div', p.subtitle || 'Initializing UI workspace...');
      sub.className = 'sch-splash-sub';
      const barWrap = document.createElement('div');
      barWrap.className = 'sch-splash-progress';
      const pct = textElement('span', '68%');
      pct.className = 'sch-splash-pct';
      const bar = document.createElement('div');
      bar.className = 'sch-progress';
      const fill = document.createElement('span');
      fill.style.width = '68%';
      bar.appendChild(fill);
      barWrap.append(pct, bar);
      sp.append(title, sub, barWrap);
      wrap.appendChild(sp);
      return wrap;
    }
    case 'Transfer': {
      const tr = document.createElement('div');
      tr.className = 'sch-transfer';
      tr.style.minHeight = `${Math.max(150, Number(p.minHeight ?? 200))}px`;
      const allItems = Array.isArray(p.items) && p.items.length ? p.items : ['Alpha', 'Beta', 'Gamma', 'Delta'];
      const leftItems = allItems.slice(1);
      const rightItems = allItems.slice(0, 1);
      const makePane = (header, items) => {
        const pane = document.createElement('div');
        pane.className = 'sch-transfer-pane';
        const hd = textElement('div', `${header} (${items.length})`);
        hd.className = 'sch-transfer-hd';
        const list = document.createElement('div');
        list.className = 'sch-transfer-list';
        items.forEach((txt) => {
          const row = textElement('div', String(txt));
          row.className = 'sch-transfer-row';
          list.appendChild(row);
        });
        pane.append(hd, list);
        return pane;
      };
      const mid = document.createElement('div');
      mid.className = 'sch-transfer-actions';
      const toRight = textElement('span', '›');
      toRight.className = 'sch-transfer-btn';
      const toLeft = textElement('span', '‹');
      toLeft.className = 'sch-transfer-btn';
      mid.append(toRight, toLeft);
      tr.append(makePane(p.sourceHeader || 'Available', leftItems), mid, makePane(p.targetHeader || 'Selected', rightItems));
      wrap.appendChild(tr);
      return wrap;
    }
    case 'ScrollBar': {
      const vertical = p.orientation === 'Vertical';
      const sb = document.createElement('div');
      sb.className = `sch-scrollbar ${vertical ? 'is-vertical' : 'is-horizontal'}`;
      const thumb = document.createElement('span');
      thumb.className = 'sch-scrollbar-thumb';
      sb.appendChild(thumb);
      wrap.appendChild(sb);
      return wrap;
    }
    case 'AnnotatedScrollBar': {
      const box = document.createElement('div');
      box.className = 'sch-annotated-sb';
      const labels = document.createElement('div');
      labels.className = 'sch-annotated-labels';
      for (const stop of ['Top', 'Middle', 'End']) {
        labels.appendChild(textElement('span', stop));
      }
      const track = document.createElement('div');
      track.className = 'sch-scrollbar is-vertical';
      const thumb = document.createElement('span');
      thumb.className = 'sch-scrollbar-thumb';
      track.appendChild(thumb);
      box.append(labels, track);
      wrap.appendChild(box);
      return wrap;
    }
    case 'QmlView': {
      wrap.classList.add('sch-qml');
      wrap.style.minHeight = `${p.minHeight ?? 160}px`;
      wrap.append(textElement('strong', 'QmlView'), textElement('span', `${p.sample || 'Basic'}.qml · ${p.contextText || 'Shared Fluent-Qt title'}`));
      return wrap;
    }
    default: {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'sch-btn Standard';
      btn.textContent = p.text || p.title || `Open ${node.type}`;
      if (interactive) {
        btn.addEventListener('click', () => fireSchematicEvent(node.id, 'triggered', node.type));
      }
      wrap.appendChild(btn);
      return wrap;
    }
  }
}

/* Canvas Hitbox & Figma WYSIWYG Drag Overlay */
const HEAVY_WASM_TYPES = new Set(['QmlView', 'SpatialView', 'ParticleBackdrop']);

function nodeContainsHeavyControl(node) {
  if (!node) return false;
  if (HEAVY_WASM_TYPES.has(node.type)) return true;
  return (node.children || []).some(nodeContainsHeavyControl);
}

function measureSchematicRects() {
  const map = {};
  const shellRect = els.canvasShell.getBoundingClientRect();
  els.schematicStage.querySelectorAll('[data-sch-id]').forEach((el) => {
    const r = el.getBoundingClientRect();
    map[el.dataset.schId] = {
      x: Math.round(r.left - shellRect.left),
      y: Math.round(r.top - shellRect.top),
      w: Math.round(r.width),
      h: Math.round(r.height),
      visible: r.width > 0 && r.height > 0,
    };
  });
  return map;
}

function getLayoutRects() {
  if (state.wasmReady && state.wasmLayout && state.wasmLayout.nodes) {
    return state.wasmLayout.nodes;
  }
  return measureSchematicRects();
}

function syncSchematicShellHeight() {
  if (state.wasmReady || els.schematicStage.offsetHeight <= 0) return;
  const minH = Math.max(540, Math.ceil(els.schematicStage.scrollHeight + 16));
  if (Math.abs((parseInt(els.canvasShell.style.minHeight || '540', 10)) - minH) > 8) {
    els.canvasShell.style.minHeight = `${minH}px`;
  }
}

function syncOverlay() {
  if (state.mode !== 'edit') return;
  if (state.dragSession?.active) return;
  const rects = getLayoutRects();
  const doc = currentDoc();

  syncSchematicShellHeight();

  const existingBoxes = new Map();
  Array.from(els.hitboxLayer.children).forEach((el) => {
    if (el.dataset?.canvasNode) existingBoxes.set(el.dataset.canvasNode, el);
  });
  const orderedBoxes = [];
  const walk = (node, depth) => {
    const r = rects[node.id];
    if (r && r.visible) {
      let box = existingBoxes.get(node.id);
      if (box) {
        existingBoxes.delete(node.id);
      } else {
        box = document.createElement('div');
        box.className = 'canvas-hitbox';
        box.dataset.canvasNode = node.id;
        box.addEventListener('click', (e) => {
          e.stopPropagation();
          selectNode(node.id);
        });
        box.addEventListener('mouseenter', () => {
          if (state.dragSession?.active) return;
          state.hoveredNodeId = node.id;
          updateHoverBox(getLayoutRects()[node.id]);
        });
        if (node.id !== 'root') {
          attachPointerDrag(box, () => ({ kind: 'move', nodeId: node.id }));
        }
      }
      box.style.left = `${r.x}px`;
      box.style.top = `${r.y}px`;
      box.style.width = `${r.w}px`;
      box.style.height = `${r.h}px`;
      box.style.zIndex = String(10 + depth);
      box.dataset.draggable = node.id !== 'root' ? '1' : '0';
      orderedBoxes.push(box);
    }
    for (const child of node.children || []) walk(child, depth + 1);
  };
  walk(doc.root, 0);
  existingBoxes.forEach((obsolete) => obsolete.remove());
  for (let i = 0; i < orderedBoxes.length; i++) {
    const expected = orderedBoxes[i];
    const curAtIdx = els.hitboxLayer.children[i];
    if (curAtIdx !== expected) {
      els.hitboxLayer.insertBefore(expected, curAtIdx || null);
    }
  }

  const selRect = rects[state.selectedId];
  const isRoot = state.selectedId === 'root';
  const showSelectionBox = Boolean(selRect && selRect.visible && (!isRoot || state.hasUserSelection));
  if (showSelectionBox) {
    els.selectionBox.hidden = false;
    els.selectionBox.style.left = `${Math.max(1, selRect.x)}px`;
    els.selectionBox.style.top = `${Math.max(1, selRect.y)}px`;
    els.selectionBox.style.width = `${Math.max(24, selRect.w - 2)}px`;
    els.selectionBox.style.height = `${Math.max(20, selRect.h - 2)}px`;
    els.selectionBar.classList.toggle('is-inside', isRoot || selRect.y < 24);
    els.selectionOps.hidden = isRoot;
    const selNode = findNode(doc.root, state.selectedId);
    els.selectionTag.textContent = selNode ? nodeHumanLabel(selNode) : state.selectedId;
    els.selUp.disabled = isRoot;
    els.selDown.disabled = isRoot;
    els.selDup.disabled = isRoot;
    els.selDel.disabled = isRoot;
  } else {
    els.selectionBox.hidden = true;
  }
}

function updateHoverBox(rect) {
  if (!rect || !rect.visible || state.hoveredNodeId === 'root' || state.hoveredNodeId === state.selectedId) {
    els.hoverBox.hidden = true;
    return;
  }
  els.hoverBox.hidden = false;
  els.hoverBox.style.left = `${rect.x}px`;
  els.hoverBox.style.top = `${rect.y}px`;
  els.hoverBox.style.width = `${rect.w}px`;
  els.hoverBox.style.height = `${rect.h}px`;
}

function clearDropVisuals() {
  els.dropContainerBox.hidden = true;
  if (els.dropContainerTag) els.dropContainerTag.textContent = '';
  if (els.dropGuideX) els.dropGuideX.hidden = true;
  if (els.dropGuideY) els.dropGuideY.hidden = true;
  els.dropLine.hidden = true;
  if (els.dragPreviewBox) els.dragPreviewBox.hidden = true;
  document.querySelectorAll('.tree-row').forEach(clearTreeRowDropClasses);
}

function showTargetContainerOverlay(doc, rects, target) {
  const parentRect = rects?.[target.targetParentId] || target.parentRect;
  const parentNode = findNode(doc.root, target.targetParentId);
  if (parentRect && parentRect.visible) {
    els.dropContainerBox.hidden = false;
    els.dropContainerBox.style.left = `${parentRect.x}px`;
    els.dropContainerBox.style.top = `${parentRect.y}px`;
    els.dropContainerBox.style.width = `${parentRect.w}px`;
    els.dropContainerBox.style.height = `${parentRect.h}px`;
    if (els.dropContainerTag) {
      els.dropContainerTag.textContent = parentNode ? nodeHumanLabel(parentNode) : target.targetParentId;
    }
  } else {
    els.dropContainerBox.hidden = true;
  }

  const snapX = target.anchorPlacement?.snapX;
  const snapY = target.anchorPlacement?.snapY;
  if (els.dropGuideX) {
    els.dropGuideX.hidden = snapX === null || snapX === undefined;
    if (!els.dropGuideX.hidden) els.dropGuideX.style.left = `${snapX}px`;
  }
  if (els.dropGuideY) {
    els.dropGuideY.hidden = snapY === null || snapY === undefined;
    if (!els.dropGuideY.hidden) els.dropGuideY.style.top = `${snapY}px`;
  }
}

function showDropLineOverlay(target) {
  if (!target || target.targetParentType === 'AnchorBox' || target.isEmptyContainer || target.isNoop) {
    els.dropLine.hidden = true;
    return;
  }
  const ar = target.anchorChildRect;
  if (!ar || !ar.visible) {
    els.dropLine.hidden = true;
    return;
  }
  els.dropLine.hidden = false;
  if (target.horizontal) {
    els.dropLine.dataset.orientation = 'h';
    els.dropLine.style.left = `${target.before ? ar.x - 2 : ar.x + ar.w - 1}px`;
    els.dropLine.style.top = `${ar.y}px`;
    els.dropLine.style.width = '3px';
    els.dropLine.style.height = `${Math.max(20, ar.h)}px`;
  } else {
    els.dropLine.dataset.orientation = 'v';
    els.dropLine.style.left = `${ar.x}px`;
    els.dropLine.style.top = `${target.before ? ar.y - 2 : ar.y + ar.h - 1}px`;
    els.dropLine.style.width = `${Math.max(40, ar.w)}px`;
    els.dropLine.style.height = '3px';
  }
}

function showDragPreviewBox(rect, label) {
  if (!rect || !els.dragPreviewBox) return;
  els.dragPreviewBox.hidden = false;
  els.dragPreviewBox.style.left = `${Math.max(1, rect.x)}px`;
  els.dragPreviewBox.style.top = `${Math.max(1, rect.y)}px`;
  els.dragPreviewBox.style.width = `${Math.max(24, rect.w)}px`;
  els.dragPreviewBox.style.height = `${Math.max(20, rect.h)}px`;
  els.dragPreviewBox.classList.toggle('is-inside-tag', rect.y < 24);
  if (els.dragPreviewTag) {
    els.dragPreviewTag.textContent = `${label} · ${Math.round(rect.w)}×${Math.round(rect.h)}`;
  }
  els.dropLine.hidden = true;
  els.dragGhost?.classList.add('is-docked');
}

function applyLivePreviewRects(drag, rects, target) {
  if (!drag || !target) return;
  const previewDoc = drag.previewDoc || drag.baseDoc;
  showTargetContainerOverlay(previewDoc, rects, target);
  const pr = (drag.previewNodeId ? rects?.[drag.previewNodeId] : null)
    || (target.targetParentType === 'AnchorBox' ? target.anchorPlacement?.previewRect : null);
  const prVisible = Boolean(pr && (pr.visible === undefined || pr.visible));
  if (prVisible && els.dragPreviewBox) {
    showDragPreviewBox(pr, nodeHumanLabel(drag.previewNode));
  } else {
    if (els.dragPreviewBox) els.dragPreviewBox.hidden = true;
    els.dragGhost?.classList.remove('is-docked');
    showDropLineOverlay(target);
  }
}

function buildRestoreAnchorPatch(origNode, baseDoc) {
  const parentInfo = origNode ? findParent(baseDoc.root, origNode.id) : null;
  const np = origNode?.props || {};
  const patch = {};
  for (const key of ANCHOR_PROP_KEYS) {
    patch[key] = Object.prototype.hasOwnProperty.call(np, key) ? np[key] : null;
  }
  if (patch.anchorMargin === null && parentInfo?.parent?.props?.padding !== undefined) {
    patch.anchorMargin = parentInfo.parent.props.padding;
  }
  return patch;
}

function revertWasmDragPreview(drag) {
  if (!state.wasmReady || drag.lastRenderedWasmKey === null) return;
  if (drag.payload.kind === 'move' && drag.patchedInPlace && drag.wasmAnchorParentId
      && drag.wasmAnchorParentId === drag.baseAnchorParentId) {
    const origNode = findNode(drag.baseDoc.root, drag.previewNodeId);
    syncPatchToWasm(drag.previewNodeId, buildRestoreAnchorPatch(origNode, drag.baseDoc), drag.baseDoc, { preview: true });
  } else {
    syncToWasm(drag.baseDoc, { preview: true });
  }
  drag.patchedInPlace = false;
  drag.wasmAnchorParentId = drag.baseAnchorParentId || null;
}

function executeDrop(payload, targetParentId, targetIndex) {
  const doc = currentDoc();
  let res;
  if (payload.kind === 'palette') {
    const fresh = createNode(payload.type, doc);
    res = insertNode(doc, targetParentId, targetIndex, fresh);
  } else if (payload.kind === 'move') {
    res = moveNode(doc, payload.nodeId, targetParentId, targetIndex);
  } else {
    return;
  }
  if (!res.ok) {
    showToast(res.error);
    return;
  }
  state.collapsedIds.delete(targetParentId);
  commitDocument(res.document, res.nodeId, true);
}

function positionDragGhost(clientX, clientY) {
  if (!els.dragGhost) return;
  const docked = els.dragGhost.classList.contains('is-docked');
  const gx = Math.max(8, Math.min(clientX + (docked ? 0 : 14), window.innerWidth - (docked ? 200 : 292)));
  const gy = Math.max(8, Math.min(clientY + (docked ? 0 : 14), window.innerHeight - (docked ? 56 : 196)));
  els.dragGhost.style.transform = `translate3d(${Math.round(gx)}px, ${Math.round(gy)}px, 0)`;
}

function revertCanvasPreview(drag) {
  if (!drag) return;
  clearTimeout(drag.wasmPreviewTimerId);
  drag.wasmPreviewTimerId = null;
  if (drag.hasPreviewOnCanvas) {
    renderSchematic(drag.baseDoc);
    syncSchematicShellHeight();
    revertWasmDragPreview(drag);
    drag.hasPreviewOnCanvas = false;
    drag.lastRenderedWasmKey = null;
  }
  drag.activeTarget = null;
  drag.placementKey = null;
  drag.previewDoc = null;
}

function teardownDragDom(drag) {
  clearTimeout(drag?.wasmPreviewTimerId);
  if (drag?.autoScrollRafId) cancelAnimationFrame(drag.autoScrollRafId);
  document.body.classList.remove('is-studio-dragging');
  drag?.sourceEl?.classList.remove('is-dragging-source');
  if (els.dragGhost) {
    els.dragGhost.hidden = true;
    els.dragGhost.classList.remove('is-docked');
    if (els.dragGhostBody) els.dragGhostBody.innerHTML = '';
    if (els.dragGhostPill) els.dragGhostPill.textContent = '';
  }
  clearDropVisuals();
}

function startDragAutoScroll(drag) {
  const step = () => {
    if (!state.dragSession || state.dragSession !== drag || !drag.active) return;
    if (els.stageScroll) {
      const sr = els.stageScroll.getBoundingClientRect();
      if (drag.lastX >= sr.left && drag.lastX <= sr.right) {
        let delta = 0;
        const edge = 48;
        if (drag.lastY >= sr.top && drag.lastY < sr.top + edge) {
          delta = -Math.ceil(((sr.top + edge - drag.lastY) / edge) * 14);
        } else if (drag.lastY <= sr.bottom && drag.lastY > sr.bottom - edge) {
          delta = Math.ceil(((drag.lastY - (sr.bottom - edge)) / edge) * 14);
        }
        if (delta !== 0) {
          const prev = els.stageScroll.scrollTop;
          els.stageScroll.scrollTop += delta;
          if (els.stageScroll.scrollTop !== prev) {
            updateDragAt(drag, drag.lastX, drag.lastY);
          }
        }
      }
    }
    drag.autoScrollRafId = requestAnimationFrame(step);
  };
  drag.autoScrollRafId = requestAnimationFrame(step);
}

function activateDrag(drag, clientX, clientY) {
  if (state.introCardVisible) dismissIntroCard({ remember: true });
  if (state.tourOpen) closeIntroTour();
  drag.pending = false;
  drag.active = true;
  state.suppressNextClick = true;
  closeActiveComboMenu(false);
  flushLiveDraft({ syncWasm: true });

  drag.baseDoc = cloneDoc(currentDoc());
  drag.baseRects = getLayoutRects();
  drag.grabOffsetX = 0;
  drag.grabOffsetY = 0;
  drag.dragSize = { w: 120, h: 36 };
  drag.baseAnchorParentId = null;
  drag.wasmAnchorParentId = null;
  drag.patchedInPlace = false;

  if (drag.payload.kind === 'palette') {
    if (state.mode === 'play') {
      drag.wasPlayMode = true;
      els.editOverlay.classList.remove('is-play');
    }
    drag.previewNode = createNode(drag.payload.type, drag.baseDoc);
    drag.previewNodeId = drag.previewNode.id;
  } else {
    const parentInfo = findParent(drag.baseDoc.root, drag.payload.nodeId);
    const found = parentInfo?.node || null;
    if (!found) {
      teardownDragDom(drag);
      state.dragSession = null;
      return;
    }
    drag.previewNode = cloneDoc(found);
    drag.previewNodeId = found.id;
    drag.baseAnchorParentId = parentInfo?.parent?.type === 'AnchorBox' ? parentInfo.parent.id : null;
    drag.wasmAnchorParentId = drag.baseAnchorParentId;

    const srcRect = drag.baseRects?.[found.id];
    if (srcRect && srcRect.visible) {
      drag.dragSize = { w: srcRect.w, h: srcRect.h };
      const shellRect = els.canvasShell.getBoundingClientRect();
      const startedOnCanvas = drag.sourceEl?.classList?.contains('canvas-hitbox')
        && drag.startX >= shellRect.left && drag.startX <= shellRect.right
        && drag.startY >= shellRect.top && drag.startY <= shellRect.bottom;
      if (startedOnCanvas) {
        const px = drag.startX - shellRect.left;
        const py = drag.startY - shellRect.top;
        drag.grabOffsetX = Math.max(-srcRect.w / 2, Math.min(srcRect.w / 2, px - (srcRect.x + srcRect.w / 2)));
        drag.grabOffsetY = Math.max(-srcRect.h / 2, Math.min(srcRect.h / 2, py - (srcRect.y + srcRect.h / 2)));
      }
    }
  }

  if (els.dragGhostLabel) els.dragGhostLabel.textContent = nodeHumanLabel(drag.previewNode);
  if (els.dragGhostBody) {
    els.dragGhostBody.innerHTML = '';
    els.dragGhostBody.appendChild(renderSchematicNode(drag.previewNode, { interactive: false }));
  }
  if (els.dragGhostPill) els.dragGhostPill.textContent = '';
  if (els.dragGhost) {
    els.dragGhost.classList.remove('is-docked');
    els.dragGhost.hidden = false;
  }

  if (drag.payload.kind === 'palette' && els.dragGhostBody?.firstElementChild) {
    const ghostChildRect = els.dragGhostBody.firstElementChild.getBoundingClientRect();
    if (ghostChildRect.width >= 24 && ghostChildRect.height >= 20) {
      drag.dragSize = {
        w: Math.min(360, Math.round(ghostChildRect.width)),
        h: Math.min(240, Math.round(ghostChildRect.height)),
      };
    }
  }

  document.body.classList.add('is-studio-dragging');
  drag.sourceEl?.classList.add('is-dragging-source');
  els.selectionBox.hidden = true;
  els.hoverBox.hidden = true;

  updateDragAt(drag, clientX, clientY);
  startDragAutoScroll(drag);
}

function updateDragAt(drag, clientX, clientY) {
  if (!drag || !drag.active) return;
  positionDragGhost(clientX, clientY);

  const shellRect = els.canvasShell.getBoundingClientRect();
  const canDropCanvas = state.mode === 'edit' || drag.payload.kind === 'palette';
  const overCanvas = canDropCanvas
    && clientX >= shellRect.left - 12 && clientX <= shellRect.right + 12
    && clientY >= shellRect.top - 12 && clientY <= shellRect.bottom + 12;

  if (overCanvas) {
    document.querySelectorAll('.tree-row').forEach(clearTreeRowDropClasses);
    drag.outlineTarget = null;

    const x = Math.max(1, Math.min(shellRect.width - 1, clientX - shellRect.left));
    const y = Math.max(1, Math.min(shellRect.height - 1, clientY - shellRect.top));
    const tier = resolveViewportTier(els.canvasShell.clientWidth || state.viewportWidth);
    const target = resolveDropTarget(drag.baseDoc, drag.baseRects, x, y, drag.payload, tier, {
      anchorX: x - drag.grabOffsetX,
      anchorY: y - drag.grabOffsetY,
      dragSize: drag.dragSize,
    });
    if (!target) {
      revertCanvasPreview(drag);
      clearDropVisuals();
      els.dragGhost?.classList.remove('is-docked');
      positionDragGhost(clientX, clientY);
      return;
    }

    drag.activeTarget = target;
    const nodeName = nodeHumanLabel(drag.previewNode);
    const parentName = nodeHumanLabel(findNode(drag.baseDoc.root, target.targetParentId));
    if (els.dragGhostPill) {
      if (target.isNoop) {
        els.dragGhostPill.textContent = `${nodeName} · ${t('dragNoop')}`;
      } else if (target.targetParentType === 'AnchorBox') {
        const ap = target.anchorPlacement?.anchorProps;
        const posName = optionLabel(null, ap?.anchorPosition || 'TopLeft');
        const ox = ap?.anchorOffsetX ?? 0;
        const oy = ap?.anchorOffsetY ?? 0;
        els.dragGhostPill.textContent = (ox !== 0 || oy !== 0)
          ? `${nodeName} · ${posName} (${ox}, ${oy})`
          : `${nodeName} · ${posName}`;
      } else {
        els.dragGhostPill.textContent = `${nodeName} → ${parentName}`;
      }
    }

    if (target.placementKey !== drag.placementKey) {
      drag.placementKey = target.placementKey;
      if (target.isNoop) {
        drag.previewDoc = drag.baseDoc;
        if (drag.hasPreviewOnCanvas) {
          renderSchematic(drag.baseDoc);
          syncSchematicShellHeight();
          clearTimeout(drag.wasmPreviewTimerId);
          drag.wasmPreviewTimerId = null;
          revertWasmDragPreview(drag);
          drag.hasPreviewOnCanvas = false;
          drag.lastRenderedWasmKey = null;
        }
        applyLivePreviewRects(drag, drag.baseRects, target);
      } else {
        const previewRes = buildDragPreviewDocument(drag.baseDoc, drag.payload, target, drag.previewNode);
        if (previewRes.ok) {
          drag.previewDoc = previewRes.document;
          drag.previewNodeId = previewRes.nodeId;
          drag.hasPreviewOnCanvas = true;
          renderSchematic(drag.previewDoc);
          syncSchematicShellHeight();

          if (!state.wasmReady) {
            applyLivePreviewRects(drag, measureSchematicRects(), target);
          } else {
            const canPatchAnchor = Boolean(
              target.targetParentType === 'AnchorBox'
              && target.anchorPlacement?.anchorProps
              && drag.wasmAnchorParentId === target.targetParentId,
            );
            showTargetContainerOverlay(drag.previewDoc, drag.baseRects, target);
            if (target.targetParentType === 'AnchorBox' && target.anchorPlacement?.previewRect) {
              showDragPreviewBox(target.anchorPlacement.previewRect, nodeHumanLabel(drag.previewNode));
            } else {
              if (els.dragPreviewBox) els.dragPreviewBox.hidden = true;
              els.dragGhost?.classList.remove('is-docked');
              showDropLineOverlay(target);
            }

            clearTimeout(drag.wasmPreviewTimerId);
            const isFirst = drag.lastRenderedWasmKey === null;
            const delay = isFirst
              ? 0
              : (canPatchAnchor ? 16 : (nodeContainsHeavyControl(drag.previewNode) ? 120 : 45));
            const sendPreview = () => {
              if (!state.dragSession || state.dragSession !== drag || !drag.previewDoc) return;
              drag.lastRenderedWasmKey = target.placementKey;
              if (canPatchAnchor) {
                drag.patchedInPlace = true;
                syncPatchToWasm(drag.previewNodeId, target.anchorPlacement.anchorProps, drag.previewDoc, { preview: true });
              } else {
                drag.patchedInPlace = false;
                drag.wasmAnchorParentId = target.targetParentType === 'AnchorBox' ? target.targetParentId : null;
                syncToWasm(drag.previewDoc, { preview: true });
              }
            };
            if (delay === 0) sendPreview();
            else drag.wasmPreviewTimerId = setTimeout(sendPreview, delay);
          }
        }
      }
    } else if (!state.wasmReady) {
      applyLivePreviewRects(drag, measureSchematicRects(), target);
    } else if (state.wasmLayout?.nodes && drag.lastRenderedWasmKey === target.placementKey) {
      applyLivePreviewRects(drag, state.wasmLayout.nodes, target);
    }

    positionDragGhost(clientX, clientY);
    return;
  }

  revertCanvasPreview(drag);
  clearDropVisuals();
  els.dragGhost?.classList.remove('is-docked');

  const hitEl = document.elementFromPoint(clientX, clientY);
  const row = hitEl?.closest?.('#outline-tree .tree-row');
  document.querySelectorAll('.tree-row').forEach(clearTreeRowDropClasses);
  drag.outlineTarget = null;

  if (row) {
    const rowNode = findNode(drag.baseDoc.root, row.dataset.nodeId);
    if (rowNode) {
      const placement = computeTreeDropPlacement(clientY, rowNode, row);
      if (placement && canExecuteDrop(drag.payload, placement.targetParentId)) {
        row.classList.add(`is-drop-${placement.visual}`);
        drag.outlineTarget = placement;
        if (els.dragGhostPill) {
          const targetNode = findNode(drag.baseDoc.root, placement.targetParentId);
          els.dragGhostPill.textContent = `${nodeHumanLabel(drag.previewNode)} → ${nodeHumanLabel(targetNode)}`;
        }
      }
    }
  } else if (els.dragGhostPill) {
    els.dragGhostPill.textContent = t('dragCancelHint');
  }
  positionDragGhost(clientX, clientY);
}

function finishDrag(drag, clientX, clientY) {
  if (!drag) return;
  state.dragSession = null;
  if (!drag.active) {
    teardownDragDom(drag);
    return;
  }

  drag.lastX = clientX;
  drag.lastY = clientY;
  // Re-evaluate at the exact release coordinates for instantaneous jump-drops.
  state.dragSession = drag;
  updateDragAt(drag, clientX, clientY);
  state.dragSession = null;

  const target = drag.activeTarget;
  const previewDoc = drag.previewDoc;
  const previewNodeId = drag.previewNodeId;
  const outlineTarget = drag.outlineTarget;
  const wasRenderedInWasm = Boolean(state.wasmReady && target && drag.lastRenderedWasmKey === target.placementKey);
  const canPatchAnchor = Boolean(
    state.wasmReady
    && target?.targetParentType === 'AnchorBox'
    && target?.anchorPlacement?.anchorProps
    && drag.wasmAnchorParentId === target?.targetParentId,
  );

  teardownDragDom(drag);

  if (target && previewDoc) {
    if (target.isNoop) {
      revertCanvasPreview(drag);
      if (drag.wasPlayMode) els.editOverlay.classList.add('is-play');
      renderSchematic();
      requestAnimationFrame(syncOverlay);
    } else {
      state.liveDraftDoc = null;
      state.history.push(previewDoc);
      state.selectedId = previewNodeId;
      state.hasUserSelection = true;
      state.collapsedIds.delete(target.targetParentId);
      if (drag.wasPlayMode) setMode('edit');
      renderAll();
      if (!wasRenderedInWasm) {
        if (canPatchAnchor) {
          syncPatchToWasm(previewNodeId, target.anchorPlacement.anchorProps, previewDoc, { preview: false });
        } else {
          syncToWasm();
        }
      }
      syncSelectionToWasm();
    }
  } else if (outlineTarget) {
    revertCanvasPreview(drag);
    executeDrop(drag.payload, outlineTarget.targetParentId, outlineTarget.targetIndex);
  } else {
    revertCanvasPreview(drag);
    if (drag.wasPlayMode) els.editOverlay.classList.add('is-play');
    renderSchematic();
    requestAnimationFrame(syncOverlay);
  }

  setTimeout(() => { state.suppressNextClick = false; }, 0);
}

function cancelDrag() {
  const drag = state.dragSession;
  if (!drag) return;
  state.dragSession = null;
  const wasActive = drag.active;
  teardownDragDom(drag);
  if (wasActive) {
    revertCanvasPreview(drag);
    if (drag.wasPlayMode) els.editOverlay.classList.add('is-play');
    renderSchematic();
    requestAnimationFrame(syncOverlay);
  }
  setTimeout(() => { state.suppressNextClick = false; }, 0);
}

function attachPointerDrag(el, getPayload, { ignoreSelector = '' } = {}) {
  if (!el) return;
  el.addEventListener('pointerdown', (e) => {
    if (e.button !== 0 || e.metaKey || e.ctrlKey || e.altKey) return;
    if (ignoreSelector && e.target.closest(ignoreSelector)) return;
    if (state.dragSession) return;
    const payload = typeof getPayload === 'function' ? getPayload() : getPayload;
    if (!payload) return;
    state.dragSession = {
      pending: true,
      active: false,
      pointerId: e.pointerId,
      startX: e.clientX,
      startY: e.clientY,
      lastX: e.clientX,
      lastY: e.clientY,
      sourceEl: el,
      payload,
      grabOffsetX: 0,
      grabOffsetY: 0,
      dragSize: { w: 120, h: 36 },
      baseAnchorParentId: null,
      wasmAnchorParentId: null,
      patchedInPlace: false,
      baseDoc: null,
      baseRects: null,
      previewNode: null,
      previewNodeId: null,
      previewDoc: null,
      activeTarget: null,
      outlineTarget: null,
      placementKey: null,
      lastRenderedWasmKey: null,
      hasPreviewOnCanvas: false,
      wasmPreviewTimerId: null,
      autoScrollRafId: 0,
      wasPlayMode: false,
    };
  });
}

/* WASM Bridge */
function postToWasm(payload) {
  if (!els.wasmStage.contentWindow) return;
  els.wasmStage.contentWindow.postMessage(
    { source: 'fluent-qt-site', ...payload },
    window.location.origin,
  );
}

function syncToWasm(doc = currentDoc(), { preview = false } = {}) {
  if (!state.wasmReady) return;
  state.wasmRequestId++;
  postToWasm({
    type: 'studio-render',
    session: state.wasmSession,
    requestId: state.wasmRequestId,
    preview,
    document: doc,
  });
}

function syncPatchToWasm(nodeId, patch, doc = currentDoc(), { preview = false } = {}) {
  if (!state.wasmReady || !nodeId || !patch || !Object.keys(patch).length) return;
  state.wasmRequestId++;
  postToWasm({
    type: 'studio-patch',
    session: state.wasmSession,
    requestId: state.wasmRequestId,
    preview,
    nodeId,
    patch,
    document: doc,
  });
}

function updateSchematicBanner() {
  if (!els.schematicBanner) return;
  const show = Boolean(state.wasmFallback && !state.schematicBannerDismissed);
  els.schematicBanner.hidden = !show;
  if (!show) return;
  const prefix = t('schematicBanner.text');
  if (els.schematicBannerText) {
    els.schematicBannerText.textContent = state.wasmShowRetry
      ? `${prefix} ${t('schematicBanner.localHint')}`
      : prefix;
  }
  if (els.schematicBannerCmd) {
    els.schematicBannerCmd.hidden = !state.wasmShowRetry;
  }
  if (els.btnDismissSchematicBanner) {
    const dismissLabel = t('schematicBanner.dismiss');
    els.btnDismissSchematicBanner.title = dismissLabel;
    els.btnDismissSchematicBanner.setAttribute('aria-label', dismissLabel);
  }
}

function updateRuntimeBadge() {
  if (state.wasmReady && state.wasmPainted) {
    state.wasmShowRetry = false;
    els.runtimeBadge.dataset.state = 'wasm';
    els.runtimeBadge.textContent = t('wasmReady');
    els.btnRetryWasm.hidden = true;
  } else if (state.wasmFallback) {
    els.runtimeBadge.dataset.state = 'schematic';
    els.runtimeBadge.textContent = t('schematicMode');
    els.btnRetryWasm.hidden = !state.wasmShowRetry;
  } else {
    state.wasmShowRetry = false;
    els.runtimeBadge.dataset.state = 'loading';
    els.runtimeBadge.textContent = t('wasmLoading');
    if (els.wasmLoaderStage) {
      els.wasmLoaderStage.textContent = t(state.wasmReady ? 'loader.mounting' : 'loader.fetching');
    }
    els.btnRetryWasm.hidden = true;
  }
  updateSchematicBanner();
}

function revealWasmStage() {
  if (!state.wasmReady || state.wasmPainted || state.wasmFallback) return;
  clearTimeout(state.wasmRevealTimerId);
  state.wasmRevealTimerId = null;
  state.wasmPainted = true;
  els.canvasShell.classList.remove('is-wasm-loading', 'is-static-only');
  els.canvasShell.classList.add('is-wasm-ready');
  updateRuntimeBadge();
  requestAnimationFrame(syncOverlay);
  maybeShowFirstVisitIntro();
}

function activateFallback(showRetry = true) {
  clearTimeout(state.wasmTimeoutId);
  clearTimeout(state.wasmRevealTimerId);
  state.wasmRevealTimerId = null;
  state.wasmReady = false;
  state.wasmPainted = false;
  state.wasmFallback = true;
  state.wasmShowRetry = Boolean(showRetry);
  els.canvasShell.classList.remove('is-wasm-loading', 'is-wasm-ready');
  els.canvasShell.classList.add('is-static-only');
  updateRuntimeBadge();
  requestAnimationFrame(syncOverlay);
  maybeShowFirstVisitIntro();
}

async function probeWasm() {
  try {
    return (await fetch('../gallery/fluent_qt_gallery.wasm', { method: 'HEAD' })).ok;
  } catch {
    return false;
  }
}

async function initWasmStage(probe) {
  clearTimeout(state.wasmTimeoutId);
  clearTimeout(state.wasmRevealTimerId);
  state.wasmRevealTimerId = null;
  if (params.get('renderer') === 'schematic') {
    activateFallback(false);
    return;
  }

  state.wasmReady = false;
  state.wasmPainted = false;
  state.wasmFallback = false;
  state.wasmLayout = null;
  els.canvasShell.classList.remove('is-wasm-ready', 'is-static-only');
  els.canvasShell.classList.add('is-wasm-loading');
  updateRuntimeBadge();

  if (!await (probe ?? probeWasm())) {
    activateFallback(true);
    return;
  }

  state.wasmTimeoutId = setTimeout(() => {
    if (!state.wasmReady) activateFallback(true);
  }, 15000);

  const iframeUrl = new URL('../gallery/', window.location.href);
  iframeUrl.searchParams.set('embed', 'site');
  iframeUrl.searchParams.set('studio', '1');
  iframeUrl.searchParams.set('host-theme', state.canvasTheme);
  iframeUrl.searchParams.set('host-session', state.wasmSession);
  iframeUrl.searchParams.set('render-scale', 'native');
  els.wasmStage.src = iframeUrl.toString();
}

window.addEventListener('message', (event) => {
  if (event.origin !== window.location.origin || event.source !== els.wasmStage.contentWindow) return;
  const data = event.data;
  if (!data || data.source !== 'fluent-qt-gallery' || data.session !== state.wasmSession) return;

  if (data.state === 'studio-ready') {
    clearTimeout(state.wasmTimeoutId);
    state.wasmReady = true;
    state.wasmPainted = false;
    state.wasmFallback = false;
    updateRuntimeBadge();
    postToWasm({ type: 'theme', theme: state.canvasTheme });
    syncToWasm();
    postToWasm({ type: 'studio-select', session: state.wasmSession, nodeId: state.selectedId });
    clearTimeout(state.wasmRevealTimerId);
    state.wasmRevealTimerId = setTimeout(revealWasmStage, 500);
    return;
  }

  if (data.state === 'studio-applied') {
    if (data.detail && data.detail.ok === false && data.detail.error) {
      showToast(data.detail.error);
    }
    return;
  }

  if (data.state === 'studio-layout' && data.detail) {
    state.wasmLayout = data.detail;
    const pad = currentDoc().root?.type === 'AnchorBox' ? 0 : 20;
    const targetH = Math.max(540, Math.min(2400, Math.ceil((data.detail.contentHeight || 540) + pad)));
    if (Math.abs((parseInt(els.canvasShell.style.height || '540', 10)) - targetH) > 6) {
      els.canvasShell.style.height = `${targetH}px`;
      els.wasmStage.style.height = `${targetH}px`;
      postToWasm({ type: 'studio-refresh', session: state.wasmSession });
    }
    if (!state.wasmPainted) {
      requestAnimationFrame(revealWasmStage);
    }
    const drag = state.dragSession;
    if (drag?.active) {
      if (drag.activeTarget && drag.lastRenderedWasmKey === drag.activeTarget.placementKey) {
        applyLivePreviewRects(drag, data.detail.nodes, drag.activeTarget);
      }
      return;
    }
    syncOverlay();
    return;
  }

  if (data.state === 'studio-inspected' && data.detail) {
    if (state.dragSession?.active) return;
    const detail = data.detail;
    if (detail.supported && detail.schema?.properties) {
      state.schemaCache[detail.type] = detail.schema;
    }
    if (detail.nodeId === state.selectedId) {
      state.nodeInspection = detail;
      const doc = currentDoc();
      const node = findNode(doc.root, state.selectedId);
      const meta = node ? STUDIO_CATALOG[node.type] : null;
      if (node && meta) {
        const model = buildInspectorModel(node, doc, meta);
        if (state.inspectorController && state.inspectorController.nodeId === node.id
            && state.inspectorController.schemaKey === model.schemaKey) {
          state.inspectorController.values = { ...model.values };
          state.inspectorController.effective = { ...model.effective };
          state.inspectorController.sync(model.values, model.effective, model.inspected);
        } else {
          renderInspector();
        }
        renderCodeDrawer();
      }
    }
    return;
  }

  if (data.state === 'studio-event' && data.detail) {
    const { nodeId, event: evtName, value } = data.detail;
    const shown = evtName === 'textChanged' && String(value ?? '').length > 36
      ? `${String(value).length} chars`
      : value;
    setEventPill(`${nodeId} · ${evtName}${shown !== undefined && shown !== '' ? ` (${shown})` : ''}`);
    syncPlayInteractionToDoc(nodeId, evtName, value);
    return;
  }

  if (data.state === 'error' || data.state === 'exit') {
    activateFallback(true);
  }
});

/* Node Actions */
function moveSelectedByOffset(delta) {
  flushLiveDraft({ syncWasm: false });
  if (state.selectedId === 'root') {
    showToast(t('rootProtected'));
    return;
  }
  const doc = currentDoc();
  const info = findParent(doc.root, state.selectedId);
  if (!info || !info.parent) return;
  const maxIdx = info.parent.children.length;
  const targetIdx = delta < 0
    ? Math.max(0, info.index + delta)
    : Math.min(maxIdx, info.index + delta + 1);
  const res = moveNode(doc, state.selectedId, info.parent.id, targetIdx);
  if (res.ok) commitDocument(res.document, state.selectedId, true);
}

function duplicateSelected() {
  flushLiveDraft({ syncWasm: false });
  if (state.selectedId === 'root') {
    showToast(t('rootProtected'));
    return;
  }
  const res = duplicateNode(currentDoc(), state.selectedId);
  if (!res.ok) {
    showToast(res.error);
    return;
  }
  commitDocument(res.document, res.nodeId, true);
}

function deleteSelected() {
  flushLiveDraft({ syncWasm: false });
  if (state.selectedId === 'root') {
    showToast(t('rootProtected'));
    return;
  }
  const res = removeNode(currentDoc(), state.selectedId);
  if (!res.ok) {
    showToast(res.error);
    return;
  }
  commitDocument(res.document, res.nextSelectedId, true);
}

/* Wire Events */
els.modeEdit.addEventListener('click', () => setMode('edit'));
els.modePlay.addEventListener('click', () => setMode('play'));

els.tabProps?.addEventListener('click', () => setInspectorTab('props'));
els.tabLayers?.addEventListener('click', () => setInspectorTab('layers'));

window.addEventListener('storage', (e) => {
  if (e.key === 'fluent-qt-theme' || e.key === null) inheritTheme();
});
window.addEventListener('pageshow', inheritTheme);
systemThemeQuery.addEventListener('change', inheritTheme);
forcedColorsQuery.addEventListener('change', inheritTheme);

els.widthButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    const width = Number(btn.dataset.width) || 860;
    state.viewportWidth = width;
    els.widthButtons.forEach((b) => {
      const active = b === btn;
      b.classList.toggle('is-active', active);
      b.setAttribute('aria-pressed', String(active));
    });
    els.stageFrame.style.setProperty('--stage-width', `${width}px`);
    if (els.artboardTitle) els.artboardTitle.textContent = `Artboard · ${width}px`;
    syncViewportTier(width);
    clearTimeout(state.viewportTimerId);
    requestAnimationFrame(() => {
      postToWasm({ type: 'studio-refresh', session: state.wasmSession });
      syncOverlay();
    });
    state.viewportTimerId = setTimeout(() => {
      postToWasm({ type: 'studio-refresh', session: state.wasmSession });
      syncOverlay();
    }, 100);
  });
});

els.btnUndo.addEventListener('click', () => {
  flushLiveDraft({ syncWasm: false });
  if (!state.history.canUndo()) return;
  state.liveDraftDoc = null;
  state.history.undo();
  renderAll();
  syncToWasm();
  syncSelectionToWasm();
});

els.btnRedo.addEventListener('click', () => {
  flushLiveDraft({ syncWasm: false });
  if (!state.history.canRedo()) return;
  state.liveDraftDoc = null;
  state.history.redo();
  renderAll();
  syncToWasm();
  syncSelectionToWasm();
});

els.btnResetProps?.addEventListener('click', resetSelectedProps);
els.btnDuplicate?.addEventListener('click', duplicateSelected);
els.btnDelete?.addEventListener('click', deleteSelected);
els.selAiRefine?.addEventListener('click', (e) => {
  e.stopPropagation();
  openAiModal({ mode: 'refine' });
});
els.selUp.addEventListener('click', (e) => { e.stopPropagation(); moveSelectedByOffset(-1); });

els.artboardThemeButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    const nextTheme = btn.dataset.artboardTheme === 'dark' ? 'dark' : 'light';
    const nextDoc = cloneDoc(currentDoc());
    nextDoc.design = { ...(nextDoc.design || {}), theme: nextTheme };
    commitDocument(nextDoc, state.selectedId, true, false);
  });
});

els.artboardSwatches.forEach((btn) => {
  btn.addEventListener('click', () => {
    const hex = btn.dataset.artboardAccent;
    if (!hex) return;
    const nextDoc = cloneDoc(currentDoc());
    nextDoc.design = { ...(nextDoc.design || {}), accentColor: hex };
    commitDocument(nextDoc, state.selectedId, true, false);
  });
});

els.btnArtboardReset?.addEventListener('click', () => {
  const nextDoc = cloneDoc(currentDoc());
  if (!nextDoc.design) return;
  delete nextDoc.design;
  commitDocument(nextDoc, state.selectedId, true, false);
});
els.selDown.addEventListener('click', (e) => { e.stopPropagation(); moveSelectedByOffset(1); });
els.selDup.addEventListener('click', (e) => { e.stopPropagation(); duplicateSelected(); });
els.selDel.addEventListener('click', (e) => { e.stopPropagation(); deleteSelected(); });

els.paletteSearch.addEventListener('input', renderPalette);
els.btnClearSearch?.addEventListener('click', () => {
  els.paletteSearch.value = '';
  els.paletteSearch.focus();
  renderPalette();
});
els.btnRetryWasm.addEventListener('click', () => {
  state.schematicBannerDismissed = false;
  initWasmStage();
});
els.btnDismissSchematicBanner?.addEventListener('click', () => {
  state.schematicBannerDismissed = true;
  updateSchematicBanner();
});

els.btnCode.addEventListener('click', () => {
  if (state.introCardVisible) dismissIntroCard({ remember: true });
  state.drawerOpen = !state.drawerOpen;
  els.codeDrawer.hidden = !state.drawerOpen;
  els.btnCode.setAttribute('aria-expanded', String(state.drawerOpen));
  renderCodeDrawer();
});

els.btnCloseDrawer.addEventListener('click', () => {
  state.drawerOpen = false;
  els.codeDrawer.hidden = true;
  els.btnCode.setAttribute('aria-expanded', 'false');
});

els.drawerTabs.forEach((tab) => {
  tab.addEventListener('click', () => {
    state.codeFormat = tab.dataset.format;
    els.drawerTabs.forEach((b) => {
      const active = b === tab;
      b.classList.toggle('is-active', active);
      b.setAttribute('aria-selected', String(active));
    });
    renderCodeDrawer();
  });
});

els.btnCopyCode.addEventListener('click', async () => {
  const txt = els.codeOutput.textContent || '';
  try {
    await navigator.clipboard.writeText(txt);
    showToast(t('copiedCode'));
  } catch (_) {
    showToast(t('copiedCode'));
  }
});

els.btnDownloadCode.addEventListener('click', () => {
  const extMap = { cpp: 'hpp', python: 'py', ui: 'ui', json: 'json' };
  const ext = extMap[state.codeFormat] || 'txt';
  const blob = new Blob([els.codeOutput.textContent || ''], { type: 'text/plain;charset=utf-8' });
  const a = document.createElement('a');
  a.href = URL.createObjectURL(blob);
  a.download = `fluent_studio_page.${ext}`;
  a.click();
  URL.revokeObjectURL(a.href);
});

/* AI One-Sentence Page Composer */
const AI_STORAGE_KEY = 'fluent-studio-ai-config';

function defaultWorkerEndpoint() {
  const meta = document.querySelector('meta[name="fluent-studio-ai-endpoint"]');
  const fromMeta = meta?.getAttribute('content')?.trim() || '';
  const fromGlobal = typeof window.FLUENT_STUDIO_AI_ENDPOINT === 'string'
    ? window.FLUENT_STUDIO_AI_ENDPOINT.trim()
    : '';
  return fromGlobal || fromMeta;
}

function loadAiConfig() {
  const defaults = {
    provider: 'auto',
    workerUrl: defaultWorkerEndpoint(),
    baseUrl: AI_PROVIDER_PRESETS.deepseek.baseUrl,
    model: AI_PROVIDER_PRESETS.deepseek.model,
    apiKey: '',
    keyOrigin: '',
  };
  try {
    const raw = localStorage.getItem(AI_STORAGE_KEY);
    if (!raw) return defaults;
    const saved = JSON.parse(raw);
    if (!saved || typeof saved !== 'object') return defaults;
    const keyOrigin = providerOrigin(saved.baseUrl);
    return {
      keyOrigin,
      provider: ['auto', 'custom'].includes(saved.provider) ? saved.provider : 'auto',
      workerUrl: typeof saved.workerUrl === 'string' && saved.workerUrl.trim() ? saved.workerUrl.trim() : defaults.workerUrl,
      baseUrl: typeof saved.baseUrl === 'string' && saved.baseUrl.trim() ? saved.baseUrl.trim() : defaults.baseUrl,
      model: typeof saved.model === 'string' && saved.model.trim() ? saved.model.trim() : defaults.model,
      apiKey: keyOrigin && saved.keyOrigin === keyOrigin && typeof saved.apiKey === 'string' ? saved.apiKey : '',
    };
  } catch (_) {
    return defaults;
  }
}

const aiConfig = loadAiConfig();

function saveAiConfig() {
  try {
    localStorage.setItem(AI_STORAGE_KEY, JSON.stringify({
      provider: aiConfig.provider,
      workerUrl: aiConfig.workerUrl === defaultWorkerEndpoint() ? '' : aiConfig.workerUrl,
      baseUrl: aiConfig.baseUrl,
      model: aiConfig.model,
      apiKey: aiConfig.apiKey,
      keyOrigin: aiConfig.keyOrigin,
    }));
  } catch (_) {
    // Ignore storage errors in restricted contexts.
  }
}

function setAiStatus(text, tone = '') {
  if (!els.aiStatus) return;
  els.aiStatus.textContent = text || '';
  if (tone) els.aiStatus.dataset.tone = tone;
  else delete els.aiStatus.dataset.tone;
}

function setAiBusy(busy) {
  state.aiBusy = Boolean(busy);
  if (els.btnAiRun) {
    els.btnAiRun.disabled = state.aiBusy;
    els.btnAiRun.classList.toggle('is-busy', state.aiBusy);
    els.btnAiRun.textContent = t(state.aiBusy ? 'ai.running' : 'ai.run');
  }
}

function syncAiSettingsUi() {
  els.aiProviderButtons.forEach((btn) => {
    const active = btn.dataset.aiProvider === aiConfig.provider;
    btn.classList.toggle('is-active', active);
    btn.setAttribute('aria-pressed', String(active));
  });
  if (els.aiWorkerFields) els.aiWorkerFields.hidden = aiConfig.provider !== 'auto';
  if (els.aiCustomFields) els.aiCustomFields.hidden = aiConfig.provider !== 'custom';
  if (els.aiWorkerUrl && els.aiWorkerUrl.value !== aiConfig.workerUrl) {
    els.aiWorkerUrl.value = aiConfig.workerUrl;
  }
  if (els.aiBaseUrl && els.aiBaseUrl.value !== aiConfig.baseUrl) {
    els.aiBaseUrl.value = aiConfig.baseUrl;
  }
  if (els.aiModel && els.aiModel.value !== aiConfig.model) {
    els.aiModel.value = aiConfig.model;
  }
  if (els.aiApiKey && els.aiApiKey.value !== aiConfig.apiKey) {
    els.aiApiKey.value = aiConfig.apiKey;
  }
}

function canRefineSelectedNode() {
  return Boolean(state.selectedId && state.selectedId !== 'root' && findNode(currentDoc().root, state.selectedId));
}

function updateAiModalModeUi() {
  const refineOk = canRefineSelectedNode();
  if (state.aiMode === 'refine' && !refineOk) state.aiMode = 'create';

  els.aiModeButtons.forEach((btn) => {
    const m = btn.dataset.aiMode;
    if (m === 'refine') btn.disabled = !refineOk;
    const active = m === state.aiMode;
    btn.classList.toggle('is-active', active);
    btn.setAttribute('aria-pressed', String(active));
  });

  if (els.aiTargetPill) {
    if (state.aiMode === 'refine' && refineOk) {
      const targetNode = findNode(currentDoc().root, state.selectedId);
      const label = targetNode ? `${t('ai.targetLabel')}${targetNode.type} #${targetNode.id}` : '';
      els.aiTargetPill.textContent = label;
      els.aiTargetPill.hidden = !label;
    } else {
      els.aiTargetPill.hidden = true;
      els.aiTargetPill.textContent = '';
    }
  }

  els.aiStyleButtons.forEach((btn) => {
    const active = btn.dataset.aiStyle === state.aiStylePreset;
    btn.classList.toggle('is-active', active);
    btn.setAttribute('aria-pressed', String(active));
  });

  if (els.aiPromptInput) {
    const key = state.aiMode === 'refine'
      ? 'ai.promptRefinePlaceholder'
      : (state.aiMode === 'iterate' ? 'ai.promptIteratePlaceholder' : 'ai.promptPlaceholder');
    els.aiPromptInput.placeholder = t(key);
  }
}

function openAiModal({ mode } = {}) {
  if (!els.aiModal) return;
  if (state.introCardVisible) dismissIntroCard({ remember: true });
  if (state.tourOpen) closeIntroTour();
  if (els.templatesModal?.open) closeTemplatesModal();
  closeActiveComboMenu(false);
  const refineOk = canRefineSelectedNode();
  if (mode === 'refine' && refineOk) {
    state.aiMode = 'refine';
  } else if (mode === 'iterate' || mode === 'create') {
    state.aiMode = mode;
  } else if (state.hasUserSelection && refineOk) {
    state.aiMode = 'refine';
  } else {
    state.aiMode = 'create';
  }
  state.aiOpen = true;
  els.aiModal.hidden = false;
  els.btnAi?.setAttribute('aria-expanded', 'true');
  syncAiSettingsUi();
  updateAiModalModeUi();
  if (!state.aiBusy) setAiStatus('');
  requestAnimationFrame(() => els.aiPromptInput?.focus());
}

function closeAiModal() {
  if (!els.aiModal) return;
  if (state.aiAbortController) {
    state.aiAbortController.abort();
    state.aiAbortController = null;
  }
  setAiBusy(false);
  state.aiOpen = false;
  els.aiModal.hidden = true;
  els.btnAi?.setAttribute('aria-expanded', 'false');
}

function toggleAiModal() {
  if (state.aiOpen) closeAiModal();
  else openAiModal();
}

function loadTemplate(template) {
  commitDocument(template.document, 'root', true, false);
  presetForSnapshot.set(state.history.current(), template.id);
}

function renderTemplatesGrid() {
  if (!els.templatesGrid) return;
  els.templatesGrid.innerHTML = '';
  for (const tpl of Object.values(STUDIO_TEMPLATES)) {
    const card = document.createElement('button');
    card.type = 'button';
    card.className = 'template-card';
    card.dataset.templateId = tpl.id;
    if (presetForSnapshot.get(state.history.current()) === tpl.id) card.setAttribute('aria-current', 'true');
    const head = document.createElement('div');
    head.className = 'template-card-head';
    const title = document.createElement('strong');
    title.className = 'template-card-title';
    title.textContent = templateLabel(tpl);
    const badge = document.createElement('span');
    badge.className = 'template-card-swatch';
    const docTheme = tpl.document.design?.theme;
    const accent = tpl.document?.design?.accentColor || '#0067C0';
    if (docTheme) badge.dataset.themeMode = docTheme;
    const dot = document.createElement('span');
    dot.className = 'template-card-dot';
    dot.style.background = accent;
    const themeLabel = document.createElement('span');
    themeLabel.textContent = docTheme ? t(`artboard.${docTheme}`) : t('followTheme');
    badge.append(dot, themeLabel);
    head.append(title, badge);

    const desc = document.createElement('span');
    desc.className = 'template-card-desc';
    desc.textContent = templateDescription(tpl);

    card.append(head, desc);
    card.addEventListener('click', () => {
      if (state.mode === 'play') setMode('edit');
      loadTemplate(tpl);
      closeTemplatesModal();
      const msg = t('templates.loadedToast').replace('{name}', templateLabel(tpl));
      showToast(msg);
      setEventPill(msg);
    });
    els.templatesGrid.appendChild(card);
  }
}

function openTemplatesModal() {
  if (!els.templatesModal) return;
  if (state.tourOpen) closeIntroTour();
  if (state.aiOpen) closeAiModal();
  closeActiveComboMenu(false);
  renderTemplatesGrid();
  els.templatesModal.showModal();
  els.btnTemplates?.setAttribute('aria-expanded', 'true');
  els.templatesGrid.querySelector('button')?.focus();
}

function closeTemplatesModal() {
  if (!els.templatesModal?.open) return;
  els.templatesModal.close();
  els.btnTemplates.setAttribute('aria-expanded', 'false');
  els.btnTemplates.focus();
}

function formatAiSourceLabel(result) {
  const base = result.model || 'AI';
  const u = result.usage;
  if (!u || !(u.inputTokens > 0)) return base;
  const fmtCount = (n) => (n >= 1000 ? `${(n / 1000).toFixed(1)}k` : String(n));
  const tok = t('aiTokens')
    .replace('{in}', fmtCount(u.inputTokens))
    .replace('{out}', fmtCount(u.outputTokens));
  if (u.cachedTokens > 0) {
    return t('aiCached')
      .replace('{base}', base)
      .replace('{rate}', String(u.cacheHitRate))
      .replace('{tok}', tok);
  }
  return `${base} · ${tok}`;
}

function applyAiGeneratedDocument(result) {
  if (state.mode === 'play') setMode('edit');
  const nextFocusId = result.focusNodeId || result.document.root.id;
  commitDocument(result.document, nextFocusId, true, nextFocusId !== result.document.root.id);
  const sourceLabel = formatAiSourceLabel(result);
  const msg = t('ai.generatedToast')
    .replace('{count}', String(result.count))
    .replace('{source}', sourceLabel);
  closeAiModal();
  showToast(msg);
  setEventPill(msg);
}

async function runAiGenerate(promptOverride = null) {
  if (state.aiBusy) return;
  const prompt = String(promptOverride ?? els.aiPromptInput?.value ?? '').trim();
  if (!prompt) {
    setAiStatus(t('ai.statusEmptyPrompt'), 'error');
    els.aiPromptInput?.focus();
    return;
  }
  if (state.aiAbortController) state.aiAbortController.abort();
  const controller = new AbortController();
  state.aiAbortController = controller;
  setAiBusy(true);
  setAiStatus(t('ai.statusGenerating'));

  let lastProgressKey = '';
  const handleProgress = (evt) => {
    if (!evt || state.aiAbortController !== controller) return;
    if (evt.phase === 'thinking') {
      if (lastProgressKey === 'thinking') return;
      lastProgressKey = 'thinking';
      setAiStatus(t('ai.statusThinking'), 'info');
    } else if (evt.phase === 'generating') {
      const count = Math.max(1, Number(evt.widgets) || 0);
      const key = `gen:${count}`;
      if (lastProgressKey === key) return;
      lastProgressKey = key;
      setAiStatus(t('ai.statusStreaming').replace('{widgets}', String(count)), 'info');
    }
  };

  try {
    const activeDoc = currentDoc();
    const targetNodeId = state.aiMode === 'refine' && canRefineSelectedNode() ? state.selectedId : null;
    const result = await generateStudioPageWithAi({
      prompt,
      lang: state.lang,
      mode: state.aiMode,
      currentDoc: state.aiMode === 'create' ? null : activeDoc,
      targetNodeId,
      stylePreset: state.aiStylePreset,
      config: { ...aiConfig },
      signal: controller.signal,
      onProgress: handleProgress,
    });
    if (controller.signal.aborted || state.aiAbortController !== controller) return;
    if (!result.ok) {
      const errMsg = (result.errorCode && state.localeBundle?.aiErrors?.[result.errorCode])
        || result.error
        || 'Failed to generate page.';
      setAiStatus(errMsg, result.needsConfig ? 'info' : 'error');
      if (result.needsConfig && els.aiSettingsDetails) {
        els.aiSettingsDetails.open = true;
      }
      return;
    }
    applyAiGeneratedDocument(result);
  } finally {
    if (state.aiAbortController === controller) {
      state.aiAbortController = null;
      setAiBusy(false);
    }
  }
}

els.btnAi?.addEventListener('click', () => openAiModal());
els.btnCloseAi?.addEventListener('click', closeAiModal);
els.aiModal?.addEventListener('click', (e) => {
  if (e.target === els.aiModal) closeAiModal();
});
els.btnTemplates?.addEventListener('click', () => {
  if (els.templatesModal?.open) closeTemplatesModal();
  else openTemplatesModal();
});
els.btnCloseTemplates?.addEventListener('click', closeTemplatesModal);
els.templatesModal?.addEventListener('cancel', (event) => {
  event.preventDefault();
  closeTemplatesModal();
});
els.templatesModal?.addEventListener('keydown', (event) => {
  if (event.key !== 'Tab') return;
  const first = els.btnCloseTemplates;
  const last = els.templatesGrid.lastElementChild;
  if (event.shiftKey && document.activeElement === first) {
    event.preventDefault();
    last?.focus();
  } else if (!event.shiftKey && document.activeElement === last) {
    event.preventDefault();
    first.focus();
  }
});
els.templatesModal?.addEventListener('click', (e) => {
  if (e.target === els.templatesModal) closeTemplatesModal();
});
els.aiModeButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    if (btn.disabled) return;
    const nextMode = btn.dataset.aiMode;
    if (!['create', 'iterate', 'refine'].includes(nextMode)) return;
    state.aiMode = nextMode;
    updateAiModalModeUi();
    els.aiPromptInput?.focus();
  });
});
els.aiStyleButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    state.aiStylePreset = btn.dataset.aiStyle || 'auto';
    updateAiModalModeUi();
  });
});
els.aiProviderButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    aiConfig.provider = btn.dataset.aiProvider === 'custom' ? 'custom' : 'auto';
    saveAiConfig();
    syncAiSettingsUi();
  });
});
els.aiPresetButtons.forEach((btn) => {
  btn.addEventListener('click', () => {
    const preset = AI_PROVIDER_PRESETS[btn.dataset.aiPreset];
    if (!preset) return;
    aiConfig.provider = 'custom';
    changeAiEndpoint(aiConfig, preset.baseUrl);
    aiConfig.model = preset.model;
    saveAiConfig();
    syncAiSettingsUi();
  });
});
els.aiWorkerUrl?.addEventListener('input', () => {
  aiConfig.workerUrl = els.aiWorkerUrl.value.trim();
  saveAiConfig();
});
els.aiBaseUrl?.addEventListener('input', () => {
  changeAiEndpoint(aiConfig, els.aiBaseUrl.value.trim());
  els.aiApiKey.value = aiConfig.apiKey;
  saveAiConfig();
});
els.aiModel?.addEventListener('input', () => {
  aiConfig.model = els.aiModel.value.trim();
  saveAiConfig();
});
els.aiApiKey?.addEventListener('input', () => {
  aiConfig.keyOrigin = providerOrigin(aiConfig.baseUrl);
  aiConfig.apiKey = els.aiApiKey.value;
  saveAiConfig();
});
els.btnAiRun?.addEventListener('click', () => runAiGenerate());
els.aiPromptInput?.addEventListener('keydown', (e) => {
  if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) {
    e.preventDefault();
    runAiGenerate();
  }
});

els.editOverlay.addEventListener('mouseleave', () => {
  if (state.dragSession?.active) return;
  state.hoveredNodeId = null;
  els.hoverBox.hidden = true;
});

document.addEventListener('click', (e) => {
  if (!state.suppressNextClick) return;
  state.suppressNextClick = false;
  e.preventDefault();
  e.stopPropagation();
}, true);

window.addEventListener('pointermove', (e) => {
  const drag = state.dragSession;
  if (!drag || e.pointerId !== drag.pointerId) return;
  drag.lastX = e.clientX;
  drag.lastY = e.clientY;
  if (!drag.active) {
    if (Math.hypot(e.clientX - drag.startX, e.clientY - drag.startY) < 4) return;
    activateDrag(drag, e.clientX, e.clientY);
    return;
  }
  updateDragAt(drag, e.clientX, e.clientY);
});

window.addEventListener('pointerup', (e) => {
  const drag = state.dragSession;
  if (!drag || e.pointerId !== drag.pointerId) return;
  finishDrag(drag, e.clientX, e.clientY);
});

window.addEventListener('pointercancel', (e) => {
  const drag = state.dragSession;
  if (!drag || e.pointerId !== drag.pointerId) return;
  cancelDrag();
});

window.addEventListener('blur', () => {
  if (state.dragSession) cancelDrag();
});

const INTRO_STORAGE_KEY = 'fluent-qt-studio-intro-v1';
const TOUR_STEPS = [
  { selector: '#palette-panel', placement: 'right', titleKey: 'tour.step1Title', descKey: 'tour.step1Desc' },
  { selector: '#btn-ai', placement: 'bottom', titleKey: 'tour.step2Title', descKey: 'tour.step2Desc' },
  { selector: '#artboard-bar', placement: 'bottom', titleKey: 'tour.step3Title', descKey: 'tour.step3Desc' },
  { selector: '#btn-code', placement: 'bottom-end', titleKey: 'tour.step4Title', descKey: 'tour.step4Desc' },
];

function hasSeenIntro() {
  try {
    return localStorage.getItem(INTRO_STORAGE_KEY) === 'seen';
  } catch (_) {
    return false;
  }
}

function markIntroSeen() {
  try {
    localStorage.setItem(INTRO_STORAGE_KEY, 'seen');
  } catch (_) {}
}

function shouldAutoShowIntro() {
  if (params.get('intro') === '1') {
    try {
      localStorage.removeItem(INTRO_STORAGE_KEY);
    } catch (_) {}
    return true;
  }
  if (params.get('intro') === '0' || decodedHashDoc || params.get('template')) return false;
  return !hasSeenIntro();
}

function maybeShowFirstVisitIntro() {
  if (state.introEvaluated) return;
  state.introEvaluated = true;
  if (!shouldAutoShowIntro()) return;
  startIntroTour(0);
}

function dismissIntroCard() {}

function positionTourStep() {
  if (!state.tourOpen || !els.tourSpotlight || !els.tourPopover) return;
  const step = TOUR_STEPS[state.tourStep] || TOUR_STEPS[0];
  const target = qs(step.selector);
  if (!target) return;
  const rect = target.getBoundingClientRect();
  const pad = 6;
  const vw = window.innerWidth;
  const vh = window.innerHeight;
  const sx = Math.max(4, Math.round(rect.left - pad));
  const sy = Math.max(4, Math.round(rect.top - pad));
  const sw = Math.min(vw - sx - 4, Math.round(rect.width + pad * 2));
  const sh = Math.min(vh - sy - 4, Math.round(rect.height + pad * 2));
  els.tourSpotlight.style.left = `${sx}px`;
  els.tourSpotlight.style.top = `${sy}px`;
  els.tourSpotlight.style.width = `${Math.max(24, sw)}px`;
  els.tourSpotlight.style.height = `${Math.max(24, sh)}px`;

  const popRect = els.tourPopover.getBoundingClientRect();
  const popW = popRect.width || 336;
  const popH = popRect.height || 184;
  const gap = 14;
  let px = sx;
  let py = sy + sh + gap;
  if (step.placement === 'right') {
    px = sx + sw + gap;
    py = Math.max(64, sy + 24);
    if (px + popW > vw - 12) {
      px = sx + 12;
      py = sy + 24;
    }
  } else if (step.placement === 'bottom-end') {
    px = sx + sw - popW;
    py = sy + sh + gap;
  } else {
    px = sx;
    py = sy + sh + gap;
  }
  px = Math.max(12, Math.min(vw - popW - 12, Math.round(px)));
  py = Math.max(12, Math.min(vh - popH - 12, Math.round(py)));
  els.tourPopover.style.left = `${px}px`;
  els.tourPopover.style.top = `${py}px`;
}

function renderTourStep() {
  if (!state.tourOpen) return;
  const total = TOUR_STEPS.length;
  const idx = Math.max(0, Math.min(total - 1, state.tourStep));
  state.tourStep = idx;
  const step = TOUR_STEPS[idx];
  if (els.tourStepBadge) els.tourStepBadge.textContent = `${idx + 1} / ${total}`;
  if (els.tourTitle) els.tourTitle.textContent = t(step.titleKey);
  if (els.tourDesc) els.tourDesc.textContent = t(step.descKey);
  if (els.btnTourPrev) {
    els.btnTourPrev.hidden = idx === 0;
    els.btnTourPrev.textContent = t('tour.prev');
  }
  if (els.btnTourNext) {
    els.btnTourNext.textContent = t(idx === total - 1 ? 'tour.done' : 'tour.next');
  }
  els.tourDots.forEach((dot, i) => {
    const active = i === idx;
    dot.classList.toggle('is-active', active);
    dot.setAttribute('aria-selected', String(active));
  });
  positionTourStep();
  requestAnimationFrame(positionTourStep);
}

function startIntroTour(stepIndex = 0) {
  dismissIntroCard({ remember: true });
  closeActiveComboMenu(false);
  if (state.aiOpen) closeAiModal();
  if (els.templatesModal?.open) closeTemplatesModal();
  if (!els.tourLayer) return;
  state.tourOpen = true;
  state.tourStep = Math.max(0, Math.min(TOUR_STEPS.length - 1, Number(stepIndex) || 0));
  els.tourLayer.hidden = false;
  renderTourStep();
}

function closeIntroTour() {
  markIntroSeen();
  state.tourOpen = false;
  if (els.tourLayer) els.tourLayer.hidden = true;
}

els.btnIntro?.addEventListener('click', () => {
  if (state.tourOpen) closeIntroTour();
  else startIntroTour(0);
});
els.btnTourClose?.addEventListener('click', closeIntroTour);
document.addEventListener('pointerdown', (e) => {
  if (!state.tourOpen) return;
  const target = e.target;
  if (target instanceof Node && (els.tourPopover?.contains(target) || els.btnIntro?.contains(target))) {
    return;
  }
  closeIntroTour();
}, true);
els.btnTourPrev?.addEventListener('click', () => {
  if (state.tourStep > 0) {
    state.tourStep -= 1;
    renderTourStep();
  }
});
els.btnTourNext?.addEventListener('click', () => {
  if (state.tourStep < TOUR_STEPS.length - 1) {
    state.tourStep += 1;
    renderTourStep();
  } else {
    closeIntroTour();
  }
});
els.tourDots.forEach((dot) => {
  dot.addEventListener('click', () => {
    const idx = Number(dot.dataset.tourDot);
    if (Number.isFinite(idx)) {
      state.tourStep = idx;
      renderTourStep();
    }
  });
});
window.addEventListener('resize', () => {
  if (state.tourOpen) positionTourStep();
});

document.addEventListener('keydown', (e) => {
  if (els.templatesModal?.open || document.body.inert) return;
  if (state.tourOpen) {
    if (e.key === 'Escape') {
      e.preventDefault();
      closeIntroTour();
      return;
    }
    if (e.key === 'ArrowRight') {
      e.preventDefault();
      if (state.tourStep < TOUR_STEPS.length - 1) {
        state.tourStep += 1;
        renderTourStep();
      } else {
        closeIntroTour();
      }
      return;
    }
    if (e.key === 'ArrowLeft') {
      e.preventDefault();
      if (state.tourStep > 0) {
        state.tourStep -= 1;
        renderTourStep();
      }
      return;
    }
  }
  if (e.key === 'Escape' && state.aiOpen) {
    e.preventDefault();
    closeAiModal();
    return;
  }
  if (e.key === 'Escape' && state.dragSession?.active) {
    e.preventDefault();
    cancelDrag();
    return;
  }
  if (e.key === 'Escape' && state.introCardVisible) {
    e.preventDefault();
    dismissIntroCard({ remember: true });
    return;
  }
  const tag = e.target?.tagName;
  const isEditingField = tag === 'INPUT' || tag === 'TEXTAREA' || tag === 'SELECT';
  const mod = e.metaKey || e.ctrlKey;

  if (mod && e.key.toLowerCase() === 'k') {
    e.preventDefault();
    toggleAiModal();
    return;
  }

  if (!isEditingField && mod && e.key.toLowerCase() === 'z') {
    e.preventDefault();
    flushLiveDraft({ syncWasm: false });
    if (e.shiftKey) {
      if (state.history.canRedo()) { state.liveDraftDoc = null; state.history.redo(); renderAll(); syncToWasm(); syncSelectionToWasm(); }
    } else if (state.history.canUndo()) {
      state.liveDraftDoc = null; state.history.undo(); renderAll(); syncToWasm(); syncSelectionToWasm();
    }
  } else if (!isEditingField && mod && e.key.toLowerCase() === 'y') {
    e.preventDefault();
    flushLiveDraft({ syncWasm: false });
    if (state.history.canRedo()) { state.liveDraftDoc = null; state.history.redo(); renderAll(); syncToWasm(); syncSelectionToWasm(); }
  } else if (!isEditingField && mod && e.key.toLowerCase() === 'd') {
    e.preventDefault();
    duplicateSelected();
  } else if (!isEditingField && (e.key === 'Delete' || e.key === 'Backspace')) {
    e.preventDefault();
    deleteSelected();
  } else if (!isEditingField && !activeComboMenu && !mod && state.mode === 'edit' && state.selectedId !== 'root'
      && ['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
    const doc = currentDoc();
    const parentInfo = findParent(doc.root, state.selectedId);
    if (parentInfo?.parent?.type === 'AnchorBox') {
      e.preventDefault();
      const node = parentInfo.node;
      const np = node.props || {};
      const pos = np.anchorPosition
        || ((parentInfo.index === 0 && (node.type === 'ParticleBackdrop' || node.type === 'Card')) ? 'Fill' : 'TopLeft');
      if (pos === 'Fill') return;
      const step = e.shiftKey ? 8 : 1;
      const dx = e.key === 'ArrowLeft' ? -step : (e.key === 'ArrowRight' ? step : 0);
      const dy = e.key === 'ArrowUp' ? -step : (e.key === 'ArrowDown' ? step : 0);
      const nextPatch = {};
      if (dx !== 0 && !['TopBar', 'BottomBar'].includes(pos)) {
        nextPatch.anchorOffsetX = Math.max(-1200, Math.min(1200, (Number(np.anchorOffsetX) || 0) + dx));
      }
      if (dy !== 0 && !['LeftDock', 'RightDock'].includes(pos)) {
        const baseOy = np.anchorOffsetY ?? (!np.anchorPosition && pos === 'TopLeft' ? parentInfo.index * 44 : 0);
        nextPatch.anchorOffsetY = Math.max(-1200, Math.min(1200, (Number(baseOy) || 0) + dy));
      }
      if (Object.keys(nextPatch).length > 0) {
        const pad = parentInfo.parent.props?.padding ?? 12;
        if (!np.anchorPosition) nextPatch.anchorPosition = pos;
        if (np.anchorMargin === undefined) nextPatch.anchorMargin = pad;
        applyPropPatch(node.id, nextPatch, true);
      }
    }
  }
});

new ResizeObserver(() => {
  const measuredW = els.canvasShell?.clientWidth || state.viewportWidth;
  syncViewportTier(Math.min(state.viewportWidth, measuredW));
  if (state.mode === 'edit') {
    syncOverlay();
  }
}).observe(els.canvasShell);

/* Boot */
syncViewportTier(state.viewportWidth);
setInspectorTab(state.inspectorTab);
syncAiSettingsUi();
applyLanguage();
renderPalette();
renderAll();
document.documentElement.dataset.i18nReady = '1';
document.body.inert = false;
initWasmStage(wasmProbe);
