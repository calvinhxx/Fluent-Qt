"use strict";

// Picking borrows the existing example. All edits belong to a separate Qt preview.
window.FluentQtPlayground = {
  create(root, transport) {
    if (!root.hasAttribute('data-playground')) return null;
    const zh = document.documentElement.lang === 'zh-CN';
    const words = zh ? {
      preview: '属性动态预览',
      close: '关闭预览', more: '更多属性', reset: '重置属性', copy: '复制代码', copied: '已复制',
      copying: '复制中…', failed: '复制失败，请重试。', loading: '正在加载预览…',
      unavailable: '预览暂时无法加载', retry: '重新尝试', invalid: '输入无效，未应用这次修改。',
      search: '查找属性或 API', follow: '跟随主题', empty: '没有匹配的属性', language: '代码语言',
      numeric: '请输入有效数字。', noTargets: '当前示例没有可选取的组件。',
      options: {ChevronDown: '向下', ChevronUp: '向上', ChevronRight: '向右', Standard: '标准', Accent: '强调', Subtle: '轻量', Small: '小', StandardSize: '默认', Medium: '中', Large: '大', ExtraLarge: '特大', None: '无', Add: '加号', CheckMark: '对勾', Save: '保存', FavoriteStar: '星标', Settings: '设置', Search: '搜索', Mail: '邮件', Home: '主页', Document: '文档', Info: '信息', Warning: '警告', ErrorBadge12: '错误', TextOnly: '仅文字', IconBefore: '图标在前', IconAfter: '图标在后', IconOnly: '仅图标', IconAbove: '图标在上', IconBelow: '图标在下', Rest: '默认', Hover: '悬停', Pressed: '按下', Disabled: '禁用', Unchecked: '未选中', PartiallyChecked: '部分选中', Checked: '已选中', Horizontal: '水平', Vertical: '垂直', NoTicks: '无刻度', TicksAbove: '上方刻度', TicksBelow: '下方刻度', TicksLeft: '左侧刻度', TicksRight: '右侧刻度', TicksBothSides: '双侧刻度', Peek: '点击预览', Hidden: '隐藏', Visible: '始终显示', Left: '左侧', Right: '右侧', Leading: '首端', Trailing: '尾端', Single: '单选', Multiple: '多选', Extended: '扩展多选', Compact: '紧凑', Inline: '行内', Caption: '辅助说明', Body: '正文', BodyStrong: '正文加粗', BodyLarge: '大号正文', BodyLargeStrong: '大号正文加粗', Subtitle: '副标题', Title: '标题', TitleLarge: '大标题', Display: '展示标题', ElideLeft: '左侧省略', ElideRight: '右侧省略', ElideMiddle: '中间省略', ElideNone: '不省略', Auto: '自动', Automatic: '自动', Stacked: '上下堆叠', Circular: '圆形', Square: '方形', Available: '在线', Away: '离开', Busy: '忙碌', DoNotDisturb: '请勿打扰', Offline: '离线', Dot: '圆点', Icon: '图标', Value: '数值', Informational: '通知', Attention: '关注', Caution: '警告', Success: '成功', Critical: '严重', Error: '错误', Validating: '校验中', Running: '运行中', Paused: '已暂停', Branded: '品牌样式', Simple: '简洁样式', Custom: '自定义', ImageCard: '图文卡片', AvatarTextRow: '头像文本行', TextBlock: '文本块', Top: '顶部', TopStart: '左上角', TopEnd: '右上角', Bottom: '底部', BottomStart: '左下角', BottomEnd: '右下角', TopLeft: '上左', TopRight: '上右', BottomLeft: '下左', BottomRight: '下右', LeftTop: '左上', LeftBottom: '左下', RightTop: '右上', RightBottom: '右下', Full: '居中/全幅', Day: '日', Month: '月', Year: '年', Monday: '周一', Tuesday: '周二', Wednesday: '周三', Thursday: '周四', Friday: '周五', Saturday: '周六', Sunday: '周日', FullMonthName: '完整月份', AbbreviatedMonthName: '缩写月份', NumericMonth: '数字月份', TwoDigitMonth: '两位月份', DayInteger: '日期数字', TwoDigitDay: '两位日期', DayIntegerWithAbbreviatedWeekday: '日期与星期', FullYear: '完整年份', TwoDigitYear: '两位年份', TwelveHourClock: '12 小时制', TwentyFourHourClock: '24 小时制', Layer: '默认层', LayerAlt: '交替层', Canvas: '画布底色', Beginning: '开头省略', Middle: '中间省略', List: '列表', Detail: '详情', LeftCompact: '左侧紧凑', LeftMinimal: '左侧极简', ScrollButtons: '滚动按钮', MoreButton: '更多按钮', Equal: '等宽', SizeToContent: '自适应内容', OnHover: '悬停显示', Always: '始终显示', Manual: '手动', Contain: '适应内容', ScaleDown: '缩小适应', Zoom: '缩放', Pan: '平移', Collapsed: '折叠', VisibleOnPointerOver: '悬停显示', FastTransformation: '快速采样', SmoothTransformation: '平滑采样', Line: '折线图', Area: '面积图', Bar: '柱状图', HorizontalBar: '条形图', Pie: '饼图', Donut: '环形图', Scatter: '散点图', Sparkline: '迷你图', Alternate: '交替', SlideFade: '滑动淡入', ScaleFade: '缩放淡入', Immediate: '立即切换', Transient: '轻量模式', Solid: '纯色', Transparent: '透明背景', FlowingRibbons: '流动光带', FloatingDots: '漂浮光点', Starfield: '星空穿梭', Raster: 'CPU 光栅', OpenGL: 'OpenGL 硬件', Mica: '云母 (Mica)', Acrylic: '亚克力 (Acrylic)', Design: '设计', Engineering: '工程', Research: '研究'}
    } : {
      preview: 'Property preview',
      close: 'Close preview', more: 'More properties', reset: 'Reset properties', copy: 'Copy code', copied: 'Copied',
      copying: 'Copying…', failed: 'Could not copy. Try again.', loading: 'Loading preview…',
      unavailable: 'Preview could not load', retry: 'Try again', invalid: 'Invalid input. This change was not applied.',
      search: 'Find a property or API', follow: 'Follow theme', empty: 'No matching properties', language: 'Code language',
      numeric: 'Enter a valid number.', noTargets: 'No editable components in this example.',
      options: {ChevronDown: 'Down', ChevronUp: 'Up', ChevronRight: 'Right', Standard: 'Standard', Accent: 'Accent', Subtle: 'Subtle', Small: 'Small', StandardSize: 'Default', Medium: 'Medium', Large: 'Large', ExtraLarge: 'Extra large', None: 'None', Add: 'Add', CheckMark: 'CheckMark', Save: 'Save', FavoriteStar: 'Favorite star', Settings: 'Settings', Search: 'Search', Mail: 'Mail', Home: 'Home', Document: 'Document', Info: 'Info', Warning: 'Warning', ErrorBadge12: 'Error badge', TextOnly: 'Text only', IconBefore: 'Icon before text', IconAfter: 'Icon after text', IconOnly: 'Icon only', IconAbove: 'Icon above text', IconBelow: 'Icon below text', Rest: 'Rest', Hover: 'Hover', Pressed: 'Pressed', Disabled: 'Disabled', Unchecked: 'Unchecked', PartiallyChecked: 'Partially checked', Checked: 'Checked', Horizontal: 'Horizontal', Vertical: 'Vertical', NoTicks: 'No ticks', TicksAbove: 'Ticks above', TicksBelow: 'Ticks below', TicksLeft: 'Ticks left', TicksRight: 'Ticks right', TicksBothSides: 'Both sides', Peek: 'Peek', Hidden: 'Hidden', Visible: 'Visible', Left: 'Left', Right: 'Right', Leading: 'Leading', Trailing: 'Trailing', Single: 'Single', Multiple: 'Multiple', Extended: 'Extended', Compact: 'Compact', Inline: 'Inline', Caption: 'Caption', Body: 'Body', BodyStrong: 'Body strong', BodyLarge: 'Body large', BodyLargeStrong: 'Body large strong', Subtitle: 'Subtitle', Title: 'Title', TitleLarge: 'Title large', Display: 'Display', ElideLeft: 'Elide left', ElideRight: 'Elide right', ElideMiddle: 'Elide middle', ElideNone: 'No elide', Auto: 'Auto', Automatic: 'Automatic', Stacked: 'Stacked', Circular: 'Circular', Square: 'Square', Available: 'Available', Away: 'Away', Busy: 'Busy', DoNotDisturb: 'Do not disturb', Offline: 'Offline', Dot: 'Dot', Icon: 'Icon', Value: 'Value', Informational: 'Informational', Attention: 'Attention', Caution: 'Caution', Success: 'Success', Critical: 'Critical', Error: 'Error', Validating: 'Validating', Running: 'Running', Paused: 'Paused', Branded: 'Branded', Simple: 'Simple', Custom: 'Custom', ImageCard: 'Image card', AvatarTextRow: 'Avatar & text', TextBlock: 'Text block', Top: 'Top', TopStart: 'Top start', TopEnd: 'Top end', Bottom: 'Bottom', BottomStart: 'Bottom start', BottomEnd: 'Bottom end', TopLeft: 'Top left', TopRight: 'Top right', BottomLeft: 'Bottom left', BottomRight: 'Bottom right', LeftTop: 'Left top', LeftBottom: 'Left bottom', RightTop: 'Right top', RightBottom: 'Right bottom', Full: 'Full / Center', Day: 'Day', Month: 'Month', Year: 'Year', Monday: 'Monday', Tuesday: 'Tuesday', Wednesday: 'Wednesday', Thursday: 'Thursday', Friday: 'Friday', Saturday: 'Saturday', Sunday: 'Sunday', FullMonthName: 'Full month name', AbbreviatedMonthName: 'Abbreviated month', NumericMonth: 'Numeric month', TwoDigitMonth: 'Two-digit month', DayInteger: 'Day integer', TwoDigitDay: 'Two-digit day', DayIntegerWithAbbreviatedWeekday: 'Day & weekday', FullYear: 'Full year', TwoDigitYear: 'Two-digit year', TwelveHourClock: '12-hour clock', TwentyFourHourClock: '24-hour clock', Layer: 'Layer', LayerAlt: 'Layer alt', Canvas: 'Canvas', Beginning: 'Beginning', Middle: 'Middle', List: 'List', Detail: 'Detail', LeftCompact: 'Left compact', LeftMinimal: 'Left minimal', ScrollButtons: 'Scroll buttons', MoreButton: 'More button', Equal: 'Equal', SizeToContent: 'Size to content', OnHover: 'On hover', Always: 'Always', Manual: 'Manual', Contain: 'Contain', ScaleDown: 'Scale down', Zoom: 'Zoom', Pan: 'Pan', Collapsed: 'Collapsed', VisibleOnPointerOver: 'On hover', FastTransformation: 'Fast', SmoothTransformation: 'Smooth', Line: 'Line', Area: 'Area', Bar: 'Bar', HorizontalBar: 'Horizontal bar', Pie: 'Pie', Donut: 'Donut', Scatter: 'Scatter', Sparkline: 'Sparkline', Alternate: 'Alternate', SlideFade: 'Slide fade', ScaleFade: 'Scale fade', Immediate: 'Immediate', Transient: 'Transient', Solid: 'Solid', Transparent: 'Transparent', FlowingRibbons: 'Flowing ribbons', FloatingDots: 'Floating dots', Starfield: 'Starfield', Raster: 'CPU raster', OpenGL: 'OpenGL', Mica: 'Mica', Acrylic: 'Acrylic'}
    };
    const localized = value => value?.[zh ? 'zh' : 'en'] || value?.en || '';
    const dataLayout = root.dataset.playgroundLayout === 'data';
    const optionLabel = (field, value) => {
      const label = field.optionLabels?.[value] || field.valueLabels?.[String(value)];
      return (typeof label === 'string' ? label : localized(label)) || words.options[value] || String(value);
    };
    const opener = transport.trigger;
    const display = root.querySelector('.preview-display');
    const rows = new Map();
    let ready = false, opened = false, schemaBuilt = false, generation = 0;
    let issued = 0, expected = 0, applied = 0, acceptedRevision = -1, timer, replyTimer, positionFrame, freshState;
    let pending = new Map(), targets = [], targetSize = null, current = null, language = 'cpp';
    const propertyGroups = new Map();
    let separateStateText = null, separateTextValues = null, sharedTextValue = null, stateTextToggle = null;
    const backgroundState = new Map();
    const dropdowns = new Set();
    let requestedHover = false;
    let liveFrame = null, liveSession = '', liveInitial = null, liveReady = false;
    let initialPreviewHeight = 0, liveViewport = null;
    let dropdownSequence = 0;
    const node = (tag, cls, text) => { const n = document.createElement(tag); if (cls) n.className = cls; if (text !== undefined) n.textContent = text; return n; };
    const parseStringList = raw => String(raw ?? '').split(/\r?\n/).map(item => item.trim()).filter(Boolean);
    const sameStringList = (a, b) => Array.isArray(a) && Array.isArray(b)
      && a.length === b.length && a.every((item, index) => item === b[index]);
    const icon = name => { const n = node('span', `playground-icon playground-icon-${name}`); n.setAttribute('aria-hidden', 'true'); return n; };
    const action = (label, glyph, cls = '') => {
      const b = node('button', `playground-action ${cls}`); b.type = 'button'; b.title = label; b.setAttribute('aria-label', label);
      if (glyph) b.append(icon(glyph)); return b;
    };
    const backdrop = node('div', 'playground-backdrop'); backdrop.hidden = true;
    backdrop.setAttribute('aria-hidden', 'true');
    const popup = node('section', 'playground-popover'); popup.id = 'playground-popover'; popup.hidden = true;
    popup.tabIndex = -1;
    popup.setAttribute('role', 'dialog'); popup.setAttribute('aria-modal', 'true'); popup.setAttribute('aria-labelledby', 'playground-heading');

    // Dialog Header: Title, Live indicator, and top actions (Language switch, Reset, Copy code, Close)
    const header = node('div', 'playground-heading');
    const headerTitle = node('div', 'playground-heading-title');
    const titleNode = node('strong', '', root.dataset.component); titleNode.id = 'playground-heading';
    headerTitle.append(titleNode, node('span', 'playground-badge', words.preview));

    popup.classList.toggle('playground-data-layout', dataLayout);
    const headerActions = node('div', 'playground-heading-actions');

    // Single language switch entrance
    const langTabs = node('div', 'playground-code-tabs');
    langTabs.setAttribute('role', 'tablist');
    langTabs.setAttribute('aria-label', words.language);
    const cppTab = node('button', 'playground-code-tab', 'C++');
    cppTab.type = 'button'; cppTab.setAttribute('role', 'tab'); cppTab.dataset.lang = 'cpp';
    cppTab.setAttribute('aria-selected', String(language === 'cpp'));
    const pythonTab = node('button', 'playground-code-tab', 'PySide6');
    pythonTab.type = 'button'; pythonTab.setAttribute('role', 'tab'); pythonTab.dataset.lang = 'python';
    pythonTab.setAttribute('aria-selected', String(language === 'python'));
    langTabs.append(cppTab, pythonTab);

    // Reset button at top
    const reset = action(words.reset, 'undo', 'playground-action playground-reset');
    reset.disabled = true;
    reset.setAttribute('aria-label', words.reset);

    // Minimalist Copy code button (docked in code header next to lang tabs)
    const copy = action(words.copy, 'copy', 'playground-copy');
    copy.disabled = true;
    copy.setAttribute('aria-label', words.copy);

    // Close button
    const close = action(words.close, 'close', 'playground-action playground-close');
    close.title = `${words.close} (Esc)`;
    close.setAttribute('aria-label', `${words.close} (Esc)`);

    headerActions.append(reset, close);
    header.append(headerTitle, headerActions);
    popup.append(header);

    const stage = node('div', 'playground-preview');
    const previewContent = node('div', 'playground-preview-content');
    const previewState = node('div', 'playground-preview-state');
    const previewStateLine = node('div', 'playground-preview-state-line');
    const previewSpinner = node('span', 'playground-preview-spinner'); previewSpinner.setAttribute('aria-hidden', 'true');
    const previewStateText = node('span'); previewStateText.setAttribute('role', 'status'); previewStateText.setAttribute('aria-atomic', 'true');
    previewStateLine.append(previewSpinner, previewStateText);
    const retry = action(words.retry, 'undo', 'playground-preview-retry');
    retry.append(document.createTextNode(words.retry)); retry.hidden = true;
    previewState.append(previewStateLine, retry);
    previewContent.append(previewState);
    stage.append(previewContent);
    const interactionFeedback = node('p', 'playground-interaction-feedback');
    interactionFeedback.setAttribute('role', 'status'); interactionFeedback.hidden = true;
    stage.append(interactionFeedback);
    function postLive(type, command, requestId, selection = String(generation)) {
      liveFrame?.contentWindow?.postMessage({source: 'fluent-qt-site', type, session: liveSession,
        example: transport.example(), selection, requestId, command}, location.origin);
    }
    function layoutLive() {
      if (!liveFrame || !current?.preview?.live || !previewContent.clientWidth || !controlsPane.clientHeight) return;
      const preview = liveViewport || current.preview;
      const width = 680;
      const displayWidth = Math.max(280, Math.min(680, preview.displayWidth || preview.width));
      const containedSurface = current.schema.component === 'drawer-view' ? preview.surface : null;
      const layoutAnchor = containedSurface
        || (current.schema.surface && current.schema.surface !== 'control' && preview.surface ? preview.surface : null)
        || preview.target;
      const controlHeight = Math.max(84, (layoutAnchor?.y || 0) + (layoutAnchor?.height || 0) + 16);
      if (!initialPreviewHeight) initialPreviewHeight = !containedSurface && preview.hasOpenOverlay ? preview.height : controlHeight;
      const baseHeight = Math.max(initialPreviewHeight, controlHeight);
      // Reserve usable editor space even when the native control has a tall minimum size.
      const editorReserve = Math.min(180, controlsPane.clientHeight * 0.4);
      const maxHeight = Math.max(84, Math.min(360,
        controlsPane.clientHeight - categoryTabsBar.offsetHeight - editorReserve - 38));
      const scale = Math.min(1, previewContent.clientWidth / displayWidth, maxHeight / baseHeight);
      // Use layout pixels, not transformed rectangles whose rounding can oscillate by one pixel.
      const availableCanvasHeight = controlsPane.clientHeight - previewContent.offsetTop - stage.clientTop;
      const canvasHeight = Math.max(baseHeight, Math.min(640, Math.floor(availableCanvasHeight / scale)));
      liveFrame.style.width = `${width}px`;
      liveFrame.style.height = `${canvasHeight}px`;
      liveFrame.style.transform = `translateX(-50%) scale(${scale})`;
      // Popups may paint over the editor without entering layout or moving their anchor.
      liveFrame.style.clipPath = containedSurface
        ? `inset(${containedSurface.y}px ${Math.max(0, width - containedSurface.x - containedSurface.width)}px ${Math.max(0, canvasHeight - containedSurface.y - containedSurface.height)}px ${containedSurface.x}px round 8px)`
        : `inset(0 0 ${Math.max(0, canvasHeight - Math.max(baseHeight, preview.height))}px 0)`;
      previewContent.style.height = `${Math.ceil(baseHeight * scale)}px`;
      stage.classList.toggle('playground-overlay-open', current.schema.component !== 'drawer-view'
        && Boolean(preview.hasVisibleOverlay ?? preview.hasOpenOverlay));
    }
    function acceptViewport(viewport) {
      if (!viewport) return;
      if (!liveViewport || (viewport.geometryRevision || 0) >= (liveViewport.geometryRevision || 0)) liveViewport = viewport;
      layoutLive();
    }
    function beginLive(initial) {
      liveInitial = initial?.values || {};
      liveSession = `playground-${generation}-${Date.now()}`;
      liveReady = false;
      acceptedRevision = -1;
      const url = new URL(root.dataset.runtimeUrl, location.href);
      Object.entries({embed: 'site', preview: 'component', playground: 'interactive', ...transport.example(),
        'host-theme': root.dataset.previewTheme || document.documentElement.dataset.theme,
        'host-session': liveSession, 'render-scale': 'native'}).forEach(([key, value]) => url.searchParams.set(key, value));
      liveFrame = node('iframe', 'playground-live-frame');
      liveFrame.title = `${initial?.schema.title || root.dataset.component} ${zh ? '交互预览' : 'interactive preview'}`;
      liveFrame.tabIndex = 0;
      liveFrame.style.width = '680px';
      liveFrame.style.visibility = 'hidden';
      const frame = liveFrame;
      frame.fluentQtViewport = (session, viewport) => {
        if (opened && liveFrame === frame && session === liveSession) acceptViewport(viewport);
      };
      frame.addEventListener('load', () => {
        if (!opened || liveFrame !== frame) return;
        try {
          if (!frame.contentDocument?.documentElement.hasAttribute('data-fluent-qt-loaded')) unavailable();
        } catch { unavailable(); }
      });
      frame.addEventListener('error', () => { if (opened && liveFrame === frame) unavailable(); });
      liveFrame.src = url.href;
      previewContent.append(liveFrame);
      stage.classList.add('playground-live');
      clearTimeout(replyTimer);
      replyTimer = setTimeout(() => unavailable(), 45000);
      if (initial) transport.send('component-playground-pick', {operation: 'close'}, ++issued, String(generation));
    }
    window.addEventListener('message', event => {
      if (!opened || !liveFrame || event.source !== liveFrame.contentWindow || event.origin !== location.origin
          || event.data?.source !== 'fluent-qt-gallery' || event.data.session !== liveSession) return;
      if (event.data.state === 'ready' && !liveReady) {
        liveReady = true;
        send({operation: 'open', initialValues: liveInitial}, true);
      } else if (event.data.state === 'playground-state') {
        receive(event.data.detail);
      } else if (['error', 'exit'].includes(event.data.state)) unavailable();
      else if (event.data.state === 'playground-escape') dismiss();
      else if (event.data.state === 'playground-focus-exit') {
        const controls = focusableControls(), index = controls.indexOf(liveFrame);
        const next = (index + (event.data.detail?.backward ? -1 : 1) + controls.length) % controls.length;
        controls[next]?.focus({preventScroll: true});
      }
    });
    new ResizeObserver(layoutLive).observe(previewContent);

    const status = node('p', 'playground-status'); status.setAttribute('role', 'status'); status.hidden = true;
    popup.append(status);

    // Segmented category tabs
    const categoryTabsBar = node('div', 'playground-category-tabs');
    categoryTabsBar.setAttribute('role', 'tablist');
    categoryTabsBar.setAttribute('aria-label', zh ? '属性分类' : 'Property categories');

    const CATEGORIES = [
      { id: 'basic', label: zh ? '基础属性' : 'Basic' },
      { id: 'icon', label: zh ? '图标与布局' : 'Icon & Layout' },
      { id: 'appearance', label: zh ? '交互与外观' : 'Style & State' }
    ];

    let activeCategoryTab = 'basic';
    const categoryTabButtons = new Map();
    const categoryTabPanels = new Map();

    const categoryPanelsContainer = node('div', 'playground-category-panels');
    categoryPanelsContainer.id = 'playground-extra';

    CATEGORIES.forEach((cat, index) => {
      const isFirst = index === 0;
      const tabBtn = node('button', 'playground-category-tab');
      tabBtn.type = 'button';
      tabBtn.setAttribute('role', 'tab');
      tabBtn.dataset.tab = cat.id;
      tabBtn.id = `tab-category-${cat.id}`;
      tabBtn.setAttribute('aria-controls', `panel-category-${cat.id}`);
      tabBtn.setAttribute('aria-selected', isFirst ? 'true' : 'false');
      tabBtn.tabIndex = isFirst ? 0 : -1;
      tabBtn.textContent = cat.label;

      const panel = node('div', 'playground-tabpanel playground-fields');
      panel.id = `panel-category-${cat.id}`;
      panel.dataset.tab = cat.id;
      panel.setAttribute('role', 'tabpanel');
      panel.setAttribute('aria-labelledby', `tab-category-${cat.id}`);
      panel.hidden = !isFirst;

      tabBtn.addEventListener('click', () => switchCategoryTab(cat.id));
      tabBtn.addEventListener('keydown', event => {
        if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
        event.preventDefault();
        const ids = CATEGORIES.map(c => c.id).filter(id => !categoryTabButtons.get(id).hidden);
        const idx = ids.indexOf(cat.id);
        let nextIdx = idx;
        if (event.key === 'ArrowLeft') nextIdx = (idx - 1 + ids.length) % ids.length;
        else if (event.key === 'ArrowRight') nextIdx = (idx + 1) % ids.length;
        else if (event.key === 'Home') nextIdx = 0;
        else if (event.key === 'End') nextIdx = ids.length - 1;
        switchCategoryTab(ids[nextIdx], true);
      });

      categoryTabsBar.append(tabBtn);
      categoryPanelsContainer.append(panel);
      categoryTabButtons.set(cat.id, tabBtn);
      categoryTabPanels.set(cat.id, panel);
    });

    function switchCategoryTab(tabId, focus = false) {
      activeCategoryTab = tabId;
      categoryTabButtons.forEach((btn, id) => {
        const selected = id === tabId;
        btn.setAttribute('aria-selected', selected ? 'true' : 'false');
        btn.tabIndex = selected ? 0 : -1;
        if (selected && focus) btn.focus();
      });
      categoryTabPanels.forEach((p, id) => {
        p.hidden = id !== tabId;
      });
    }

    const extra = categoryPanelsContainer;
    const common = categoryTabPanels.get('basic');

    // Inline more/collapse toggle retained for backwards compatibility
    const more = action(words.more, null, 'playground-more');
    const moreText = node('span', 'playground-more-text', words.more);
    more.append(moreText, icon('down'));
    more.setAttribute('aria-expanded', 'true');
    more.hidden = true;
    more.style.display = 'none';



    // Left pane: Controls
    const controlsPane = node('div', 'playground-pane-controls');
    controlsPane.id = 'playground-controls-pane';
    const fieldsPlaceholder = node('div', 'playground-fields-placeholder'); fieldsPlaceholder.setAttribute('aria-hidden', 'true');
    for (let index = 0; index < 3; index += 1) fieldsPlaceholder.append(node('span'), node('span'));
    controlsPane.append(stage, categoryTabsBar, fieldsPlaceholder, categoryPanelsContainer, more);

    // Right pane: Live generated source code view
    const codePane = node('div', 'playground-pane-code');
    codePane.id = 'playground-source-pane';
    const codeHeader = node('div', 'playground-code-header');
    const codeTitleWrap = node('div', 'playground-code-title-wrap');
    codeTitleWrap.append(icon('code'), node('span', 'playground-code-title', zh ? '源码预览' : 'Source Code'));

    const codeHeaderActions = node('div', 'playground-code-header-actions');
    codeHeaderActions.append(langTabs, copy);
    codeHeader.append(codeTitleWrap, codeHeaderActions);

    const codeBox = node('div', 'playground-code-box');
    const codePre = node('pre', 'playground-code-pre');
    const codeText = node('code', 'playground-code-text');
    codePre.append(codeText);
    const codePlaceholder = node('div', 'playground-code-placeholder'); codePlaceholder.setAttribute('aria-hidden', 'true');
    for (const width of [42, 30, 76, 62, 48]) {
      const line = node('span'); line.style.width = `${width}%`; codePlaceholder.append(line);
    }
    codeBox.append(codePre, codePlaceholder);
    codePane.append(codeHeader, codeBox);

    // Draggable Splitter between Controls Pane and Code Pane
    const splitter = node('div', 'playground-splitter');
    splitter.setAttribute('role', 'separator');
    splitter.setAttribute('aria-orientation', 'vertical');
    splitter.setAttribute('aria-label', zh ? '调整左右两栏宽度' : 'Resize split panes');
    splitter.setAttribute('tabindex', '0');
    splitter.title = zh ? '拖拽调整左右栏宽度' : 'Drag to resize panes';

    const splitterBar = node('div', 'playground-splitter-bar');
    splitter.append(splitterBar);

    const mobileViews = node('div', 'playground-mobile-views');
    mobileViews.setAttribute('role', 'group');
    mobileViews.setAttribute('aria-label', zh ? '预览视图' : 'Preview view');
    const mobileButtons = new Map();
    for (const [view, label, target] of [
      ['controls', zh ? '属性' : 'Properties', controlsPane.id],
      ['code', zh ? '代码' : 'Code', codePane.id]
    ]) {
      const button = action(label, null, 'playground-mobile-view');
      button.textContent = label;
      button.dataset.view = view;
      button.setAttribute('aria-controls', target);
      button.addEventListener('click', () => showMobileView(view));
      mobileViews.append(button); mobileButtons.set(view, button);
    }
    function showMobileView(view) {
      popup.dataset.mobileView = view;
      mobileButtons.forEach((button, key) => button.setAttribute('aria-pressed', String(key === view)));
    }
    showMobileView('controls');
    popup.append(mobileViews);
    const split = node('div', 'playground-split');
    split.append(controlsPane, splitter, codePane);
    popup.append(split);

    let isSplitDragging = false;
    let splitStartX = 0;
    let splitStartLeftW = 0;

    function startSplitDrag(event) {
      if (event.button !== 0) return;
      isSplitDragging = true;
      splitStartX = event.clientX;
      splitStartLeftW = controlsPane.getBoundingClientRect().width;
      splitter.classList.add('dragging');
      popup.classList.add('playground-split-resizing');
      splitter.setPointerCapture(event.pointerId);
      event.preventDefault();
    }

    function onSplitMove(event) {
      if (!isSplitDragging) return;
      const dx = event.clientX - splitStartX;
      const splitRect = split.getBoundingClientRect();
      const minLeft = 240;
      const maxLeft = Math.max(minLeft, splitRect.width - 220);
      const nextLeft = Math.max(minLeft, Math.min(splitStartLeftW + dx, maxLeft));
      controlsPane.style.width = `${nextLeft}px`;
      controlsPane.style.flex = '0 0 auto';
    }

    function stopSplitDrag(event) {
      if (!isSplitDragging) return;
      isSplitDragging = false;
      splitter.classList.remove('dragging');
      popup.classList.remove('playground-split-resizing');
      try { splitter.releasePointerCapture(event.pointerId); } catch (_) {}
      layoutLive();
    }

    splitter.addEventListener('pointerdown', startSplitDrag);
    splitter.addEventListener('pointermove', onSplitMove);
    splitter.addEventListener('pointerup', stopSplitDrag);
    splitter.addEventListener('pointercancel', stopSplitDrag);

    splitter.addEventListener('keydown', event => {
      if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
        event.preventDefault();
        const delta = event.key === 'ArrowLeft' ? -16 : 16;
        const splitRect = split.getBoundingClientRect();
        const currentW = controlsPane.getBoundingClientRect().width;
        const minLeft = 240;
        const maxLeft = Math.max(minLeft, splitRect.width - 220);
        const nextLeft = Math.max(minLeft, Math.min(currentW + delta, maxLeft));
        controlsPane.style.width = `${nextLeft}px`;
        controlsPane.style.flex = '0 0 auto';
      }
    });

    // Resizing handles
    let isResizing = false;
    let resizeDir = '';
    let resizeStartX = 0, resizeStartY = 0;
    let initialW = 0, initialH = 0;
    let initialLeft = 0, initialTop = 0;
    let userResized = false;

    const resizeE = node('div', 'playground-resize-handle playground-resize-e');
    const resizeS = node('div', 'playground-resize-handle playground-resize-s');
    const resizeSE = node('div', 'playground-resize-handle playground-resize-se');
    const gripSvg = document.createElementNS('http://www.w3.org/2000/svg', 'svg');
    gripSvg.setAttribute('class', 'playground-resize-grip');
    gripSvg.setAttribute('viewBox', '0 0 10 10');
    gripSvg.setAttribute('fill', 'none');
    gripSvg.setAttribute('stroke', 'currentColor');
    gripSvg.setAttribute('stroke-width', '1.2');
    gripSvg.setAttribute('stroke-linecap', 'round');
    const gripPath = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    gripPath.setAttribute('d', 'M8 2L2 8M8 5L5 8M8 8L8 8');
    gripSvg.append(gripPath);
    resizeSE.append(gripSvg);
    popup.append(resizeE, resizeS, resizeSE);

    function startResize(dir, event) {
      if (event.button !== 0) return;
      isResizing = true;
      resizeDir = dir;
      resizeStartX = event.clientX;
      resizeStartY = event.clientY;
      const rect = popup.getBoundingClientRect();
      initialW = rect.width;
      initialH = rect.height;
      initialLeft = rect.left;
      initialTop = rect.top;
      popup.classList.add('playground-resizing');
      event.target.setPointerCapture(event.pointerId);
      event.preventDefault();
      event.stopPropagation();
    }

    function onResizeMove(event) {
      if (!isResizing) return;
      const dx = event.clientX - resizeStartX;
      const dy = event.clientY - resizeStartY;
      const isWide = window.innerWidth >= 1100;
      const minW = isWide ? 600 : 300;
      const maxW = window.innerWidth - initialLeft - 8;
      const minH = 320;
      const maxH = window.innerHeight - initialTop - 8;

      if (resizeDir.includes('e')) {
        const nextW = Math.max(minW, Math.min(initialW + dx, maxW));
        popup.style.width = `${nextW}px`;
      }
      if (resizeDir.includes('s')) {
        const nextH = Math.max(minH, Math.min(initialH + dy, maxH));
        popup.style.height = `${nextH}px`;
      }
      userResized = true;
    }

    function stopResize(event) {
      if (!isResizing) return;
      isResizing = false;
      popup.classList.remove('playground-resizing');
      try { event.target.releasePointerCapture(event.pointerId); } catch (_) {}
      schedulePosition();
    }

    resizeE.addEventListener('pointerdown', e => startResize('e', e));
    resizeE.addEventListener('pointermove', onResizeMove);
    resizeE.addEventListener('pointerup', stopResize);
    resizeE.addEventListener('pointercancel', stopResize);

    resizeS.addEventListener('pointerdown', e => startResize('s', e));
    resizeS.addEventListener('pointermove', onResizeMove);
    resizeS.addEventListener('pointerup', stopResize);
    resizeS.addEventListener('pointercancel', stopResize);

    resizeSE.addEventListener('pointerdown', e => startResize('se', e));
    resizeSE.addEventListener('pointermove', onResizeMove);
    resizeSE.addEventListener('pointerup', stopResize);
    resizeSE.addEventListener('pointercancel', stopResize);

    // Dragging Header to Move
    let isDragging = false;
    let dragStartX = 0, dragStartY = 0;
    let userMoved = false;

    header.addEventListener('pointerdown', event => {
      if (event.button !== 0) return;
      if (event.target.closest('button, input, select, a, [role="tab"]')) return;
      isDragging = true;
      dragStartX = event.clientX;
      dragStartY = event.clientY;
      const rect = popup.getBoundingClientRect();
      initialLeft = rect.left;
      initialTop = rect.top;
      popup.classList.add('playground-dragging');
      header.setPointerCapture(event.pointerId);
      event.preventDefault();
    });

    header.addEventListener('pointermove', event => {
      if (!isDragging) return;
      const dx = event.clientX - dragStartX;
      const dy = event.clientY - dragStartY;
      const margin = 8;
      const width = popup.offsetWidth;
      const height = popup.offsetHeight;
      const maxLeft = window.innerWidth - width - margin;
      const maxTop = window.innerHeight - height - margin;
      const nextLeft = Math.max(margin, Math.min(initialLeft + dx, maxLeft));
      const nextTop = Math.max(margin, Math.min(initialTop + dy, maxTop));
      popup.style.left = `${nextLeft}px`;
      popup.style.top = `${nextTop}px`;
      userMoved = true;
    });

    const stopDrag = event => {
      if (!isDragging) return;
      isDragging = false;
      popup.classList.remove('playground-dragging');
      try { header.releasePointerCapture(event.pointerId); } catch (_) {}
    };
    header.addEventListener('pointerup', stopDrag);
    header.addEventListener('pointercancel', stopDrag);

    document.body.append(backdrop, popup);

    function isolateBackground() {
      for (const child of document.body.children) {
        if (child === popup || child === backdrop || child.matches('script, style, link')) continue;
        if (!backgroundState.has(child)) backgroundState.set(child, child.inert);
        child.inert = true;
      }
    }
    const backgroundObserver = new MutationObserver(() => { if (opened) isolateBackground(); });
    function restoreBackground() {
      backgroundObserver.disconnect();
      for (const [element, inert] of backgroundState) element.inert = inert;
      backgroundState.clear();
    }
    function focusableControls() {
      return [...popup.querySelectorAll('button, input, select, textarea, a[href], [tabindex]')]
        .filter(element => !element.disabled && element.tabIndex >= 0 && !element.closest('[inert]')
          && element.getClientRects().length && getComputedStyle(element).visibility !== 'hidden');
    }
    document.addEventListener('focusin', event => {
      if (opened && !popup.contains(event.target)) (focusableControls()[0] || popup).focus({preventScroll: true});
    });

    function highlightCodeHtml(source) {
      if (!source) return '';
      const esc = s => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
      const wrapBreaks = s => esc(s).replace(/(-&gt;|\(|::|,\s*|\.)/g, '$1<wbr>');
      const tokenRegex = /(\/\/[^\n]*|#[^\n]*)|("[^"\\]*(?:\\.[^"\\]*)*"|'[^'\\]*(?:\\.[^'\\]*)*')|\b(auto|const|constexpr|new|delete|return|for|if|else|this|using|namespace|explicit|class|struct|import|from|as|in|and|or|not|self|None|None_|True|False|true|false|nullptr)\b|\b(fluent|fluentqt|Button|CompoundButton|DropDownButton|HyperlinkButton|RepeatButton|SplitButton|ToggleButton|ToggleSplitButton|CheckBox|RadioButton|ColorPicker|ComboBox|FileDropZone|MultiSelectComboBox|TreeSelect|RangeSlider|Slider|RatingControl|ToggleSwitch|LineEdit|AutoSuggestBox|TagInput|CodeBlock|Label|NumberBox|PasswordBox|TextEdit|FontIcon|CalendarDatePicker|CalendarView|DatePicker|DateTimeRangePicker|TimePicker|Avatar|InfoBadge|InfoBar|ProgressBar|ProgressRing|SplashScreen|Shimmer|Toast|ToolTip|Accordion|Card|Divider|Expander|Field|DataGridToolBar|PropertyPanel|ListDetailView|PropertyDefinition|Breadcrumb|BreadcrumbItem|NavigationView|Pivot|PivotItem|SelectorBar|SelectorBarItem|Stepper|StepperItem|TabView|TabViewItem|AnnotatedScrollBar|AnnotatedScrollBarLabel|ImageViewer|PipsPager|ScrollBar|ScrollView|ChartView|ChartModel|LineChart|AreaChart|BarChart|HorizontalBarChart|PieChart|DonutChart|ScatterChart|Sparkline|DataGrid|DrawerView|FileListView|FlipView|FlowView|GridView|ListView|Timeline|Transfer|SplitView|SplitViewPaneOptions|StackView|TreeView|FluentMenu|FluentMenuBar|FluentMenuItem|CommandBar|CommandPalette|CommandBarFlyout|ContentDialog|Dialog|Flyout|Popup|TeachingTip|CoachMark|TitleBar|Window|ParticleBackdrop|ParticlesSession|SpatialView|SpatialItem|SpatialSession|QmlView|QmlSession|FluentElement|Typography|FontRole|ButtonStyle|ButtonSize|ButtonLayout|PasswordRevealMode|QueryButtonPlacement|SelectionMode|SpinButtonPlacementMode|CalendarContentLevel|MonthFormat|DayFormat|YearFormat|ClockIdentifier|AvatarShape|AvatarSize|PresenceStatus|InfoBadgeDisplayMode|InfoBadgeStatus|InfoBarSeverity|ProgressRingSize|ProgressRingStatus|Presentation|ShimmerTemplate|Severity|Placement|PreferredPlacement|ExpansionMode|Appearance|LabelPosition|ValidationState|LayoutMode|SearchVisibility|Pane|BreadcrumbSize|OverflowMode|DisplayMode|OverflowBehavior|TabWidthMode|CloseButtonOverlayMode|FitMode|WheelAction|ControlsVisibility|PipsPagerButtonVisibility|ScrollMode|ScrollBarVisibility|ZoomMode|ChartType|DrawerEdge|StackViewTransitionType|NodeAlignment|ShowMode|BackdropEffect|Effect|BackgroundMode|RenderMode|WidgetOwnership|QMargins|QPoint|QPointF|QSize|QRect|QColor|QImage|QPair|QDate|QTime|QDateTime|QPixmap|QPainter|QAction|QWidget|QLabel|QVector3D|QUrl|QVBoxLayout|QHBoxLayout|QStringLiteral|QStringList|QStringListModel|QString|QVariantList|QVariantMap|QList|QVector|QStandardItemModel|QStandardItem|QSlider|TickPosition|Orientation|DayOfWeek|CheckState|TextElideMode|TransformationMode|Qt)\b|\b(\d+(?:\.\d+)?)\b/g;
      let out = '', last = 0, m;
      while ((m = tokenRegex.exec(source)) !== null) {
        if (m.index > last) out += wrapBreaks(source.slice(last, m.index));
        if (m[1]) out += `<span class="syn-cmt">${esc(m[1])}</span>`;
        else if (m[2]) out += `<span class="syn-str">${esc(m[2])}</span>`;
        else if (m[3]) out += `<span class="syn-kw">${esc(m[3])}</span>`;
        else if (m[4]) out += `<span class="syn-typ">${esc(m[4])}</span>`;
        else if (m[5]) out += `<span class="syn-num">${esc(m[5])}</span>`;
        last = tokenRegex.lastIndex;
      }
      if (last < source.length) out += wrapBreaks(source.slice(last));
      return out;
    }

    function updateCodeDisplay() {
      if (!codeText) return;
      if (!current) { codeText.textContent = ''; return; }
      const raw = current[language] || '';
      codeText.innerHTML = highlightCodeHtml(raw);
    }



    function createFluentDropdown(select, labelMap = {}) {
      const wrapper = node('div', 'playground-dropdown');
      if (select.parentElement) {
        select.parentElement.insertBefore(wrapper, select);
      }

      select.classList.add('playground-native-select');
      select.tabIndex = -1;
      select.setAttribute('aria-hidden', 'true');

      const trigger = node('button', 'playground-dropdown-trigger');
      trigger.type = 'button';
      trigger.setAttribute('role', 'combobox');
      trigger.setAttribute('aria-haspopup', 'listbox');
      trigger.setAttribute('aria-expanded', 'false');

      const triggerText = node('span', 'playground-dropdown-value');
      const triggerIcon = icon('down');
      trigger.append(triggerText, triggerIcon);

      const menu = node('div', 'playground-dropdown-menu');
      menu.id = `playground-options-${++dropdownSequence}`;
      menu.setAttribute('role', 'listbox');
      menu.hidden = true;
      trigger.setAttribute('aria-controls', menu.id);
      popup.append(menu);

      function updateTrigger() {
        const opt = select.selectedOptions[0] || select.options[0];
        triggerText.textContent = opt ? (labelMap[opt.value] || opt.text) : '';
        trigger.disabled = select.disabled;
      }

      function renderMenu() {
        menu.replaceChildren();
        Array.from(select.options).forEach(opt => {
          const item = node('button', 'playground-dropdown-item');
          item.type = 'button';
          item.setAttribute('role', 'option');
          item.dataset.value = opt.value;
          item.disabled = opt.disabled;
          const isSelected = opt.value === select.value;
          item.setAttribute('aria-selected', String(isSelected));
          if (isSelected) item.classList.add('selected');

          const check = node('span', 'playground-dropdown-check');
          check.innerHTML = '<svg viewBox="0 0 16 16" width="12" height="12" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 8.5L6.5 12L13 4.5"/></svg>';
          check.style.visibility = isSelected ? 'visible' : 'hidden';

          const itemLabel = node('span', 'playground-dropdown-label', labelMap[opt.value] || opt.text);
          item.append(check, itemLabel);

          item.addEventListener('click', e => {
            e.stopPropagation();
            if (select.value !== opt.value) {
              select.value = opt.value;
              select.dispatchEvent(new Event('change', { bubbles: true }));
            }
            close();
            trigger.focus();
          });
          menu.append(item);
        });
      }

      function positionMenu() {
        const rect = trigger.getBoundingClientRect();
        menu.style.position = 'fixed';
        menu.style.minWidth = `${Math.max(rect.width, 130)}px`;
        menu.style.width = 'max-content';
        menu.style.maxWidth = `${Math.max(220, window.innerWidth - 24)}px`;
        const menuWidth = menu.offsetWidth || Math.max(rect.width, 130);
        const clampedLeft = Math.max(12, Math.min(rect.left, window.innerWidth - menuWidth - 12));
        menu.style.left = `${clampedLeft}px`;
        const spaceBelow = window.innerHeight - rect.bottom;
        const menuHeight = menu.offsetHeight || 130;
        if (spaceBelow < menuHeight + 10 && rect.top > menuHeight) {
          menu.style.top = `${rect.top - menuHeight - 4}px`;
        } else {
          menu.style.top = `${rect.bottom + 4}px`;
        }
        menu.style.zIndex = '1300';
      }

      function open() {
        if (trigger.disabled || select.disabled) return;
        for (const dropdown of dropdowns) dropdown.close();
        renderMenu();
        menu.hidden = false;
        trigger.setAttribute('aria-expanded', 'true');
        positionMenu();
        document.addEventListener('pointerdown', onDocPointer, true);
        document.addEventListener('keydown', onDocKey, true);
        (menu.querySelector('[aria-selected="true"]:not(:disabled)') || menu.querySelector('[role="option"]:not(:disabled)'))?.focus();
      }

      function close() {
        menu.hidden = true;
        trigger.setAttribute('aria-expanded', 'false');
        document.removeEventListener('pointerdown', onDocPointer, true);
        document.removeEventListener('keydown', onDocKey, true);
      }

      function onDocPointer(e) {
        if (!wrapper.contains(e.target) && !menu.contains(e.target)) close();
      }

      function onDocKey(e) {
        if (e.key === 'Escape') {
          e.preventDefault();
          e.stopPropagation();
          close();
          trigger.focus();
        } else if (e.key === 'Tab') {
          close();
          trigger.focus();
        } else if (['ArrowDown', 'ArrowUp', 'Home', 'End'].includes(e.key)) {
          e.preventDefault();
          e.stopPropagation();
          const options = [...menu.querySelectorAll('[role="option"]:not(:disabled)')];
          const index = options.indexOf(document.activeElement);
          const next = e.key === 'Home' ? 0 : e.key === 'End' ? options.length - 1
            : (index + (e.key === 'ArrowDown' ? 1 : -1) + options.length) % options.length;
          options[next]?.focus();
        }
      }

      trigger.addEventListener('click', e => {
        e.stopPropagation();
        if (menu.hidden) open(); else close();
      });
      trigger.addEventListener('keydown', event => {
        if (['ArrowDown', 'ArrowUp'].includes(event.key)) {
          event.preventDefault();
          open();
        }
      });

      select.addEventListener('change', updateTrigger);
      updateTrigger();

      wrapper.append(trigger, select);
      const dropdown = { wrapper, trigger, menu, updateTrigger, close };
      dropdowns.add(dropdown);
      return dropdown;
    }

    function createFluentTemporalPicker(input, field) {
      const kind = field.type; // 'date' | 'time' | 'datetime'
      const pad2 = n => String(n).padStart(2, '0');
      const wrapper = node('div', 'playground-dropdown playground-temporal-picker');
      wrapper.dataset.temporalKind = kind;
      if (input.parentElement) {
        input.parentElement.insertBefore(wrapper, input);
      }

      input.classList.add('playground-native-select');
      input.tabIndex = -1;
      input.setAttribute('aria-hidden', 'true');

      const trigger = node('button', 'playground-dropdown-trigger playground-temporal-trigger');
      trigger.type = 'button';
      trigger.setAttribute('aria-haspopup', 'dialog');
      trigger.setAttribute('aria-expanded', 'false');

      const triggerText = node('span', 'playground-dropdown-value playground-temporal-value');
      const triggerGlyph = node('span', 'playground-temporal-icon');
      triggerGlyph.setAttribute('aria-hidden', 'true');
      triggerGlyph.innerHTML = kind === 'time'
        ? '<svg viewBox="0 0 16 16" width="14" height="14" fill="none" stroke="currentColor" stroke-width="1.35" stroke-linecap="round" stroke-linejoin="round"><circle cx="8" cy="8" r="6"/><path d="M8 4.6V8l2.4 1.5"/></svg>'
        : '<svg viewBox="0 0 16 16" width="14" height="14" fill="none" stroke="currentColor" stroke-width="1.35" stroke-linecap="round" stroke-linejoin="round"><rect x="2.25" y="3.25" width="11.5" height="10.5" rx="2"/><path d="M5 1.8v2.4M11 1.8v2.4M2.5 6.5h11"/></svg>';
      trigger.append(triggerText, triggerGlyph);

      let clearBtn = null;
      if (field.nullable) {
        clearBtn = node('button', 'playground-temporal-clear');
        clearBtn.type = 'button';
        clearBtn.tabIndex = -1;
        clearBtn.title = zh ? '清除' : 'Clear';
        clearBtn.setAttribute('aria-label', `${localized(field.label)}: ${zh ? '清除' : 'Clear'}`);
        clearBtn.innerHTML = '<svg viewBox="0 0 12 12" width="10" height="10" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round"><path d="M3 3l6 6M9 3L3 9"/></svg>';
        clearBtn.addEventListener('click', event => {
          event.stopPropagation();
          if (input.disabled) return;
          close();
          if (input.value !== '') {
            input.value = '';
            input.dispatchEvent(new Event('change', { bubbles: true }));
          }
          updateTrigger();
        });
      }

      const menu = node('div', 'playground-dropdown-menu playground-temporal-popover');
      menu.id = `playground-picker-${++dropdownSequence}`;
      menu.dataset.temporalKind = kind;
      menu.setAttribute('role', 'dialog');
      menu.setAttribute('aria-label', localized(field.label));
      menu.hidden = true;
      trigger.setAttribute('aria-controls', menu.id);
      popup.append(menu);

      function parseDateParts(raw) {
        const m = /^(\d{4})-(\d{2})-(\d{2})/.exec(String(raw || '').trim());
        if (!m) return null;
        const year = Number(m[1]), month = Number(m[2]), day = Number(m[3]);
        if (month < 1 || month > 12 || day < 1 || day > 31) return null;
        return { year, month, day };
      }

      function parseTimeParts(raw) {
        const m = /^(\d{1,2}):(\d{2})(?::(\d{2}))?/.exec(String(raw || '').trim());
        if (!m) return null;
        const hour = Number(m[1]), minute = Number(m[2]), second = Number(m[3] || 0);
        if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return null;
        return { hour, minute, second };
      }

      function parseDateTimeParts(raw) {
        const text = String(raw || '').trim();
        const m = /^(\d{4})-(\d{2})-(\d{2})(?:[T ](\d{1,2}):(\d{2})(?::(\d{2}))?)?/.exec(text);
        if (!m) return null;
        const year = Number(m[1]), month = Number(m[2]), day = Number(m[3]);
        if (month < 1 || month > 12 || day < 1 || day > 31) return null;
        const hour = m[4] !== undefined ? Number(m[4]) : 0;
        const minute = m[5] !== undefined ? Number(m[5]) : 0;
        const second = m[6] !== undefined ? Number(m[6]) : 0;
        return {
          date: { year, month, day },
          time: { hour: Math.max(0, Math.min(23, hour)), minute: Math.max(0, Math.min(59, minute)), second }
        };
      }

      function todayParts() {
        const now = new Date();
        return { year: now.getFullYear(), month: now.getMonth() + 1, day: now.getDate() };
      }

      function formatIsoDate(d) {
        return `${d.year}-${pad2(d.month)}-${pad2(d.day)}`;
      }

      function formatIsoTime(t) {
        return `${pad2(t.hour)}:${pad2(t.minute)}:${pad2(t.second || 0)}`;
      }

      function firstDayNumber() {
        const day = current?.values?.firstDayOfWeek;
        if (day === 'Sunday') return 0;
        if (day === 'Saturday') return 6;
        if (day === 'Monday') return 1;
        if (day === 'Tuesday') return 2;
        if (day === 'Wednesday') return 3;
        if (day === 'Thursday') return 4;
        if (day === 'Friday') return 5;
        return zh ? 1 : 0;
      }

      function isTwelveHour() {
        return kind === 'time' && current?.values?.clockIdentifier === 'TwelveHourClock';
      }

      function timeEnabledInRange() {
        return kind !== 'datetime' || current?.values?.timeEnabled !== false;
      }

      function formatTriggerLabel() {
        const raw = String(input.value || '').trim();
        if (!raw) return '';
        if (kind === 'date') {
          const d = parseDateParts(raw);
          return d ? formatIsoDate(d) : raw;
        }
        if (kind === 'time') {
          const t = parseTimeParts(raw);
          if (!t) return raw;
          if (isTwelveHour()) {
            const h12 = pad2(t.hour % 12 || 12);
            const period = t.hour >= 12 ? (zh ? '下午' : 'PM') : (zh ? '上午' : 'AM');
            return zh ? `${period} ${h12}:${pad2(t.minute)}` : `${h12}:${pad2(t.minute)} ${period}`;
          }
          return `${pad2(t.hour)}:${pad2(t.minute)}`;
        }
        const dt = parseDateTimeParts(raw);
        if (!dt) return raw;
        const dateStr = formatIsoDate(dt.date);
        return timeEnabledInRange() ? `${dateStr} ${pad2(dt.time.hour)}:${pad2(dt.time.minute)}` : dateStr;
      }

      let stagedDate = null;
      let stagedTime = null;
      let viewYear = 2026;
      let viewMonth = 11;
      let calLevel = 'day'; // 'day' | 'month' | 'year'
      let openedInitialValue = '';
      let lastSyncedValue = input.value;

      function commitValue(nextValue) {
        lastSyncedValue = nextValue;
        if (input.value !== nextValue) {
          input.value = nextValue;
          input.dispatchEvent(new Event('change', { bubbles: true }));
        }
        updateTrigger();
      }

      function syncStagedFromInput() {
        const now = new Date();
        const fallbackDate = todayParts();
        const fallbackTime = { hour: now.getHours(), minute: 0, second: 0 };
        if (kind === 'date') {
          stagedDate = parseDateParts(input.value);
          const anchor = stagedDate || fallbackDate;
          viewYear = anchor.year;
          viewMonth = anchor.month;
        } else if (kind === 'time') {
          stagedTime = parseTimeParts(input.value) || { hour: 9, minute: 0, second: 0 };
        } else {
          const dt = parseDateTimeParts(input.value);
          stagedDate = dt ? dt.date : null;
          stagedTime = dt ? dt.time : { hour: 9, minute: 0, second: 0 };
          const anchor = stagedDate || fallbackDate;
          viewYear = anchor.year;
          viewMonth = anchor.month;
        }
        if (!stagedTime) stagedTime = fallbackTime;
      }

      function commitStaged(closeAfter = false) {
        if (kind === 'date') {
          if (stagedDate) commitValue(formatIsoDate(stagedDate));
        } else if (kind === 'time') {
          commitValue(formatIsoTime(stagedTime));
        } else {
          const datePart = stagedDate || todayParts();
          stagedDate = datePart;
          const timePart = timeEnabledInRange() ? stagedTime : { hour: 0, minute: 0, second: 0 };
          commitValue(`${formatIsoDate(datePart)}T${formatIsoTime(timePart)}`);
        }
        if (closeAfter) {
          close();
          trigger.focus();
        }
      }

      function monthLabelText(year, month) {
        if (zh) return `${year}年${month}月`;
        const months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
        return `${months[month - 1]} ${year}`;
      }

      function shortMonthLabel(month) {
        if (zh) return `${month}月`;
        return ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'][month - 1];
      }

      function stepMonth(delta) {
        let total = viewYear * 12 + (viewMonth - 1) + delta;
        viewYear = Math.max(1900, Math.min(2199, Math.floor(total / 12)));
        viewMonth = ((total % 12) + 12) % 12 + 1;
      }

      function buildCalendarPane() {
        const pane = node('div', 'playground-cal-pane');
        const calHeader = node('div', 'playground-cal-header');
        const titleBtn = node('button', 'playground-cal-title-btn');
        titleBtn.type = 'button';
        const decadeStart = Math.floor(viewYear / 10) * 10;
        titleBtn.textContent = calLevel === 'day'
          ? monthLabelText(viewYear, viewMonth)
          : calLevel === 'month'
          ? (zh ? `${viewYear}年` : String(viewYear))
          : `${decadeStart} - ${decadeStart + 9}`;
        titleBtn.setAttribute('aria-label', zh ? '切换日历视图层级' : 'Zoom out calendar level');
        titleBtn.addEventListener('click', event => {
          event.stopPropagation();
          if (calLevel === 'day') calLevel = 'month';
          else if (calLevel === 'month') calLevel = 'year';
          else calLevel = 'day';
          renderMenu();
        });

        const nav = node('div', 'playground-cal-nav');
        const prevBtn = node('button', 'playground-cal-nav-btn');
        prevBtn.type = 'button';
        prevBtn.setAttribute('aria-label', zh ? '上一页' : 'Previous');
        prevBtn.innerHTML = '<svg viewBox="0 0 12 12" width="11" height="11" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 7.5L6 4l3.5 3.5"/></svg>';
        prevBtn.addEventListener('click', event => {
          event.stopPropagation();
          if (calLevel === 'day') stepMonth(-1);
          else if (calLevel === 'month') viewYear = Math.max(1900, viewYear - 1);
          else viewYear = Math.max(1900, viewYear - 10);
          renderMenu();
        });

        const nextBtn = node('button', 'playground-cal-nav-btn');
        nextBtn.type = 'button';
        nextBtn.setAttribute('aria-label', zh ? '下一页' : 'Next');
        nextBtn.innerHTML = '<svg viewBox="0 0 12 12" width="11" height="11" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 4.5L6 8l3.5-3.5"/></svg>';
        nextBtn.addEventListener('click', event => {
          event.stopPropagation();
          if (calLevel === 'day') stepMonth(1);
          else if (calLevel === 'month') viewYear = Math.min(2199, viewYear + 1);
          else viewYear = Math.min(2190, viewYear + 10);
          renderMenu();
        });

        nav.append(prevBtn, nextBtn);
        calHeader.append(titleBtn, nav);
        pane.append(calHeader);

        const today = todayParts();
        const calBody = node('div', 'playground-cal-body');
        if (calLevel === 'day') {
          const weekRow = node('div', 'playground-cal-weekdays');
          const dayNames = zh ? ['日', '一', '二', '三', '四', '五', '六'] : ['Su', 'Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa'];
          const startDow = firstDayNumber();
          for (let col = 0; col < 7; ++col) {
            const cell = node('span', 'playground-cal-weekday', dayNames[(startDow + col) % 7]);
            weekRow.append(cell);
          }

          const grid = node('div', 'playground-cal-grid');
          const firstOfMonthDow = new Date(viewYear, viewMonth - 1, 1).getDay();
          const lead = (firstOfMonthDow - startDow + 7) % 7;
          for (let i = 0; i < 42; ++i) {
            const cellDate = new Date(viewYear, viewMonth - 1, 1 - lead + i);
            const cy = cellDate.getFullYear();
            const cm = cellDate.getMonth() + 1;
            const cd = cellDate.getDate();
            const outside = cy !== viewYear || cm !== viewMonth;
            const isToday = cy === today.year && cm === today.month && cd === today.day;
            const isSelected = Boolean(stagedDate && cy === stagedDate.year && cm === stagedDate.month && cd === stagedDate.day);

            const dayBtn = node('button', 'playground-cal-day');
            dayBtn.type = 'button';
            dayBtn.textContent = String(cd);
            if (outside) dayBtn.classList.add('is-outside');
            if (isToday) dayBtn.classList.add('is-today');
            if (isSelected) dayBtn.classList.add('is-selected');
            dayBtn.setAttribute('aria-pressed', String(isSelected));
            dayBtn.setAttribute('aria-label', `${cy}-${pad2(cm)}-${pad2(cd)}`);
            dayBtn.addEventListener('click', event => {
              event.stopPropagation();
              stagedDate = { year: cy, month: cm, day: cd };
              viewYear = cy;
              viewMonth = cm;
              if (kind === 'date') {
                commitStaged(true);
              } else {
                commitStaged(false);
                renderMenu();
              }
            });
            grid.append(dayBtn);
          }
          calBody.append(weekRow, grid);
        } else {
          const grid = node('div', 'playground-cal-content-grid');
          if (calLevel === 'month') {
            for (let m = 1; m <= 12; ++m) {
              const btn = node('button', 'playground-cal-content-cell', shortMonthLabel(m));
              btn.type = 'button';
              if (viewYear === today.year && m === today.month) btn.classList.add('is-today');
              if (stagedDate && viewYear === stagedDate.year && m === stagedDate.month) btn.classList.add('is-selected');
              btn.addEventListener('click', event => {
                event.stopPropagation();
                viewMonth = m;
                calLevel = 'day';
                renderMenu();
              });
              grid.append(btn);
            }
          } else {
            const startYear = decadeStart - 1;
            for (let i = 0; i < 12; ++i) {
              const y = startYear + i;
              const btn = node('button', 'playground-cal-content-cell', String(y));
              btn.type = 'button';
              if (y < decadeStart || y > decadeStart + 9) btn.classList.add('is-outside');
              if (y === today.year) btn.classList.add('is-today');
              if (stagedDate && y === stagedDate.year) btn.classList.add('is-selected');
              btn.addEventListener('click', event => {
                event.stopPropagation();
                viewYear = y;
                calLevel = 'month';
                renderMenu();
              });
              grid.append(btn);
            }
          }
          calBody.append(grid);
        }
        pane.append(calBody);
        return pane;
      }

      function buildWheelPane() {
        const pane = node('div', 'playground-wheel-pane');
        const minuteStep = kind === 'time' ? Math.max(1, Number(current?.values?.minuteIncrement) || 1) : 1;
        const minuteItems = [];
        for (let m = 0; m < 60; m += minuteStep) minuteItems.push(m);
        let minuteIdx = 0;
        let bestDiff = 999;
        minuteItems.forEach((m, idx) => {
          const diff = Math.abs(m - stagedTime.minute);
          if (diff < bestDiff) { bestDiff = diff; minuteIdx = idx; }
        });

        const columnsHost = node('div', 'playground-wheel-columns');
        const highlightBand = node('div', 'playground-wheel-highlight');
        highlightBand.setAttribute('aria-hidden', 'true');
        columnsHost.append(highlightBand);

        const use12h = isTwelveHour();
        const cols = [];
        if (use12h) {
          const hour12Idx = stagedTime.hour % 12; // 0..11, maps 0 -> 12
          const periodIdx = stagedTime.hour >= 12 ? 1 : 0;
          cols.push({
            label: zh ? '时' : 'Hour',
            items: Array.from({ length: 12 }, (_, idx) => String(idx === 0 ? 12 : idx)),
            index: hour12Idx,
            wrap: true,
            onStep(nextIdx) {
              const isPm = stagedTime.hour >= 12;
              stagedTime.hour = (nextIdx % 12) + (isPm ? 12 : 0);
            }
          });
          cols.push({
            label: zh ? '分' : 'Min',
            items: minuteItems.map(pad2),
            index: minuteIdx,
            wrap: true,
            onStep(nextIdx) {
              stagedTime.minute = minuteItems[nextIdx];
            }
          });
          cols.push({
            label: zh ? '时段' : 'AM/PM',
            items: zh ? ['上午', '下午'] : ['AM', 'PM'],
            index: periodIdx,
            wrap: false,
            onStep(nextIdx) {
              const base = stagedTime.hour % 12;
              stagedTime.hour = nextIdx === 1 ? base + 12 : base;
            }
          });
        } else {
          cols.push({
            label: zh ? '时' : 'Hour',
            items: Array.from({ length: 24 }, (_, idx) => pad2(idx)),
            index: stagedTime.hour,
            wrap: true,
            onStep(nextIdx) {
              stagedTime.hour = nextIdx;
            }
          });
          cols.push({
            label: zh ? '分' : 'Min',
            items: minuteItems.map(pad2),
            index: minuteIdx,
            wrap: true,
            onStep(nextIdx) {
              stagedTime.minute = minuteItems[nextIdx];
            }
          });
        }

        const headerRow = node('div', 'playground-wheel-headers');
        cols.forEach((col, colIndex) => {
          headerRow.append(node('span', 'playground-wheel-header', col.label));
          if (colIndex > 0) {
            columnsHost.append(node('div', 'playground-wheel-divider'));
          }
          const colEl = node('div', 'playground-wheel-col');
          colEl.tabIndex = 0;
          colEl.setAttribute('role', 'spinbutton');
          colEl.setAttribute('aria-label', col.label);
          colEl.setAttribute('aria-valuetext', col.items[col.index] || '');

          const shiftCol = delta => {
            const len = col.items.length;
            let nextIdx = col.index + delta;
            if (col.wrap) {
              nextIdx = ((nextIdx % len) + len) % len;
            } else {
              if (nextIdx < 0 || nextIdx >= len) return;
            }
            if (nextIdx === col.index) return;
            col.onStep(nextIdx);
            commitStaged(false);
            renderMenu();
          };

          const upBtn = node('button', 'playground-wheel-step is-up');
          upBtn.type = 'button';
          upBtn.tabIndex = -1;
          upBtn.setAttribute('aria-label', `${col.label} -`);
          upBtn.innerHTML = '<svg viewBox="0 0 12 12" width="10" height="10" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 7.5L6 4l3.5 3.5"/></svg>';
          upBtn.disabled = !col.wrap && col.index <= 0;
          upBtn.addEventListener('click', event => {
            event.stopPropagation();
            shiftCol(-1);
          });
          colEl.append(upBtn);

          for (let offset = -3; offset <= 3; ++offset) {
            const len = col.items.length;
            const rawIdx = col.index + offset;
            const targetIdx = col.wrap ? ((rawIdx % len) + len) % len : rawIdx;
            const valid = targetIdx >= 0 && targetIdx < len;
            const rowBtn = node('button', 'playground-wheel-row');
            rowBtn.type = 'button';
            rowBtn.tabIndex = -1;
            rowBtn.dataset.offset = String(Math.abs(offset));
            if (offset === 0) rowBtn.classList.add('is-active');
            if (!valid) {
              rowBtn.classList.add('is-empty');
              rowBtn.disabled = true;
            } else {
              rowBtn.textContent = col.items[targetIdx];
              rowBtn.addEventListener('click', event => {
                event.stopPropagation();
                if (offset !== 0) shiftCol(offset);
              });
            }
            colEl.append(rowBtn);
          }

          const downBtn = node('button', 'playground-wheel-step is-down');
          downBtn.type = 'button';
          downBtn.tabIndex = -1;
          downBtn.setAttribute('aria-label', `${col.label} +`);
          downBtn.innerHTML = '<svg viewBox="0 0 12 12" width="10" height="10" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><path d="M2.5 4.5L6 8l3.5-3.5"/></svg>';
          downBtn.disabled = !col.wrap && col.index >= col.items.length - 1;
          downBtn.addEventListener('click', event => {
            event.stopPropagation();
            shiftCol(1);
          });
          colEl.append(downBtn);

          let wheelAccum = 0;
          colEl.addEventListener('wheel', event => {
            event.preventDefault();
            event.stopPropagation();
            const delta = event.deltaMode === 1 ? event.deltaY * 32 : event.deltaY;
            wheelAccum += delta;
            if (Math.abs(wheelAccum) < 28) return;
            const step = wheelAccum > 0 ? 1 : -1;
            wheelAccum = 0;
            shiftCol(step);
          }, { passive: false });

          colEl.addEventListener('keydown', event => {
            if (event.key === 'ArrowUp') {
              event.preventDefault();
              event.stopPropagation();
              shiftCol(-1);
            } else if (event.key === 'ArrowDown') {
              event.preventDefault();
              event.stopPropagation();
              shiftCol(1);
            } else if (event.key === 'Enter' || event.key === ' ') {
              event.preventDefault();
              event.stopPropagation();
              commitStaged(true);
            }
          });

          columnsHost.append(colEl);
        });

        pane.append(headerRow, columnsHost);
        return pane;
      }

      function renderMenu() {
        menu.replaceChildren();
        const showCalendar = kind === 'date' || kind === 'datetime';
        const showWheel = kind === 'time' || (kind === 'datetime' && timeEnabledInRange());

        const surface = node('div', 'playground-temporal-surface');
        if (showCalendar) surface.append(buildCalendarPane());
        if (showCalendar && showWheel) surface.append(node('div', 'playground-temporal-vdivider'));
        if (showWheel) surface.append(buildWheelPane());
        menu.append(surface);

        const footer = node('div', 'playground-temporal-footer');
        const leftActions = node('div', 'playground-temporal-footer-group');
        const rightActions = node('div', 'playground-temporal-footer-group');

        if (field.nullable) {
          const footerClear = node('button', 'playground-temporal-textbtn', zh ? '清除' : 'Clear');
          footerClear.type = 'button';
          footerClear.disabled = !input.value;
          footerClear.addEventListener('click', event => {
            event.stopPropagation();
            stagedDate = null;
            commitValue('');
            close();
            trigger.focus();
          });
          leftActions.append(footerClear);
        }

        if (showCalendar) {
          const todayBtn = node('button', 'playground-temporal-textbtn is-accent', zh ? '今天' : 'Today');
          todayBtn.type = 'button';
          todayBtn.addEventListener('click', event => {
            event.stopPropagation();
            const now = todayParts();
            stagedDate = now;
            viewYear = now.year;
            viewMonth = now.month;
            calLevel = 'day';
            if (kind === 'date') {
              commitStaged(true);
            } else {
              commitStaged(false);
              renderMenu();
            }
          });
          if (kind === 'date') rightActions.append(todayBtn);
          else leftActions.append(todayBtn);
        }

        if (showWheel) {
          const acceptBtn = node('button', 'playground-temporal-cmd is-accept');
          acceptBtn.type = 'button';
          acceptBtn.title = zh ? '确定' : 'Confirm';
          acceptBtn.setAttribute('aria-label', zh ? '确定' : 'Confirm');
          acceptBtn.innerHTML = '<svg viewBox="0 0 16 16" width="14" height="14" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M3 8.5L6.5 12L13 4.5"/></svg>';
          acceptBtn.addEventListener('click', event => {
            event.stopPropagation();
            commitStaged(true);
          });

          const cancelBtn = node('button', 'playground-temporal-cmd is-dismiss');
          cancelBtn.type = 'button';
          cancelBtn.title = zh ? '取消' : 'Cancel';
          cancelBtn.setAttribute('aria-label', zh ? '取消' : 'Cancel');
          cancelBtn.innerHTML = '<svg viewBox="0 0 16 16" width="14" height="14" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round"><path d="M4 4l8 8M12 4l-8 8"/></svg>';
          cancelBtn.addEventListener('click', event => {
            event.stopPropagation();
            commitValue(openedInitialValue);
            close();
            trigger.focus();
          });
          rightActions.append(acceptBtn, cancelBtn);
        }

        footer.append(leftActions, rightActions);
        menu.append(footer);
      }

      function positionPopover() {
        const rect = trigger.getBoundingClientRect();
        const margin = 8;
        menu.style.position = 'fixed';
        menu.style.zIndex = '1300';
        const mw = menu.offsetWidth || 280;
        const mh = menu.offsetHeight || 280;
        const left = Math.max(margin, Math.min(rect.left, window.innerWidth - mw - margin));
        const spaceBelow = window.innerHeight - rect.bottom;
        const top = (spaceBelow < mh + margin + 4 && rect.top > mh + margin + 4)
          ? rect.top - mh - 4
          : Math.max(margin, Math.min(rect.bottom + 4, window.innerHeight - mh - margin));
        menu.style.left = `${Math.round(left)}px`;
        menu.style.top = `${Math.round(top)}px`;
      }

      function updateTrigger() {
        const formatted = formatTriggerLabel();
        const empty = !formatted;
        triggerText.textContent = empty ? (zh ? '未设置' : 'Not set') : formatted;
        triggerText.classList.toggle('is-empty', empty);
        trigger.disabled = input.disabled;
        wrapper.classList.toggle('has-value', !empty);
        if (clearBtn) {
          clearBtn.hidden = empty || input.disabled;
          clearBtn.disabled = input.disabled;
        }
        const valueChangedExternally = input.value !== lastSyncedValue;
        lastSyncedValue = input.value;
        if (!menu.hidden && valueChangedExternally) {
          syncStagedFromInput();
          renderMenu();
          positionPopover();
        }
      }

      function open() {
        if (trigger.disabled || input.disabled) return;
        for (const dropdown of dropdowns) dropdown.close();
        openedInitialValue = input.value;
        lastSyncedValue = input.value;
        calLevel = 'day';
        syncStagedFromInput();
        renderMenu();
        menu.hidden = false;
        trigger.setAttribute('aria-expanded', 'true');
        positionPopover();
        document.addEventListener('pointerdown', onDocPointer, true);
        document.addEventListener('keydown', onDocKey, true);
        (menu.querySelector('.playground-cal-day.is-selected, .playground-wheel-col, button:not(:disabled)') || menu)?.focus({ preventScroll: true });
      }

      function close() {
        menu.hidden = true;
        trigger.setAttribute('aria-expanded', 'false');
        document.removeEventListener('pointerdown', onDocPointer, true);
        document.removeEventListener('keydown', onDocKey, true);
      }

      function onDocPointer(event) {
        if (!wrapper.contains(event.target) && !menu.contains(event.target)) close();
      }

      function onDocKey(event) {
        if (event.key === 'Escape') {
          event.preventDefault();
          event.stopPropagation();
          close();
          trigger.focus();
        }
      }

      trigger.addEventListener('click', event => {
        event.stopPropagation();
        if (menu.hidden) open(); else close();
      });
      trigger.addEventListener('keydown', event => {
        if (['ArrowDown', 'ArrowUp', 'Enter', ' '].includes(event.key)) {
          event.preventDefault();
          open();
        }
      });

      input.addEventListener('change', updateTrigger);
      updateTrigger();

      if (clearBtn) wrapper.append(trigger, clearBtn, input);
      else wrapper.append(trigger, input);

      const picker = { wrapper, trigger, menu, updateTrigger, close };
      dropdowns.add(picker);
      return picker;
    }


    function setPreviewState(state) {
      const available = state === 'ready', loading = state === 'loading';
      popup.dataset.previewState = state;
      stage.setAttribute('aria-busy', String(loading));
      previewState.hidden = available;
      previewSpinner.hidden = !loading;
      previewStateText.textContent = loading ? words.loading : available ? '' : words.unavailable;
      retry.hidden = state !== 'error';
      fieldsPlaceholder.hidden = codePlaceholder.hidden = !loading;
      categoryTabsBar.inert = extra.inert = !available;
      cppTab.disabled = pythonTab.disabled = !available;
      codePre.hidden = !available;
      if (!available) { codeText.textContent = ''; interactionFeedback.hidden = true; }
    }

    const sameExample = value => ['route', 'sample', 'state'].every(key => value?.[key] === transport.example()[key]);
    function send(command, pick = false) {
      if (command.operation === 'hover') requestedHover = command.hovered;
      else if (['reset', 'close', 'open'].includes(command.operation)) requestedHover = false;
      const id = ++issued;
      if (!['targets', 'close'].includes(command.operation)) {
        expected = id; clearTimeout(replyTimer);
        replyTimer = setTimeout(() => unavailable(), 5000);
      }
      if (liveFrame && liveReady) postLive(pick ? 'component-playground-pick' : 'component-playground', command, id);
      else transport.send(pick ? 'component-playground-pick' : 'component-playground', command, id, String(generation));
      busy();
    }
    function busy() {
      const waiting = expected > 0 || pending.size > 0;
      const unavailable = !ready || !current;
      if (liveFrame) liveFrame.style.pointerEvents = waiting || unavailable ? 'none' : '';
      copy.disabled = !ready || !current || waiting;
      reset.disabled = unavailable;
      if (stateTextToggle) stateTextToggle.disabled = unavailable || waiting;
      for (const entry of rows.values()) {
        const disabled = unavailable || entry.enabledByValue === false;
        entry.input.disabled = disabled;
        entry.input.querySelectorAll('input, button, select, textarea').forEach(input => { input.disabled = disabled || input.dataset.limitReached === 'true'; });
        if (entry.follow) entry.follow.disabled = unavailable;
        if (entry.dropdown) {
          if (disabled) entry.dropdown.close();
          entry.dropdown.updateTrigger();
        }
      }
      popup.setAttribute('aria-busy', String(waiting || (opened && popup.dataset.previewState === 'loading')));
    }
    function flush() {
      clearTimeout(timer);
      if (!pending.size || !opened || !ready || !current) return;
      const values = Object.fromEntries(pending); pending.clear(); send({operation: 'set', values});
    }
    function change(id, value, immediate = false) {
      if (!ready || !current) return;
      if (!current.schema.interactive && rows.get(id)?.field.previewInteraction === 'hover' && !requestedHover) {
        send({operation: 'hover', hovered: true});
      }
      pending.set(id, value);
      const pair = current.schema.stateTextPair;
      if (pair && separateStateText === false && (id === pair.on || id === pair.off)) {
        sharedTextValue = value;
        pending.set(pair.on, value); pending.set(pair.off, value);
      }
      expected = ++issued; busy(); clearTimeout(timer);
      if (immediate) flush(); else timer = setTimeout(flush, 100);
    }
    function clearPending() { clearTimeout(timer); clearTimeout(replyTimer); pending.clear(); expected = 0; }
    function dismiss(restoreFocus = true, notify = true) {
      const wasActive = opened;
      rows.forEach(entry => {
        entry.tableEditor?.clearDraft();
        if (entry.rowState) { entry.rowState.composing = false; entry.rowState.dirty = false; entry.rowState.forceSync = true; }
      });
      separateStateText = null; separateTextValues = null; sharedTextValue = null;
      clearPending(); opened = false; current = null; acceptedRevision = -1; applied = 0; ++generation;
      restoreBackground();
      for (const dropdown of dropdowns) dropdown.close();
      userMoved = false; userResized = false;
      popup.style.width = popup.style.height = '';
      initialPreviewHeight = 0; liveViewport = null;
      controlsPane.style.width = controlsPane.style.flex = '';
      extra.classList.remove('playground-extra-expanding', 'playground-extra-collapsing');
      more.classList.remove('playground-more-expanded');
      popup.hidden = true;
      backdrop.hidden = true;
      popup.setAttribute('aria-modal', 'false');
      transport.updateTrigger(false);
      if (freshState) { const resolve = freshState; freshState = null; resolve(false); }
      liveFrame?.remove(); liveFrame = null; liveReady = false; liveInitial = null; liveSession = '';
      previewContent.style.height = ''; stage.classList.remove('playground-live', 'playground-overlay-open');
      setPreviewState('loading');
      if (wasActive && ready && notify) {
        send({operation: 'close'}, true);
        if (restoreFocus) {
          const reclaim = () => {
            if (!opened && document.activeElement?.closest('.live-stage') && opener.isConnected)
              opener.focus({preventScroll: true});
          };
          window.addEventListener('blur', reclaim, {once: true});
          setTimeout(() => { window.removeEventListener('blur', reclaim); reclaim(); }, 250);
        }
      }
      if (wasActive && restoreFocus) opener.focus({preventScroll: true});
    }
    function openPreview(command) {
      if (!ready) return;
      dismiss(false);
      opened = true;
      backdrop.hidden = false;
      popup.hidden = false;
      popup.setAttribute('aria-modal', 'true');
      transport.updateTrigger(true);
      current = null; acceptedRevision = -1;
      const firstCategory = schemaBuilt && categoryTabButtons.get('basic').hidden
        ? [...categoryTabButtons.keys()].find(id => !categoryTabButtons.get(id).hidden) : 'basic';
      switchCategoryTab(firstCategory);
      showMobileView('controls');
      status.hidden = true; status.textContent = ''; setPreviewState('loading');
      themeChanged(); position(); close.focus({preventScroll: true});
      isolateBackground();
      backgroundObserver.observe(document.body, {childList: true});
      // Generic controls are initialized from their own fixture. Only Button borrows source values.
      if (root.dataset.component !== 'button') { beginLive(); return; }
      const target = command?.target !== undefined ? command.target : (root.dataset.component === 'button' ? 1 : (targets[0]?.id ?? 0));
      send({operation: 'open', target}, true);
    }
    function receiveTargets(state) {
      if (!sameExample(state?.example) || !Array.isArray(state.targets)) return;
      targets = state.targets;
    }
    function updateModelOptions(input, choices) {
      const signature = JSON.stringify(choices);
      if (input.dataset.choices === signature) return false;
      const value = input.value;
      input.replaceChildren();
      for (const choice of choices) {
        const label = choice.value === -1 ? (zh ? '未选择' : 'No selection')
          : choice.label?.page ? (zh ? `第 ${choice.label.page} 页` : `Page ${choice.label.page}`)
          : typeof choice.label === 'string' ? choice.label : localized(choice.label);
        const option = new Option(label, String(choice.value));
        option.disabled = choice.enabled === false;
        input.append(option);
      }
      input.value = value;
      input.dataset.choices = signature;
      return true;
    }
    function makeTableEditor(field) {
      const host = node('div', 'playground-data-editor');
      host.setAttribute('role', 'group'); host.setAttribute('aria-label', localized(field.label));
      const viewport = node('div', 'playground-data-scroll');
      const table = node('table'), head = node('thead'), body = node('tbody');
      const heading = node('tr');
      if (field.fixedRows) heading.append(node('th', '', zh ? '节点' : 'Node'));
      (field.columns || []).forEach(column => heading.append(node('th', '', localized(column.label))));
      if (!field.fixedRows) { const th = node('th'); th.setAttribute('aria-label', zh ? '删除行' : 'Remove row'); heading.append(th); }
      head.append(heading); table.append(head, body); viewport.append(table); host.append(viewport);
      const add = node('button', 'playground-data-add', zh ? '添加一行' : 'Add row'); add.type = 'button';
      add.hidden = Boolean(field.fixedRows); host.append(add);
      const read = () => [...body.rows].map(row => [...row.querySelectorAll('input')].map((input, col) =>
        field.columns[col].type === 'number' ? (input.value === '' ? null : Number(input.value)) : input.value));
      function normalizeNumber(input, committed = false) {
        if (input.type !== 'number' || input.value === '' || !Number.isFinite(input.valueAsNumber)) return;
        // Keep unfinished fractions/exponents while typing; only remove redundant integer zeros.
        const value = committed ? String(input.valueAsNumber) : input.value.replace(/^(-?)0+(?=\d)/, '$1');
        if (value !== input.value) input.value = value;
      }
      function submit() {
        const values = read();
        const inputs = [...body.querySelectorAll('input')];
        inputs.forEach(input => input.setCustomValidity(''));
        let valid = inputs.every(input => input.checkValidity());
        if (Number.isInteger(field.ascendingColumn)) {
          const col = field.ascendingColumn;
          values.forEach((row, i) => {
            if (i && row[col] < values[i - 1][col]) {
              body.rows[i].querySelectorAll('input')[col].setCustomValidity(zh ? 'X 值应按从小到大排列' : 'Keep X values in ascending order'); valid = false;
            }
          });
        }
        if (!valid) { inputs.find(input => !input.checkValidity())?.reportValidity(); return; }
        change(field.id, values);
      }
      add.addEventListener('click', () => {
        const values = read();
        const row = field.columns.map((column, i) => column.type === 'number'
          ? (i === field.ascendingColumn && values.length ? Number(values.at(-1)[i]) + 1 : 0) : '');
        values.push(row); change(field.id, values, true);
      });
      function update(values, resolved) {
        field = resolved;
        if (body.rows.length !== values.length) {
          body.replaceChildren();
          values.forEach((values, rowIndex) => {
            const tr = node('tr');
            if (field.fixedRows) tr.append(node('td', 'playground-data-row-label'));
            field.columns.forEach((column, col) => {
              const td = node('td'), input = node('input', 'playground-input');
              input.type = column.type === 'number' ? 'number' : 'text';
              if (input.type === 'number') { input.step = 'any'; input.required = !column.nullable; }
              else input.maxLength = 160;
              input.dataset.row = rowIndex; input.dataset.column = col;
              input.addEventListener('compositionstart', () => { input.dataset.composing = 'true'; });
              input.addEventListener('compositionend', () => {
                delete input.dataset.composing;
                normalizeNumber(input);
                input.dataset.dirty = 'true';
                submit();
              });
              input.addEventListener('input', event => {
                input.dataset.dirty = 'true';
                if (event?.isComposing || input.dataset.composing === 'true') return;
                normalizeNumber(input);
                submit();
              });
              input.addEventListener('keydown', event => {
                if (event.key !== 'Enter' || event.isComposing || input.dataset.composing === 'true' || event.keyCode === 229) return;
                event.preventDefault();
                normalizeNumber(input, true);
                submit();
                flush();
              });
              input.addEventListener('blur', () => {
                delete input.dataset.composing;
                normalizeNumber(input, true);
                if (input.dataset.dirty === 'true') submit();
                flush();
              });
              td.append(input); tr.append(td);
            });
            if (!field.fixedRows) {
              const td = node('td'), remove = action(zh ? `删除第 ${rowIndex + 1} 行` : `Remove row ${rowIndex + 1}`, 'close', 'playground-data-remove');
              remove.dataset.row = rowIndex;
              remove.addEventListener('click', () => { const rows = read(); rows.splice(rowIndex, 1); change(field.id, rows, true); });
              td.append(remove); tr.append(td);
            }
            body.append(tr);
          });
        }
        values.forEach((row, rowIndex) => {
          const label = body.rows[rowIndex].querySelector('.playground-data-row-label');
          if (label) { label.textContent = field.rowLabels?.[rowIndex] || String(rowIndex + 1); label.title = label.textContent; }
        });
        values.forEach((values, rowIndex) => [...body.rows[rowIndex].querySelectorAll('input')].forEach((input, col) => {
          const value = values[col] === null ? '' : String(values[col]);
          if (input.value === value) delete input.dataset.dirty;
          if (input.value !== value && input.dataset.dirty !== 'true' && input.dataset.composing !== 'true' && document.activeElement !== input) {
            input.value = value;
          }
          input.setAttribute('aria-label', `${localized(field.label)} · ${field.rowLabels?.[rowIndex] || rowIndex + 1} · ${localized(field.columns[col].label)}`);
        }));
        add.disabled = values.length >= (field.maximumRows || 32);
        add.dataset.limitReached = String(add.disabled);
      }
      const clearDraft = () => host.querySelectorAll('input').forEach(input => { delete input.dataset.dirty; delete input.dataset.composing; input.setCustomValidity(''); });
      return {host, update, clearDraft};
    }
    function makeField(field) {
      const quick = ['text', 'fluentStyle', 'fluentSize'].includes(field.id);
      const row = node('div', `playground-field${quick ? ' playground-quick-field' : ''}`); row.dataset.property = field.id;
      const label = node('label', '', localized(field.label)); label.htmlFor = `playground-${field.id}`; row.append(label);
      label.id = `playground-label-${field.id}`;
      if (field.id === 'checkable') {
        label.title = zh ? '开启后变为开关按钮（Toggle Button），通过下方“已选中”属性查看效果' : 'Enables toggle button behavior. Toggle "Checked" below to see the active state.';
      } else if (field.id === 'focusVisual') {
        label.title = zh ? '通过 Tab 或点击让组件获得焦点，观察焦点边框。' : 'Focus the component with Tab or a click to inspect its focus ring.';
      }
      let input, output, follow, dropdown, colorSwatch, tableEditor, isTextInput = false;
      const rowState = {composing: false, dirty: false, forceSync: false};
      function toCssColor(raw) {
        const s = String(raw || '').trim();
        const m8 = /^#([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})([0-9a-fA-F]{2})$/.exec(s);
        if (m8) return `rgba(${parseInt(m8[2], 16)}, ${parseInt(m8[3], 16)}, ${parseInt(m8[4], 16)}, ${(parseInt(m8[1], 16) / 255).toFixed(3)})`;
        if (/^#([0-9a-fA-F]{3}|[0-9a-fA-F]{6})$/.test(s)) return s;
        return 'transparent';
      }
      if (field.type === 'table') {
        tableEditor = makeTableEditor(field); input = tableEditor.host; row.classList.add('playground-field-table');
      } else if (field.type === 'multi-enum') {
        input = node('div', 'playground-multi-options');
        input.setAttribute('role', 'group'); input.setAttribute('aria-label', localized(field.label));
        field.options.forEach(option => {
          const item = node('label'); const checkbox = node('input'); checkbox.type = 'checkbox'; checkbox.value = option;
          checkbox.addEventListener('change', () => {
            if (rows.get(field.id)?.field.singleSelection && checkbox.checked) input.querySelectorAll('input').forEach(item => { item.checked = item === checkbox; });
            change(field.id, [...input.querySelectorAll('input:checked')].map(c => c.value), true);
          });
          item.append(checkbox, document.createTextNode(optionLabel(field, option))); input.append(item);
        });
      } else if ((quick && field.type === 'enum') || (field.type === 'boolean' && field.control === 'segments')) {
        input = node('div', 'playground-segments'); input.setAttribute('role', 'radiogroup'); input.setAttribute('aria-label', localized(field.label));
        const choices = field.type === 'boolean' ? [false, true] : field.options;
        choices.forEach(option => {
          const text = localized(field.valueLabels?.[String(option)]) || words.options[option] || String(option);
          const b = action(text); b.textContent = text; b.dataset.value = String(option);
          b.setAttribute('role', 'radio'); b.addEventListener('click', () => change(field.id, option, true));
          b.addEventListener('keydown', event => {
            if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
            event.preventDefault(); const buttons = [...input.children], index = buttons.indexOf(b);
            const next = event.key === 'Home' ? 0 : event.key === 'End' ? buttons.length - 1 : (index + (event.key === 'ArrowRight' ? 1 : -1) + buttons.length) % buttons.length;
            buttons[next].focus(); change(field.id, choices[next], true);
          }); input.append(b);
        });
      } else if (field.type === 'enum' || field.control === 'model-select' || field.control === 'select') {
        input = node('select', 'playground-input');
        if (field.control === 'model-select') updateModelOptions(input, field.choices || []);
        else field.options.forEach(value => input.append(new Option(optionLabel(field, value), value)));
        input.addEventListener('change', () => {
          if (field.previewOnly) send({operation: 'hover', hovered: input.value === 'Hover'});
          else change(field.id, field.control === 'model-select' ? Number(input.value) : input.value, true);
        });
        dropdown = createFluentDropdown(input, {});
        dropdown.trigger.setAttribute('aria-labelledby', label.id);
        label.addEventListener('click', event => { event.preventDefault(); dropdown.trigger.focus(); });
      } else {
        input = node(field.control === 'textarea' || field.type === 'string-list' ? 'textarea' : 'input', 'playground-input');
        if (field.type === 'boolean') input.type = 'checkbox';
        else if (['color', 'date', 'time', 'datetime'].includes(field.type)) {
          input.type = 'text';
          input.autocomplete = 'off';
          if (field.type === 'color') colorSwatch = node('span', 'playground-color-swatch');
        }
        else if (field.type === 'string-list') { input.rows = 3; input.maxLength = (field.maximumLength + 1) * field.maximumItems; }
        else if (field.type === 'text') { if (input.tagName === 'INPUT') input.type = 'text'; input.maxLength = field.maximumLength; input.autocomplete = 'off'; }
        else {
          const isRange = field.control === 'range' || ['iconRotation', 'iconScale', 'iconOffsetX', 'iconOffsetY'].includes(field.id);
          input.type = isRange ? 'range' : 'number'; input.min = field.minimum; input.max = field.maximum; input.step = field.step;
          if (isRange) output = node('output', 'playground-number');
        }
        isTextInput = input.tagName === 'TEXTAREA' || (input.tagName === 'INPUT' && !['checkbox', 'radio', 'range'].includes(input.type));
        const liveInput = ['text', 'string-list'].includes(field.type) || input.type === 'range';
        const applyInput = (immediate = !liveInput) => {
          let value = field.type === 'boolean' ? input.checked : input.value;
          if ((field.type === 'date' || field.emptyValue) && field.nullable && value === '') value = null;
          if (field.type === 'string-list') value = parseStringList(input.value);
          if (field.type === 'number' || input.type === 'range') {
            if (value !== null && (value === '' || !Number.isFinite(Number(value)))) { input.setAttribute('aria-invalid', 'true'); status.hidden = false; status.textContent = words.numeric; return false; }
            if (value !== null) value = Number(value);
          }
          rowState.dirty = false;
          if (colorSwatch) colorSwatch.style.setProperty('--swatch-color', toCssColor(value));
          input.removeAttribute('aria-invalid');
          if (output) {
            let display = String(value);
            if (field.id === 'iconRotation') display = `${value}°`;
            else if (field.id === 'iconScale') display = `${value}x`;
            else if (field.id === 'iconOffsetX' || field.id === 'iconOffsetY') display = `${value}px`;
            output.value = display;
          }
          change(field.id, value, immediate);
          return true;
        };
        input.addEventListener(liveInput ? 'input' : 'change', event => {
          if (event?.isComposing || rowState.composing) return;
          applyInput();
        });
        if (isTextInput) {
          input.addEventListener('compositionstart', () => { rowState.composing = true; });
          input.addEventListener('compositionend', () => {
            rowState.composing = false;
            if (liveInput) applyInput();
            else rowState.dirty = true;
          });
          if (!liveInput) input.addEventListener('input', () => { rowState.dirty = true; });
          if (input.tagName === 'INPUT') {
            input.addEventListener('keydown', event => {
              if (event.key !== 'Enter' || event.isComposing || rowState.composing || event.keyCode === 229) return;
              event.preventDefault();
              if (!liveInput) rowState.forceSync = true;
              if (applyInput(true)) flush();
            });
          }
        }
        input.addEventListener('blur', () => {
          rowState.composing = false;
          if (!liveInput && rowState.dirty) applyInput(true);
          flush();
          if (!pending.has(field.id) && !expected && current?.values && field.id in current.values) {
            if (field.type === 'string-list' && Array.isArray(current.values[field.id])) {
              const canonical = current.values[field.id].join('\n');
              if (sameStringList(parseStringList(input.value), current.values[field.id]) && input.value !== canonical) {
                input.value = canonical;
              }
            } else if (isTextInput && input.getAttribute('aria-invalid') !== 'true') {
              const val = current.values[field.id];
              const eff = val === null ? (field.type === 'date' || field.emptyValue ? '' : current.effective?.[field.id] ?? field.minimum) : val;
              if (input.value !== String(eff)) input.value = String(eff);
              rowState.dirty = false;
            }
          }
        });
        if (['date', 'time', 'datetime'].includes(field.type)) {
          dropdown = createFluentTemporalPicker(input, field);
          dropdown.trigger.setAttribute('aria-labelledby', label.id);
          label.addEventListener('click', event => { event.preventDefault(); dropdown.trigger.focus(); });
        }
      }
      input.id = label.htmlFor;
      if (field.description) {
        const descText = localized(field.description);
        label.title = descText;
        row.title = descText;
      }
      const control = node('div', 'playground-field-control'); if (colorSwatch) control.append(colorSwatch); control.append(dropdown ? dropdown.wrapper : input); if (output) control.append(output);
      if (field.nullable && field.type !== 'date' && !field.emptyValue) {
        row.classList.add('playground-field-nullable');
        const header = node('div', 'playground-nullable-header');
        const wrapper = node('label', 'playground-follow'); follow = node('input'); follow.type = 'checkbox'; follow.dataset.followTheme = field.id;
        follow.setAttribute('aria-label', `${localized(field.label)}: ${words.follow}`); wrapper.append(follow, document.createTextNode(words.follow));
        header.append(label, wrapper);
        row.append(header, control);
        follow.addEventListener('change', () => {
          const fallback = field.type === 'number' ? Number(input.value) : input.value;
          change(field.id, follow.checked ? null : fallback, true);
        });
      } else {
        row.append(label, control);
      }
      let hint;
      if (field.effectHint || field.disabledHint || field.previewInteraction || field.type === 'number') {
        hint = node('p', 'playground-field-note'); hint.id = `${input.id}-note`; hint.hidden = true;
        row.append(hint);
      }
      rows.set(field.id, {row, label, input, output, follow, field, dropdown, colorSwatch, toCssColor, hint, tableEditor, isTextInput, rowState});
    }
    function updateStateTextEditor(state) {
      const pair = state.schema.stateTextPair;
      if (!pair || !stateTextToggle) return;
      if (separateStateText === null) separateStateText = state.values[pair.on] !== state.values[pair.off];
      const on = rows.get(pair.on), off = rows.get(pair.off);
      on.label.textContent = separateStateText ? localized(on.field.label) : (zh ? '显示文字' : 'Display text');
      on.row.hidden = false;
      off.row.hidden = !separateStateText;
      off.enabledByValue = separateStateText;
      off.input.disabled = !separateStateText;
      stateTextToggle.textContent = separateStateText
        ? (zh ? '改用相同文字' : 'Use the same text') : (zh ? '按状态分别设置' : 'Set different text for each state');
      stateTextToggle.setAttribute('aria-expanded', String(separateStateText));
    }
    function toggleStateTextEditor() {
      if (!current) return;
      const pair = current.schema.stateTextPair;
      const values = {...current.values, ...Object.fromEntries(pending)};
      if (separateStateText) {
        separateTextValues = {[pair.on]: values[pair.on], [pair.off]: values[pair.off]};
        const active = values[pair.state] ? pair.on : pair.off;
        sharedTextValue ??= values[active] || values[pair.on] || values[pair.off] || '';
        separateStateText = false;
        change(pair.on, sharedTextValue, true);
      } else {
        sharedTextValue = values[pair.on];
        separateStateText = true;
        const restored = separateTextValues || {[pair.on]: sharedTextValue, [pair.off]: sharedTextValue};
        change(pair.on, restored[pair.on]); change(pair.off, restored[pair.off], true);
      }
      updateStateTextEditor(current);
    }
    function receive(state) {
      if (!opened || popup.dataset.previewState === 'error' || state?.selection !== String(generation) || !sameExample(state.example)) return;
      if (state.error && !state.preview) { unavailable(); return; }
      if (!state.preview?.live && liveFrame) return;
      if (state.requestId > issued || (state.requestId && (state.requestId < expected || state.requestId < applied)) || (!state.requestId && expected)) return;
      if (!state.values || !Array.isArray(state.modified) || !state.preview) { if (state.error) unavailable(); return; }
      if (state.revision < acceptedRevision) return;
      if (!state.preview.live) { beginLive(state); return; }
      if (!schemaBuilt) {
        if (state.schema?.version !== 1 || state.schema.component !== root.dataset.component || !Array.isArray(state.schema.properties)) return;
        state.schema.properties.forEach(makeField);
        titleNode.textContent = state.schema.title || state.schema.component;
        titleNode.title = state.schema.description ? localized(state.schema.description) : '';
        stage.title = titleNode.title;
        const isButton = state.schema.component === 'button';
        popup.classList.toggle('playground-generic', !isButton);
        stage.classList.toggle('playground-preview-large', !isButton);
        const iconFields = state.schema.properties.filter(field => field.category === 'icon');
        const hasIconLayout = iconFields.some(field => field.id === 'fluentLayout');
        const hasIconContent = iconFields.some(field => /icon|glyph|chevron/i.test(field.id));
        categoryTabButtons.get('icon').textContent = hasIconLayout ? (zh ? '图标与布局' : 'Icon & Layout')
          : hasIconContent ? (zh ? '图标' : 'Icons') : (zh ? '布局' : 'Layout');
        (state.schema.categories || []).forEach(category => { if (categoryTabButtons.has(category.id)) categoryTabButtons.get(category.id).textContent = localized(category.label); });
        categoryTabPanels.forEach(panel => panel.replaceChildren());
        const groupNames = {
          'primary-icon': state.schema.properties.some(field => field.group === 'dropdown-indicator')
            ? (zh ? '主图标' : 'Main icon') : (zh ? '图标' : 'Icon'),
          'dropdown-indicator': zh ? '下拉箭头' : 'Dropdown indicator',
          'query-icon': zh ? '查询图标' : 'Query icon'
        };
        const categoryGroups = new Map();
        state.schema.properties.forEach(field => {
          const panel = categoryTabPanels.get(field.category || 'basic') || categoryTabPanels.get('basic');
          if (!categoryGroups.has(panel)) categoryGroups.set(panel, new Set());
          if (field.group) categoryGroups.get(panel).add(field.group);
        });
        state.schema.properties.forEach(field => {
          const panel = categoryTabPanels.get(field.category || 'basic') || categoryTabPanels.get('basic');
          let container = panel;
          if (field.group) {
            if (!propertyGroups.has(field.group)) {
              const needsHeading = categoryGroups.get(panel).size > 1;
              const group = node(needsHeading ? 'fieldset' : 'div', 'playground-property-group'); group.dataset.group = field.group;
              if (needsHeading) group.append(node('legend', '', groupNames[field.group] || field.group));
              const fields = node('div', 'playground-group-fields');
              const hint = node('p', 'playground-field-note'); hint.hidden = true;
              hint.id = `playground-group-${field.group}-note`;
              group.append(fields, hint); panel.append(group);
              propertyGroups.set(field.group, {group, fields, hint});
            }
            container = propertyGroups.get(field.group).fields;
          }
          container.append(rows.get(field.id).row);
        });
        const pair = state.schema.stateTextPair;
        if (pair && rows.has(pair.on) && rows.has(pair.off)) {
          stateTextToggle = node('button', 'playground-text-mode'); stateTextToggle.type = 'button';
          stateTextToggle.setAttribute('aria-controls', rows.get(pair.off).input.id);
          stateTextToggle.addEventListener('click', toggleStateTextEditor);
          rows.get(pair.off).row.after(stateTextToggle);
        }
        categoryTabButtons.forEach((button, id) => { button.hidden = !categoryTabPanels.get(id).children.length; });
        if (!categoryTabPanels.get(activeCategoryTab)?.children.length) {
          const firstVisible = [...categoryTabButtons.keys()].find(id => !categoryTabButtons.get(id).hidden);
          if (firstVisible) switchCategoryTab(firstVisible);
        }

        schemaBuilt = true;
      }
      acceptedRevision = state.revision; current = state; expected = 0; setPreviewState('ready'); if (state.requestId) applied = state.requestId;
      if (freshState) { const resolve = freshState; freshState = null; resolve(!state.error); }
      const lastEvent = state.events?.at(-1);
      interactionFeedback.hidden = !lastEvent;
      if (lastEvent) interactionFeedback.textContent = zh
        ? `${lastEvent.name === 'clicked' ? '点击' : lastEvent.name === 'toggled' ? '切换' : lastEvent.name === 'browseRequested' ? '已触发选择文件请求' : lastEvent.name === 'filesDropped' ? '收到拖放文件' : lastEvent.name} · ${lastEvent.sequence}`
        : `${lastEvent.name} · ${lastEvent.sequence}`;
      requestedHover = Boolean(state.hovered);
      clearTimeout(replyTimer); retry.hidden = true; status.hidden = !state.error; status.textContent = state.error ? words.invalid : '';

      function isFieldEnabled(field, values, effectiveValue) {
        if (effectiveValue === null) return false;
        const expr = field.enabledWhen;
        if (!expr) return true;
        if (expr === 'hasIcon' || expr === 'icon') return Boolean(values.icon && values.icon !== 'None');
        if (expr === 'stackedIcon') return Boolean(values.icon && values.icon !== 'None' && ['IconAbove', 'IconBelow'].includes(values.fluentLayout));
        if (expr === 'hasVisibleIcon') return Boolean(values.icon && values.icon !== 'None') && values.fluentLayout !== 'TextOnly';
        const tokens = [];
        for (let i = 0; i < expr.length;) {
          const ch = expr[i];
          if (/\s/.test(ch)) { i += 1; continue; }
          if (expr.startsWith('&&', i) || expr.startsWith('||', i) || expr.startsWith('==', i) || expr.startsWith('!=', i) || expr.startsWith('<=', i) || expr.startsWith('>=', i)) {
            tokens.push({ t: expr.slice(i, i + 2) }); i += 2; continue;
          }
          if ('!()<>'.includes(ch)) { tokens.push({ t: ch }); i += 1; continue; }
          if (ch === '\'' || ch === '"') {
            let j = i + 1, s = '';
            while (j < expr.length && expr[j] !== ch) { if (expr[j] === '\\' && j + 1 < expr.length) j += 1; s += expr[j]; j += 1; }
            tokens.push({ t: 'lit', v: s }); i = j + 1; continue;
          }
          if (/[-0-9.]/.test(ch) && (ch !== '-' || /[0-9]/.test(expr[i + 1] || ''))) {
            let j = i + (ch === '-' ? 1 : 0);
            while (j < expr.length && /[0-9.]/.test(expr[j])) j += 1;
            tokens.push({ t: 'lit', v: Number(expr.slice(i, j)) }); i = j; continue;
          }
          let j = i;
          while (j < expr.length && /[A-Za-z0-9_.]/.test(expr[j])) j += 1;
          if (j === i) return Boolean(values[expr]);
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
          if (name === 'hasIcon' || name === 'icon') return Boolean(values.icon && values.icon !== 'None');
          if (name === 'stackedIcon') return Boolean(values.icon && values.icon !== 'None' && ['IconAbove', 'IconBelow'].includes(values.fluentLayout));
          if (name === 'hasVisibleIcon') return Boolean(values.icon && values.icon !== 'None') && values.fluentLayout !== 'TextOnly';
          if (name.endsWith('.length')) { const base = values[name.slice(0, -7)]; return base == null ? 0 : (base.length ?? 0); }
          return values[name];
        };
        const parsePrimary = () => {
          const tok = tokens[pos];
          if (!tok) return undefined;
          if (tok.t === '(') { pos += 1; const v = parseOr(); if (peek() === ')') pos += 1; return v; }
          if (tok.t === 'lit') { pos += 1; return tok.v; }
          if (tok.t === 'id') { pos += 1; return resolveId(tok.v); }
          return undefined;
        };
        const parseUnary = () => {
          if (peek() === '!') { pos += 1; return !parseUnary(); }
          return parsePrimary();
        };
        const parseCmp = () => {
          const left = parseUnary();
          const op = peek();
          if (op === '==' || op === '!=' || op === '<' || op === '<=' || op === '>' || op === '>=') {
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

      for (const [id, entry] of rows) {
        if (entry.field.previewOnly) {
          entry.input.value = state.hovered ? 'Hover' : 'Rest';
          entry.dropdown.updateTrigger();
          continue;
        }
        const field = state.schema.properties.find(field => field.id === id);
        if (field) entry.field = field;
        if (entry.field.control === 'model-select') {
          if (updateModelOptions(entry.input, field?.choices || [])) entry.dropdown.close();
        } else if ((entry.field.type === 'enum' || entry.field.control === 'select') && entry.input.tagName === 'SELECT') {
          const options = entry.field.options || [];
          if (JSON.stringify([...entry.input.options].map(option => option.value)) !== JSON.stringify(options)) {
            entry.input.replaceChildren(...options.map(value => new Option(optionLabel(entry.field, value), value)));
            entry.dropdown.close();
          }
        } else if (entry.field.type === 'number') {
          entry.input.min = entry.field.minimum;
          entry.input.max = entry.field.maximum;
          entry.input.step = entry.field.step;
        } else if (entry.field.type === 'text' && entry.input.tagName !== 'SELECT') {
          entry.input.maxLength = entry.field.maximumLength;
        }
        const value = state.values[id], effective = value === null
          ? (entry.field.type === 'date' || entry.field.emptyValue ? '' : state.effective?.[id] ?? entry.field.minimum) : value;
        if (entry.field.type === 'table') {
          entry.tableEditor.update(value, entry.field);
        } else if (entry.field.type === 'multi-enum') {
          const options = entry.field.options || [];
          const single = entry.field.singleSelection === true;
          const signature = JSON.stringify([single, options, entry.field.optionLabels]);
          if (entry.input.dataset.options !== signature) {
            entry.input.replaceChildren(); entry.input.dataset.options = signature;
            (single ? ['', ...options] : options).forEach(option => {
              const label = node('label'), choice = node('input'); choice.type = single ? 'radio' : 'checkbox'; choice.value = option;
              if (single) choice.name = `playground-choice-${id}`;
              choice.addEventListener('change', () => change(id, [...entry.input.querySelectorAll('input:checked')].map(item => item.value).filter(Boolean), true));
              label.append(choice, document.createTextNode(option ? optionLabel(entry.field, option) : (zh ? '未选择' : 'No selection'))); entry.input.append(label);
            });
          }
          entry.input.querySelectorAll('input').forEach(input => { input.checked = input.value ? value.includes(input.value) : value.length === 0; });
        } else if (entry.field.type === 'string-list') {
          if (!entry.rowState?.composing) {
            const parsed = parseStringList(entry.input.value);
            const editing = document.activeElement === entry.input;
            if (entry.rowState?.forceSync || (!editing && entry.input.value !== value.join('\n')) || !sameStringList(parsed, value)) {
              entry.input.value = value.join('\n');
            }
            if (entry.rowState) { entry.rowState.forceSync = false; entry.rowState.dirty = false; }
          }
        } else if (entry.input.classList.contains('playground-segments')) {
          [...entry.input.children].forEach(b => { const selected = b.dataset.value === String(value); b.setAttribute('aria-checked', String(selected)); b.tabIndex = selected ? 0 : -1; });
        } else if (entry.field.type === 'boolean') entry.input.checked = value;
        else {
          const editingText = entry.isTextInput && document.activeElement === entry.input;
          if (!entry.rowState?.composing && (entry.rowState?.forceSync || (!editingText && !entry.rowState?.dirty))
              && entry.input.value !== String(effective)) {
            entry.input.value = String(effective);
          }
          if (entry.rowState) { entry.rowState.forceSync = false; entry.rowState.dirty = false; }
        }
        const fixedRange = entry.field.type === 'number' && entry.field.control !== 'model-select'
          && entry.field.minimum === entry.field.maximum;
        const enabled = !fixedRange && isFieldEnabled(entry.field, state.values,
          value === null && (entry.field.type === 'date' || entry.field.emptyValue) ? true : value);
        entry.enabledByValue = enabled;
        entry.input.disabled = !enabled;
        const visible = !entry.field.visibleWhen || isFieldEnabled({enabledWhen: entry.field.visibleWhen}, state.values, true);
        entry.row.hidden = !visible;
        if (!visible) { entry.enabledByValue = false; entry.input.disabled = true; entry.dropdown?.close(); }
        const rowDisabled = !enabled && !entry.field.nullable;
        entry.row.classList.toggle('disabled', rowDisabled);
        if (entry.hint) {
          const effectVisible = !entry.field.effectWhen
            || isFieldEnabled({enabledWhen: entry.field.effectWhen}, state.values, true);
          let explanation = fixedRange ? (zh ? '当前范围只有一个可选值；先调整范围或最小间距。' : 'Only one value fits. Adjust the bounds or minimum separation first.')
            : !enabled && entry.field.disabledHint ? localized(entry.field.disabledHint)
            : !effectVisible || entry.field.effect ? localized(entry.field.effectHint) : '';
          if (!explanation && entry.field.previewInteraction === 'hover') {
            explanation = zh ? '将鼠标移到组件上查看悬停效果。' : 'Move the pointer over the component to see its hover effect.';
          }
          entry.hint.textContent = explanation;
          entry.hint.hidden = !explanation;
          const target = entry.dropdown?.trigger || entry.input;
          if (explanation) target.setAttribute('aria-describedby', entry.hint.id);
          else target.removeAttribute('aria-describedby');
        }
        if (entry.dropdown) entry.dropdown.updateTrigger();
        if (entry.colorSwatch) entry.colorSwatch.style.setProperty('--swatch-color', entry.toCssColor(effective));
        if (entry.follow) entry.follow.checked = value === null;
        if (entry.output) {
          let display = String(effective);
          if (entry.field.valueLabels?.[String(effective)]) display = localized(entry.field.valueLabels[String(effective)]);
          if (entry.field.id === 'iconRotation') display = `${effective}°`;
          else if (entry.field.id === 'iconScale') display = `${effective}x`;
          else if (entry.field.id === 'iconOffsetX' || entry.field.id === 'iconOffsetY') display = `${effective}px`;
          entry.output.value = display;
          entry.input.setAttribute('aria-valuetext', display);
        }
        entry.input.removeAttribute('aria-invalid');
      }
      for (const [id, group] of propertyGroups) {
        let message = '';
        if (id === 'primary-icon') {
          const hasIcon = rows.has('icon') ? Boolean(state.values.icon && state.values.icon !== 'None')
            : Boolean(state.values.glyph && state.values.glyph !== 'None');
          if (rows.has('icon') && !hasIcon) message = zh ? '选择主图标后可调整位置、旋转和缩放。' : 'Choose a main icon to adjust its placement and transforms.';
          else if (rows.has('glyph') && !hasIcon) message = zh ? '填写图标字形后可调整大小、旋转和颜色。' : 'Enter a glyph to adjust its size, rotation and color.';
          else if (hasIcon && state.values.fluentLayout === 'TextOnly') message = zh ? '当前为仅文字布局；切换为显示图标的布局后可调整图标效果。' : 'Text only hides the icon. Choose an icon layout to adjust its transforms.';
          else if (hasIcon && rows.get('iconRotation')?.row.hidden) message = zh ? '将图标置于文字上方或下方后可调整旋转和缩放。' : 'Place the icon above or below the text to adjust rotation and scale.';
        } else if (id === 'dropdown-indicator' && state.values.chevronGlyph === 'None')
          message = zh ? '选择下拉箭头后可调整大小和位置。' : 'Choose a dropdown indicator to adjust its size and position.';
        group.hint.textContent = message; group.hint.hidden = !message;
        const source = id === 'primary-icon' ? (rows.get('icon') || rows.get('glyph'))
          : id === 'dropdown-indicator' ? rows.get('chevronGlyph') : null;
        if (source) {
          const input = source.dropdown?.trigger || source.input;
          if (message) input.setAttribute('aria-describedby', group.hint.id);
          else input.removeAttribute('aria-describedby');
        }
      }
      categoryTabButtons.forEach((button, id) => {
        const panel = categoryTabPanels.get(id);
        const hasVisible = [...panel.querySelectorAll('.playground-field')].some(fieldRow => !fieldRow.hidden);
        button.hidden = !hasVisible;
      });
      if (categoryTabButtons.get(activeCategoryTab)?.hidden) {
        const firstVisible = [...categoryTabButtons.keys()].find(id => !categoryTabButtons.get(id).hidden);
        if (firstVisible) switchCategoryTab(firstVisible);
      }
      updateStateTextEditor(state);
      if (state.preview.live) {
        liveFrame.style.visibility = '';
        acceptViewport(state.preview);
      }
      popup.dataset.revision = String(state.revision); busy();
      updateCodeDisplay();
    }
    function unavailable() {
      clearPending(); current = null;
      liveFrame?.remove(); liveFrame = null; liveReady = false; liveSession = '';
      previewContent.style.height = ''; stage.classList.remove('playground-live', 'playground-overlay-open');
      status.hidden = true; status.textContent = '';
      showMobileView('controls');
      setPreviewState('error'); busy();
    }
    function position() {
      if (!opened) return;
      const margin = 14;
      const isWide = innerWidth >= 1100;
      let targetH, targetW;

      if (!userResized) {
        targetW = isWide
          ? Math.min(dataLayout ? 1180 : 900, Math.max(680, innerWidth - 100), innerWidth - 2 * margin)
          : Math.min(dataLayout ? 760 : 420, innerWidth - 2 * margin);
        targetH = Math.min(dataLayout ? 800 : 720, innerHeight - 2 * margin);
        popup.style.width = `${targetW}px`;
        popup.style.height = `${targetH}px`;
      }
      popup.style.maxHeight = `${innerHeight - 2 * margin}px`;

      const rect = popup.getBoundingClientRect();
      const height = userResized ? rect.height : (targetH || rect.height);
      const width = userResized ? rect.width : (targetW || rect.width);

      if (userMoved) {
        const maxLeft = innerWidth - width - margin;
        const maxTop = innerHeight - height - margin;
        popup.style.left = `${Math.max(margin, Math.min(rect.left, maxLeft))}px`;
        popup.style.top = `${Math.max(margin, Math.min(rect.top, maxTop))}px`;
        return;
      }

      // True centered modal dialog
      const left = Math.max(margin, Math.round((innerWidth - width) / 2));
      const top = Math.max(margin, Math.round((innerHeight - height) / 2));
      popup.style.left = `${left}px`; popup.style.top = `${top}px`;
    }
    function schedulePosition() { if (!positionFrame) positionFrame = requestAnimationFrame(() => { positionFrame = null; position(); }); }
    function themeChanged() {
      popup.dataset.theme = matchMedia('(forced-colors: active)').matches ? 'high-contrast' : (root.dataset.previewTheme || document.documentElement.dataset.theme);
      liveFrame?.contentWindow?.postMessage({source: 'fluent-qt-site', type: 'theme', session: liveSession, theme: popup.dataset.theme}, location.origin);
    }
    function setReady(value) {
      ready = value;
      if (!ready && opened) dismiss(false, false);
    }
    function open(target) {
      if (opened) { dismiss(); return; }
      openPreview(target === undefined ? undefined : {target});
    }
    backdrop.addEventListener('click', () => dismiss());
    close.addEventListener('click', () => dismiss()); retry.addEventListener('click', () => openPreview());
    more.addEventListener('click', () => {
      // Retained for programmatic clicks: ensure current tab is displayed
      more.setAttribute('aria-expanded', 'true');
      schedulePosition();
    });
    reset.addEventListener('click', () => {
      rows.forEach(entry => {
        entry.tableEditor?.clearDraft();
        if (entry.rowState) { entry.rowState.composing = false; entry.rowState.dirty = false; entry.rowState.forceSync = true; }
      });
      clearPending(); separateStateText = null; separateTextValues = null; sharedTextValue = null;
      send({operation: 'reset'});
    });
    function selectLanguage(lang) {
      if (language === lang) return;
      language = lang;
      cppTab.setAttribute('aria-selected', String(language === 'cpp'));
      pythonTab.setAttribute('aria-selected', String(language === 'python'));
      updateCodeDisplay();
    }
    cppTab.addEventListener('click', () => selectLanguage('cpp'));
    pythonTab.addEventListener('click', () => selectLanguage('python'));
    copy.addEventListener('click', async () => {
      if (!current || copy.disabled) return;
      const fresh = await new Promise(resolve => {
        freshState = resolve; send({operation: 'read'});
        setTimeout(() => { if (freshState === resolve) { freshState = null; resolve(false); } }, 5000);
      });
      if (!fresh || !current || !opened) return;
      const text = current[language] || '';
      let success = false;
      try {
        if (navigator.clipboard?.writeText) {
          await navigator.clipboard.writeText(text);
          success = true;
        }
      } catch (_) {}
      if (!success) {
        try {
          const ta = document.createElement('textarea');
          ta.value = text;
          ta.style.position = 'fixed';
          ta.style.opacity = '0';
          popup.append(ta);
          ta.select();
          success = document.execCommand('copy');
          ta.remove();
        } catch (_) {}
      }
      if (success) {
        copy.classList.add('copied');
        copy.title = words.copied;
        const iconSpan = copy.querySelector('.playground-icon');
        if (iconSpan) iconSpan.className = 'playground-icon playground-icon-check';
        setTimeout(() => {
          copy.classList.remove('copied');
          copy.title = words.copy;
          if (iconSpan) iconSpan.className = 'playground-icon playground-icon-copy';
        }, 1500);
      } else {
        status.hidden = false;
        status.textContent = words.failed;
      }
      busy();
    });
    popup.addEventListener('wheel', event => {
      if (event.target.closest('.playground-wheel-col')) return;
      const scroller = event.target.closest('.playground-dropdown-menu, textarea.playground-input, .playground-code-box, .playground-tabpanel');
      if (scroller) {
        event.preventDefault();
        const scale = event.deltaMode === 1 ? 16 : event.deltaMode === 2 ? scroller.clientHeight : 1;
        scroller.scrollTop += event.deltaY * scale;
        scroller.scrollLeft += event.deltaX * scale;
      } else event.preventDefault();
    }, {passive: false});
    document.addEventListener('keydown', event => {
      if (event.key === 'Escape' && opened) { event.preventDefault(); dismiss(); }
      if (event.key !== 'Tab' || !opened) return;
      const controls = focusableControls();
      const first = controls[0], last = controls[controls.length - 1];
      if (!first) { event.preventDefault(); popup.focus(); }
      else if (event.shiftKey && (document.activeElement === first || !popup.contains(document.activeElement))) {
        event.preventDefault(); last.focus();
      } else if (!event.shiftKey && (document.activeElement === last || !popup.contains(document.activeElement))) {
        event.preventDefault(); first.focus();
      }
    });
    document.addEventListener('pointerdown', event => {
      if (opened && !popup.contains(event.target) && !opener.contains(event.target) && !event.target.closest?.('.playground-dropdown-menu')) dismiss(false);
    });
    window.addEventListener('resize', schedulePosition);
    document.addEventListener('scroll', event => {
      if (popup && popup.contains(event.target)) return;
      schedulePosition();
    }, true);
    const observer = new ResizeObserver(() => { schedulePosition(); });
    observer.observe(root);
    new ResizeObserver(layoutLive).observe(controlsPane);
    window.addEventListener('pagehide', () => dismiss(false));
    window.fluentQtPlaygroundClose = () => dismiss();
    return {receive, receiveTargets, setReady, themeChanged, open, dismiss, contextChanged: () => dismiss(false), originalPointer: () => { if (opened) dismiss(false); }};
  }
};
