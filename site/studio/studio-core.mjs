import { isSafeStudioSvg, isStudioPhotoUrl } from './studio-media.mjs';
import { designerClasses, playgroundFields, qmlScenes } from './studio-metadata.mjs';

export const STUDIO_SCHEMA_VERSION = 1;
export const MAX_NODES = 160;
export const MAX_DEPTH = 12;

export const STUDIO_CATEGORIES = [
  { id: 'layout', label: { zh: '布局与容器', en: 'Layout' } },
  { id: 'basic-input', label: { zh: '基础输入', en: 'Basic input' } },
  { id: 'text-fields', label: { zh: '文本输入', en: 'Text fields' } },
  { id: 'date-time', label: { zh: '日期与时间', en: 'Date & time' } },
  { id: 'navigation', label: { zh: '导航', en: 'Navigation' } },
  { id: 'menus-toolbars', label: { zh: '菜单与工具栏', en: 'Menus & toolbars' } },
  { id: 'status-info', label: { zh: '状态与信息', en: 'Status & info' } },
  { id: 'collections', label: { zh: '集合视图', en: 'Collections' } },
  { id: 'charts', label: { zh: '图表', en: 'Charts' } },
  { id: 'scrolling', label: { zh: '滚动与媒体', en: 'Scrolling' } },
  { id: 'dialogs-flyouts', label: { zh: '对话框与浮层', en: 'Dialogs & flyouts' } },
  { id: 'windowing-foundation', label: { zh: '窗口与基础', en: 'Windowing & Foundation' } },
  { id: 'extensions', label: { zh: '扩展 · 粒子 / 3D / QML', en: 'Extensions · Particles / 3D / QML' } },
];

export const ANCHOR_CHILD_SCHEMA = [
  {
    key: 'anchorPosition',
    kind: 'select',
    options: ['TopLeft', 'TopCenter', 'TopRight', 'Center', 'BottomLeft', 'BottomCenter', 'BottomRight', 'TopBar', 'BottomBar', 'LeftDock', 'RightDock', 'Fill'],
    label: { zh: '锚点位置', en: 'Anchor Position' },
  },
  { key: 'anchorMargin', kind: 'number', min: 0, max: 96, label: { zh: '边缘边距', en: 'Anchor Margin' } },
  { key: 'anchorOffsetX', kind: 'number', min: -1200, max: 1200, label: { zh: '水平偏移 X', en: 'Offset X' } },
  { key: 'anchorOffsetY', kind: 'number', min: -1200, max: 1200, label: { zh: '垂直偏移 Y', en: 'Offset Y' } },
];

export const ANCHOR_PROP_KEYS = ['anchorPosition', 'anchorMargin', 'anchorOffsetX', 'anchorOffsetY'];

const CHART_SCHEMA = [
  { key: 'title', kind: 'text', label: { zh: '图表标题', en: 'Title' } },
  { key: 'seriesName', kind: 'text', label: { zh: '序列名称', en: 'Series Name' } },
  { key: 'values', kind: 'numlist', label: { zh: '数值（逗号分隔）', en: 'Values (comma-separated)' } },
  { key: 'minHeight', kind: 'number', min: 64, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
];

const OVERLAY_TRIGGER_SCHEMA = [
  { key: 'text', kind: 'text', label: { zh: '触发按钮文案', en: 'Trigger Button Text' } },
  { key: 'title', kind: 'text', label: { zh: '标题', en: 'Title' } },
  { key: 'message', kind: 'text', label: { zh: '展示内容', en: 'Message' } },
  { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
];

function def(type, category, zh, en, cppHeader, cppClass, pyModule, pyClass, defaultProps = {}, propSchema = [], extra = {}) {
  return {
    type,
    category,
    container: Boolean(extra.container),
    maxChildren: extra.maxChildren ?? null,
    label: { zh: `${zh} ${type}`, en },
    cppClass,
    cppHeader,
    pyClass: pyClass || type,
    pyModule,
    designerClass: designerClasses[type] || 'QWidget',
    defaultProps,
    propSchema,
  };
}

export const STUDIO_CATALOG = {
  // === Layout (11) ===
  VBox: def('VBox', 'layout', '纵向布局', 'VBox Layout', '<QVBoxLayout>', 'QWidget', 'PySide6.QtWidgets', 'QVBoxLayout',
    { spacing: 12, padding: 16 },
    [
      { key: 'spacing', kind: 'number', min: 0, max: 48, label: { zh: '子项间距', en: 'Spacing' } },
      { key: 'padding', kind: 'number', min: 0, max: 48, label: { zh: '内边距', en: 'Padding' } },
    ],
    { container: true }),
  HBox: def('HBox', 'layout', '横向布局', 'HBox Row', '<QHBoxLayout>', 'QWidget', 'PySide6.QtWidgets', 'QHBoxLayout',
    { spacing: 12, padding: 0, align: 'Stretch' },
    [
      { key: 'spacing', kind: 'number', min: 0, max: 48, label: { zh: '子项间距', en: 'Spacing' } },
      { key: 'padding', kind: 'number', min: 0, max: 48, label: { zh: '内边距', en: 'Padding' } },
      { key: 'align', kind: 'select', options: ['Stretch', 'Start', 'Center', 'End'], label: { zh: '对齐方式', en: 'Align' } },
    ],
    { container: true }),
  Grid: def('Grid', 'layout', '网格布局', 'Grid Layout', '<QGridLayout>', 'QWidget', 'PySide6.QtWidgets', 'QGridLayout',
    { columns: 2, spacing: 12, padding: 0 },
    [
      { key: 'columns', kind: 'number', min: 1, max: 4, label: { zh: '列数', en: 'Columns' } },
      { key: 'spacing', kind: 'number', min: 0, max: 48, label: { zh: '网格间距', en: 'Spacing' } },
      { key: 'padding', kind: 'number', min: 0, max: 48, label: { zh: '内边距', en: 'Padding' } },
    ],
    { container: true }),
  AnchorBox: def('AnchorBox', 'layout', '锚布局容器', 'AnchorLayout', '<FluentQt/Foundation.h>', 'fluent::AnchorLayout', 'fluentqt.foundation', 'AnchorLayout',
    { minHeight: 220, padding: 12 },
    [
      { key: 'minHeight', kind: 'number', min: 80, max: 1600, label: { zh: '最小高度', en: 'Min Height' } },
      { key: 'padding', kind: 'number', min: 0, max: 48, label: { zh: '默认边距', en: 'Default Margin' } },
    ],
    { container: true }),
  Card: def('Card', 'layout', '卡片容器', 'Card', '<FluentQt/Layout.h>', 'fluent::layout::Card', 'fluentqt.layout', 'Card',
    { appearance: 'Layer', borderVisible: true, spacing: 10, padding: 16 },
    [
      { key: 'appearance', kind: 'select', options: ['Layer', 'LayerAlt', 'Canvas'], label: { zh: '表面层级', en: 'Appearance' } },
      { key: 'borderVisible', kind: 'boolean', label: { zh: '显示边框', en: 'Border Visible' } },
      { key: 'spacing', kind: 'number', min: 0, max: 48, label: { zh: '内部间距', en: 'Spacing' } },
      { key: 'padding', kind: 'number', min: 4, max: 48, label: { zh: '内边距', en: 'Padding' } },
    ],
    { container: true }),
  Expander: def('Expander', 'layout', '展开面板', 'Expander', '<FluentQt/Layout.h>', 'fluent::layout::Expander', 'fluentqt.layout', 'Expander',
    { headerText: 'Advanced Settings', expanded: true },
    [
      { key: 'headerText', kind: 'text', label: { zh: '标题文本', en: 'Header Text' } },
      { key: 'expanded', kind: 'boolean', label: { zh: '默认展开', en: 'Expanded' } },
    ],
    { container: true }),
  Accordion: def('Accordion', 'layout', '折叠面板组', 'Accordion', '<FluentQt/Layout.h>', 'fluent::layout::Accordion', 'fluentqt.layout', 'Accordion',
    { expansionMode: 'Single' },
    [
      { key: 'expansionMode', kind: 'select', options: ['Single', 'Multiple'], label: { zh: '展开模式', en: 'Expansion Mode' } },
    ],
    { container: true }),
  Field: def('Field', 'layout', '表单字段', 'Field (1 slot)', '<FluentQt/Layout.h>', 'fluent::layout::Field', 'fluentqt.layout', 'Field',
    { labelText: 'Field label', helperText: '', required: false, labelPosition: 'Top', validationState: 'None', validationMessage: '' },
    [
      { key: 'labelText', kind: 'text', label: { zh: '字段标签', en: 'Label' } },
      { key: 'helperText', kind: 'text', label: { zh: '辅助说明', en: 'Helper Text' } },
      { key: 'required', kind: 'boolean', label: { zh: '必填星标', en: 'Required' } },
      { key: 'labelPosition', kind: 'select', options: ['Top', 'Leading'], label: { zh: '标签位置', en: 'Label Position' } },
      { key: 'validationState', kind: 'select', options: ['None', 'Error', 'Warning', 'Success', 'Validating'], label: { zh: '校验状态', en: 'Validation State' } },
      { key: 'validationMessage', kind: 'text', label: { zh: '校验提示', en: 'Validation Msg' } },
    ],
    { container: true, maxChildren: 1 }),
  Divider: def('Divider', 'layout', '分隔线', 'Divider', '<FluentQt/Layout.h>', 'fluent::layout::Divider', 'fluentqt.layout', 'Divider',
    { orientation: 'Horizontal' },
    [{ key: 'orientation', kind: 'select', options: ['Horizontal', 'Vertical'], label: { zh: '方向', en: 'Orientation' } }]),
  DataGridToolBar: def('DataGridToolBar', 'layout', '表格工具栏', 'DataGridToolBar', '<FluentQt/Layout.h>', 'fluent::layout::DataGridToolBar', 'fluentqt.layout', 'DataGridToolBar',
    { searchText: '', items: ['Active', 'Queued', 'Done'] },
    [
      { key: 'searchText', kind: 'text', label: { zh: '默认搜索词', en: 'Search Text' } },
      { key: 'items', kind: 'list', label: { zh: '状态筛选项', en: 'Filter Options' } },
    ]),
  PropertyPanel: def('PropertyPanel', 'layout', '属性面板', 'PropertyPanel', '<FluentQt/Layout.h>', 'fluent::layout::PropertyPanel', 'fluentqt.layout', 'PropertyPanel',
    { layoutMode: 'Automatic', readOnly: false },
    [
      { key: 'layoutMode', kind: 'select', options: ['Automatic', 'Stacked', 'Inline'], label: { zh: '布局模式', en: 'Layout Mode' } },
      { key: 'readOnly', kind: 'boolean', label: { zh: '只读模式', en: 'Read Only' } },
    ]),
  ListDetailView: def('ListDetailView', 'layout', '列表详情视图', 'ListDetailView', '<FluentQt/Layout.h>', 'fluent::layout::ListDetailView', 'fluentqt.layout', 'ListDetailView',
    { listPaneWidth: 220, minHeight: 200 },
    [
      { key: 'listPaneWidth', kind: 'number', min: 140, max: 420, label: { zh: '列表栏宽度', en: 'List Pane Width' } },
      { key: 'minHeight', kind: 'number', min: 150, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ],
    { container: true }),

  // === Basic input (19) ===
  Button: def('Button', 'basic-input', '按钮', 'Button', '<FluentQt/BasicInput.h>', 'fluent::basicinput::Button', 'fluentqt.basicinput', 'Button',
    { text: 'Save Changes', fluentStyle: 'Accent', fluentSize: 'StandardSize', enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '按钮文本', en: 'Text' } },
      { key: 'fluentStyle', kind: 'select', options: ['Standard', 'Accent', 'Subtle'], label: { zh: '样式', en: 'Style' } },
      { key: 'fluentSize', kind: 'select', options: ['Small', 'StandardSize', 'Large'], label: { zh: '尺寸', en: 'Size' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  CompoundButton: def('CompoundButton', 'basic-input', '复合按钮', 'CompoundButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::CompoundButton', 'fluentqt.basicinput', 'CompoundButton',
    { text: 'Build Artifacts', secondaryText: 'Compile release binaries and verify checksums', enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '主标题', en: 'Primary Text' } },
      { key: 'secondaryText', kind: 'text', label: { zh: '副标题', en: 'Secondary Text' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  ToggleButton: def('ToggleButton', 'basic-input', '切换按钮', 'ToggleButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::ToggleButton', 'fluentqt.basicinput', 'ToggleButton',
    { text: 'Pin Panel', checked: false, enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '文本', en: 'Text' } },
      { key: 'checked', kind: 'boolean', label: { zh: '选中', en: 'Checked' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  DropDownButton: def('DropDownButton', 'basic-input', '下拉按钮', 'DropDownButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::DropDownButton', 'fluentqt.basicinput', 'DropDownButton',
    { text: 'Export', items: ['C++17 Header', 'PySide6 Script', 'Qt Designer .ui'], enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '按钮文本', en: 'Text' } },
      { key: 'items', kind: 'list', label: { zh: '菜单项（每行一项）', en: 'Menu Items' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  SplitButton: def('SplitButton', 'basic-input', '分裂按钮', 'SplitButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::SplitButton', 'fluentqt.basicinput', 'SplitButton',
    { text: 'Build', items: ['Build Debug', 'Build Release', 'Build WASM'], enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '主操作文本', en: 'Primary Action' } },
      { key: 'items', kind: 'list', label: { zh: '子菜单项', en: 'Menu Items' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  ToggleSplitButton: def('ToggleSplitButton', 'basic-input', '切换分裂按钮', 'ToggleSplitButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::ToggleSplitButton', 'fluentqt.basicinput', 'ToggleSplitButton',
    { text: 'Live Sync', checked: true, items: ['150 ms', '500 ms', 'Manual'], enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '按钮文本', en: 'Text' } },
      { key: 'checked', kind: 'boolean', label: { zh: '激活', en: 'Checked' } },
      { key: 'items', kind: 'list', label: { zh: '选项', en: 'Menu Items' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  HyperlinkButton: def('HyperlinkButton', 'basic-input', '链接按钮', 'HyperlinkButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::HyperlinkButton', 'fluentqt.basicinput', 'HyperlinkButton',
    { text: 'View documentation', enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '文本', en: 'Text' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  RepeatButton: def('RepeatButton', 'basic-input', '重复按钮', 'RepeatButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::RepeatButton', 'fluentqt.basicinput', 'RepeatButton',
    { text: 'Hold to Increment', delay: 250, interval: 60, enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '按钮文本', en: 'Text' } },
      { key: 'delay', kind: 'number', min: 50, max: 2000, label: { zh: '触发延迟 (ms)', en: 'Delay (ms)' } },
      { key: 'interval', kind: 'number', min: 20, max: 1000, label: { zh: '重复间隔 (ms)', en: 'Interval (ms)' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  CheckBox: def('CheckBox', 'basic-input', '复选框', 'CheckBox', '<FluentQt/BasicInput.h>', 'fluent::basicinput::CheckBox', 'fluentqt.basicinput', 'CheckBox',
    { text: 'Enable diagnostic logging', checked: true, enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '标签', en: 'Text' } },
      { key: 'checked', kind: 'boolean', label: { zh: '勾选', en: 'Checked' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  RadioButton: def('RadioButton', 'basic-input', '单选按钮', 'RadioButton', '<FluentQt/BasicInput.h>', 'fluent::basicinput::RadioButton', 'fluentqt.basicinput', 'RadioButton',
    { text: 'Option A', checked: false, enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '标签', en: 'Text' } },
      { key: 'checked', kind: 'boolean', label: { zh: '选中', en: 'Checked' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  ToggleSwitch: def('ToggleSwitch', 'basic-input', '开关', 'ToggleSwitch', '<FluentQt/BasicInput.h>', 'fluent::basicinput::ToggleSwitch', 'fluentqt.basicinput', 'ToggleSwitch',
    { isOn: true, onContent: 'On', offContent: 'Off', enabled: true },
    [
      { key: 'isOn', kind: 'boolean', label: { zh: '开启状态', en: 'Is On' } },
      { key: 'onContent', kind: 'text', label: { zh: '开启文案', en: 'On Text' } },
      { key: 'offContent', kind: 'text', label: { zh: '关闭文案', en: 'Off Text' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  ComboBox: def('ComboBox', 'basic-input', '下拉选择框', 'ComboBox', '<FluentQt/BasicInput.h>', 'fluent::basicinput::ComboBox', 'fluentqt.basicinput', 'ComboBox',
    { items: ['Windows 11 Mica', 'macOS Vibrancy', 'Linux Wayland'], currentIndex: 0, enabled: true },
    [
      { key: 'items', kind: 'list', label: { zh: '选项（每行一项）', en: 'Items (one per line)' } },
      { key: 'currentIndex', kind: 'number', min: 0, max: 20, label: { zh: '默认索引', en: 'Selected Index' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  MultiSelectComboBox: def('MultiSelectComboBox', 'basic-input', '多选下拉框', 'MultiSelectComboBox', '<FluentQt/BasicInput.h>', 'fluent::basicinput::MultiSelectComboBox', 'fluentqt.basicinput', 'MultiSelectComboBox',
    { placeholderText: 'Select modules...', items: ['Core', 'Charts', 'Spatial', 'QmlBridge'], searchEnabled: true, enabled: true },
    [
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'items', kind: 'list', label: { zh: '可选项（每行一项）', en: 'Items' } },
      { key: 'searchEnabled', kind: 'boolean', label: { zh: '允许搜索', en: 'Searchable' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  TreeSelect: def('TreeSelect', 'basic-input', '树形选择器', 'TreeSelect', '<FluentQt/BasicInput.h>', 'fluent::basicinput::TreeSelect', 'fluentqt.basicinput', 'TreeSelect',
    { placeholderText: 'Select category...', items: ['Buttons', 'Navigation', 'Charts'], enabled: true },
    [
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'items', kind: 'list', label: { zh: '叶子节点', en: 'Tree Items' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  Slider: def('Slider', 'basic-input', '滑块', 'Slider', '<FluentQt/BasicInput.h>', 'fluent::basicinput::Slider', 'fluentqt.basicinput', 'Slider',
    { minimum: 0, maximum: 100, value: 45, enabled: true },
    [
      { key: 'minimum', kind: 'number', min: -1000, max: 1000, label: { zh: '最小值', en: 'Minimum' } },
      { key: 'maximum', kind: 'number', min: 1, max: 10000, label: { zh: '最大值', en: 'Maximum' } },
      { key: 'value', kind: 'number', min: -1000, max: 10000, label: { zh: '当前值', en: 'Value' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  RangeSlider: def('RangeSlider', 'basic-input', '区间滑块', 'RangeSlider', '<FluentQt/BasicInput.h>', 'fluent::basicinput::RangeSlider', 'fluentqt.basicinput', 'RangeSlider',
    { minimum: 0, maximum: 100, lowerValue: 20, upperValue: 75, enabled: true },
    [
      { key: 'minimum', kind: 'number', min: -1000, max: 1000, label: { zh: '最小值', en: 'Minimum' } },
      { key: 'maximum', kind: 'number', min: 2, max: 10000, label: { zh: '最大值', en: 'Maximum' } },
      { key: 'lowerValue', kind: 'number', min: -1000, max: 10000, label: { zh: '下限值', en: 'Lower Value' } },
      { key: 'upperValue', kind: 'number', min: -1000, max: 10000, label: { zh: '上限值', en: 'Upper Value' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  RatingControl: def('RatingControl', 'basic-input', '评分控件', 'RatingControl', '<FluentQt/BasicInput.h>', 'fluent::basicinput::RatingControl', 'fluentqt.basicinput', 'RatingControl',
    { maxRating: 5, value: 4, enabled: true },
    [
      { key: 'maxRating', kind: 'number', min: 1, max: 10, label: { zh: '最大星数', en: 'Max Stars' } },
      { key: 'value', kind: 'number', min: 0, max: 10, label: { zh: '评分', en: 'Rating' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  ColorPicker: def('ColorPicker', 'basic-input', '颜色选择器', 'ColorPicker', '<FluentQt/BasicInput.h>', 'fluent::basicinput::ColorPicker', 'fluentqt.basicinput', 'ColorPicker',
    { color: '#0062B0', alphaEnabled: false, enabled: true },
    [
      { key: 'color', kind: 'text', label: { zh: '十六进制颜色', en: 'Hex Color' } },
      { key: 'alphaEnabled', kind: 'boolean', label: { zh: '透明通道', en: 'Alpha Channel' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  FileDropZone: def('FileDropZone', 'basic-input', '文件拖放区', 'FileDropZone', '<FluentQt/BasicInput.h>', 'fluent::basicinput::FileDropZone', 'fluentqt.basicinput', 'FileDropZone',
    { title: 'Drop files here', description: 'PNG, SVG, or .ui design files', hintText: 'Max 25 MB per file', browseText: 'Browse files', enabled: true },
    [
      { key: 'title', kind: 'text', label: { zh: '标题', en: 'Title' } },
      { key: 'description', kind: 'text', label: { zh: '描述', en: 'Description' } },
      { key: 'hintText', kind: 'text', label: { zh: '限制提示', en: 'Hint Text' } },
      { key: 'browseText', kind: 'text', label: { zh: '按钮文本', en: 'Browse Button' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),

  // === Text fields (8) ===
  Label: def('Label', 'text-fields', '文本标签', 'Label', '<FluentQt/TextFields.h>', 'fluent::textfields::Label', 'fluentqt.textfields', 'Label',
    { text: 'Section Heading', typography: 'Subtitle', colorRole: 'Default', wordWrap: true },
    [
      { key: 'text', kind: 'text', label: { zh: '文本内容', en: 'Text' } },
      { key: 'typography', kind: 'select', options: ['Caption', 'Body', 'BodyStrong', 'BodyLarge', 'BodyLargeStrong', 'Subtitle', 'Title', 'TitleLarge', 'Display'], label: { zh: '字体层级', en: 'Typography' } },
      { key: 'colorRole', kind: 'select', options: ['Default', 'Primary', 'Secondary', 'Tertiary', 'Accent', 'Critical'], label: { zh: '语义色彩', en: 'Color Role' } },
      { key: 'wordWrap', kind: 'boolean', label: { zh: '自动换行', en: 'Word Wrap' } },
    ]),
  LineEdit: def('LineEdit', 'text-fields', '单行输入框', 'LineEdit', '<FluentQt/TextFields.h>', 'fluent::textfields::LineEdit', 'fluentqt.textfields', 'LineEdit',
    { text: '', placeholderText: 'Enter value...', enabled: true },
    [
      { key: 'text', kind: 'text', label: { zh: '文本', en: 'Text' } },
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  PasswordBox: def('PasswordBox', 'text-fields', '密码输入框', 'PasswordBox', '<FluentQt/TextFields.h>', 'fluent::textfields::PasswordBox', 'fluentqt.textfields', 'PasswordBox',
    { header: '', placeholderText: 'Enter secret token...', enabled: true },
    [
      { key: 'header', kind: 'text', label: { zh: '标题', en: 'Header' } },
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  NumberBox: def('NumberBox', 'text-fields', '数值输入框', 'NumberBox', '<FluentQt/TextFields.h>', 'fluent::textfields::NumberBox', 'fluentqt.textfields', 'NumberBox',
    { header: '', minimum: 0, maximum: 1000, value: 24, enabled: true },
    [
      { key: 'header', kind: 'text', label: { zh: '标题', en: 'Header' } },
      { key: 'minimum', kind: 'number', min: -100000, max: 100000, label: { zh: '最小值', en: 'Minimum' } },
      { key: 'maximum', kind: 'number', min: 1, max: 100000, label: { zh: '最大值', en: 'Maximum' } },
      { key: 'value', kind: 'number', min: -100000, max: 100000, label: { zh: '数值', en: 'Value' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  TextEdit: def('TextEdit', 'text-fields', '多行输入框', 'TextEdit', '<FluentQt/TextFields.h>', 'fluent::textfields::TextEdit', 'fluentqt.textfields', 'TextEdit',
    { plainText: '', placeholderText: 'Multi-line notes...', minVisibleLines: 3, enabled: true },
    [
      { key: 'plainText', kind: 'textarea', label: { zh: '默认文本', en: 'Text' } },
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'minVisibleLines', kind: 'number', min: 2, max: 12, label: { zh: '最小行数', en: 'Min Lines' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  AutoSuggestBox: def('AutoSuggestBox', 'text-fields', '建议输入框', 'AutoSuggestBox', '<FluentQt/TextFields.h>', 'fluent::textfields::AutoSuggestBox', 'fluentqt.textfields', 'AutoSuggestBox',
    { header: '', placeholderText: 'Search components...', suggestions: ['Button', 'DataGrid', 'NavigationView', 'ChartView'], enabled: true },
    [
      { key: 'header', kind: 'text', label: { zh: '标题', en: 'Header' } },
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'suggestions', kind: 'list', label: { zh: '候选项（每行一项）', en: 'Suggestions' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  TagInput: def('TagInput', 'text-fields', '标签输入框', 'TagInput', '<FluentQt/TextFields.h>', 'fluent::textfields::TagInput', 'fluentqt.textfields', 'TagInput',
    { placeholderText: 'Add tag...', items: ['C++17', 'Qt 6', 'FluentUI'], enabled: true },
    [
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'items', kind: 'list', label: { zh: '初始标签（每行一项）', en: 'Tags' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  CodeBlock: def('CodeBlock', 'text-fields', '代码块', 'CodeBlock', '<FluentQt/TextFields.h>', 'fluent::textfields::CodeBlock', 'fluentqt.textfields', 'CodeBlock',
    { headerText: 'Snippet', language: 'cpp', code: 'auto* btn = new Button("Save", card);\nbtn->setFluentStyle(Button::Accent);', expanded: true },
    [
      { key: 'headerText', kind: 'text', label: { zh: '标题', en: 'Header' } },
      { key: 'language', kind: 'select', options: ['cpp', 'python', 'qml'], label: { zh: '语言', en: 'Language' } },
      { key: 'code', kind: 'textarea', label: { zh: '代码内容', en: 'Code' } },
      { key: 'expanded', kind: 'boolean', label: { zh: '默认展开', en: 'Expanded' } },
    ]),

  // === Date & time (5) ===
  DatePicker: def('DatePicker', 'date-time', '日期选择器', 'DatePicker', '<FluentQt/DateTime.h>', 'fluent::date_time::DatePicker', 'fluentqt.date_time', 'DatePicker',
    { enabled: true }, [{ key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } }]),
  TimePicker: def('TimePicker', 'date-time', '时间选择器', 'TimePicker', '<FluentQt/DateTime.h>', 'fluent::date_time::TimePicker', 'fluentqt.date_time', 'TimePicker',
    { enabled: true }, [{ key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } }]),
  CalendarDatePicker: def('CalendarDatePicker', 'date-time', '日历日期选择器', 'CalendarDatePicker', '<FluentQt/DateTime.h>', 'fluent::date_time::CalendarDatePicker', 'fluentqt.date_time', 'CalendarDatePicker',
    { placeholderText: 'Pick a date', enabled: true },
    [
      { key: 'placeholderText', kind: 'text', label: { zh: '占位提示', en: 'Placeholder' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  CalendarView: def('CalendarView', 'date-time', '日历视图', 'CalendarView', '<FluentQt/DateTime.h>', 'fluent::date_time::CalendarView', 'fluentqt.date_time', 'CalendarView',
    { enabled: true }, [{ key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } }]),
  DateTimeRangePicker: def('DateTimeRangePicker', 'date-time', '日期时间范围选择器', 'DateTimeRangePicker', '<FluentQt/DateTime.h>', 'fluent::date_time::DateTimeRangePicker', 'fluentqt.date_time', 'DateTimeRangePicker',
    { timeEnabled: true, presetsVisible: true, enabled: true },
    [
      { key: 'timeEnabled', kind: 'boolean', label: { zh: '包含时间选择', en: 'Include Time' } },
      { key: 'presetsVisible', kind: 'boolean', label: { zh: '显示快捷范围', en: 'Show Presets' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),

  // === Navigation (6) ===
  Breadcrumb: def('Breadcrumb', 'navigation', '面包屑导航', 'Breadcrumb', '<FluentQt/Navigation.h>', 'fluent::navigation::Breadcrumb', 'fluentqt.navigation', 'Breadcrumb',
    { items: ['Home', 'Settings', 'Appearance'] },
    [{ key: 'items', kind: 'list', label: { zh: '路径节点（每行一项）', en: 'Items (one per line)' } }]),
  Stepper: def('Stepper', 'navigation', '步骤导航', 'Stepper', '<FluentQt/Navigation.h>', 'fluent::navigation::Stepper', 'fluentqt.navigation', 'Stepper',
    { items: ['Account', 'Profile', 'Confirm'], currentIndex: 1 },
    [
      { key: 'items', kind: 'list', label: { zh: '步骤名称（每行一项）', en: 'Steps (one per line)' } },
      { key: 'currentIndex', kind: 'number', min: 0, max: 20, label: { zh: '当前步骤索引', en: 'Active Index' } },
    ]),
  Pivot: def('Pivot', 'navigation', '枢轴导航', 'Pivot', '<FluentQt/Navigation.h>', 'fluent::navigation::Pivot', 'fluentqt.navigation', 'Pivot',
    { items: ['Overview', 'Metrics', 'Logs'], selectedIndex: 0 },
    [
      { key: 'items', kind: 'list', label: { zh: '分栏标题', en: 'Headers' } },
      { key: 'selectedIndex', kind: 'number', min: 0, max: 20, label: { zh: '默认索引', en: 'Selected Index' } },
    ]),
  SelectorBar: def('SelectorBar', 'navigation', '选择栏', 'SelectorBar', '<FluentQt/Navigation.h>', 'fluent::navigation::SelectorBar', 'fluentqt.navigation', 'SelectorBar',
    { items: ['All', 'Active', 'Archived'], selectedIndex: 0 },
    [
      { key: 'items', kind: 'list', label: { zh: '选项（每行一项）', en: 'Items' } },
      { key: 'selectedIndex', kind: 'number', min: 0, max: 20, label: { zh: '默认索引', en: 'Selected Index' } },
    ]),
  TabView: def('TabView', 'navigation', '标签页视图', 'TabView', '<FluentQt/Navigation.h>', 'fluent::navigation::TabView', 'fluentqt.navigation', 'TabView',
    { items: ['main.cpp', 'studio.json', 'CMakeLists.txt'], selectedIndex: 0, tabsClosable: true },
    [
      { key: 'items', kind: 'list', label: { zh: '标签页列表', en: 'Tabs' } },
      { key: 'selectedIndex', kind: 'number', min: 0, max: 20, label: { zh: '当前索引', en: 'Selected Index' } },
      { key: 'tabsClosable', kind: 'boolean', label: { zh: '显示关闭按钮', en: 'Closable' } },
    ]),
  NavigationView: def('NavigationView', 'navigation', '导航视图', 'NavigationView', '<FluentQt/Navigation.h>', 'fluent::navigation::NavigationView', 'fluentqt.navigation', 'NavigationView',
    { displayMode: 'Left', paneOpen: true, items: ['Home', 'Analytics', 'Settings'], minHeight: 240 },
    [
      { key: 'displayMode', kind: 'select', options: ['Left', 'LeftCompact', 'Top', 'Auto'], label: { zh: '导航模式', en: 'Display Mode' } },
      { key: 'paneOpen', kind: 'boolean', label: { zh: '展开侧栏', en: 'Pane Open' } },
      { key: 'items', kind: 'list', label: { zh: '导航项', en: 'Nav Items' } },
      { key: 'minHeight', kind: 'number', min: 180, max: 460, label: { zh: '最小高度', en: 'Min Height' } },
    ],
    { container: true }),

  // === Menus & toolbars (5) ===
  MenuBar: def('MenuBar', 'menus-toolbars', '菜单栏', 'MenuBar', '<FluentQt/MenusToolbars.h>', 'fluent::menus_toolbars::FluentMenuBar', 'fluentqt.menus_toolbars', 'FluentMenuBar',
    { items: ['File', 'Edit', 'View', 'Help'], enabled: true },
    [
      { key: 'items', kind: 'list', label: { zh: '顶级菜单项', en: 'Top Menus' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  CommandBar: def('CommandBar', 'menus-toolbars', '命令栏', 'CommandBar', '<FluentQt/MenusToolbars.h>', 'fluent::menus_toolbars::CommandBar', 'fluentqt.menus_toolbars', 'CommandBar',
    { items: ['New', 'Save', 'Share', 'Settings'], labelPosition: 'Right', enabled: true },
    [
      { key: 'items', kind: 'list', label: { zh: '主命令项', en: 'Primary Actions' } },
      { key: 'labelPosition', kind: 'select', options: ['Right', 'Collapsed'], label: { zh: '文本显示', en: 'Label Position' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  Menu: def('Menu', 'menus-toolbars', '菜单', 'Menu', '<FluentQt/MenusToolbars.h>', 'fluent::menus_toolbars::FluentMenu', 'fluentqt.menus_toolbars', 'FluentMenu',
    { text: 'Open FluentMenu', title: 'Context Menu', message: 'Copy / Duplicate', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  CommandBarFlyout: def('CommandBarFlyout', 'menus-toolbars', '浮动命令栏', 'CommandBarFlyout', '<FluentQt/MenusToolbars.h>', 'fluent::menus_toolbars::CommandBarFlyout', 'fluentqt.menus_toolbars', 'CommandBarFlyout',
    { text: 'Open CommandBarFlyout', title: 'Quick Actions', message: 'Cut / Copy / Properties', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  CommandPalette: def('CommandPalette', 'menus-toolbars', '命令面板', 'CommandPalette', '<FluentQt/MenusToolbars.h>', 'fluent::menus_toolbars::CommandPalette', 'fluentqt.menus_toolbars', 'CommandPalette',
    { text: 'Open CommandPalette', title: 'Command Palette', message: 'Search actions...', enabled: true }, OVERLAY_TRIGGER_SCHEMA),

  // === Status & info (9) ===
  InfoBar: def('InfoBar', 'status-info', '信息栏', 'InfoBar', '<FluentQt/StatusInfo.h>', 'fluent::status_info::InfoBar', 'fluentqt.status_info', 'InfoBar',
    { title: 'Heads up', message: 'Settings synced with workspace.', severity: 'Informational', isClosable: false },
    [
      { key: 'title', kind: 'text', label: { zh: '标题', en: 'Title' } },
      { key: 'message', kind: 'text', label: { zh: '正文', en: 'Message' } },
      { key: 'severity', kind: 'select', options: ['Informational', 'Success', 'Warning', 'Error'], label: { zh: '级别', en: 'Severity' } },
      { key: 'isClosable', kind: 'boolean', label: { zh: '可关闭', en: 'Closable' } },
    ]),
  InfoBadge: def('InfoBadge', 'status-info', '信息徽章', 'InfoBadge', '<FluentQt/StatusInfo.h>', 'fluent::status_info::InfoBadge', 'fluentqt.status_info', 'InfoBadge',
    { value: 5, status: 'Informational', displayMode: 'Value' },
    [
      { key: 'value', kind: 'number', min: 0, max: 999, label: { zh: '数值', en: 'Value' } },
      { key: 'status', kind: 'select', options: ['Informational', 'Attention', 'Caution', 'Success', 'Critical'], label: { zh: '状态', en: 'Status' } },
      { key: 'displayMode', kind: 'select', options: ['Auto', 'Dot', 'Value'], label: { zh: '显示模式', en: 'Display Mode' } },
    ]),
  Avatar: def('Avatar', 'status-info', '头像', 'Avatar', '<FluentQt/StatusInfo.h>', 'fluent::status_info::Avatar', 'fluentqt.status_info', 'Avatar',
    { name: 'Ada Lovelace', avatarSize: 'Medium', presence: 'Available' },
    [
      { key: 'name', kind: 'text', label: { zh: '名称', en: 'Name' } },
      { key: 'avatarSize', kind: 'select', options: ['Small', 'Medium', 'Large', 'ExtraLarge'], label: { zh: '尺寸', en: 'Size' } },
      { key: 'presence', kind: 'select', options: ['None', 'Available', 'Away', 'Busy', 'DoNotDisturb', 'Offline'], label: { zh: '在线状态', en: 'Presence' } },
    ]),
  ProgressBar: def('ProgressBar', 'status-info', '进度条', 'ProgressBar', '<FluentQt/StatusInfo.h>', 'fluent::status_info::ProgressBar', 'fluentqt.status_info', 'ProgressBar',
    { minimum: 0, maximum: 100, value: 64 },
    [
      { key: 'minimum', kind: 'number', min: 0, max: 1000, label: { zh: '最小值', en: 'Min' } },
      { key: 'maximum', kind: 'number', min: 1, max: 1000, label: { zh: '最大值', en: 'Max' } },
      { key: 'value', kind: 'number', min: 0, max: 1000, label: { zh: '当前值', en: 'Value' } },
    ]),
  ProgressRing: def('ProgressRing', 'status-info', '进度环', 'ProgressRing', '<FluentQt/StatusInfo.h>', 'fluent::status_info::ProgressRing', 'fluentqt.status_info', 'ProgressRing',
    { minimum: 0, maximum: 100, value: 68 },
    [
      { key: 'minimum', kind: 'number', min: 0, max: 1000, label: { zh: '最小值', en: 'Min' } },
      { key: 'maximum', kind: 'number', min: 1, max: 1000, label: { zh: '最大值', en: 'Max' } },
      { key: 'value', kind: 'number', min: 0, max: 1000, label: { zh: '当前值', en: 'Value' } },
    ]),
  Shimmer: def('Shimmer', 'status-info', '加载占位', 'Shimmer', '<FluentQt/StatusInfo.h>', 'fluent::status_info::Shimmer', 'fluentqt.status_info', 'Shimmer',
    { shimmerTemplate: 'AvatarTextRow', active: true },
    [
      { key: 'shimmerTemplate', kind: 'select', options: ['AvatarTextRow', 'ImageCard', 'TextBlock'], label: { zh: '骨架模板', en: 'Template' } },
      { key: 'active', kind: 'boolean', label: { zh: '启用扫光', en: 'Active' } },
    ]),
  Toast: def('Toast', 'status-info', '轻提示', 'Toast', '<FluentQt/StatusInfo.h>', 'fluent::status_info::Toast', 'fluentqt.status_info', 'Toast',
    { text: 'Show Toast', title: 'Toast Notice', message: 'Studio Toast notification', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  ToolTip: def('ToolTip', 'status-info', '工具提示', 'ToolTip', '<FluentQt/StatusInfo.h>', 'fluent::status_info::ToolTip', 'fluentqt.status_info', 'ToolTip',
    { text: 'Fluent ToolTip preview', title: 'ToolTip', message: 'Hover helper text', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  SplashScreen: def('SplashScreen', 'status-info', '启动屏幕', 'SplashScreen', '<FluentQt/StatusInfo.h>', 'fluent::status_info::SplashScreen', 'fluentqt.status_info', 'SplashScreen',
    { title: 'Fluent-Qt', subtitle: 'Initializing UI workspace...', minHeight: 190 },
    [
      { key: 'title', kind: 'text', label: { zh: '主标题', en: 'Title' } },
      { key: 'subtitle', kind: 'text', label: { zh: '副标题', en: 'Subtitle' } },
      { key: 'minHeight', kind: 'number', min: 140, max: 360, label: { zh: '最小高度', en: 'Min Height' } },
    ]),

  // === Collections (12) ===
  ListView: def('ListView', 'collections', '列表视图', 'ListView', '<FluentQt/Collections.h>', 'fluent::collections::ListView', 'fluentqt.collections', 'ListView',
    { items: ['Design tokens', 'Adaptive layout', 'Native C++ UILib'], minHeight: 156 },
    [
      { key: 'items', kind: 'list', label: { zh: '列表项（每行一项）', en: 'Rows (one per line)' } },
      { key: 'minHeight', kind: 'number', min: 96, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  GridView: def('GridView', 'collections', '网格视图', 'GridView', '<FluentQt/Collections.h>', 'fluent::collections::GridView', 'fluentqt.collections', 'GridView',
    { headerText: 'Tiles', items: ['Alpha', 'Beta', 'Gamma', 'Delta'], minHeight: 170 },
    [
      { key: 'headerText', kind: 'text', label: { zh: '标题', en: 'Header' } },
      { key: 'items', kind: 'list', label: { zh: '条目', en: 'Items' } },
      { key: 'mediaTheme', kind: 'select', options: ['None', 'Landscape', 'Coffee', 'Architecture', 'City', 'Portrait', 'Technology', 'Abstract', 'Music'], label: { zh: '摄影主题', en: 'Media Theme' } },
      { key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  TreeView: def('TreeView', 'collections', '树形视图', 'TreeView', '<FluentQt/Collections.h>', 'fluent::collections::TreeView', 'fluentqt.collections', 'TreeView',
    { items: ['src/', 'app/', 'tests/'], minHeight: 175 },
    [
      { key: 'items', kind: 'list', label: { zh: '子节点（每行一项）', en: 'Nodes' } },
      { key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  DataGrid: def('DataGrid', 'collections', '数据表格', 'DataGrid', '<FluentQt/Collections.h>', 'fluent::collections::DataGrid', 'fluentqt.collections', 'DataGrid',
    { minHeight: 190 },
    [
      { key: 'headers', kind: 'list', label: { zh: '表头列名', en: 'Headers' } },
      { key: 'minHeight', kind: 'number', min: 140, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  Timeline: def('Timeline', 'collections', '时间线', 'Timeline', '<FluentQt/Collections.h>', 'fluent::collections::Timeline', 'fluentqt.collections', 'Timeline',
    { items: ['Queued', 'Compiling WASM', 'Published'], minHeight: 200 },
    [
      { key: 'items', kind: 'list', label: { zh: '节点阶段', en: 'Milestones' } },
      { key: 'minHeight', kind: 'number', min: 140, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  Transfer: def('Transfer', 'collections', '双栏选择', 'Transfer', '<FluentQt/Collections.h>', 'fluent::collections::Transfer', 'fluentqt.collections', 'Transfer',
    { sourceHeader: 'Available', targetHeader: 'Selected', items: ['Alpha', 'Beta', 'Gamma', 'Delta'], minHeight: 220 },
    [
      { key: 'sourceHeader', kind: 'text', label: { zh: '左栏标题', en: 'Source Header' } },
      { key: 'targetHeader', kind: 'text', label: { zh: '右栏标题', en: 'Target Header' } },
      { key: 'items', kind: 'list', label: { zh: '候选条目', en: 'Items' } },
      { key: 'minHeight', kind: 'number', min: 180, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  FlipView: def('FlipView', 'collections', '翻页视图', 'FlipView', '<FluentQt/Collections.h>', 'fluent::collections::FlipView', 'fluentqt.collections', 'FlipView',
    { showButtons: true, showIndicator: true, minHeight: 170 },
    [
      { key: 'showButtons', kind: 'boolean', label: { zh: '翻页按钮', en: 'Nav Buttons' } },
      { key: 'showIndicator', kind: 'boolean', label: { zh: '页码指示点', en: 'Indicator Dots' } },
      { key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ],
    { container: true }),
  FlowView: def('FlowView', 'collections', '流式视图', 'FlowView', '<FluentQt/Collections.h>', 'fluent::collections::FlowView', 'fluentqt.collections', 'FlowView',
    { headerText: 'Flow Tags', items: ['Tokens', 'Animation', 'Widgets', 'WebAssembly'], minHeight: 150 },
    [
      { key: 'headerText', kind: 'text', label: { zh: '标题', en: 'Header' } },
      { key: 'items', kind: 'list', label: { zh: '流式条目', en: 'Items' } },
      { key: 'minHeight', kind: 'number', min: 110, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),
  SplitView: def('SplitView', 'collections', '分隔视图', 'SplitView', '<FluentQt/Collections.h>', 'fluent::collections::SplitView', 'fluentqt.collections', 'SplitView',
    { orientation: 'Horizontal', minHeight: 180 },
    [
      { key: 'orientation', kind: 'select', options: ['Horizontal', 'Vertical'], label: { zh: '分隔方向', en: 'Orientation' } },
      { key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ],
    { container: true }),
  StackView: def('StackView', 'collections', '堆叠视图', 'StackView', '<FluentQt/Collections.h>', 'fluent::collections::StackView', 'fluentqt.collections', 'StackView',
    { minHeight: 150 },
    [{ key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } }],
    { container: true }),
  DrawerView: def('DrawerView', 'collections', '抽屉视图', 'DrawerView', '<FluentQt/Collections.h>', 'fluent::collections::DrawerView', 'fluentqt.collections', 'DrawerView',
    { text: 'Open DrawerView', title: 'Drawer Panel', message: 'Slide-out Fluent drawer', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  FileListView: def('FileListView', 'collections', '文件列表', 'FileListView', '<FluentQt/Collections.h>', 'fluent::collections::FileListView', 'fluentqt.collections', 'FileListView',
    { items: ['FluentQt.wasm', 'design-tokens.json'], minHeight: 150 },
    [
      { key: 'items', kind: 'list', label: { zh: '文件名列表', en: 'Files' } },
      { key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
    ]),

  // === Charts (9) ===
  LineChart: def('LineChart', 'charts', '折线图', 'LineChart', '<FluentQt/Charts.h>', 'fluent::charts::LineChart', 'fluentqt.charts', 'LineChart',
    { title: 'Latency (ms)', seriesName: 'P95', values: [24, 38, 32, 54, 48, 72], minHeight: 200 }, CHART_SCHEMA),
  AreaChart: def('AreaChart', 'charts', '面积图', 'AreaChart', '<FluentQt/Charts.h>', 'fluent::charts::AreaChart', 'fluentqt.charts', 'AreaChart',
    { title: 'Memory Footprint (MB)', seriesName: 'RSS', values: [28, 34, 42, 39, 51, 47], minHeight: 200 }, CHART_SCHEMA),
  BarChart: def('BarChart', 'charts', '柱状图', 'BarChart', '<FluentQt/Charts.h>', 'fluent::charts::BarChart', 'fluentqt.charts', 'BarChart',
    { title: 'Monthly Active', seriesName: 'Users', values: [18, 34, 46, 52, 68, 79], minHeight: 200 }, CHART_SCHEMA),
  HorizontalBarChart: def('HorizontalBarChart', 'charts', '条形图', 'HorizontalBarChart', '<FluentQt/Charts.h>', 'fluent::charts::HorizontalBarChart', 'fluentqt.charts', 'HorizontalBarChart',
    { title: 'Coverage (%)', seriesName: 'Modules', values: [88, 92, 76, 95, 84], minHeight: 200 }, CHART_SCHEMA),
  PieChart: def('PieChart', 'charts', '饼图', 'PieChart', '<FluentQt/Charts.h>', 'fluent::charts::PieChart', 'fluentqt.charts', 'PieChart',
    { title: 'Target Split', seriesName: 'Targets', values: [42, 28, 18, 12], minHeight: 200 }, CHART_SCHEMA),
  DonutChart: def('DonutChart', 'charts', '环形图', 'DonutChart', '<FluentQt/Charts.h>', 'fluent::charts::DonutChart', 'fluentqt.charts', 'DonutChart',
    { title: 'Build Mix', seriesName: 'Platforms', values: [45, 30, 25], minHeight: 200 }, CHART_SCHEMA),
  ScatterChart: def('ScatterChart', 'charts', '散点图', 'ScatterChart', '<FluentQt/Charts.h>', 'fluent::charts::ScatterChart', 'fluentqt.charts', 'ScatterChart',
    { title: 'Frame Distribution', seriesName: 'Samples', values: [14, 42, 26, 58, 35, 69], minHeight: 200 }, CHART_SCHEMA),
  Sparkline: def('Sparkline', 'charts', '迷你趋势图', 'Sparkline', '<FluentQt/Charts.h>', 'fluent::charts::Sparkline', 'fluentqt.charts', 'Sparkline',
    { title: 'CPU Trend', seriesName: 'Load', values: [12, 19, 14, 24, 21, 31], minHeight: 76 }, CHART_SCHEMA),
  ChartView: def('ChartView', 'charts', '图表视图', 'ChartView', '<FluentQt/Charts.h>', 'fluent::charts::ChartView', 'fluentqt.charts', 'ChartView',
    { title: 'Unified ChartView', seriesName: 'Metric', chartType: 'Line', values: [22, 36, 44, 41, 58, 66], minHeight: 200 },
    [
      { key: 'chartType', kind: 'select', options: ['Line', 'Area', 'Bar', 'Pie', 'Donut'], label: { zh: '图表类型', en: 'Chart Type' } },
      ...CHART_SCHEMA,
    ]),

  // === Scrolling (5) ===
  ScrollView: def('ScrollView', 'scrolling', '滚动视图', 'ScrollView', '<FluentQt/Scrolling.h>', 'fluent::scrolling::ScrollView', 'fluentqt.scrolling', 'ScrollView',
    { minHeight: 180 },
    [{ key: 'minHeight', kind: 'number', min: 120, max: 420, label: { zh: '最小高度', en: 'Min Height' } }],
    { container: true }),
  PipsPager: def('PipsPager', 'scrolling', '圆点分页器', 'PipsPager', '<FluentQt/Scrolling.h>', 'fluent::scrolling::PipsPager', 'fluentqt.scrolling', 'PipsPager',
    { pages: 6 },
    [{ key: 'pages', kind: 'number', min: 2, max: 20, label: { zh: '总页数', en: 'Page Count' } }]),
  ImageViewer: def('ImageViewer', 'scrolling', '图片查看器', 'ImageViewer', '<FluentQt/Scrolling.h>', 'fluent::scrolling::ImageViewer', 'fluentqt.scrolling', 'ImageViewer',
    { minHeight: 180 },
    [
      { key: 'minHeight', kind: 'number', min: 140, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
      { key: 'mediaTheme', kind: 'select', options: ['Landscape', 'Coffee', 'Architecture', 'City', 'Portrait', 'Technology', 'Abstract', 'Music'], label: { zh: '摄影主题', en: 'Media Theme' } },
      { key: 'svg', kind: 'textarea', label: { zh: '矢量插画 SVG', en: 'Inline SVG' } },
      { key: 'imageUrl', kind: 'text', label: { zh: '图片链接', en: 'Image URL' } },
    ]),
  ScrollBar: def('ScrollBar', 'scrolling', '滚动条', 'ScrollBar', '<FluentQt/Scrolling.h>', 'fluent::scrolling::ScrollBar', 'fluentqt.scrolling', 'ScrollBar',
    { orientation: 'Horizontal' },
    [{ key: 'orientation', kind: 'select', options: ['Horizontal', 'Vertical'], label: { zh: '方向', en: 'Orientation' } }]),
  AnnotatedScrollBar: def('AnnotatedScrollBar', 'scrolling', '标注滚动条', 'AnnotatedScrollBar', '<FluentQt/Scrolling.h>', 'fluent::scrolling::AnnotatedScrollBar', 'fluentqt.scrolling', 'AnnotatedScrollBar',
    { enabled: true }, [{ key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } }]),

  // === Dialogs & flyouts (6) ===
  ContentDialog: def('ContentDialog', 'dialogs-flyouts', '内容对话框', 'ContentDialog', '<FluentQt/DialogsFlyouts.h>', 'fluent::dialogs_flyouts::ContentDialog', 'fluentqt.dialogs_flyouts', 'ContentDialog',
    { text: 'Open ContentDialog', title: 'Confirm Action', message: 'Triggered from Page Studio canvas.', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  Dialog: def('Dialog', 'dialogs-flyouts', '对话框', 'Dialog', '<FluentQt/DialogsFlyouts.h>', 'fluent::dialogs_flyouts::Dialog', 'fluentqt.dialogs_flyouts', 'Dialog',
    { text: 'Open Dialog', title: 'Fluent Dialog', message: 'Click outside or press Esc to close.', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  Flyout: def('Flyout', 'dialogs-flyouts', '浮出面板', 'Flyout', '<FluentQt/DialogsFlyouts.h>', 'fluent::dialogs_flyouts::Flyout', 'fluentqt.dialogs_flyouts', 'Flyout',
    { text: 'Open Flyout', title: 'Flyout', message: 'Anchored Fluent Flyout', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  Popup: def('Popup', 'dialogs-flyouts', '弹出层', 'Popup', '<FluentQt/DialogsFlyouts.h>', 'fluent::dialogs_flyouts::Popup', 'fluentqt.dialogs_flyouts', 'Popup',
    { text: 'Open Popup', title: 'Popup', message: 'Fluent Popup surface', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  TeachingTip: def('TeachingTip', 'dialogs-flyouts', '教学提示', 'TeachingTip', '<FluentQt/DialogsFlyouts.h>', 'fluent::dialogs_flyouts::TeachingTip', 'fluentqt.dialogs_flyouts', 'TeachingTip',
    { text: 'Open TeachingTip', title: 'TeachingTip', message: 'Contextual onboarding guidance.', enabled: true }, OVERLAY_TRIGGER_SCHEMA),
  CoachMark: def('CoachMark', 'dialogs-flyouts', '引导标记', 'CoachMark', '<FluentQt/DialogsFlyouts.h>', 'fluent::dialogs_flyouts::CoachMark', 'fluentqt.dialogs_flyouts', 'CoachMark',
    { text: 'Open CoachMark', title: 'CoachMark', message: 'CoachMark spotlight anchored to control', enabled: true }, OVERLAY_TRIGGER_SCHEMA),

  // === Windowing & Foundation (3) ===
  Window: def('Window', 'windowing-foundation', '主窗口', 'Window', '<FluentQt/Windowing.h>', 'fluent::windowing::Window', 'fluentqt.windowing', 'Window',
    { title: 'Fluent Window', backdropEffect: 'Mica', minHeight: 260, padding: 16, spacing: 12 },
    [
      { key: 'title', kind: 'text', label: { zh: '窗口标题', en: 'Window Title' } },
      { key: 'backdropEffect', kind: 'select', options: ['Solid', 'Mica', 'Acrylic'], label: { zh: '背景材质', en: 'Backdrop Effect' } },
      { key: 'minHeight', kind: 'number', min: 180, max: 640, label: { zh: '最小高度', en: 'Min Height' } },
      { key: 'padding', kind: 'number', min: 4, max: 48, label: { zh: '内边距', en: 'Padding' } },
      { key: 'spacing', kind: 'number', min: 0, max: 48, label: { zh: '内部间距', en: 'Spacing' } },
    ],
    { container: true }),
  TitleBar: def('TitleBar', 'windowing-foundation', '标题栏', 'TitleBar', '<FluentQt/Windowing.h>', 'fluent::windowing::TitleBar', 'fluentqt.windowing', 'TitleBar',
    { title: 'Fluent Studio Window', enabled: true },
    [
      { key: 'title', kind: 'text', label: { zh: '窗口标题', en: 'Window Title' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
  FontIcon: def('FontIcon', 'windowing-foundation', '字体图标', 'FontIcon', '<FluentQt/Foundation.h>', 'fluent::FontIcon', 'fluentqt.foundation', 'FontIcon',
    { glyph: 'Settings', iconSize: 24 },
    [
      { key: 'glyph', kind: 'select', options: ['Settings', 'Home', 'Search', 'Save', 'Edit', 'Add', 'Share', 'Info', 'Document'], label: { zh: '图标字形', en: 'Glyph' } },
      { key: 'iconSize', kind: 'number', min: 12, max: 64, label: { zh: '图标尺寸', en: 'Icon Size' } },
    ]),

  // === Extensions: Particles / Spatial 3D / QmlBridge (4) ===
  ParticleBackdrop: def('ParticleBackdrop', 'extensions', '粒子动态背景', 'ParticleBackdrop', '<FluentQt/Particles.h>', 'fluent::particles::ParticleBackdrop', 'fluentqt.particles', 'ParticleBackdrop',
    { effect: 'FlowingRibbons', backgroundMode: 'Solid', particleCount: 180, animationEnabled: true, interactive: true, minHeight: 200, padding: 16, spacing: 10 },
    [
      { key: 'effect', kind: 'select', options: ['FlowingRibbons', 'FloatingDots', 'Starfield'], label: { zh: '粒子预设', en: 'Effect' } },
      { key: 'backgroundMode', kind: 'select', options: ['Solid', 'Transparent'], label: { zh: '背景模式', en: 'Background Mode' } },
      { key: 'particleCount', kind: 'number', min: 0, max: 960, label: { zh: '粒子数量', en: 'Particle Count' } },
      { key: 'animationEnabled', kind: 'boolean', label: { zh: '启用动画', en: 'Animation Enabled' } },
      { key: 'interactive', kind: 'boolean', label: { zh: '鼠标交互', en: 'Interactive' } },
      { key: 'minHeight', kind: 'number', min: 120, max: 560, label: { zh: '最小高度', en: 'Min Height' } },
    ],
    { container: true }),
  SpatialView: def('SpatialView', 'extensions', '3D 空间视图', 'SpatialView', '<FluentQt/Spatial.h>', 'fluent::spatial::SpatialView', 'fluentqt.spatial', 'SpatialView',
    { spatialEnabled: true, cameraDistance: 1000, zoom: 100, pointerTrackingEnabled: true, renderMode: 'Raster', minHeight: 260 },
    [
      { key: 'spatialEnabled', kind: 'boolean', label: { zh: '启用 3D 透视', en: 'Spatial Enabled' } },
      { key: 'cameraDistance', kind: 'number', min: 200, max: 4000, label: { zh: '相机距离', en: 'Camera Distance' } },
      { key: 'zoom', kind: 'number', min: 25, max: 250, label: { zh: '缩放 (%)', en: 'Zoom (%)' } },
      { key: 'pointerTrackingEnabled', kind: 'boolean', label: { zh: '指针视差', en: 'Pointer Tracking' } },
      { key: 'renderMode', kind: 'select', options: ['Raster', 'Auto', 'OpenGL'], label: { zh: '渲染后端', en: 'Render Mode' } },
      { key: 'minHeight', kind: 'number', min: 160, max: 640, label: { zh: '最小高度', en: 'Min Height' } },
    ],
    { container: true }),
  SpatialItem: def('SpatialItem', 'extensions', '3D 变换条目', 'SpatialItem (1 slot)', '<FluentQt/Spatial.h>', 'fluent::spatial::SpatialItem', 'fluentqt.spatial', 'SpatialItem',
    { posX: 0, posY: 0, posZ: 40, rotX: 6, rotY: -16, rotZ: 0, scale: 100, surfaceIntensity: 75, hoverLift: 6 },
    [
      { key: 'posX', kind: 'number', min: -600, max: 600, label: { zh: '位置 X', en: 'Position X' } },
      { key: 'posY', kind: 'number', min: -600, max: 600, label: { zh: '位置 Y', en: 'Position Y' } },
      { key: 'posZ', kind: 'number', min: -400, max: 400, label: { zh: '深度 Z', en: 'Depth Z' } },
      { key: 'rotX', kind: 'number', min: -60, max: 60, label: { zh: '绕 X 旋转 (°)', en: 'Rotation X (°)' } },
      { key: 'rotY', kind: 'number', min: -60, max: 60, label: { zh: '绕 Y 旋转 (°)', en: 'Rotation Y (°)' } },
      { key: 'rotZ', kind: 'number', min: -60, max: 60, label: { zh: '绕 Z 旋转 (°)', en: 'Rotation Z (°)' } },
      { key: 'scale', kind: 'number', min: 25, max: 250, label: { zh: '缩放 (%)', en: 'Scale (%)' } },
      { key: 'surfaceIntensity', kind: 'number', min: 0, max: 100, label: { zh: '表面高光 (%)', en: 'Surface Intensity (%)' } },
      { key: 'hoverLift', kind: 'number', min: 0, max: 16, label: { zh: '悬停抬升', en: 'Hover Lift' } },
    ],
    { container: true, maxChildren: 1 }),
  QmlView: def('QmlView', 'extensions', 'QML 桥接视图', 'QmlView', '<FluentQt/QmlBridge.h>', 'fluent::qmlbridge::QmlView', 'fluentqt.qmlbridge', 'QmlView',
    { sample: 'Basic', contextText: 'Shared Fluent-Qt title', minHeight: 160, enabled: true },
    [
      { key: 'sample', kind: 'select', options: ['Basic', 'Theme', 'Context', 'Action'], label: { zh: 'QML 示例页', en: 'QML Sample' } },
      { key: 'contextText', kind: 'text', label: { zh: '上下文标题', en: 'Context Text' } },
      { key: 'minHeight', kind: 'number', min: 96, max: 420, label: { zh: '最小高度', en: 'Min Height' } },
      { key: 'enabled', kind: 'boolean', label: { zh: '启用', en: 'Enabled' } },
    ]),
};

export const STUDIO_TEMPLATES = {
  starter: {
    id: 'starter',
    label: { zh: '自由画布', en: 'Free Canvas' },
    document: {
      schemaVersion: 1,
      root: {
        id: 'root',
        type: 'AnchorBox',
        props: { minHeight: 540, padding: 16 },
        children: [
          {
            id: 'button_1',
            type: 'Button',
            props: {
              text: 'Button',
              fluentStyle: 'Accent',
              fluentSize: 'StandardSize',
              enabled: true,
              anchorPosition: 'Center',
              anchorMargin: 16,
              anchorOffsetX: 0,
              anchorOffsetY: 0,
            },
          },
        ],
      },
    },
  },
  settings: {
    id: 'settings',
    label: { zh: '系统设置页', en: 'App Settings' },
    document: {
      schemaVersion: 1,
      root: {
        id: 'root',
        type: 'VBox',
        props: { spacing: 14, padding: 20 },
        children: [
          { id: 'crumb_1', type: 'Breadcrumb', props: { items: ['Home', 'Settings', 'Appearance'] } },
          { id: 'title_1', type: 'Label', props: { text: 'Workspace Preferences', typography: 'Title', colorRole: 'Default', wordWrap: true } },
          { id: 'info_1', type: 'InfoBar', props: { title: 'Fluent-Qt Live Canvas', message: 'Every widget here is rendered by real C++ Qt Widgets.', severity: 'Informational', isClosable: false } },
          {
            id: 'card_1',
            type: 'Card',
            props: { appearance: 'Layer', borderVisible: true, spacing: 12, padding: 16 },
            children: [
              {
                id: 'grid_1',
                type: 'Grid',
                props: { columns: 2, spacing: 14, padding: 0 },
                children: [
                  {
                    id: 'field_theme',
                    type: 'Field',
                    props: { labelText: 'Default Density', helperText: 'Choose control sizing preset', required: false, labelPosition: 'Top', validationState: 'None', validationMessage: '' },
                    children: [{ id: 'combo_density', type: 'ComboBox', props: { items: ['Compact', 'Standard', 'Touch Friendly'], currentIndex: 1, enabled: true } }],
                  },
                  {
                    id: 'field_port',
                    type: 'Field',
                    props: { labelText: 'Render Scale (%)', helperText: 'Adjust backing-store scale factor', required: false, labelPosition: 'Top', validationState: 'None', validationMessage: '' },
                    children: [{ id: 'slider_scale', type: 'Slider', props: { minimum: 50, maximum: 200, value: 100, enabled: true } }],
                  },
                ],
              },
              {
                id: 'row_switch',
                type: 'HBox',
                props: { spacing: 12, padding: 0, align: 'Center' },
                children: [
                  { id: 'lbl_motion', type: 'Label', props: { text: 'Enable smooth hardware motion', typography: 'BodyStrong', colorRole: 'Default', wordWrap: true } },
                  { id: 'switch_motion', type: 'ToggleSwitch', props: { isOn: true, onContent: 'Enabled', offContent: 'Reduced', enabled: true } },
                ],
              },
            ],
          },
          {
            id: 'exp_1',
            type: 'Expander',
            props: { headerText: 'Diagnostic & Sync', expanded: true },
            children: [
              { id: 'chk_diag', type: 'CheckBox', props: { text: 'Collect anonymous layout performance metrics', checked: true, enabled: true } },
              { id: 'prog_sync', type: 'ProgressBar', props: { minimum: 0, maximum: 100, value: 72 } },
            ],
          },
          {
            id: 'row_actions',
            type: 'HBox',
            props: { spacing: 10, padding: 0, align: 'Start' },
            children: [
              { id: 'btn_apply', type: 'Button', props: { text: 'Apply Settings', fluentStyle: 'Accent', fluentSize: 'StandardSize', enabled: true } },
              { id: 'btn_reset', type: 'Button', props: { text: 'Reset Defaults', fluentStyle: 'Standard', fluentSize: 'StandardSize', enabled: true } },
            ],
          },
        ],
      },
    },
  },
  'account-form': {
    id: 'account-form',
    label: { zh: '注册表单向导', en: 'Onboarding Form' },
    document: {
      schemaVersion: 1,
      root: {
        id: 'root',
        type: 'VBox',
        props: { spacing: 14, padding: 20 },
        children: [
          { id: 'title_form', type: 'Label', props: { text: 'Create Developer Account', typography: 'Title', colorRole: 'Default', wordWrap: true } },
          { id: 'step_form', type: 'Stepper', props: { items: ['Credentials', 'Profile', 'Review'], currentIndex: 1 } },
          {
            id: 'card_form',
            type: 'Card',
            props: { appearance: 'Layer', borderVisible: true, spacing: 12, padding: 16 },
            children: [
              {
                id: 'fld_user',
                type: 'Field',
                props: { labelText: 'Display Name', helperText: 'Visible across projects', required: true, labelPosition: 'Top', validationState: 'None', validationMessage: '' },
                children: [{ id: 'edt_user', type: 'LineEdit', props: { text: '', placeholderText: 'Ada Lovelace', enabled: true } }],
              },
              {
                id: 'fld_bio',
                type: 'Field',
                props: { labelText: 'Project Summary', helperText: 'Optional application context', required: false, labelPosition: 'Top', validationState: 'None', validationMessage: '' },
                children: [{ id: 'txt_bio', type: 'TextEdit', props: { plainText: '', placeholderText: 'Desktop workbench for telemetry...', minVisibleLines: 3, enabled: true } }],
              },
              { id: 'chk_agree', type: 'CheckBox', props: { text: 'Agree to MIT & third-party license terms', checked: true, enabled: true } },
            ],
          },
          {
            id: 'row_form_btn',
            type: 'HBox',
            props: { spacing: 10, padding: 0, align: 'Start' },
            children: [
              { id: 'btn_next', type: 'Button', props: { text: 'Create Workspace', fluentStyle: 'Accent', fluentSize: 'StandardSize', enabled: true } },
              { id: 'btn_cancel', type: 'HyperlinkButton', props: { text: 'Back to templates', enabled: true } },
            ],
          },
        ],
      },
    },
  },
  analytics: {
    id: 'analytics',
    label: { zh: '监控仪表盘', en: 'Analytics Dashboard' },
    document: {
      schemaVersion: 1,
      root: {
        id: 'root',
        type: 'VBox',
        props: { spacing: 14, padding: 20 },
        children: [
          {
            id: 'hdr_row',
            type: 'HBox',
            props: { spacing: 12, padding: 0, align: 'Center' },
            children: [
              { id: 'av_owner', type: 'Avatar', props: { name: 'Fluent Ops', avatarSize: 'Large', presence: 'Available' } },
              { id: 'lbl_dash', type: 'Label', props: { text: 'Production Runtime Overview', typography: 'Title', colorRole: 'Default', wordWrap: true } },
              { id: 'badge_live', type: 'InfoBadge', props: { value: 12, status: 'Success', displayMode: 'Value' } },
            ],
          },
          {
            id: 'kpi_grid',
            type: 'Grid',
            props: { columns: 2, spacing: 14, padding: 0 },
            children: [
              {
                id: 'kpi_card_1',
                type: 'Card',
                props: { appearance: 'Layer', borderVisible: true, spacing: 8, padding: 14 },
                children: [
                  { id: 'kpi_lbl_1', type: 'Label', props: { text: 'Frame Budget Utilization', typography: 'BodyStrong', colorRole: 'Secondary', wordWrap: true } },
                  { id: 'kpi_ring', type: 'ProgressRing', props: { minimum: 0, maximum: 100, value: 38 } },
                ],
              },
              {
                id: 'kpi_card_2',
                type: 'Card',
                props: { appearance: 'Layer', borderVisible: true, spacing: 8, padding: 14 },
                children: [
                  { id: 'kpi_lbl_2', type: 'Label', props: { text: 'User Satisfaction Score', typography: 'BodyStrong', colorRole: 'Secondary', wordWrap: true } },
                  { id: 'kpi_rate', type: 'RatingControl', props: { maxRating: 5, value: 4.5, enabled: true } },
                ],
              },
            ],
          },
          {
            id: 'charts_grid',
            type: 'Grid',
            props: { columns: 2, spacing: 14, padding: 0 },
            children: [
              { id: 'chart_line', type: 'LineChart', props: { title: 'Latency (ms)', seriesName: 'P95', values: [18, 22, 19, 31, 26, 21], minHeight: 200 } },
              { id: 'chart_bar', type: 'BarChart', props: { title: 'Daily Builds', seriesName: 'CI Jobs', values: [42, 55, 48, 71, 86, 94], minHeight: 200 } },
            ],
          },
          { id: 'list_events', type: 'ListView', props: { items: ['v1.12.1 · Qt 6.9.3 macOS arm64 verified', 'v1.12.1 · QtQuick / QmlBridge smoke passed', 'Gallery · 99 controls active'], minHeight: 132 } },
        ],
      },
    },
  },
  blank: {
    id: 'blank',
    label: { zh: '空白画布', en: 'Blank Page' },
    document: {
      schemaVersion: 1,
      root: {
        id: 'root',
        type: 'AnchorBox',
        props: { minHeight: 540, padding: 16 },
        children: [],
      },
    },
  },
};

export function cloneDoc(doc) {
  return JSON.parse(JSON.stringify(doc));
}

export function collectIds(node, set = new Set()) {
  if (!node || typeof node !== 'object') return set;
  if (node.id) set.add(node.id);
  for (const child of node.children || []) collectIds(child, set);
  return set;
}

export function createNode(type, doc = null) {
  const meta = STUDIO_CATALOG[type];
  if (!meta) throw new Error(`Unsupported studio component type: ${type}`);
  const used = doc?.root ? collectIds(doc.root) : new Set();
  const slug = type.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase();
  let seq = 1;
  while (used.has(`${slug}_${seq}`)) seq++;
  const node = {
    id: `${slug}_${seq}`,
    type,
    props: structuredClone(meta.defaultProps || {}),
  };
  if (meta.container) node.children = [];
  return node;
}

export function findNode(node, id) {
  if (!node || !id) return null;
  if (node.id === id) return node;
  for (const child of node.children || []) {
    const match = findNode(child, id);
    if (match) return match;
  }
  return null;
}

export function findParent(node, targetId, parent = null) {
  if (!node || !targetId) return null;
  if (node.id === targetId) return { node, parent, index: parent ? parent.children.indexOf(node) : -1 };
  for (const child of node.children || []) {
    const match = findParent(child, targetId, node);
    if (match) return match;
  }
  return null;
}

export function countNodes(node) {
  if (!node || typeof node !== 'object') return 0;
  let total = 1;
  for (const child of node.children || []) total += countNodes(child);
  return total;
}

export function subtreeHeight(node) {
  if (!node || !(node.children || []).length) return 1;
  let deepest = 0;
  for (const child of node.children) {
    deepest = Math.max(deepest, subtreeHeight(child));
  }
  return 1 + deepest;
}

export function nodeDepth(root, targetId, depth = 1) {
  if (!root) return -1;
  if (root.id === targetId) return depth;
  for (const child of root.children || []) {
    const found = nodeDepth(child, targetId, depth + 1);
    if (found > 0) return found;
  }
  return -1;
}

export function canAcceptChild(parentNode, deltaCount = 1) {
  if (!parentNode) return false;
  const meta = STUDIO_CATALOG[parentNode.type];
  if (!meta || !meta.container) return false;
  if (meta.maxChildren === null) return true;
  const current = (parentNode.children || []).length;
  return current + deltaCount <= meta.maxChildren;
}

const MAX_DOCUMENT_BYTES = 128 * 1024;
const safeKey = key => /^[A-Za-z_][A-Za-z_0-9]*$/.test(key) && !Object.hasOwn(Object.prototype, key)
  && key !== 'prototype';
const record = value => value !== null && typeof value === 'object' && !Array.isArray(value);
const validText = value => typeof value === 'string' && !/[\u0000-\u0008\u000b\u000c\u000e-\u001f]/.test(value);

const VALID_DESIGN_THEMES = new Set(['light', 'dark']);
const HEX_COLOR_RE = /^#[0-9a-fA-F]{6}$/;
function validProperty(meta, key, value) {
  if (!safeKey(key)) return false;
  const field = meta.propSchema.find(f => f.key === key) || ANCHOR_CHILD_SCHEMA.find(f => f.key === key);
  const extra = playgroundFields[meta.type]?.[key];
  const kind = field?.kind || extra?.type || typeof meta.defaultProps[key];
  const options = field?.options || extra?.options;
  if (value === null) return Boolean(extra?.nullable || key === 'cornerRadius');
  if (meta.type === 'ImageViewer' && key === 'svg') {
    return value === '' || isSafeStudioSvg(value);
  }
  if (meta.type === 'ImageViewer' && key === 'imageUrl') {
    return validText(value) && value.length <= 512 && (value === '' || isStudioPhotoUrl(value));
  }
  switch (kind) {
    case 'number': return Number.isFinite(value);
    case 'boolean': return typeof value === 'boolean';
    case 'select': case 'enum':
      if (key === 'icon' || key === 'glyph') {
        return typeof value === 'string' && value.length <= 80 && safeKey(value);
      }
      return typeof value === 'string' && (options ? options.includes(value) : safeKey(value));
    case 'numlist': return Array.isArray(value) && value.every(Number.isFinite);
    case 'multi-enum': return Array.isArray(value) && value.every(v => options?.includes(v));
    case 'list': case 'string-list': return Array.isArray(value) && value.every(validText);
    case 'table': return Array.isArray(value) && value.every(row => Array.isArray(row)
      && row.every(v => v === null || validText(v) || Number.isFinite(v) || typeof v === 'boolean'));
    case 'text': case 'textarea': case 'string': case 'color': case 'date': case 'time': case 'datetime':
      return validText(value);
    default: return false;
  }
}

function requireValidDocument(doc) {
  const check = validateDocument(doc);
  if (!check.ok) throw new TypeError(check.error);
}

export function validateDocument(doc) {
  if (!doc || typeof doc !== 'object' || !doc.root || typeof doc.root !== 'object') {
    return { ok: false, error: 'Document must contain a root container object.' };
  }
  if (doc.schemaVersion !== undefined && doc.schemaVersion !== STUDIO_SCHEMA_VERSION) {
    return { ok: false, error: `Unsupported schemaVersion: ${doc.schemaVersion}` };
  }
  if (doc.design !== undefined) {
    if (!record(doc.design)) {
      return { ok: false, error: 'Document design must be an object.' };
    }
    for (const [dk, dv] of Object.entries(doc.design)) {
      if (!['theme', 'accentColor'].includes(dk)) {
        return { ok: false, error: `Unsupported design property: ${dk}` };
      }
      if (dk === 'theme' && (typeof dv !== 'string' || !VALID_DESIGN_THEMES.has(dv))) {
        return { ok: false, error: 'Document design.theme must be "light" or "dark".' };
      }
      if (dk === 'accentColor' && (typeof dv !== 'string' || !HEX_COLOR_RE.test(dv))) {
        return { ok: false, error: 'Document design.accentColor must be a #RRGGBB hex color.' };
      }
    }
  }
  const seen = new Set();
  let count = 0;

  function walk(node, depth) {
    if (!node || typeof node !== 'object') return 'Node must be an object.';
    if (depth > MAX_DEPTH) return `Maximum nesting depth (${MAX_DEPTH}) exceeded.`;
    count++;
    if (count > MAX_NODES) return `Maximum node count (${MAX_NODES}) exceeded.`;
    if (typeof node.id !== 'string' || node.id.length > 128 || !safeKey(node.id)) return 'Every node requires a string id.';
    if (seen.has(node.id)) return `Duplicate node id: ${node.id}`;
    seen.add(node.id);
    const meta = typeof node.type === 'string' && Object.hasOwn(STUDIO_CATALOG, node.type) && STUDIO_CATALOG[node.type];
    if (!meta) return `Unsupported component type: ${node.type}`;
    if (node.props !== undefined && (!record(node.props)
        || !Object.entries(node.props).every(([k, v]) => validProperty(meta, k, v)))) {
      return `Invalid properties for ${node.type}.`;
    }
    const children = node.children ?? [];
    if (!Array.isArray(children)) return `Node ${node.id} children must be an array.`;
    if (!meta.container && children.length > 0) return `${node.type} cannot contain child nodes.`;
    if (meta.maxChildren !== null && children.length > meta.maxChildren) {
      return `${node.type} accepts at most ${meta.maxChildren} child.`;
    }
    for (const child of children) {
      const err = walk(child, depth + 1);
      if (err) return err;
    }
    return null;
  }

  const error = walk(doc.root, 1);
  return error ? { ok: false, error } : { ok: true, count };
}

export function insertNode(doc, parentId, index, newNode) {
  const next = cloneDoc(doc);
  const parent = findNode(next.root, parentId);
  if (!parent) return { ok: false, error: 'Target container not found.' };
  if (!canAcceptChild(parent, 1)) return { ok: false, error: `${parent.type} cannot accept another child.` };
  const addedCount = countNodes(newNode);
  if (countNodes(next.root) + addedCount > MAX_NODES) {
    return { ok: false, error: `Maximum ${MAX_NODES} nodes reached.` };
  }
  const depthOfParent = nodeDepth(next.root, parentId);
  if (depthOfParent + subtreeHeight(newNode) > MAX_DEPTH) {
    return { ok: false, error: `Maximum depth (${MAX_DEPTH}) exceeded.` };
  }
  parent.children = parent.children || [];
  const clamped = Math.max(0, Math.min(parent.children.length, index ?? parent.children.length));
  parent.children.splice(clamped, 0, newNode);
  const check = validateDocument(next);
  if (!check.ok) return check;
  return { ok: true, document: next, nodeId: newNode.id };
}

export function moveNode(doc, nodeId, targetParentId, targetIndex) {
  if (!nodeId || nodeId === doc.root?.id) return { ok: false, error: 'Cannot move the root container.' };
  const next = cloneDoc(doc);
  const found = findParent(next.root, nodeId);
  if (!found || !found.parent) return { ok: false, error: 'Source node not found.' };
  if (nodeId === targetParentId || findNode(found.node, targetParentId)) {
    return { ok: false, error: 'Cannot move a container inside itself.' };
  }
  const targetParent = findNode(next.root, targetParentId);
  if (!targetParent) return { ok: false, error: 'Target container not found.' };

  const sameParent = found.parent.id === targetParentId;
  if (!sameParent && !canAcceptChild(targetParent, 1)) {
    return { ok: false, error: `${targetParent.type} cannot accept another child.` };
  }
  const depthTarget = nodeDepth(next.root, targetParentId);
  if (depthTarget + subtreeHeight(found.node) > MAX_DEPTH) {
    return { ok: false, error: `Maximum depth (${MAX_DEPTH}) exceeded.` };
  }

  found.parent.children.splice(found.index, 1);
  targetParent.children = targetParent.children || [];
  let insertAt = targetIndex ?? targetParent.children.length;
  if (sameParent && found.index < insertAt) insertAt--;
  insertAt = Math.max(0, Math.min(targetParent.children.length, insertAt));
  targetParent.children.splice(insertAt, 0, found.node);
  return { ok: true, document: next, nodeId };
}

export function removeNode(doc, nodeId) {
  if (!nodeId || nodeId === doc.root?.id) return { ok: false, error: 'Cannot delete the root container.' };
  const next = cloneDoc(doc);
  const found = findParent(next.root, nodeId);
  if (!found || !found.parent) return { ok: false, error: 'Node not found.' };
  found.parent.children.splice(found.index, 1);
  const siblings = found.parent.children;
  const fallback = siblings[Math.min(found.index, siblings.length - 1)]?.id || found.parent.id;
  return { ok: true, document: next, nextSelectedId: fallback };
}

export function duplicateNode(doc, nodeId) {
  if (!nodeId || nodeId === doc.root?.id) return { ok: false, error: 'Cannot duplicate root container.' };
  const found = findParent(doc.root, nodeId);
  if (!found || !found.parent) return { ok: false, error: 'Node not found.' };
  const used = collectIds(doc.root);
  function reid(src) {
    const slug = src.type.replace(/([a-z0-9])([A-Z])/g, '$1_$2').toLowerCase();
    let seq = 1;
    while (used.has(`${slug}_${seq}`)) seq++;
    const freshId = `${slug}_${seq}`;
    used.add(freshId);
    const copy = { id: freshId, type: src.type, props: structuredClone(src.props || {}) };
    if (src.children) copy.children = src.children.map(reid);
    return copy;
  }
  const clone = reid(found.node);
  if (found.parent.type === 'AnchorBox') {
    const pos = clone.props.anchorPosition || 'TopLeft';
    if (!['Fill', 'TopBar', 'BottomBar', 'LeftDock', 'RightDock'].includes(pos)) {
      if (!clone.props.anchorPosition) clone.props.anchorPosition = pos;
      if (clone.props.anchorMargin === undefined) {
        clone.props.anchorMargin = found.parent.props?.padding ?? 12;
      }
      const baseOy = found.node.props?.anchorOffsetY
        ?? (!found.node.props?.anchorPosition && pos === 'TopLeft' ? found.index * 44 : 0);
      clone.props.anchorOffsetX = clampNum((clone.props.anchorOffsetX ?? 0) + 24, -1200, 1200);
      clone.props.anchorOffsetY = clampNum(baseOy + 24, -1200, 1200);
    }
  }
  return insertNode(doc, found.parent.id, found.index + 1, clone);
}

export function updateNodeProps(doc, nodeId, patch) {
  const next = cloneDoc(doc);
  const target = findNode(next.root, nodeId);
  if (!target) return { ok: false, error: 'Node not found.' };
  const merged = { ...(target.props || {}) };
  for (const [key, val] of Object.entries(patch || {})) {
    if (!safeKey(key)) return { ok: false, error: 'Invalid property name.' };
    if (val === undefined) delete merged[key];
    else if (validProperty(STUDIO_CATALOG[target.type], key, val)) merged[key] = val;
    else return { ok: false, error: `Invalid ${target.type} property: ${key}` };
  }
  target.props = merged;
  return { ok: true, document: next };
}

export function resetNodeProps(doc, nodeId, preserveKeys = []) {
  const next = cloneDoc(doc);
  const target = findNode(next.root, nodeId);
  if (!target) return { ok: false, error: 'Node not found.' };
  const meta = STUDIO_CATALOG[target.type];
  const restored = cloneDoc(meta?.defaultProps || {});
  const prev = target.props || {};
  for (const key of preserveKeys) {
    if (Object.prototype.hasOwnProperty.call(prev, key)) restored[key] = prev[key];
  }
  target.props = restored;
  return { ok: true, document: next };
}

export function mapInteractionEventToProps(node, eventName, value) {
  if (!node || !eventName) return null;
  const toBool = (v) => (typeof v === 'string' ? (v !== 'false' && v !== '0' && v !== '') : Boolean(v));
  const toFiniteNum = (v) => {
    if (v === '' || v === null || v === undefined || typeof v === 'boolean') return null;
    const n = Number(v);
    return Number.isFinite(n) ? n : null;
  };
  const toFiniteInt = (v) => {
    const n = toFiniteNum(v);
    return n === null ? null : Math.trunc(n);
  };
  const type = node.type;
  if (type === 'ToggleSwitch' && eventName === 'toggled') {
    return { isOn: toBool(value) };
  }
  if (type === 'CheckBox' && eventName === 'toggled') {
    const on = toBool(value);
    return { checked: on, checkState: on ? 'Checked' : 'Unchecked' };
  }
  if (type === 'CheckBox' && eventName === 'checkStateChanged') {
    const key = String(value);
    if (!['Unchecked', 'PartiallyChecked', 'Checked'].includes(key)) return null;
    return { checked: key === 'Checked', checkState: key };
  }
  if ((type === 'RadioButton' || type === 'ToggleButton' || type === 'ToggleSplitButton' || type === 'Button')
      && eventName === 'toggled') {
    return { checked: toBool(value) };
  }
  if ((type === 'Slider' || type === 'NumberBox' || type === 'RatingControl') && eventName === 'valueChanged') {
    const num = toFiniteNum(value);
    return num === null ? null : { value: num };
  }
  if (type === 'RangeSlider' && eventName === 'valuesChanged') {
    const parts = String(value ?? '').split('..');
    if (parts.length !== 2) return null;
    const lo = toFiniteNum(parts[0]);
    const hi = toFiniteNum(parts[1]);
    return (lo !== null && hi !== null) ? { lowerValue: lo, upperValue: hi } : null;
  }
  if ((type === 'ComboBox' || type === 'FlipView') && eventName === 'currentIndexChanged') {
    const idx = toFiniteInt(value);
    return idx === null ? null : { currentIndex: idx };
  }
  if ((type === 'Expander' || type === 'CodeBlock') && eventName === 'expandedChanged') {
    return { expanded: Boolean(value) };
  }
  if ((type === 'LineEdit' || type === 'AutoSuggestBox') && eventName === 'textChanged') {
    return { text: String(value ?? '') };
  }
  if (type === 'TextEdit' && eventName === 'textChanged') {
    return { plainText: String(value ?? '') };
  }
  if (type === 'ColorPicker' && eventName === 'colorChanged') {
    return { color: String(value ?? '') };
  }
  if (type === 'CalendarDatePicker' && eventName === 'dateChanged') {
    return { date: String(value ?? '') };
  }
  if (type === 'CalendarView' && eventName === 'selectedDateChanged') {
    return { selectedDate: String(value ?? '') };
  }
  if ((type === 'Pivot' || type === 'SelectorBar' || type === 'TabView') && eventName === 'selectedIndexChanged') {
    const idx = toFiniteInt(value);
    return idx === null ? null : { selectedIndex: idx };
  }
  if (type === 'Stepper' && eventName === 'stepRequested') {
    const idx = toFiniteInt(value);
    return idx === null ? null : { currentIndex: idx };
  }
  if (type === 'PipsPager' && eventName === 'pageChanged') {
    const idx = toFiniteInt(value);
    return idx === null ? null : { selectedPageIndex: idx };
  }
  if (type === 'DataGridToolBar' && eventName === 'searchTextChanged') {
    return { searchText: String(value ?? '') };
  }
  return null;
}

export function createHistory(initialDocument, maxEntries = 60) {
  let states = [cloneDoc(initialDocument)];
  let cursor = 0;
  return {
    current: () => states[cursor],
    canUndo: () => cursor > 0,
    canRedo: () => cursor < states.length - 1,
    push(doc) {
      states = states.slice(0, cursor + 1);
      states.push(cloneDoc(doc));
      if (states.length > maxEntries) states.shift();
      cursor = states.length - 1;
      return states[cursor];
    },
    replaceCurrent(doc) {
      states = states.slice(0, cursor + 1);
      states[cursor] = cloneDoc(doc);
      return states[cursor];
    },
    undo() {
      if (cursor > 0) cursor--;
      return states[cursor];
    },
    redo() {
      if (cursor < states.length - 1) cursor++;
      return states[cursor];
    },
    reset(doc) {
      states = [cloneDoc(doc)];
      cursor = 0;
      return states[0];
    },
  };
}

function cppStr(str) {
  const escaped = String(str ?? '')
    .replace(/\\/g, '\\\\')
    .replace(/"/g, '\\"')
    .replace(/\r/g, '')
    .replace(/\n/g, '\\n');
  return `QStringLiteral("${escaped}")`;
}

function cppIconExpr(name) {
  return /^[A-Z]/.test(name) ? `Typography::Icons::${name}` : `Typography::Icons::glyph(${cppStr(name)})`;
}

function pyIconExpr(name) {
  return /^[A-Z]/.test(name) ? `Typography.Icons.${name}` : `Typography.Icons.glyph(${pyStr(name)})`;
}

function safeIdent(id, usedVars) {
  const clean = String(id || 'item').replace(/[^a-zA-Z0-9_]/g, '_').replace(/^([0-9])/, '_$1');
  let name = clean || 'item';
  let n = 2;
  while (usedVars.has(name)) name = `${clean}_${n++}`;
  usedVars.add(name);
  return name;
}

function schemaValuesEqual(a, b) {
  if (a === b) return true;
  if (Array.isArray(a) && Array.isArray(b)) {
    return a.length === b.length && a.every((v, idx) => v === b[idx]);
  }
  return false;
}

function collectPendingSchemaFields(node, schema, emittedKeys) {
  const props = node?.props || {};
  const schemaFields = schema?.properties;
  if (!Array.isArray(schemaFields) || !schemaFields.length) return [];
  const pending = [];
  const handledAxisProps = new Set();

  for (const field of schemaFields) {
    const id = field.id;
    if (!id || emittedKeys.has(id) || field.snapshotOnly || field.previewOnly || field.adapter) continue;
    if (['focusVisual', 'isOpen', 'open'].includes(id)) continue;

    if (field.sizeAxis || field.pointAxis || field.pointFAxis || field.vectorAxis) {
      const groupKey = field.property || (id.startsWith('iconOffset') ? 'iconOffset' : id);
      if (handledAxisProps.has(groupKey)) continue;
      const peers = schemaFields.filter((f) => (f.property || (f.id.startsWith('iconOffset') ? 'iconOffset' : f.id)) === groupKey);
      const anyModified = peers.some((f) => {
        if (!Object.prototype.hasOwnProperty.call(props, f.id) || props[f.id] === undefined) return false;
        return f.default === undefined || !schemaValuesEqual(props[f.id], f.default);
      });
      if (!anyModified) continue;
      handledAxisProps.add(groupKey);
      pending.push({ kind: 'axis-group', property: groupKey, setter: field.setter, peers });
      continue;
    }

    if (!Object.prototype.hasOwnProperty.call(props, id)) continue;
    const val = props[id];
    if (val === undefined) continue;
    if (field.default !== undefined && schemaValuesEqual(val, field.default)) continue;
    if (val === null && id !== 'cornerRadius') continue;
    pending.push({ kind: 'field', field, val });
  }
  return pending;
}

function toSetterName(propName) {
  if (!propName) return '';
  return `set${propName[0].toUpperCase()}${propName.slice(1)}`;
}

const CHART_TYPES = new Set(['LineChart', 'AreaChart', 'BarChart', 'HorizontalBarChart', 'PieChart', 'DonutChart', 'ScatterChart', 'Sparkline', 'ChartView']);
const OVERLAY_TYPES = new Set(['Menu', 'CommandBarFlyout', 'CommandPalette', 'Toast', 'ToolTip', 'DrawerView', 'ContentDialog', 'Dialog', 'Flyout', 'Popup', 'TeachingTip', 'CoachMark']);

export function generateCpp(doc, extraSchemas = {}) {
  requireValidDocument(doc);
  const headers = new Set(['<QWidget>', '<QVBoxLayout>']);
  const namespaces = new Set();
  const lines = [];
  const usedVars = new Set(['parent']);

  function resolveCppEnumPrefix(prefix) {
    if (!prefix) return '';
    for (const ns of namespaces) {
      const token = `${ns}::`;
      if (prefix.startsWith(token)) return prefix.slice(token.length);
    }
    if (prefix.startsWith('Q') && prefix.includes('::') && !prefix.startsWith('Qt::')) {
      const head = prefix.slice(0, prefix.indexOf('::'));
      headers.add(`<${head}>`);
    }
    return prefix;
  }

  function emitSchemaPropsCpp(v, node, emittedKeys) {
    const schema = extraSchemas?.[node.type];
    const pending = collectPendingSchemaFields(node, schema, emittedKeys);
    const p = node.props || {};
    for (const item of pending) {
      if (item.kind === 'axis-group') {
        const setter = item.setter || toSetterName(item.property);
        if (!setter) continue;
        const axisVal = (axisKey) => {
          const peer = item.peers.find((f) => (f.pointAxis || f.sizeAxis || f.pointFAxis || f.vectorAxis || (f.id.endsWith('X') ? 'x' : 'y')) === axisKey);
          if (!peer) return 0;
          return p[peer.id] ?? peer.default ?? 0;
        };
        const first = item.peers[0];
        if (first.sizeAxis) {
          headers.add('<QSize>');
          lines.push(`${v}->${setter}(QSize(${Math.round(axisVal('width'))}, ${Math.round(axisVal('height'))}));`);
        } else if (first.pointFAxis) {
          headers.add('<QPointF>');
          lines.push(`${v}->${setter}(QPointF(${Number(axisVal('x'))}, ${Number(axisVal('y'))}));`);
        } else if (first.vectorAxis) {
          headers.add('<QVector3D>');
          lines.push(`${v}->${setter}(QVector3D(${Number(axisVal('x'))}, ${Number(axisVal('y'))}, ${Number(axisVal('z'))}));`);
        } else {
          headers.add('<QPoint>');
          lines.push(`${v}->${setter}(QPoint(${Math.round(axisVal('x'))}, ${Math.round(axisVal('y'))}));`);
        }
        continue;
      }

      const { field, val } = item;
      const id = field.id;
      if (id === 'icon') {
        if (typeof val === 'string' && safeKey(val) && val !== 'None') {
          headers.add('<FluentQt/Design.h>');
          lines.push(`${v}->setIconGlyph(${cppIconExpr(val)});`);
        }
        continue;
      }
      if (id === 'cornerRadius') {
        if (val !== null) {
          const r = Math.round(Number(val) || 0);
          headers.add('<QMargins>');
          lines.push(`${v}->setCornerRadii(QMargins(${r}, ${r}, ${r}, ${r}));`);
        }
        continue;
      }

      const setter = field.setter || toSetterName(field.property || id);
      if (!setter) continue;
      if ((field.glyphEnum || field.type === 'enum') && !safeKey(String(val))) continue;
      if (field.glyphEnum) {
        if (typeof val === 'string' && safeKey(val) && val !== 'None') {
          headers.add('<FluentQt/Design.h>');
          lines.push(`${v}->${setter}(${cppIconExpr(val)});`);
        }
      } else if (field.type === 'enum') {
        const prefix = resolveCppEnumPrefix(field.cppPrefix || '');
        lines.push(`${v}->${setter}(${prefix}${val});`);
      } else if (field.type === 'boolean') {
        lines.push(`${v}->${setter}(${val ? 'true' : 'false'});`);
      } else if (field.type === 'number') {
        const num = field.integer ? Math.round(Number(val)) : Number(val);
        if (Number.isFinite(num)) lines.push(`${v}->${setter}(${num});`);
      } else if (field.type === 'text') {
        lines.push(`${v}->${setter}(${cppStr(val)});`);
      } else if (field.type === 'color') {
        headers.add('<QColor>');
        lines.push(`${v}->${setter}(QColor(${cppStr(val)}));`);
      } else if (field.type === 'date') {
        headers.add('<QDate>');
        lines.push(`${v}->${setter}(QDate::fromString(${cppStr(val)}, Qt::ISODate));`);
      } else if (field.type === 'time') {
        headers.add('<QTime>');
        lines.push(`${v}->${setter}(QTime::fromString(${cppStr(val)}, Qt::ISODate));`);
      } else if (field.type === 'datetime') {
        headers.add('<QDateTime>');
        lines.push(`${v}->${setter}(QDateTime::fromString(${cppStr(val)}, Qt::ISODate));`);
      } else if (field.type === 'string-list' && Array.isArray(val) && !field.itemList) {
        headers.add('<QStringList>');
        lines.push(`${v}->${setter}(QStringList{${val.map(cppStr).join(', ')}});`);
      }
    }
  }

  function registerMeta(meta) {
    if (meta.cppHeader) headers.add(meta.cppHeader);
    if (meta.cppClass && meta.cppClass.includes('::')) {
      namespaces.add(meta.cppClass.slice(0, meta.cppClass.lastIndexOf('::')));
    } else if (meta.cppClass === 'fluent::FontIcon') {
      namespaces.add('fluent');
    }
  }

  function scanAndEmit(node, varParent, addExpr = null) {
    const meta = STUDIO_CATALOG[node.type];
    const p = node.props || {};
    const v = varParent || safeIdent(node.id, usedVars);
    registerMeta(meta);
    const parentArg = addExpr ? `${addExpr.parentArg}` : 'parent';

    if (node.type === 'VBox' || node.type === 'HBox' || node.type === 'Grid') {
      const layVar = `${v}Layout`;
      const layCls = node.type === 'VBox' ? 'QVBoxLayout' : (node.type === 'HBox' ? 'QHBoxLayout' : 'QGridLayout');
      headers.add(`<${layCls}>`);
      const pad = p.padding ?? (node.type === 'VBox' ? 16 : 0);
      const spacing = p.spacing ?? 12;
      lines.push(`auto* ${v} = new QWidget(${parentArg});`);
      lines.push(`auto* ${layVar} = new ${layCls}(${v});`);
      lines.push(`${layVar}->setContentsMargins(${pad}, ${pad}, ${pad}, ${pad});`);
      lines.push(`${layVar}->setSpacing(${spacing});`);
      (node.children || []).forEach((child, idx) => {
        const cv = safeIdent(child.id, usedVars);
        scanAndEmit(child, cv, { parentArg: v });
        if (node.type === 'Grid') {
          const cols = Math.max(1, Math.min(4, p.columns ?? 2));
          lines.push(`${layVar}->addWidget(${cv}, ${Math.floor(idx / cols)}, ${idx % cols});`);
        } else {
          lines.push(`${layVar}->addWidget(${cv});`);
        }
      });
      if (node.type === 'HBox' && ['Start', 'Center', 'End'].includes(p.align)) lines.push(`${layVar}->addStretch(1);`);
      return v;
    }

    if (node.type === 'AnchorBox') {
      headers.add('<QMargins>');
      const layVar = `${v}Layout`;
      const pad = p.padding ?? 12;
      lines.push(`auto* ${v} = new QWidget(${parentArg});`);
      lines.push(`${v}->setMinimumHeight(${p.minHeight ?? 220});`);
      lines.push(`auto* ${layVar} = new AnchorLayout(${v});`);
      (node.children || []).forEach((child, idx) => {
        const cv = safeIdent(child.id, usedVars);
        const cp = child.props || {};
        scanAndEmit(child, cv, { parentArg: v });
        const aVar = `${cv}Anchors`;
        const m = cp.anchorMargin ?? pad;
        const ox = cp.anchorOffsetX ?? 0;
        const pos = cp.anchorPosition || ((idx === 0 && (child.type === 'ParticleBackdrop' || child.type === 'Card')) ? 'Fill' : 'TopLeft');
        const oy = cp.anchorOffsetY ?? ((pos === 'TopLeft' && !cp.anchorPosition) ? idx * 44 : 0);
        lines.push(`AnchorLayout::Anchors ${aVar};`);
        if (pos === 'Fill') {
          lines.push(`${aVar}.fill = true;`);
          lines.push(`${aVar}.fillMargins = QMargins(${m}, ${m}, ${m}, ${m});`);
        } else if (pos === 'TopCenter') {
          lines.push(`${aVar}.top = {${v}, AnchorLayout::Edge::Top, ${m + oy}};`);
          lines.push(`${aVar}.horizontalCenter = {${v}, AnchorLayout::Edge::HCenter, ${ox}};`);
        } else if (pos === 'TopRight') {
          lines.push(`${aVar}.top = {${v}, AnchorLayout::Edge::Top, ${m + oy}};`);
          lines.push(`${aVar}.right = {${v}, AnchorLayout::Edge::Right, ${-m + ox}};`);
        } else if (pos === 'Center') {
          lines.push(`${aVar}.horizontalCenter = {${v}, AnchorLayout::Edge::HCenter, ${ox}};`);
          lines.push(`${aVar}.verticalCenter = {${v}, AnchorLayout::Edge::VCenter, ${oy}};`);
        } else if (pos === 'BottomLeft') {
          lines.push(`${aVar}.bottom = {${v}, AnchorLayout::Edge::Bottom, ${-m + oy}};`);
          lines.push(`${aVar}.left = {${v}, AnchorLayout::Edge::Left, ${m + ox}};`);
        } else if (pos === 'BottomCenter') {
          lines.push(`${aVar}.bottom = {${v}, AnchorLayout::Edge::Bottom, ${-m + oy}};`);
          lines.push(`${aVar}.horizontalCenter = {${v}, AnchorLayout::Edge::HCenter, ${ox}};`);
        } else if (pos === 'BottomRight') {
          lines.push(`${aVar}.bottom = {${v}, AnchorLayout::Edge::Bottom, ${-m + oy}};`);
          lines.push(`${aVar}.right = {${v}, AnchorLayout::Edge::Right, ${-m + ox}};`);
        } else if (pos === 'TopBar') {
          lines.push(`${aVar}.top = {${v}, AnchorLayout::Edge::Top, ${m + oy}};`);
          lines.push(`${aVar}.left = {${v}, AnchorLayout::Edge::Left, ${m}};`);
          lines.push(`${aVar}.right = {${v}, AnchorLayout::Edge::Right, ${-m}};`);
        } else if (pos === 'BottomBar') {
          lines.push(`${aVar}.bottom = {${v}, AnchorLayout::Edge::Bottom, ${-m + oy}};`);
          lines.push(`${aVar}.left = {${v}, AnchorLayout::Edge::Left, ${m}};`);
          lines.push(`${aVar}.right = {${v}, AnchorLayout::Edge::Right, ${-m}};`);
        } else if (pos === 'LeftDock') {
          lines.push(`${aVar}.left = {${v}, AnchorLayout::Edge::Left, ${m + ox}};`);
          lines.push(`${aVar}.top = {${v}, AnchorLayout::Edge::Top, ${m}};`);
          lines.push(`${aVar}.bottom = {${v}, AnchorLayout::Edge::Bottom, ${-m}};`);
        } else if (pos === 'RightDock') {
          lines.push(`${aVar}.right = {${v}, AnchorLayout::Edge::Right, ${-m + ox}};`);
          lines.push(`${aVar}.top = {${v}, AnchorLayout::Edge::Top, ${m}};`);
          lines.push(`${aVar}.bottom = {${v}, AnchorLayout::Edge::Bottom, ${-m}};`);
        } else {
          lines.push(`${aVar}.top = {${v}, AnchorLayout::Edge::Top, ${m + oy}};`);
          lines.push(`${aVar}.left = {${v}, AnchorLayout::Edge::Left, ${m + ox}};`);
        }
        lines.push(`${layVar}->addAnchoredWidget(${cv}, ${aVar});`);
      });
      return v;
    }

    if (node.type === 'Window') {
      const bodyVar = `${v}Body`;
      const layVar = `${v}Layout`;
      const pad = p.padding ?? 16;
      lines.push(`auto* ${v} = new Window(${parentArg});`);
      lines.push(`${v}->setWindowTitle(${cppStr(p.title ?? 'Fluent Window')});`);
      if (p.backdropEffect && p.backdropEffect !== 'Mica') {
        lines.push(`${v}->setBackdropEffect(BackdropEffect::${p.backdropEffect});`);
      }
      lines.push(`auto* ${bodyVar} = new QWidget(${v});`);
      lines.push(`auto* ${layVar} = new QVBoxLayout(${bodyVar});`);
      lines.push(`${layVar}->setContentsMargins(${pad}, ${pad}, ${pad}, ${pad});`);
      lines.push(`${layVar}->setSpacing(${p.spacing ?? 12});`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        scanAndEmit(child, cv, { parentArg: bodyVar });
        lines.push(`${layVar}->addWidget(${cv});`);
      }
      lines.push(`${v}->setContentWidget(${bodyVar});`);
      emitSchemaPropsCpp(v, node, new Set(['title', 'backdropEffect', 'minHeight', 'padding', 'spacing']));
      return v;
    }

    if (node.type === 'ParticleBackdrop') {
      const layVar = `${v}Layout`;
      const pad = p.padding ?? 16;
      lines.push(`auto* ${v} = new ParticleBackdrop(${parentArg});`);
      if (p.effect) lines.push(`${v}->setEffect(ParticleBackdrop::${p.effect});`);
      if (p.backgroundMode) lines.push(`${v}->setBackgroundMode(ParticleBackdrop::${p.backgroundMode});`);
      lines.push(`${v}->setParticleCount(${p.particleCount ?? 180});`);
      if (p.animationEnabled === false) lines.push(`${v}->setAnimationEnabled(false);`);
      if (p.interactive === false) lines.push(`${v}->setInteractive(false);`);
      lines.push(`${v}->setMinimumHeight(${p.minHeight ?? 200});`);
      lines.push(`auto* ${layVar} = new QVBoxLayout(${v});`);
      lines.push(`${layVar}->setContentsMargins(${pad}, ${pad}, ${pad}, ${pad});`);
      lines.push(`${layVar}->setSpacing(${p.spacing ?? 10});`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        scanAndEmit(child, cv, { parentArg: v });
        lines.push(`${layVar}->addWidget(${cv});`);
      }
      emitSchemaPropsCpp(v, node, new Set(['effect', 'backgroundMode', 'particleCount', 'animationEnabled', 'interactive', 'minHeight', 'padding', 'spacing']));
      return v;
    }

    if (node.type === 'SpatialView' || node.type === 'SpatialItem') {
      headers.add('<FluentQt/Foundation.h>');
      headers.add('<QVector3D>');
      const sessVar = `${v}Session`;
      const viewVar = v;
      lines.push(`auto* ${sessVar} = new SpatialSession(${parentArg}, ${parentArg});`);
      lines.push(`${sessVar}->activate();`);
      lines.push(`auto* ${viewVar} = ${sessVar}->isActive() ? ${sessVar}->createView(${parentArg}) : new SpatialView(${parentArg});`);
      if (node.type === 'SpatialView') {
        const zoomNum = Number(p.zoom ?? 100);
        const normZoom = zoomNum <= 4 ? zoomNum : (zoomNum / 100);
        lines.push(`${viewVar}->setSpatialEnabled(${p.spatialEnabled !== false ? 'true' : 'false'});`);
        lines.push(`${viewVar}->setCameraDistance(${p.cameraDistance ?? 1000});`);
        lines.push(`${viewVar}->setZoom(${normZoom.toFixed(2)});`);
        lines.push(`${viewVar}->setPointerTrackingEnabled(${p.pointerTrackingEnabled !== false ? 'true' : 'false'});`);
        lines.push(`${viewVar}->setMinimumHeight(${p.minHeight ?? 260});`);
        (node.children || []).forEach((child) => {
          if (child.type === 'SpatialItem') {
            const inner = (child.children || [])[0];
            const cv = safeIdent(`${child.id}_content`, usedVars);
            const itemVar = safeIdent(child.id, usedVars);
            if (inner) scanAndEmit(inner, cv, { parentArg: 'nullptr' });
            else lines.push(`auto* ${cv} = new Card;`);
            const cp = child.props || {};
            const rawScale = Number(cp.scale ?? 100);
            const rawSurf = Number(cp.surfaceIntensity ?? 75);
            lines.push(`auto* ${itemVar} = ${viewVar}->addWidget(${cv}, fluent::WidgetOwnership::Owned);`);
            lines.push(`${itemVar}->setPosition(QVector3D(${cp.positionX ?? cp.posX ?? 0}, ${cp.positionY ?? cp.posY ?? 0}, ${cp.positionZ ?? cp.posZ ?? 40}));`);
            lines.push(`${itemVar}->setRotation(QVector3D(${cp.rotationX ?? cp.rotX ?? 6}, ${cp.rotationY ?? cp.rotY ?? -16}, ${cp.rotationZ ?? cp.rotZ ?? 0}));`);
            lines.push(`${itemVar}->setScale(${(rawScale <= 4 ? rawScale : (rawScale / 100)).toFixed(2)});`);
            lines.push(`${itemVar}->setSurfaceIntensity(${(rawSurf <= 1 ? rawSurf : (rawSurf / 100)).toFixed(2)});`);
            lines.push(`${itemVar}->setHoverLift(${cp.hoverLift ?? 6});`);
            emitSchemaPropsCpp(itemVar, child, new Set(['posX', 'posY', 'posZ', 'rotX', 'rotY', 'rotZ', 'positionX', 'positionY', 'positionZ', 'rotationX', 'rotationY', 'rotationZ', 'scale', 'surfaceIntensity', 'hoverLift']));
          } else {
            const cv = safeIdent(child.id, usedVars);
            const itemVar = `${cv}Item`;
            scanAndEmit(child, cv, { parentArg: 'nullptr' });
            lines.push(`auto* ${itemVar} = ${viewVar}->addWidget(${cv}, fluent::WidgetOwnership::Owned);`);
            lines.push(`${itemVar}->setSurfaceIntensity(0.75);`);
          }
        });
        emitSchemaPropsCpp(viewVar, node, new Set(['spatialEnabled', 'cameraDistance', 'zoom', 'pointerTrackingEnabled', 'minHeight']));
      } else {
        const inner = (node.children || [])[0];
        const cv = safeIdent(`${node.id}_content`, usedVars);
        const item = safeIdent(`${node.id}_item`, usedVars);
        if (inner) scanAndEmit(inner, cv, { parentArg: 'nullptr' });
        else lines.push(`auto* ${cv} = new Card;`);
        const rawScale = Number(p.scale ?? 100);
        const rawSurf = Number(p.surfaceIntensity ?? 75);
        lines.push(`auto* ${item} = ${viewVar}->addWidget(${cv}, fluent::WidgetOwnership::Owned);`);
        lines.push(`${item}->setPosition(QVector3D(${p.positionX ?? p.posX ?? 0}, ${p.positionY ?? p.posY ?? 0}, ${p.positionZ ?? p.posZ ?? 40}));`);
        lines.push(`${item}->setRotation(QVector3D(${p.rotationX ?? p.rotX ?? 6}, ${p.rotationY ?? p.rotY ?? -16}, ${p.rotationZ ?? p.rotZ ?? 0}));`);
        lines.push(`${item}->setScale(${(rawScale <= 4 ? rawScale : (rawScale / 100)).toFixed(2)});`);
        lines.push(`${item}->setSurfaceIntensity(${(rawSurf <= 1 ? rawSurf : (rawSurf / 100)).toFixed(2)});`);
        lines.push(`${item}->setHoverLift(${p.hoverLift ?? 6});`);
        emitSchemaPropsCpp(item, node, new Set(['posX', 'posY', 'posZ', 'rotX', 'rotY', 'rotZ', 'positionX', 'positionY', 'positionZ', 'rotationX', 'rotationY', 'rotationZ', 'scale', 'surfaceIntensity', 'hoverLift']));
        return viewVar;
      }
      return viewVar;
    }

    if (node.type === 'Card') {
      const layVar = `${v}Layout`;
      lines.push(`auto* ${v} = new Card(${parentArg});`);
      if (p.appearance && p.appearance !== 'Layer') lines.push(`${v}->setAppearance(Card::${p.appearance});`);
      if (p.borderVisible === false) lines.push(`${v}->setBorderVisible(false);`);
      emitSchemaPropsCpp(v, node, new Set(['appearance', 'borderVisible', 'spacing', 'padding']));
      lines.push(`auto* ${layVar} = new QVBoxLayout(${v});`);
      const pad = p.padding ?? 16;
      lines.push(`${layVar}->setContentsMargins(${pad}, ${pad}, ${pad}, ${pad});`);
      lines.push(`${layVar}->setSpacing(${p.spacing ?? 10});`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        scanAndEmit(child, cv, { parentArg: v });
        lines.push(`${layVar}->addWidget(${cv});`);
      }
      return v;
    }

    if (node.type === 'Expander') {
      headers.add('<FluentQt/Foundation.h>');
      const bodyVar = `${v}Body`;
      const layVar = `${v}Layout`;
      lines.push(`auto* ${v} = new Expander(${parentArg});`);
      lines.push(`${v}->setHeaderText(${cppStr(p.headerText ?? 'Section')});`);
      lines.push(`${v}->setExpanded(${p.expanded !== false ? 'true' : 'false'});`);
      emitSchemaPropsCpp(v, node, new Set(['headerText', 'expanded']));
      lines.push(`auto* ${bodyVar} = new QWidget;`);
      lines.push(`auto* ${layVar} = new QVBoxLayout(${bodyVar});`);
      lines.push(`${layVar}->setContentsMargins(16, 12, 16, 14);`);
      lines.push(`${layVar}->setSpacing(10);`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        scanAndEmit(child, cv, { parentArg: bodyVar });
        lines.push(`${layVar}->addWidget(${cv});`);
      }
      lines.push(`${v}->setContentWidget(${bodyVar}, fluent::WidgetOwnership::Owned);`);
      return v;
    }

    if (node.type === 'Field') {
      headers.add('<FluentQt/Foundation.h>');
      lines.push(`auto* ${v} = new Field(${parentArg});`);
      lines.push(`${v}->setLabelText(${cppStr(p.labelText ?? 'Field')});`);
      if (p.helperText) lines.push(`${v}->setHelperText(${cppStr(p.helperText)});`);
      if (p.required) lines.push(`${v}->setRequired(true);`);
      if (p.labelPosition === 'Leading') lines.push(`${v}->setLabelPosition(Field::LabelPosition::Leading);`);
      if (p.validationState && p.validationState !== 'None') lines.push(`${v}->setValidationState(Field::ValidationState::${p.validationState});`);
      if (p.validationMessage) lines.push(`${v}->setValidationMessage(${cppStr(p.validationMessage)});`);
      emitSchemaPropsCpp(v, node, new Set(['labelText', 'helperText', 'required', 'labelPosition', 'validationState', 'validationMessage']));
      if ((node.children || []).length > 0) {
        const ed = node.children[0];
        const edVar = safeIdent(ed.id, usedVars);
        scanAndEmit(ed, edVar, { parentArg: 'nullptr' });
        lines.push(`${v}->setEditor(${edVar}, fluent::WidgetOwnership::Owned);`);
      }
      return v;
    }

    if (node.type === 'Accordion' || node.type === 'ScrollView' || node.type === 'SplitView' || node.type === 'FlipView' || node.type === 'StackView') {
      headers.add('<FluentQt/Foundation.h>');
      lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
      if (node.type === 'Accordion' && p.expansionMode) lines.push(`${v}->setExpansionMode(Accordion::ExpansionMode::${p.expansionMode});`);
      if (node.type === 'SplitView' && p.orientation === 'Vertical') lines.push(`${v}->setOrientation(Qt::Vertical);`);
      emitSchemaPropsCpp(v, node, new Set(['expansionMode', 'orientation', 'minHeight']));
      const children = node.children || [];
      if (node.type === 'ScrollView') {
        const inner = `${v}Content`;
        const ilay = `${v}Layout`;
        lines.push(`auto* ${inner} = new QWidget;`);
        lines.push(`auto* ${ilay} = new QVBoxLayout(${inner});`);
        for (const child of children) {
          const cv = safeIdent(child.id, usedVars);
          scanAndEmit(child, cv, { parentArg: inner });
          lines.push(`${ilay}->addWidget(${cv});`);
        }
        lines.push(`${v}->setContentWidget(${inner}, fluent::WidgetOwnership::Owned);`);
      } else {
        children.forEach((child, idx) => {
          const cv = safeIdent(child.id, usedVars);
          scanAndEmit(child, cv, { parentArg: v });
          if (node.type === 'Accordion') lines.push(`${v}->addItem(${cv}, fluent::WidgetOwnership::Owned);`);
          else if (node.type === 'SplitView') lines.push(`${v}->addPane(${cv}, fluent::WidgetOwnership::Owned);`);
          else if (node.type === 'FlipView') lines.push(`${v}->addPage(${cv}, fluent::WidgetOwnership::Owned);`);
          else if (idx === 0) lines.push(`${v}->setInitialItem(${cv}, fluent::WidgetOwnership::Owned);`);
          else lines.push(`${v}->push(${cv}, fluent::WidgetOwnership::Owned);`);
        });
      }
      return v;
    }

    if (node.type === 'ListDetailView' || node.type === 'NavigationView') {
      headers.add('<FluentQt/Foundation.h>');
      lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
      if (node.type === 'ListDetailView') {
        lines.push(`${v}->setListPaneWidth(${p.listPaneWidth ?? 220});`);
      } else {
        if (p.displayMode) lines.push(`${v}->setDisplayMode(NavigationView::DisplayMode::${p.displayMode});`);
      }
      emitSchemaPropsCpp(v, node, new Set(['listPaneWidth', 'displayMode', 'items', 'minHeight']));
      (node.children || []).forEach((child, idx) => {
        const cv = safeIdent(child.id, usedVars);
        scanAndEmit(child, cv, { parentArg: v });
        if (node.type === 'NavigationView') lines.push(`${v}->contentHost()->insertPage(${idx}, ${cv}, fluent::WidgetOwnership::Owned);`);
      });
      if (node.type === 'NavigationView' && (node.children || []).length > 0) {
        lines.push(`${v}->contentHost()->setCurrentIndex(0, 0, false);`);
      }
      return v;
    }

    if (CHART_TYPES.has(node.type)) {
      headers.add('<QPointF>');
      const mv = `${v}Model`;
      const vals = (p.values || [24, 42, 38, 64]).map((n, i) => `QPointF(${i + 1}, ${Number(n) || 0})`).join(', ');
      lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
      if (node.type === 'ChartView' && p.chartType) lines.push(`${v}->setChartType(ChartView::${p.chartType});`);
      lines.push(`auto* ${mv} = new ChartModel(${v});`);
      lines.push(`${mv}->setName(${cppStr(p.seriesName ?? 'Series')});`);
      lines.push(`${mv}->setDataSnapshot(ChartData::fromPoints({${vals}}));`);
      lines.push(`${v}->setModel(${mv});`);
      if (p.title) lines.push(`${v}->setTitle(${cppStr(p.title)});`);
      lines.push(`${v}->setMinimumHeight(${p.minHeight ?? (node.type === 'Sparkline' ? 76 : 200)});`);
      emitSchemaPropsCpp(v, node, new Set(['chartType', 'seriesName', 'values', 'title', 'minHeight']));
      return v;
    }

    if (OVERLAY_TYPES.has(node.type)) {
      headers.add('<FluentQt/BasicInput.h>');
      namespaces.add('fluent::basicinput');
      lines.push(`auto* ${v} = new Button(${cppStr(p.text ?? `Open ${node.type}`)}, ${parentArg});`);
      lines.push(`QObject::connect(${v}, &Button::clicked, ${v}, [${v}]() { /* Open ${node.type} */ });`);
      return v;
    }

    const emitted = new Set(['enabled']);
    switch (node.type) {
      case 'Divider':
        lines.push(`auto* ${v} = new Divider(${p.orientation === 'Vertical' ? 'Qt::Vertical, ' : ''}${parentArg});`);
        emitted.add('orientation');
        break;
      case 'Button':
      case 'ToggleButton':
      case 'HyperlinkButton':
      case 'RepeatButton':
        lines.push(`auto* ${v} = new ${node.type}(${cppStr(p.text ?? node.type)}, ${parentArg});`);
        emitted.add('text');
        if (node.type === 'Button' && p.fluentStyle && p.fluentStyle !== 'Standard') lines.push(`${v}->setFluentStyle(Button::${p.fluentStyle});`);
        if (node.type === 'Button' && p.fluentSize && p.fluentSize !== 'StandardSize') lines.push(`${v}->setFluentSize(Button::${p.fluentSize});`);
        if (node.type === 'Button' && typeof p.icon === 'string' && safeKey(p.icon) && p.icon !== 'None') {
          headers.add('<FluentQt/Design.h>');
          lines.push(`${v}->setIconGlyph(${cppIconExpr(p.icon)});`);
        }
        emitted.add('fluentStyle');
        emitted.add('fluentSize');
        emitted.add('icon');
        if (node.type === 'ToggleButton') {
          if (p.checked) lines.push(`${v}->setChecked(true);`);
          emitted.add('checked');
        }
        break;
      case 'CompoundButton':
        lines.push(`auto* ${v} = new CompoundButton(${cppStr(p.text ?? 'Action')}, ${cppStr(p.secondaryText ?? '')}, ${parentArg});`);
        emitted.add('text');
        emitted.add('secondaryText');
        break;
      case 'DropDownButton':
      case 'SplitButton':
      case 'ToggleSplitButton': {
        headers.add('<FluentQt/MenusToolbars.h>');
        namespaces.add('fluent::menus_toolbars');
        const mv = `${v}Menu`;
        lines.push(`auto* ${v} = new ${node.type}(${cppStr(p.text ?? node.type)}, ${parentArg});`);
        lines.push(`auto* ${mv} = new FluentMenu(QString(), ${v});`);
        for (const item of p.items || ['Action 1', 'Action 2']) lines.push(`${mv}->addAction(${cppStr(item)});`);
        lines.push(`${v}->setMenu(${mv});`);
        emitted.add('text');
        emitted.add('items');
        break;
      }
      case 'CheckBox':
        lines.push(`auto* ${v} = new CheckBox(${cppStr(p.text ?? 'CheckBox')}, ${parentArg});`);
        if (p.checkState && p.checkState !== 'Unchecked') lines.push(`${v}->setCheckState(Qt::${p.checkState});`);
        else if (p.checked && !p.checkState) lines.push(`${v}->setChecked(true);`);
        emitted.add('text');
        emitted.add('checked');
        emitted.add('checkState');
        break;
      case 'RadioButton':
        lines.push(`auto* ${v} = new RadioButton(${cppStr(p.text ?? node.type)}, ${parentArg});`);
        if (p.checked) lines.push(`${v}->setChecked(true);`);
        emitted.add('text');
        emitted.add('checked');
        break;
      case 'ToggleSwitch':
        lines.push(`auto* ${v} = new ToggleSwitch(${parentArg});`);
        if (p.onContent && p.onContent !== 'On') lines.push(`${v}->setOnContent(${cppStr(p.onContent)});`);
        if (p.offContent && p.offContent !== 'Off') lines.push(`${v}->setOffContent(${cppStr(p.offContent)});`);
        if (p.isOn) lines.push(`${v}->setIsOn(true);`);
        emitted.add('onContent');
        emitted.add('offContent');
        emitted.add('isOn');
        break;
      case 'ComboBox':
        lines.push(`auto* ${v} = new ComboBox(${parentArg});`);
        lines.push(`${v}->addItems({${(p.items || ['Option A', 'Option B']).map(cppStr).join(', ')}});`);
        if (p.currentIndex !== undefined && p.currentIndex !== 0) lines.push(`${v}->setCurrentIndex(${p.currentIndex});`);
        emitted.add('items');
        emitted.add('currentIndex');
        break;
      case 'Slider':
      case 'RangeSlider':
        lines.push(`auto* ${v} = new ${node.type}(${p.orientation === 'Vertical' ? 'Qt::Vertical' : 'Qt::Horizontal'}, ${parentArg});`);
        lines.push(`${v}->setRange(${p.minimum ?? 0}, ${p.maximum ?? 100});`);
        if (node.type === 'Slider') lines.push(`${v}->setValue(${p.value ?? 45});`);
        else lines.push(`${v}->setValues(${p.lowerValue ?? 20}, ${p.upperValue ?? 75});`);
        emitted.add('orientation');
        emitted.add('minimum');
        emitted.add('maximum');
        emitted.add('value');
        emitted.add('lowerValue');
        emitted.add('upperValue');
        break;
      case 'RatingControl':
        lines.push(`auto* ${v} = new RatingControl(${parentArg});`);
        lines.push(`${v}->setValue(${p.value ?? 4});`);
        emitted.add('value');
        break;
      case 'ColorPicker':
        headers.add('<QColor>');
        lines.push(`auto* ${v} = new ColorPicker(${parentArg});`);
        lines.push(`${v}->setColor(QColor(${cppStr(p.color ?? '#0062B0')}));`);
        emitted.add('color');
        break;
      case 'FileDropZone':
        lines.push(`auto* ${v} = new FileDropZone(${parentArg});`);
        if (p.title) lines.push(`${v}->setTitle(${cppStr(p.title)});`);
        if (p.description) lines.push(`${v}->setDescription(${cppStr(p.description)});`);
        emitted.add('title');
        emitted.add('description');
        break;
      case 'Label': {
        lines.push(`auto* ${v} = new Label(${cppStr(p.text ?? 'Label')}, ${parentArg});`);
        const typo = p.fluentTypography || p.typography;
        if (typo && typo !== 'Body') {
          headers.add('<FluentQt/Design.h>');
          lines.push(`${v}->setFluentTypography(Typography::FontRole::${typo});`);
        }
        if (p.colorRole && p.colorRole !== 'Default') lines.push(`${v}->setTextColorRole(Label::TextColorRole::${p.colorRole});`);
        emitted.add('text');
        emitted.add('typography');
        emitted.add('fluentTypography');
        emitted.add('colorRole');
        break;
      }
      case 'LineEdit':
      case 'PasswordBox':
      case 'AutoSuggestBox':
        lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
        if (p.header) lines.push(`${v}->setHeader(${cppStr(p.header)});`);
        if (p.placeholderText) lines.push(`${v}->setPlaceholderText(${cppStr(p.placeholderText)});`);
        if (node.type === 'LineEdit' && p.text) lines.push(`${v}->setText(${cppStr(p.text)});`);
        if (node.type === 'AutoSuggestBox') lines.push(`${v}->setSuggestions({${(p.suggestions || []).map(cppStr).join(', ')}});`);
        emitted.add('header');
        emitted.add('placeholderText');
        emitted.add('text');
        emitted.add('suggestions');
        break;
      case 'NumberBox':
        lines.push(`auto* ${v} = new NumberBox(${parentArg});`);
        lines.push(`${v}->setRange(${p.minimum ?? 0}, ${p.maximum ?? 1000});`);
        lines.push(`${v}->setValue(${p.value ?? 24});`);
        emitted.add('minimum');
        emitted.add('maximum');
        emitted.add('value');
        break;
      case 'TextEdit':
        lines.push(`auto* ${v} = new TextEdit(${parentArg});`);
        if (p.placeholderText) lines.push(`${v}->setPlaceholderText(${cppStr(p.placeholderText)});`);
        if (p.plainText) lines.push(`${v}->setPlainText(${cppStr(p.plainText)});`);
        emitted.add('placeholderText');
        emitted.add('plainText');
        break;
      case 'TagInput':
        lines.push(`auto* ${v} = new TagInput(${parentArg});`);
        for (const item of p.items || []) lines.push(`${v}->addItem(${cppStr(item.toLowerCase())}, ${cppStr(item)});`);
        emitted.add('items');
        break;
      case 'CodeBlock':
        lines.push(`auto* ${v} = new CodeBlock(${cppStr(p.code ?? '')}, ${parentArg});`);
        if (p.headerText) lines.push(`${v}->setHeaderText(${cppStr(p.headerText)});`);
        if (p.language) lines.push(`${v}->setLanguage(${cppStr(p.language)});`);
        emitted.add('code');
        emitted.add('headerText');
        emitted.add('language');
        break;
      case 'InfoBar':
        lines.push(`auto* ${v} = new InfoBar(${parentArg});`);
        lines.push(`${v}->setTitle(${cppStr(p.title ?? 'Status')});`);
        lines.push(`${v}->setMessage(${cppStr(p.message ?? '')});`);
        if (p.severity && p.severity !== 'Informational') lines.push(`${v}->setSeverity(InfoBar::${p.severity});`);
        lines.push(`${v}->setIsOpen(true);`);
        emitted.add('title');
        emitted.add('message');
        emitted.add('severity');
        break;
      case 'InfoBadge':
        lines.push(`auto* ${v} = new InfoBadge(${parentArg});`);
        lines.push(`${v}->setValue(${p.value ?? 5});`);
        if (p.status && p.status !== 'Informational') lines.push(`${v}->setStatus(InfoBadge::InfoBadgeStatus::${p.status});`);
        if (p.displayMode && p.displayMode !== 'Auto') lines.push(`${v}->setDisplayMode(InfoBadge::InfoBadgeDisplayMode::${p.displayMode});`);
        emitted.add('value');
        emitted.add('status');
        emitted.add('displayMode');
        break;
      case 'Avatar':
        lines.push(`auto* ${v} = new Avatar(${parentArg});`);
        lines.push(`${v}->setName(${cppStr(p.name ?? 'User')});`);
        if (p.avatarSize && p.avatarSize !== 'Medium') lines.push(`${v}->setAvatarSize(Avatar::AvatarSize::${p.avatarSize});`);
        if (p.presence && p.presence !== 'None') lines.push(`${v}->setPresence(Avatar::PresenceStatus::${p.presence});`);
        emitted.add('name');
        emitted.add('avatarSize');
        emitted.add('presence');
        break;
      case 'ProgressBar':
      case 'ProgressRing':
        lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
        lines.push(`${v}->setMinimum(${Math.round(p.minimum ?? 0)});`);
        lines.push(`${v}->setMaximum(${Math.round(p.maximum ?? 100)});`);
        lines.push(`${v}->setValue(${Math.round(p.value ?? 64)});`);
        if (node.type === 'ProgressRing') {
          lines.push(`${v}->setIsIndeterminate(false);`);
          lines.push(`${v}->setIsActive(true);`);
          lines.push(`${v}->setBackgroundVisible(true);`);
        }
        emitted.add('minimum');
        emitted.add('maximum');
        emitted.add('value');
        break;
      case 'Breadcrumb':
        lines.push(`auto* ${v} = new Breadcrumb(${parentArg});`);
        lines.push(`${v}->setItems({${(p.items || ['Home']).map(cppStr).join(', ')}});`);
        emitted.add('items');
        break;
      case 'Stepper':
        lines.push(`auto* ${v} = new Stepper(${parentArg});`);
        for (const step of p.items || ['Step 1', 'Step 2']) lines.push(`${v}->addItem(${cppStr(step)});`);
        lines.push(`${v}->setCurrentIndex(${p.currentIndex ?? 0});`);
        emitted.add('items');
        emitted.add('currentIndex');
        break;
      case 'Pivot':
      case 'SelectorBar':
        lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
        for (const item of p.items || ['Tab 1', 'Tab 2']) lines.push(`${v}->addItem(${cppStr(item)});`);
        lines.push(`${v}->setSelectedIndex(${p.selectedIndex ?? 0});`);
        emitted.add('items');
        emitted.add('selectedIndex');
        break;
      case 'TabView':
        lines.push(`auto* ${v} = new TabView(${parentArg});`);
        for (const item of p.tabs || p.items || ['Tab 1', 'Tab 2']) lines.push(`${v}->addTab(${cppStr(item)});`);
        lines.push(`${v}->setSelectedIndex(${p.selectedIndex ?? 0});`);
        emitted.add('items');
        emitted.add('tabs');
        emitted.add('selectedIndex');
        break;
      case 'MenuBar':
        lines.push(`auto* ${v} = new FluentMenuBar(${parentArg});`);
        for (const item of p.items || ['File', 'Edit']) lines.push(`${v}->addMenu(new FluentMenu(${cppStr(item)}, ${v}));`);
        emitted.add('items');
        break;
      case 'CommandBar':
        headers.add('<QAction>');
        lines.push(`auto* ${v} = new CommandBar(${parentArg});`);
        for (const item of p.items || ['New', 'Save']) lines.push(`${v}->addPrimaryAction(new QAction(${cppStr(item)}, ${v}));`);
        emitted.add('items');
        break;
      case 'ImageViewer': {
        lines.push(`auto* ${v} = new ImageViewer(${parentArg});`);
        if (p.svg) {
          headers.add('<QBuffer>');
          headers.add('<QImageReader>');
          const buffer = safeIdent(`${v}Svg`, usedVars);
          const reader = safeIdent(`${v}Reader`, usedVars);
          lines.push(`QBuffer ${buffer};`,
            `${buffer}.setData(${cppStr(p.svg)}.toUtf8());`,
            `${buffer}.open(QIODevice::ReadOnly);`,
            `QImageReader ${reader}(&${buffer}, "svg");`,
            `${reader}.setScaledSize(${reader}.size().scaled(QSize(640, 380), Qt::KeepAspectRatio));`,
            `${v}->setImage(${reader}.read());`);
        }
        break;
      }
      case 'DataGrid': {
        headers.add('<QStandardItemModel>');
        headers.add('<QStandardItem>');
        const mv = `${v}Model`;
        lines.push(`auto* ${v} = new DataGrid(${parentArg});`);
        lines.push(`auto* ${mv} = new QStandardItemModel(${v});`);
        if (Array.isArray(p.headers) && p.headers.length) {
          lines.push(`${mv}->setHorizontalHeaderLabels({${p.headers.map(cppStr).join(', ')}});`);
        }
        if (Array.isArray(p.modelData) && p.modelData.length) {
          headers.add('<QList>');
          for (const row of p.modelData) {
            const cells = (Array.isArray(row) ? row : [row]).map(c => `new QStandardItem(${cppStr(c ?? '')})`).join(', ');
            lines.push(`${mv}->appendRow(QList<QStandardItem*>{${cells}});`);
          }
        } else {
          for (const item of p.items || ['Row 1', 'Row 2']) lines.push(`${mv}->appendRow(new QStandardItem(${cppStr(item)}));`);
        }
        lines.push(`${v}->setModel(${mv});`);
        emitted.add('headers');
        emitted.add('modelData');
        emitted.add('items');
        break;
      }
      case 'ListView':
      case 'GridView':
      case 'FlowView':
      case 'TreeView':
      case 'Timeline':
      case 'Transfer':
      case 'FileListView':
      case 'MultiSelectComboBox':
      case 'TreeSelect': {
        headers.add('<QStandardItemModel>');
        headers.add('<QStandardItem>');
        const mv = `${v}Model`;
        lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
        lines.push(`auto* ${mv} = new QStandardItemModel(${v});`);
        for (const item of p.items || ['Row 1', 'Row 2']) lines.push(`${mv}->appendRow(new QStandardItem(${cppStr(item)}));`);
        lines.push(`${v}->setModel(${mv});`);
        emitted.add('items');
        break;
      }
      case 'TitleBar':
        headers.add('<FluentQt/TextFields.h>');
        namespaces.add('fluent::textfields');
        lines.push(`auto* ${v} = new TitleBar(${parentArg});`);
        lines.push(`${v}->setContentWidget(new Label(${cppStr(p.title ?? 'Fluent Window')}, ${v}));`);
        emitted.add('title');
        break;
      case 'FontIcon':
        headers.add('<FluentQt/Design.h>');
        lines.push(`auto* ${v} = new FontIcon(${cppIconExpr(p.glyph || 'Settings')}, ${parentArg});`);
        lines.push(`${v}->setIconSize(${p.iconSize ?? 24});`);
        emitted.add('glyph');
        emitted.add('iconSize');
        break;
      case 'QmlView': {
        headers.add('<QUrl>');
        const sessVar = `${v}Session`;
        lines.push(`auto* ${sessVar} = new QmlSession(${parentArg}, ${parentArg});`);
        lines.push(`${sessVar}->activate();`);
        lines.push(`auto* ${v} = ${sessVar}->isActive() ? ${sessVar}->createView(${parentArg}) : new QmlView(${parentArg});`);
        lines.push(`${v}->setMinimumHeight(${p.minHeight ?? 160});`);
        const sample = p.sample || 'Basic';
        if (sample === 'Context' || sample === 'Action') {
          const contextType = sample === 'Context' ? 'QLineEdit' : 'QSpinBox';
          headers.add(`<${contextType}>`);
          lines.push(`auto* ${v}Context = new ${contextType}(${v});`, `${v}Context->hide();`);
          if (sample === 'Context') lines.push(`${v}Context->setText(${cppStr(p.contextText)});`);
          lines.push(`${v}->setContextObject(${cppStr(sample === 'Context' ? 'widgetInput' : 'widgetProgress')}, ${v}Context);`);
        }
        lines.push(`${v}->load(QUrl(${cppStr(qmlScenes[sample])}));`);
        emitted.add('sample');
        emitted.add('minHeight');
        break;
      }
      default:
        lines.push(`auto* ${v} = new ${node.type}(${parentArg});`);
        break;
    }
    emitSchemaPropsCpp(v, node, emitted);
    if (p.enabled === false) lines.push(`${v}->setEnabled(false);`);
    return v;
  }

  if (doc.design?.accentColor && HEX_COLOR_RE.test(doc.design.accentColor)) {
    headers.add('<FluentQt/Foundation.h>');
    headers.add('<QColor>');
    lines.push(`fluent::UserTheme::applyAccentOverride(QColor(${cppStr(doc.design.accentColor)}));`);
  }
  const rootVar = scanAndEmit(doc.root, 'root', null);
  const sortedHeaders = [...headers].sort();
  const usingLines = [...namespaces].sort().map((ns) => `using namespace ${ns};`);

  return [
    ...sortedHeaders.map((h) => `#include ${h}`),
    '',
    'QWidget* createStudioPage(QWidget* parent = nullptr)',
    '{',
    ...usingLines.map((u) => `    ${u}`),
    '',
    ...lines.map((l) => `    ${l}`),
    `    return ${rootVar};`,
    '}',
  ].join('\n');
}

function pyStr(str) {
  return JSON.stringify(String(str ?? ''));
}

export function generatePython(doc, extraSchemas = {}) {
  requireValidDocument(doc);
  const fluentImports = new Map();
  const qtWidgets = new Set(['QWidget']);
  const qtCore = new Set();
  const qtGui = new Set();
  const lines = [];
  const usedVars = new Set(['parent']);

  function needFluent(mod, cls) {
    if (!fluentImports.has(mod)) fluentImports.set(mod, new Set());
    fluentImports.get(mod).add(cls);
  }

  function resolvePythonEnumPrefix(prefix) {
    if (!prefix) return '';
    if (prefix.startsWith('Qt.')) {
      qtCore.add('Qt');
    } else if (prefix.startsWith('Q')) {
      const cls = prefix.slice(0, prefix.indexOf('.'));
      if (cls) qtWidgets.add(cls);
    } else if (prefix.startsWith('fluentqt.')) {
      const parts = prefix.split('.');
      if (parts.length >= 3) {
        if (['Button', 'CheckBox', 'RadioButton', 'ToggleSwitch', 'ComboBox', 'Slider', 'RangeSlider', 'RatingControl', 'ColorPicker', 'FileDropZone', 'MultiSelectComboBox', 'TreeSelect', 'Label', 'LineEdit', 'PasswordBox', 'NumberBox', 'TextEdit', 'AutoSuggestBox', 'TagInput', 'CodeBlock', 'SelectionMode'].includes(parts[1])) {
          needFluent('fluentqt', parts[1]);
        } else {
          needFluent(`${parts[0]}.${parts[1]}`, parts[2]);
        }
      }
    }
    return prefix;
  }

  function emitSchemaPropsPython(v, node, emittedKeys) {
    const schema = extraSchemas?.[node.type];
    const pending = collectPendingSchemaFields(node, schema, emittedKeys);
    const p = node.props || {};
    for (const item of pending) {
      if (item.kind === 'axis-group') {
        const setter = item.setter || toSetterName(item.property);
        if (!setter) continue;
        const axisVal = (axisKey) => {
          const peer = item.peers.find((f) => (f.pointAxis || f.sizeAxis || f.pointFAxis || f.vectorAxis || (f.id.endsWith('X') ? 'x' : 'y')) === axisKey);
          if (!peer) return 0;
          return p[peer.id] ?? peer.default ?? 0;
        };
        const first = item.peers[0];
        if (first.sizeAxis) {
          qtCore.add('QSize');
          lines.push(`${v}.${setter}(QSize(${Math.round(axisVal('width'))}, ${Math.round(axisVal('height'))}))`);
        } else if (first.pointFAxis) {
          qtCore.add('QPointF');
          lines.push(`${v}.${setter}(QPointF(${Number(axisVal('x'))}, ${Number(axisVal('y'))}))`);
        } else if (first.vectorAxis) {
          qtGui.add('QVector3D');
          lines.push(`${v}.${setter}(QVector3D(${Number(axisVal('x'))}, ${Number(axisVal('y'))}, ${Number(axisVal('z'))}))`);
        } else {
          qtCore.add('QPoint');
          lines.push(`${v}.${setter}(QPoint(${Math.round(axisVal('x'))}, ${Math.round(axisVal('y'))}))`);
        }
        continue;
      }

      const { field, val } = item;
      const id = field.id;
      if (id === 'icon') {
        if (typeof val === 'string' && safeKey(val) && val !== 'None') {
          needFluent('fluentqt.design', 'Typography');
          lines.push(`${v}.setIconGlyph(${pyIconExpr(val)})`);
        }
        continue;
      }
      if (id === 'cornerRadius') {
        if (val !== null) {
          const r = Math.round(Number(val) || 0);
          qtCore.add('QMargins');
          lines.push(`${v}.setCornerRadii(QMargins(${r}, ${r}, ${r}, ${r}))`);
        }
        continue;
      }

      const setter = field.setter || toSetterName(field.property || id);
      if (!setter) continue;
      if ((field.glyphEnum || field.type === 'enum') && !safeKey(String(val))) continue;
      if (field.glyphEnum) {
        if (typeof val === 'string' && safeKey(val) && val !== 'None') {
          needFluent('fluentqt.design', 'Typography');
          lines.push(`${v}.${setter}(${pyIconExpr(val)})`);
        }
      } else if (field.type === 'enum') {
        const prefix = resolvePythonEnumPrefix(field.pythonPrefix || '');
        lines.push(`${v}.${setter}(${prefix}${val})`);
      } else if (field.type === 'boolean') {
        lines.push(`${v}.${setter}(${val ? 'True' : 'False'})`);
      } else if (field.type === 'number') {
        const num = field.integer ? Math.round(Number(val)) : Number(val);
        if (Number.isFinite(num)) lines.push(`${v}.${setter}(${num})`);
      } else if (field.type === 'text') {
        lines.push(`${v}.${setter}(${pyStr(val)})`);
      } else if (field.type === 'color') {
        qtGui.add('QColor');
        lines.push(`${v}.${setter}(QColor(${pyStr(val)}))`);
      } else if (field.type === 'date') {
        qtCore.add('QDate');
        qtCore.add('Qt');
        lines.push(`${v}.${setter}(QDate.fromString(${pyStr(val)}, Qt.DateFormat.ISODate))`);
      } else if (field.type === 'time') {
        qtCore.add('QTime');
        qtCore.add('Qt');
        lines.push(`${v}.${setter}(QTime.fromString(${pyStr(val)}, Qt.DateFormat.ISODate))`);
      } else if (field.type === 'datetime') {
        qtCore.add('QDateTime');
        qtCore.add('Qt');
        lines.push(`${v}.${setter}(QDateTime.fromString(${pyStr(val)}, Qt.DateFormat.ISODate))`);
      } else if (field.type === 'string-list' && Array.isArray(val) && !field.itemList) {
        lines.push(`${v}.${setter}(${JSON.stringify(val)})`);
      }
    }
  }

  function emitNode(node, fixedVar, parentExpr) {
    const p = node.props || {};
    const v = fixedVar || safeIdent(node.id, usedVars);
    const parentArg = parentExpr ?? 'parent';

    if (node.type === 'VBox' || node.type === 'HBox' || node.type === 'Grid') {
      const layCls = node.type === 'VBox' ? 'QVBoxLayout' : (node.type === 'HBox' ? 'QHBoxLayout' : 'QGridLayout');
      qtWidgets.add(layCls);
      const layVar = `${v}_layout`;
      const pad = p.padding ?? (node.type === 'VBox' ? 16 : 0);
      lines.push(`${v} = QWidget(${parentArg})`);
      lines.push(`${layVar} = ${layCls}(${v})`);
      lines.push(`${layVar}.setContentsMargins(${pad}, ${pad}, ${pad}, ${pad})`);
      lines.push(`${layVar}.setSpacing(${p.spacing ?? 12})`);
      (node.children || []).forEach((child, idx) => {
        const cv = safeIdent(child.id, usedVars);
        emitNode(child, cv, v);
        if (node.type === 'Grid') {
          const cols = Math.max(1, Math.min(4, p.columns ?? 2));
          lines.push(`${layVar}.addWidget(${cv}, ${Math.floor(idx / cols)}, ${idx % cols})`);
        } else {
          lines.push(`${layVar}.addWidget(${cv})`);
        }
      });
      return v;
    }

    const meta = STUDIO_CATALOG[node.type];
    needFluent(meta.pyModule, meta.pyClass);

    if (node.type === 'AnchorBox') {
      needFluent('fluentqt.foundation', 'anchors');
      const layVar = `${v}_layout`;
      const pad = p.padding ?? 12;
      lines.push(`${v} = QWidget(${parentArg})`);
      lines.push(`${v}.setMinimumHeight(${p.minHeight ?? 220})`);
      lines.push(`${layVar} = AnchorLayout(${v})`);
      (node.children || []).forEach((child, idx) => {
        const cv = safeIdent(child.id, usedVars);
        const cp = child.props || {};
        emitNode(child, cv, v);
        const m = cp.anchorMargin ?? pad;
        const ox = cp.anchorOffsetX ?? 0;
        const pos = cp.anchorPosition || ((idx === 0 && (child.type === 'ParticleBackdrop' || child.type === 'Card')) ? 'Fill' : 'TopLeft');
        const oy = cp.anchorOffsetY ?? ((pos === 'TopLeft' && !cp.anchorPosition) ? idx * 44 : 0);
        let specExpr = `top=(${v}, ${m + oy}), left=(${v}, ${m + ox})`;
        if (pos === 'Fill') specExpr = `fill=${m}`;
        else if (pos === 'TopCenter') specExpr = `top=(${v}, ${m + oy}), horizontal_center=(${v}, ${ox})`;
        else if (pos === 'TopRight') specExpr = `top=(${v}, ${m + oy}), right=(${v}, ${-m + ox})`;
        else if (pos === 'Center') {
          specExpr = (ox === 0 && oy === 0)
            ? `center_in=${v}`
            : `horizontal_center=(${v}, ${ox}), vertical_center=(${v}, ${oy})`;
        }
        else if (pos === 'BottomLeft') specExpr = `bottom=(${v}, ${-m + oy}), left=(${v}, ${m + ox})`;
        else if (pos === 'BottomCenter') specExpr = `bottom=(${v}, ${-m + oy}), horizontal_center=(${v}, ${ox})`;
        else if (pos === 'BottomRight') specExpr = `bottom=(${v}, ${-m + oy}), right=(${v}, ${-m + ox})`;
        else if (pos === 'TopBar') specExpr = `top=(${v}, ${m + oy}), left=(${v}, ${m}), right=(${v}, ${-m})`;
        else if (pos === 'BottomBar') specExpr = `bottom=(${v}, ${-m + oy}), left=(${v}, ${m}), right=(${v}, ${-m})`;
        else if (pos === 'LeftDock') specExpr = `left=(${v}, ${m + ox}), top=(${v}, ${m}), bottom=(${v}, ${-m})`;
        else if (pos === 'RightDock') specExpr = `right=(${v}, ${-m + ox}), top=(${v}, ${m}), bottom=(${v}, ${-m})`;
        lines.push(`${layVar}.addWidget(${cv}, anchors(${specExpr}))`);
      });
      return v;
    }

    if (node.type === 'Window') {
      qtWidgets.add('QVBoxLayout');
      const bodyVar = `${v}_body`;
      const layVar = `${v}_layout`;
      const pad = p.padding ?? 16;
      lines.push(`${v} = Window(${parentArg})`);
      lines.push(`${v}.setWindowTitle(${pyStr(p.title ?? 'Fluent Window')})`);
      lines.push(`${bodyVar} = QWidget(${v})`);
      lines.push(`${layVar} = QVBoxLayout(${bodyVar})`);
      lines.push(`${layVar}.setContentsMargins(${pad}, ${pad}, ${pad}, ${pad})`);
      lines.push(`${layVar}.setSpacing(${p.spacing ?? 12})`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        emitNode(child, cv, bodyVar);
        lines.push(`${layVar}.addWidget(${cv})`);
      }
      lines.push(`${v}.setContentWidget(${bodyVar})`);
      emitSchemaPropsPython(v, node, new Set(['title', 'minHeight', 'padding', 'spacing']));
      return v;
    }

    if (node.type === 'ParticleBackdrop') {
      qtWidgets.add('QVBoxLayout');
      const layVar = `${v}_layout`;
      const pad = p.padding ?? 16;
      lines.push(`${v} = ParticleBackdrop(${parentArg})`);
      if (p.effect) lines.push(`${v}.setEffect(ParticleBackdrop.Effect.${p.effect})`);
      if (p.backgroundMode) lines.push(`${v}.setBackgroundMode(ParticleBackdrop.BackgroundMode.${p.backgroundMode})`);
      lines.push(`${v}.setParticleCount(${p.particleCount ?? 180})`);
      lines.push(`${v}.setMinimumHeight(${p.minHeight ?? 200})`);
      lines.push(`${layVar} = QVBoxLayout(${v})`);
      lines.push(`${layVar}.setContentsMargins(${pad}, ${pad}, ${pad}, ${pad})`);
      lines.push(`${layVar}.setSpacing(${p.spacing ?? 10})`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        emitNode(child, cv, v);
        lines.push(`${layVar}.addWidget(${cv})`);
      }
      emitSchemaPropsPython(v, node, new Set(['effect', 'backgroundMode', 'particleCount', 'minHeight', 'padding', 'spacing']));
      return v;
    }

    if (node.type === 'SpatialView' || node.type === 'SpatialItem') {
      needFluent('fluentqt.spatial', 'SpatialSession');
      needFluent('fluentqt.spatial', 'SpatialView');
      qtGui.add('QVector3D');
      const sessVar = `${v}_session`;
      const viewVar = v;
      lines.push(`${sessVar} = SpatialSession(${parentArg}, ${parentArg})`);
      lines.push(`${sessVar}.activate()`);
      lines.push(`${viewVar} = ${sessVar}.createView(${parentArg}) or SpatialView(${parentArg})`);
      if (node.type === 'SpatialView') {
        const zoomNum = Number(p.zoom ?? 100);
        const normZoom = zoomNum <= 4 ? zoomNum : (zoomNum / 100);
        lines.push(`${viewVar}.setCameraDistance(${p.cameraDistance ?? 1000})`);
        lines.push(`${viewVar}.setZoom(${normZoom.toFixed(2)})`);
        lines.push(`${viewVar}.setMinimumHeight(${p.minHeight ?? 260})`);
        for (const child of node.children || []) {
          if (child.type === 'SpatialItem') {
            const inner = (child.children || [])[0];
            const cv = safeIdent(`${child.id}_content`, usedVars);
            const itemVar = safeIdent(child.id, usedVars);
            if (inner) emitNode(inner, cv, 'None');
            else {
              needFluent('fluentqt.layout', 'Card');
              lines.push(`${cv} = Card()`);
            }
            const cp = child.props || {};
            const rawScale = Number(cp.scale ?? 100);
            const rawSurf = Number(cp.surfaceIntensity ?? 75);
            lines.push(`${itemVar} = ${viewVar}.addOwnedWidget(${cv})`);
            lines.push(`${itemVar}.setPosition(QVector3D(${cp.positionX ?? cp.posX ?? 0}, ${cp.positionY ?? cp.posY ?? 0}, ${cp.positionZ ?? cp.posZ ?? 40}))`);
            lines.push(`${itemVar}.setRotation(QVector3D(${cp.rotationX ?? cp.rotX ?? 6}, ${cp.rotationY ?? cp.rotY ?? -16}, ${cp.rotationZ ?? cp.rotZ ?? 0}))`);
            lines.push(`${itemVar}.setScale(${(rawScale <= 4 ? rawScale : (rawScale / 100)).toFixed(2)})`);
            lines.push(`${itemVar}.setSurfaceIntensity(${(rawSurf <= 1 ? rawSurf : (rawSurf / 100)).toFixed(2)})`);
            emitSchemaPropsPython(itemVar, child, new Set(['posX', 'posY', 'posZ', 'rotX', 'rotY', 'rotZ', 'positionX', 'positionY', 'positionZ', 'rotationX', 'rotationY', 'rotationZ', 'scale', 'surfaceIntensity']));
          } else {
            const cv = safeIdent(child.id, usedVars);
            emitNode(child, cv, 'None');
            lines.push(`${viewVar}.addOwnedWidget(${cv})`);
          }
        }
        emitSchemaPropsPython(viewVar, node, new Set(['cameraDistance', 'zoom', 'minHeight']));
      } else {
        const inner = (node.children || [])[0];
        const cv = safeIdent(`${node.id}_content`, usedVars);
        const item = safeIdent(`${node.id}_item`, usedVars);
        if (inner) emitNode(inner, cv, 'None');
        else {
          needFluent('fluentqt.layout', 'Card');
          lines.push(`${cv} = Card()`);
        }
        const rawScale = Number(p.scale ?? 100);
        const rawSurf = Number(p.surfaceIntensity ?? 75);
        lines.push(`${item} = ${viewVar}.addOwnedWidget(${cv})`);
        lines.push(`${item}.setPosition(QVector3D(${p.positionX ?? p.posX ?? 0}, ${p.positionY ?? p.posY ?? 0}, ${p.positionZ ?? p.posZ ?? 40}))`);
        lines.push(`${item}.setRotation(QVector3D(${p.rotationX ?? p.rotX ?? 6}, ${p.rotationY ?? p.rotY ?? -16}, ${p.rotationZ ?? p.rotZ ?? 0}))`);
        lines.push(`${item}.setScale(${(rawScale <= 4 ? rawScale : (rawScale / 100)).toFixed(2)})`);
        lines.push(`${item}.setSurfaceIntensity(${(rawSurf <= 1 ? rawSurf : (rawSurf / 100)).toFixed(2)})`);
        emitSchemaPropsPython(item, node, new Set(['posX', 'posY', 'posZ', 'rotX', 'rotY', 'rotZ', 'positionX', 'positionY', 'positionZ', 'rotationX', 'rotationY', 'rotationZ', 'scale', 'surfaceIntensity']));
        return viewVar;
      }
      return viewVar;
    }

    if (node.type === 'Card') {
      qtWidgets.add('QVBoxLayout');
      const layVar = `${v}_layout`;
      const pad = p.padding ?? 16;
      lines.push(`${v} = Card(${parentArg})`);
      if (p.appearance && p.appearance !== 'Layer') lines.push(`${v}.setAppearance(Card.Appearance.${p.appearance})`);
      if (p.borderVisible === false) lines.push(`${v}.setBorderVisible(False)`);
      emitSchemaPropsPython(v, node, new Set(['appearance', 'borderVisible', 'spacing', 'padding']));
      lines.push(`${layVar} = QVBoxLayout(${v})`);
      lines.push(`${layVar}.setContentsMargins(${pad}, ${pad}, ${pad}, ${pad})`);
      lines.push(`${layVar}.setSpacing(${p.spacing ?? 10})`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        emitNode(child, cv, v);
        lines.push(`${layVar}.addWidget(${cv})`);
      }
      return v;
    }

    if (node.type === 'Expander') {
      qtWidgets.add('QVBoxLayout');
      const bodyVar = `${v}_body`;
      const layVar = `${v}_layout`;
      lines.push(`${v} = Expander(${parentArg})`);
      lines.push(`${v}.setHeaderText(${pyStr(p.headerText ?? 'Section')})`);
      lines.push(`${v}.setExpanded(${p.expanded !== false ? 'True' : 'False'})`);
      emitSchemaPropsPython(v, node, new Set(['headerText', 'expanded']));
      lines.push(`${bodyVar} = QWidget()`);
      lines.push(`${layVar} = QVBoxLayout(${bodyVar})`);
      for (const child of node.children || []) {
        const cv = safeIdent(child.id, usedVars);
        emitNode(child, cv, bodyVar);
        lines.push(`${layVar}.addWidget(${cv})`);
      }
      lines.push(`${v}.setOwnedContentWidget(${bodyVar})`);
      return v;
    }

    if (node.type === 'Field') {
      lines.push(`${v} = Field(${parentArg})`);
      lines.push(`${v}.setLabelText(${pyStr(p.labelText ?? 'Field')})`);
      if (p.helperText) lines.push(`${v}.setHelperText(${pyStr(p.helperText)})`);
      if (p.required) lines.push(`${v}.setRequired(True)`);
      if (p.labelPosition === 'Leading') lines.push(`${v}.setLabelPosition(Field.LabelPosition.Leading)`);
      if (p.validationState && p.validationState !== 'None') lines.push(`${v}.setValidationState(Field.ValidationState.${p.validationState})`);
      emitSchemaPropsPython(v, node, new Set(['labelText', 'helperText', 'required', 'labelPosition', 'validationState']));
      if ((node.children || []).length > 0) {
        const ed = node.children[0];
        const ev = safeIdent(ed.id, usedVars);
        emitNode(ed, ev, 'None');
        lines.push(`${v}.setOwnedEditor(${ev})`);
      }
      return v;
    }

    if (meta.container) {
      lines.push(`${v} = ${meta.pyClass}(${parentArg})`);
      emitSchemaPropsPython(v, node, new Set(['items', 'minHeight']));
      if (node.type === 'ScrollView') {
        qtWidgets.add('QVBoxLayout');
        const inner = `${v}_content`;
        const ilay = `${v}_layout`;
        lines.push(`${inner} = QWidget()`);
        lines.push(`${ilay} = QVBoxLayout(${inner})`);
        for (const child of node.children || []) {
          const cv = safeIdent(child.id, usedVars);
          emitNode(child, cv, inner);
          lines.push(`${ilay}.addWidget(${cv})`);
        }
        lines.push(`${v}.setOwnedContentWidget(${inner})`);
      } else {
        (node.children || []).forEach((child, idx) => {
          const cv = safeIdent(child.id, usedVars);
          emitNode(child, cv, v);
          if (node.type === 'Accordion') lines.push(`${v}.addOwnedItem(${cv})`);
          else if (node.type === 'SplitView') lines.push(`${v}.addOwnedPane(${cv})`);
          else if (node.type === 'FlipView') lines.push(`${v}.addOwnedPage(${cv})`);
          else if (node.type === 'StackView') lines.push(idx === 0 ? `${v}.setInitialOwnedItem(${cv})` : `${v}.pushOwnedItem(${cv})`);
        });
      }
      return v;
    }

    const parentComma = parentArg ? `, ${parentArg}` : '';
    if (CHART_TYPES.has(node.type)) {
      needFluent('fluentqt.charts', 'ChartModel');
      needFluent('fluentqt.charts', 'ChartData');
      qtCore.add('QPointF');
      const mv = `${v}_model`;
      const pts = (p.values || [24, 42, 38, 64]).map((n, i) => `QPointF(${i + 1}, ${Number(n) || 0})`).join(', ');
      lines.push(`${v} = ${meta.pyClass}(${parentArg})`);
      if (node.type === 'ChartView' && p.chartType) lines.push(`${v}.setChartType(ChartView.ChartType.${p.chartType})`);
      lines.push(`${mv} = ChartModel(${v})`);
      lines.push(`${mv}.setName(${pyStr(p.seriesName ?? 'Series')})`);
      lines.push(`${mv}.setDataSnapshot(ChartData.fromPoints([${pts}]))`);
      lines.push(`${v}.setModel(${mv})`);
      if (p.title) lines.push(`${v}.setTitle(${pyStr(p.title)})`);
      lines.push(`${v}.setMinimumHeight(${p.minHeight ?? (node.type === 'Sparkline' ? 76 : 200)})`);
      emitSchemaPropsPython(v, node, new Set(['chartType', 'seriesName', 'values', 'title', 'minHeight']));
      return v;
    }

    if (OVERLAY_TYPES.has(node.type)) {
      needFluent('fluentqt.basicinput', 'Button');
      lines.push(`${v} = Button(${pyStr(p.text ?? `Open ${node.type}`)}${parentComma})`);
      lines.push(`# Trigger ${meta.pyClass} when ${v}.clicked is emitted`);
      return v;
    }

    const pyEmitted = new Set(['enabled']);
    switch (node.type) {
      case 'SplitButton':
      case 'ToggleSplitButton':
      case 'Button':
      case 'ToggleButton':
      case 'HyperlinkButton':
      case 'RepeatButton':
      case 'CheckBox':
      case 'RadioButton':
        lines.push(`${v} = ${meta.pyClass}(${pyStr(p.text ?? node.type)}${parentComma})`);
        pyEmitted.add('text');
        if (node.type === 'Button' && p.fluentStyle && p.fluentStyle !== 'Standard') lines.push(`${v}.setFluentStyle(Button.ButtonStyle.${p.fluentStyle})`);
        if (node.type === 'Button' && p.fluentSize && p.fluentSize !== 'StandardSize') lines.push(`${v}.setFluentSize(Button.ButtonSize.${p.fluentSize})`);
        if (node.type === 'Button' && typeof p.icon === 'string' && safeKey(p.icon) && p.icon !== 'None') {
          needFluent('fluentqt.design', 'Typography');
          lines.push(`${v}.setIconGlyph(${pyIconExpr(p.icon)})`);
        }
        pyEmitted.add('fluentStyle');
        pyEmitted.add('fluentSize');
        pyEmitted.add('icon');
        if (node.type === 'CheckBox') {
          if (p.checkState && p.checkState !== 'Unchecked') {
            qtCore.add('Qt');
            lines.push(`${v}.setCheckState(Qt.CheckState.${p.checkState})`);
          } else if (p.checked && !p.checkState) {
            lines.push(`${v}.setChecked(True)`);
          }
          pyEmitted.add('checked');
          pyEmitted.add('checkState');
        } else if (node.type === 'ToggleButton' || node.type === 'RadioButton') {
          if (p.checked) lines.push(`${v}.setChecked(True)`);
          pyEmitted.add('checked');
        }
        break;
      case 'CompoundButton':
        lines.push(`${v} = CompoundButton(${pyStr(p.text ?? 'Action')}, ${pyStr(p.secondaryText ?? '')}${parentComma})`);
        pyEmitted.add('text');
        pyEmitted.add('secondaryText');
        break;
      case 'ToggleSwitch':
        lines.push(`${v} = ToggleSwitch(${parentArg})`);
        if (p.isOn) lines.push(`${v}.setIsOn(True)`);
        pyEmitted.add('isOn');
        break;
      case 'ComboBox':
        lines.push(`${v} = ComboBox(${parentArg})`);
        lines.push(`${v}.addItems(${JSON.stringify(p.items || ['Option A', 'Option B'])})`);
        pyEmitted.add('items');
        break;
      case 'Slider':
      case 'RangeSlider':
        qtCore.add('Qt');
        lines.push(`${v} = ${node.type}(${p.orientation === 'Vertical' ? 'Qt.Orientation.Vertical' : 'Qt.Orientation.Horizontal'}${parentComma})`);
        lines.push(`${v}.setRange(${p.minimum ?? 0}, ${p.maximum ?? 100})`);
        if (node.type === 'Slider') lines.push(`${v}.setValue(${p.value ?? 45})`);
        else lines.push(`${v}.setValues(${p.lowerValue ?? 20}, ${p.upperValue ?? 75})`);
        pyEmitted.add('orientation');
        pyEmitted.add('minimum');
        pyEmitted.add('maximum');
        pyEmitted.add('value');
        pyEmitted.add('lowerValue');
        pyEmitted.add('upperValue');
        break;
      case 'Label': {
        lines.push(`${v} = Label(${pyStr(p.text ?? 'Label')}${parentComma})`);
        const typo = p.fluentTypography || p.typography;
        if (typo && typo !== 'Body') {
          needFluent('fluentqt.design', 'Typography');
          lines.push(`${v}.setFluentTypography(Typography.FontRole.${typo})`);
        }
        pyEmitted.add('text');
        pyEmitted.add('typography');
        pyEmitted.add('fluentTypography');
        break;
      }
      case 'ProgressBar':
      case 'ProgressRing':
        lines.push(`${v} = ${meta.pyClass}(${parentArg})`);
        lines.push(`${v}.setMinimum(${p.minimum ?? 0})`);
        lines.push(`${v}.setMaximum(${p.maximum ?? 100})`);
        lines.push(`${v}.setValue(${p.value ?? 64})`);
        if (node.type === 'ProgressRing') {
          lines.push(`${v}.setIsIndeterminate(False)`);
          lines.push(`${v}.setIsActive(True)`);
          lines.push(`${v}.setBackgroundVisible(True)`);
        }
        pyEmitted.add('minimum');
        pyEmitted.add('maximum');
        pyEmitted.add('value');
        break;
      case 'ImageViewer': {
        lines.push(`${v} = ImageViewer(${parentArg})`);
        if (p.svg) {
          qtCore.add('QBuffer');
          qtCore.add('QIODevice');
          qtCore.add('QSize');
          qtCore.add('Qt');
          qtGui.add('QImageReader');
          const buffer = safeIdent(`${v}_svg`, usedVars);
          const reader = safeIdent(`${v}_reader`, usedVars);
          lines.push(`${buffer} = QBuffer()`,
            `${buffer}.setData(${pyStr(p.svg)}.encode("utf-8"))`,
            `${buffer}.open(QIODevice.OpenModeFlag.ReadOnly)`,
            `${reader} = QImageReader(${buffer}, b"svg")`,
            `${reader}.setScaledSize(${reader}.size().scaled(QSize(640, 380), Qt.AspectRatioMode.KeepAspectRatio))`,
            `${v}.setImage(${reader}.read())`);
        }
        break;
      }
      case 'DataGrid': {
        qtGui.add('QStandardItemModel');
        qtGui.add('QStandardItem');
        const mv = `${v}_model`;
        lines.push(`${v} = DataGrid(${parentArg})`);
        lines.push(`${mv} = QStandardItemModel(${v})`);
        if (Array.isArray(p.headers) && p.headers.length) {
          lines.push(`${mv}.setHorizontalHeaderLabels([${p.headers.map(pyStr).join(', ')}])`);
        }
        if (Array.isArray(p.modelData) && p.modelData.length) {
          for (const row of p.modelData) {
            const cells = (Array.isArray(row) ? row : [row]).map(c => `QStandardItem(${pyStr(c ?? '')})`).join(', ');
            lines.push(`${mv}.appendRow([${cells}])`);
          }
        } else {
          for (const item of p.items || ['Row 1', 'Row 2']) lines.push(`${mv}.appendRow(QStandardItem(${pyStr(item)}))`);
        }
        lines.push(`${v}.setModel(${mv})`);
        pyEmitted.add('headers');
        pyEmitted.add('modelData');
        pyEmitted.add('items');
        break;
      }
      case 'ListView':
      case 'GridView':
      case 'FlowView':
      case 'TreeView':
      case 'Timeline':
      case 'Transfer':
      case 'FileListView':
      case 'MultiSelectComboBox':
      case 'TreeSelect': {
        qtGui.add('QStandardItemModel');
        qtGui.add('QStandardItem');
        const mv = `${v}_model`;
        lines.push(`${v} = ${meta.pyClass}(${parentArg})`);
        lines.push(`${mv} = QStandardItemModel(${v})`);
        for (const item of p.items || ['Row 1', 'Row 2']) lines.push(`${mv}.appendRow(QStandardItem(${pyStr(item)}))`);
        lines.push(`${v}.setModel(${mv})`);
        pyEmitted.add('items');
        break;
      }
      case 'FontIcon':
        needFluent('fluentqt.design', 'Typography');
        lines.push(`${v} = FontIcon(${pyIconExpr(p.glyph || 'Settings')}${parentComma})`);
        lines.push(`${v}.setIconSize(${p.iconSize ?? 24})`);
        pyEmitted.add('glyph');
        pyEmitted.add('iconSize');
        break;
      case 'QmlView': {
        needFluent('fluentqt.qmlbridge', 'QmlSession');
        qtCore.add('QUrl');
        const sessVar = `${v}_session`;
        lines.push(`${sessVar} = QmlSession(${parentArg}, ${parentArg})`);
        lines.push(`${sessVar}.activate()`);
        lines.push(`${v} = ${sessVar}.createView(${parentArg}) or QmlView(${parentArg})`);
        lines.push(`${v}.setMinimumHeight(${p.minHeight ?? 160})`);
        const sample = p.sample || 'Basic';
        if (sample === 'Context' || sample === 'Action') {
          const contextType = sample === 'Context' ? 'QLineEdit' : 'QSpinBox';
          qtWidgets.add(contextType);
          lines.push(`${v}_context = ${contextType}(${v})`, `${v}_context.hide()`);
          if (sample === 'Context') lines.push(`${v}_context.setText(${pyStr(p.contextText)})`);
          lines.push(`${v}.setContextObject(${pyStr(sample === 'Context' ? 'widgetInput' : 'widgetProgress')}, ${v}_context)`);
        }
        lines.push(`${v}.load(QUrl(${pyStr(qmlScenes[sample])}))`);
        pyEmitted.add('sample');
        pyEmitted.add('minHeight');
        break;
      }
      default:
        lines.push(`${v} = ${meta.pyClass}(${parentArg})`);
        if (p.title && ['SplashScreen', 'FileDropZone'].includes(node.type)) {
          lines.push(`${v}.setTitle(${pyStr(p.title)})`);
          pyEmitted.add('title');
        }
        if (node.type === 'InfoBar') {
          lines.push(`${v}.setTitle(${pyStr(p.title ?? 'Status')})`);
          lines.push(`${v}.setMessage(${pyStr(p.message ?? '')})`);
          lines.push(`${v}.setIsOpen(True)`);
          pyEmitted.add('title');
          pyEmitted.add('message');
        }
        if (node.type === 'Avatar') { lines.push(`${v}.setName(${pyStr(p.name ?? 'User')})`); pyEmitted.add('name'); }
        if (node.type === 'Breadcrumb') { lines.push(`${v}.setItems(${JSON.stringify(p.items || ['Home'])})`); pyEmitted.add('items'); }
        if (node.type === 'Stepper' || node.type === 'Pivot' || node.type === 'SelectorBar') {
          for (const item of p.items || ['Step 1', 'Step 2']) lines.push(`${v}.addItem(${pyStr(item)})`);
          pyEmitted.add('items');
        }
        if (node.type === 'TabView') {
          for (const item of p.tabs || p.items || ['Tab 1', 'Tab 2']) lines.push(`${v}.addTab(${pyStr(item)})`);
          pyEmitted.add('items');
          pyEmitted.add('tabs');
        }
        break;
    }
    emitSchemaPropsPython(v, node, pyEmitted);
    if (p.enabled === false) lines.push(`${v}.setEnabled(False)`);
    return v;
  }

  if (doc.design?.accentColor && HEX_COLOR_RE.test(doc.design.accentColor)) {
    needFluent('fluentqt.foundation', 'set_accent_color');
    qtGui.add('QColor');
    lines.push(`set_accent_color(QColor(${pyStr(doc.design.accentColor)}))`);
  }
  const rootVar = emitNode(doc.root, 'root', 'parent');
  const importLines = [];
  if (qtCore.size) importLines.push(`from PySide6.QtCore import ${[...qtCore].sort().join(', ')}`);
  if (qtGui.size) importLines.push(`from PySide6.QtGui import ${[...qtGui].sort().join(', ')}`);
  importLines.push(`from PySide6.QtWidgets import ${[...qtWidgets].sort().join(', ')}`);
  for (const mod of [...fluentImports.keys()].sort()) {
    importLines.push(`from ${mod} import ${[...fluentImports.get(mod)].sort().join(', ')}`);
  }

  return [
    ...importLines,
    '',
    'def create_studio_page(parent=None):',
    ...lines.map((l) => `    ${l}`),
    `    return ${rootVar}`,
  ].join('\n');
}

function xmlEscape(value) {
  return String(value ?? '')
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function xmlProp(name, kind, value, indent) {
  return `${indent}<property name="${xmlEscape(name)}"><${kind}>${xmlEscape(value)}</${kind}></property>`;
}

export function generateDesignerUi(doc) {
  requireValidDocument(doc);
  const customWidgets = new Map();
  const lines = [
    '<?xml version="1.0" encoding="UTF-8"?>',
    '<ui version="4.0">',
    ' <class>StudioPage</class>',
    ' <widget class="QWidget" name="StudioPage">',
    '  <property name="geometry"><rect><x>0</x><y>0</y><width>880</width><height>640</height></rect></property>',
  ];

  const layoutTypes = ['VBox', 'HBox', 'Grid', 'AnchorBox'];
  const hostForms = ['Window', 'Dialog', 'ContentDialog', 'SpatialItem'];
  const pageMethods = {Field: 'setFormPage', Expander: 'setFormPage', ScrollView: 'setFormPage'};
  const aliases = {minHeight: 'minimumHeight', typography: 'fluentTypography', colorRole: 'textColorRole'};

  function property(name, value, indent) {
    if (Array.isArray(value)) {
      const entries = value.map(v => `<string>${xmlEscape(v)}</string>`).join('');
      lines.push(`${indent}<property name="${name}" stdset="0"><stringlist>${entries}</stringlist></property>`);
    } else if (typeof value === 'boolean' || typeof value === 'number' || typeof value === 'string') {
      const tag = typeof value === 'boolean' ? 'bool'
        : typeof value === 'number' ? (Number.isInteger(value) ? 'number' : 'double') : 'string';
      lines.push(`${indent}<property name="${name}" stdset="0"><${tag}>${xmlEscape(value)}</${tag}></property>`);
    }
  }

  function emitLayout(node, indent) {
    const p = node.props || {};
    const anchored = node.type === 'AnchorBox';
    const grid = node.type === 'Grid';
    const cls = anchored || grid ? 'QGridLayout' : node.type === 'HBox' ? 'QHBoxLayout' : 'QVBoxLayout';
    lines.push(`${indent}<layout class="${cls}" name="layout_${node.id}">`);
    property('spacing', anchored ? 0 : p.spacing ?? 12, indent + ' ');
    for (const side of ['left', 'top', 'right', 'bottom']) {
      lines.push(xmlProp(side + 'Margin', 'number', anchored ? 0 : p.padding ?? 0, indent + ' '));
    }
    const cols = Math.max(1, Math.min(4, p.columns ?? 2));
    (node.children || []).forEach((child, index) => {
      if (anchored) {
        emitAnchored(child, index, p.padding ?? 12, indent + ' ');
        return;
      }
      const cell = grid ? ` row="${Math.floor(index / cols)}" column="${index % cols}"` : '';
      lines.push(`${indent} <item${cell}>`);
      emitWidget(child, indent + '  ');
      lines.push(`${indent} </item>`);
    });
    lines.push(`${indent}</layout>`);
  }

  function emitAnchored(child, index, padding, indent) {
    const p = child.props || {};
    const pos = p.anchorPosition || (index === 0 && ['ParticleBackdrop', 'Card'].includes(child.type) ? 'Fill' : 'TopLeft');
    const x = p.anchorOffsetX ?? 0;
    const y = p.anchorOffsetY ?? (p.anchorPosition ? 0 : index * 44);
    const m = p.anchorMargin ?? padding;
    const centerX = pos.includes('Center'), centerY = pos === 'Center';
    const left = pos.includes('Left'), right = pos.includes('Right');
    const top = pos.includes('Top'), bottom = pos.includes('Bottom');
    const horizontal = centerX ? 'HCenter' : left ? 'Left' : right ? 'Right' : '';
    const vertical = centerY ? 'VCenter' : top ? 'Top' : bottom ? 'Bottom' : '';
    const align = [horizontal, vertical].filter(Boolean).map(v => 'Qt::Align' + v).join('|');
    const margins = [
      centerX ? Math.max(0, 2*x) : right ? 0 : m + (left ? x : 0),
      centerY ? Math.max(0, 2*y) : bottom ? 0 : m + (top ? y : 0),
      centerX ? Math.max(0, -2*x) : left ? 0 : m - (right ? x : 0),
      centerY ? Math.max(0, -2*y) : top ? 0 : m - (bottom ? y : 0),
    ];
    lines.push(`${indent}<item row="0" column="0">`);
    lines.push(`${indent} <layout class="QGridLayout" name="anchorLayout_${child.id}">`);
    ['leftMargin','topMargin','rightMargin','bottomMargin'].forEach((side,i) =>
      lines.push(xmlProp(side, 'number', Math.max(0,margins[i]), indent + '  ')));
    lines.push(`${indent}  <item row="0" column="0"${align ? ` alignment="${align}"` : ''}>`);
    emitWidget(child, indent + '   ');
    lines.push(`${indent}  </item></layout></item>`);
  }

  function emitWidget(node, indent) {
    const p = node.props || {};
    const isLayout = layoutTypes.includes(node.type), isHost = hostForms.includes(node.type);
    const cls = STUDIO_CATALOG[node.type].designerClass;
    if (!cls) throw new Error(`Missing Designer registration for ${node.type}`);
    const pageMethod = pageMethods[node.type];
    if (cls !== 'QWidget') customWidgets.set(cls, {
      extendsClass: node.type === 'ComboBox' ? 'QComboBox' : 'QWidget',
      isContainer: node.type === 'Card' || Boolean(pageMethod), pageMethod,
    });
    lines.push(`${indent}<widget class="${cls}" name="${node.id}">`);
    if (isHost) {
      property('fluentFormHost', node.type, indent);
      property('windowTitle', p.title ?? node.type, indent);
      if (node.type === 'SpatialItem') {
        property('fluentItemScale', (p.scale ?? 100) / 100, indent);
        property('fluentItemRotationY', p.rotY ?? -16, indent);
      }
    }
    if (node.type === 'InfoBar') property('isOpen', true, indent);
    if (node.type === 'ImageViewer' && p.svg) {
      lines.push(`${indent} <!-- To display the illustration in Designer, add the SVG to a .qrc and set the pixmap property. -->`);
    }
    if (node.type === 'ComboBox') {
      for (const item of p.items || []) lines.push(`${indent} <item>${xmlProp('text','string',item,'')}</item>`);
    }
    if (CHART_TYPES.has(node.type)) property('formSeries', [p.seriesName || 'Series', ...(p.values || [])], indent);
    if (['ListView','Stepper','Breadcrumb','Pivot','SelectorBar'].includes(node.type) && p.items) {
      property('formItems', p.items, indent);
    }
    for (const [key, value] of Object.entries(p)) {
      if (['seriesName', 'values', 'items'].includes(key)) continue;
      if (key.startsWith('anchor') || ['padding','align','spacing','columns'].includes(key)) continue;
      if (isLayout && key !== 'minHeight') continue;
      if (isHost && ['title','posX','posY','posZ','rotX','rotY','rotZ','scale'].includes(key)) continue;
      if (cls.endsWith('Button') && (OVERLAY_TYPES.has(node.type) || node.type === 'SplashScreen')) {
        if (['text', 'enabled'].includes(key)) property(key, value, indent);
        else if (key === 'message' || (node.type === 'SplashScreen' && key === 'title')) property('surfaceText', value, indent);
        continue;
      }
      property(aliases[key] || key, value, indent);
    }
    if (isLayout || node.type === 'Card' || isHost) {
      emitLayout(node, indent + ' ');
    } else if (pageMethod && node.children?.length) {
      if (node.type === 'Field') emitWidget(node.children[0], indent + ' ');
      else emitWidget({id: node.id + '_body', type: 'VBox', props: {padding: 0}, children: node.children}, indent + ' ');
    } else if (node.children?.length) {
      emitLayout(node, indent + ' ');
    }
    lines.push(`${indent}</widget>`);
  }

  if (hostForms.includes(doc.root.type)) {
    property('fluentFormHost', doc.root.type, '  ');
    property('windowTitle', doc.root.props?.title || doc.root.type, '  ');
    if (doc.root.type === 'SpatialItem') {
      property('fluentItemScale', (doc.root.props?.scale ?? 100) / 100, '  ');
      property('fluentItemRotationY', doc.root.props?.rotY ?? -16, '  ');
    }
    emitLayout(doc.root, '  ');
  } else if (layoutTypes.includes(doc.root.type)) {
    if (doc.root.props?.minHeight) property('minimumHeight', doc.root.props.minHeight, '  ');
    emitLayout(doc.root, '  ');
  } else {
    emitLayout({id: 'studioPage', type: 'VBox', props: {padding: 0}, children: [doc.root]}, '  ');
  }
  lines.push(' </widget>');
  if (customWidgets.size > 0) {
    lines.push(' <customwidgets>');
    for (const [cls, info] of customWidgets.entries()) {
      lines.push('  <customwidget>');
      lines.push(`   <class>${xmlEscape(cls)}</class>`);
      lines.push(`   <extends>${info.extendsClass}</extends>`);
      lines.push('   <header>fluentqt_designer.h</header>');
      if (info.pageMethod) lines.push(`   <addpagemethod>${info.pageMethod}</addpagemethod>`);
      if (info.isContainer) lines.push('   <container>1</container>');
      lines.push('  </customwidget>');
    }
    lines.push(' </customwidgets>');
  }
  lines.push(' <resources/>');
  lines.push(' <connections/>');
  lines.push('</ui>');
  return lines.join('\n');
}

function bytesToBase64Url(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i++) binary += String.fromCharCode(bytes[i]);
  return btoa(binary).replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
}

function base64UrlToBytes(text) {
  const norm = text.replace(/-/g, '+').replace(/_/g, '/');
  const pad = norm.length % 4 ? '='.repeat(4 - (norm.length % 4)) : '';
  const binary = atob(norm + pad);
  const out = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) out[i] = binary.charCodeAt(i);
  return out;
}

export function encodeShareState(doc) {
  requireValidDocument(doc);
  const bytes = new TextEncoder().encode(JSON.stringify(doc));
  if (bytes.length > MAX_DOCUMENT_BYTES) throw new RangeError('Studio document is too large.');
  return 'v1.' + bytesToBase64Url(bytes);
}

export function decodeShareState(hashOrToken) {
  try {
    let raw = String(hashOrToken || '').trim();
    if (raw.startsWith('#')) raw = raw.slice(1);
    if (raw.startsWith('state=')) raw = raw.slice(6);
    if (!raw.startsWith('v1.')) return { ok: false, error: 'Unsupported state token prefix.' };
    if (raw.length - 3 > Math.ceil(MAX_DOCUMENT_BYTES * 4 / 3)) return { ok: false, error: 'Studio document is too large.' };
    const decoded = new TextDecoder('utf-8', { fatal: true }).decode(base64UrlToBytes(raw.slice(3)));
    const doc = JSON.parse(decoded);
    const check = validateDocument(doc);
    if (!check.ok) return check;
    return { ok: true, document: doc };
  } catch (err) {
    return { ok: false, error: err instanceof Error ? err.message : 'Invalid state payload.' };
  }
}

function clampNum(value, min, max) {
  return Math.max(min, Math.min(max, value));
}

function quantizeStep(value, step = 4) {
  return Math.round(value / step) * step;
}

function subtreeDepth(node) {
  const children = node?.children || [];
  if (!children.length) return 1;
  let maxChild = 0;
  for (const child of children) maxChild = Math.max(maxChild, subtreeDepth(child));
  return 1 + maxChild;
}

function pickAnchorSnap(rawVal, candidates) {
  let best = null;
  let bestDist = Infinity;
  for (const cand of candidates) {
    const dist = Math.abs(rawVal - cand.target);
    if (dist > cand.threshold) continue;
    const isBetter = dist < bestDist - 0.5
      || (Math.abs(dist - bestDist) <= 0.5 && cand.kind !== 'sibling' && best?.kind === 'sibling');
    if (isBetter) {
      best = cand;
      bestDist = dist;
    }
  }
  return best;
}

export function resolveAnchorPlacement(parentRect, x, y, childW = 120, childH = 36, padding = 12, siblingRects = []) {
  const pr = parentRect || { x: 0, y: 0, w: 320, h: 220 };
  const pw = Math.max(1, pr.w);
  const ph = Math.max(1, pr.h);
  const cw = Math.max(24, Number(childW) || 120);
  const ch = Math.max(20, Number(childH) || 36);
  const parsedPad = Number(padding);
  const m = clampNum(Math.round(Number.isFinite(parsedPad) ? parsedPad : 12), 0, 96);
  const relX = clampNum(x - pr.x, 0, pw);
  const relY = clampNum(y - pr.y, 0, ph);

  const xCandidates = [
    { target: pw / 2, threshold: 10, guide: pr.x + pw / 2, kind: 'center' },
    { target: m + cw / 2, threshold: 10, guide: pr.x + m, kind: 'left' },
    { target: pw - m - cw / 2, threshold: 10, guide: pr.x + pw - m, kind: 'right' },
  ];
  const yCandidates = [
    { target: ph / 2, threshold: 10, guide: pr.y + ph / 2, kind: 'center' },
    { target: m + ch / 2, threshold: 10, guide: pr.y + m, kind: 'top' },
    { target: ph - m - ch / 2, threshold: 10, guide: pr.y + ph - m, kind: 'bottom' },
  ];

  for (const sr of siblingRects || []) {
    if (!sr || !Number.isFinite(sr.x) || !Number.isFinite(sr.y) || sr.w <= 0 || sr.h <= 0) continue;
    const srLeft = sr.x - pr.x;
    const srCenterX = srLeft + sr.w / 2;
    const srRight = srLeft + sr.w;
    xCandidates.push(
      { target: srCenterX, threshold: 6, guide: pr.x + srCenterX, kind: 'sibling' },
      { target: srLeft + cw / 2, threshold: 6, guide: pr.x + srLeft, kind: 'sibling' },
      { target: srRight - cw / 2, threshold: 6, guide: pr.x + srRight, kind: 'sibling' },
    );

    const srTop = sr.y - pr.y;
    const srCenterY = srTop + sr.h / 2;
    const srBottom = srTop + sr.h;
    yCandidates.push(
      { target: srCenterY, threshold: 6, guide: pr.y + srCenterY, kind: 'sibling' },
      { target: srTop + ch / 2, threshold: 6, guide: pr.y + srTop, kind: 'sibling' },
      { target: srBottom - ch / 2, threshold: 6, guide: pr.y + srBottom, kind: 'sibling' },
    );
  }

  const snapMatchX = pickAnchorSnap(relX, xCandidates);
  const snapMatchY = pickAnchorSnap(relY, yCandidates);
  const snappedRelX = snapMatchX ? clampNum(snapMatchX.target, 0, pw) : relX;
  const snappedRelY = snapMatchY ? clampNum(snapMatchY.target, 0, ph) : relY;
  const snapX = snapMatchX ? Math.round(snapMatchX.guide) : null;
  const snapY = snapMatchY ? Math.round(snapMatchY.guide) : null;
  const snappedXKind = snapMatchX?.kind || null;
  const snappedYKind = snapMatchY?.kind || null;

  const ratioX = snappedRelX / pw;
  const ratioY = snappedRelY / ph;

  let hZone = 'Center';
  if (snappedXKind === 'center') hZone = 'Center';
  else if (snappedXKind === 'left') hZone = 'Left';
  else if (snappedXKind === 'right') hZone = 'Right';
  else if (ratioX < 0.33) hZone = 'Left';
  else if (ratioX > 0.67) hZone = 'Right';

  let vZone = 'Center';
  if (snappedYKind === 'center') vZone = 'Center';
  else if (snappedYKind === 'top') vZone = 'Top';
  else if (snappedYKind === 'bottom') vZone = 'Bottom';
  else if (ratioY < 0.33) vZone = 'Top';
  else if (ratioY > 0.67) vZone = 'Bottom';

  let ox = 0;
  if (hZone === 'Left') {
    if (snappedXKind === 'left') {
      ox = 0;
    } else {
      const rawX = snappedRelX - (m + cw / 2);
      ox = clampNum(snapX !== null ? Math.round(rawX) : quantizeStep(rawX, 2), -1200, 1200);
    }
  } else if (hZone === 'Right') {
    if (snappedXKind === 'right') {
      ox = 0;
    } else {
      const rawX = snappedRelX - (pw - m - cw / 2);
      ox = clampNum(snapX !== null ? Math.round(rawX) : quantizeStep(rawX, 2), -1200, 1200);
    }
  } else if (snappedXKind === 'center') {
    ox = 0;
  } else {
    const rawX = snappedRelX - pw / 2;
    ox = clampNum(snapX !== null ? Math.round(rawX) : quantizeStep(rawX, 2), -1200, 1200);
  }

  let anchorPosition = 'Center';
  let oy = 0;
  if (vZone === 'Top') {
    anchorPosition = `Top${hZone}`;
    if (snappedYKind === 'top') {
      oy = 0;
    } else {
      const rawY = snappedRelY - (m + ch / 2);
      oy = clampNum(snapY !== null ? Math.round(rawY) : quantizeStep(rawY, 2), -1200, 1200);
    }
  } else if (vZone === 'Bottom') {
    anchorPosition = `Bottom${hZone}`;
    if (snappedYKind === 'bottom') {
      oy = 0;
    } else {
      const rawY = snappedRelY - (ph - m - ch / 2);
      oy = clampNum(snapY !== null ? Math.round(rawY) : quantizeStep(rawY, 2), -1200, 1200);
    }
  } else if (hZone === 'Center') {
    anchorPosition = 'Center';
    if (snappedYKind === 'center') {
      oy = 0;
    } else {
      const rawY = snappedRelY - ph / 2;
      oy = clampNum(snapY !== null ? Math.round(rawY) : quantizeStep(rawY, 2), -1200, 1200);
    }
  } else {
    // AnchorLayout has no CenterLeft/CenterRight preset; anchor relative to TopLeft/TopRight.
    anchorPosition = `Top${hZone}`;
    const rawY = snappedRelY - (m + ch / 2);
    oy = clampNum(snapY !== null ? Math.round(rawY) : quantizeStep(rawY, 2), -1200, 1200);
  }

  return {
    anchorProps: {
      anchorPosition,
      anchorMargin: m,
      anchorOffsetX: ox,
      anchorOffsetY: oy,
    },
    snapX,
    snapY,
    previewRect: {
      x: Math.round(pr.x + snappedRelX - cw / 2),
      y: Math.round(pr.y + snappedRelY - ch / 2),
      w: Math.round(cw),
      h: Math.round(ch),
    },
  };
}

export function resolveDropTarget(doc, rects, x, y, payload, viewportTier = 'expanded', options = {}) {
  if (!doc?.root || !payload) return null;
  const parentMap = new Map();
  const depthMap = new Map();

  function indexTree(node, parent, index, depth) {
    parentMap.set(node.id, { node, parent, index, depth });
    depthMap.set(node.id, depth);
    (node.children || []).forEach((child, i) => indexTree(child, node, i, depth + 1));
  }
  indexTree(doc.root, null, 0, 1);

  const srcInfo = payload.kind === 'move' ? parentMap.get(payload.nodeId) : null;
  if (payload.kind === 'move' && !srcInfo) return null;
  const movingSubtreeDepth = payload.kind === 'move' ? subtreeDepth(srcInfo.node) : 1;

  function canDropInto(targetParentId) {
    const parentEntry = parentMap.get(targetParentId);
    if (!parentEntry) return false;
    const targetParent = parentEntry.node;
    if (payload.kind === 'palette') {
      return canAcceptChild(targetParent, 1) && parentEntry.depth + 1 <= MAX_DEPTH;
    }
    if (payload.kind === 'move') {
      if (payload.nodeId === targetParentId || findNode(srcInfo.node, targetParentId)) return false;
      const sameParent = srcInfo.parent?.id === targetParentId;
      if (!sameParent && !canAcceptChild(targetParent, 1)) return false;
      return parentEntry.depth + movingSubtreeDepth <= MAX_DEPTH;
    }
    return false;
  }

  const matches = [];
  let docOrder = 0;
  function walkHits(node, parent, index, depth) {
    if (payload.kind === 'move' && node.id === payload.nodeId) return;
    const order = docOrder++;
    const r = rects?.[node.id];
    if (r && r.visible && x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h) {
      matches.push({ node, parent, index, depth, order, rect: r });
    }
    (node.children || []).forEach((child, i) => walkHits(child, node, i, depth + 1));
  }
  walkHits(doc.root, null, 0, 0);

  matches.sort((a, b) => (b.depth - a.depth) || (b.order - a.order));
  const rootRect = rects?.[doc.root.id] || { x: 0, y: 0, w: 860, h: 540, visible: true };
  const hit = matches[0] || { node: doc.root, parent: null, index: 0, depth: 0, order: 0, rect: rootRect };

  function resolveSlotInParent(parentNode, preferredChild = null) {
    const children = parentNode.children || [];
    const parentRect = rects?.[parentNode.id] || rootRect;
    const sameParent = Boolean(srcInfo && srcInfo.parent?.id === parentNode.id);

    if (parentNode.type === 'AnchorBox') {
      const padding = Number(parentNode.props?.padding ?? 12);
      const dragRect = payload.kind === 'move' ? rects?.[payload.nodeId] : null;
      const childW = options?.dragSize?.w || dragRect?.w || 120;
      const childH = options?.dragSize?.h || dragRect?.h || 36;
      const ax = Number.isFinite(options?.anchorX) ? options.anchorX : x;
      const ay = Number.isFinite(options?.anchorY) ? options.anchorY : y;
      const siblingRects = [];
      for (const ch of children) {
        if (payload.kind === 'move' && ch.id === payload.nodeId) continue;
        const sr = rects?.[ch.id];
        if (sr && sr.visible) siblingRects.push(sr);
      }
      const anchorPlacement = resolveAnchorPlacement(parentRect, ax, ay, childW, childH, padding, siblingRects);
      const targetIndex = sameParent ? srcInfo.index : children.length;
      let isNoop = false;
      if (sameParent) {
        const cur = srcInfo.node.props || {};
        const ap = anchorPlacement.anchorProps;
        isNoop = (cur.anchorPosition || 'TopLeft') === ap.anchorPosition
          && (cur.anchorMargin ?? padding) === ap.anchorMargin
          && (cur.anchorOffsetX ?? 0) === ap.anchorOffsetX
          && (cur.anchorOffsetY ?? 0) === ap.anchorOffsetY;
      }
      const ap = anchorPlacement.anchorProps;
      return {
        targetParentId: parentNode.id,
        targetParentType: parentNode.type,
        targetIndex,
        parentRect,
        horizontal: false,
        anchorChildRect: null,
        before: false,
        isEmptyContainer: children.length === 0,
        anchorPlacement,
        isNoop,
        placementKey: `${parentNode.id}:anchor:${ap.anchorPosition}:${ap.anchorMargin}:${ap.anchorOffsetX}:${ap.anchorOffsetY}`,
      };
    }

    const visiblePeers = [];
    for (let i = 0; i < children.length; i++) {
      const ch = children[i];
      if (payload.kind === 'move' && ch.id === payload.nodeId) continue;
      const r = rects?.[ch.id];
      if (r && r.visible) visiblePeers.push({ child: ch, origIdx: i, rect: r });
    }

    const horizontalGrid = parentNode.type === 'Grid' && viewportTier !== 'compact';
    const horizontalBox = parentNode.type === 'HBox' && viewportTier !== 'compact';

    if (!visiblePeers.length) {
      const onlyMovingSelf = sameParent && children.length === 1;
      return {
        targetParentId: parentNode.id,
        targetParentType: parentNode.type,
        targetIndex: sameParent ? srcInfo.index : children.length,
        parentRect,
        horizontal: horizontalGrid || horizontalBox,
        anchorChildRect: null,
        before: false,
        isEmptyContainer: true,
        anchorPlacement: null,
        isNoop: onlyMovingSelf,
        placementKey: `${parentNode.id}:empty`,
      };
    }

    let targetIndex = children.length;
    let anchorChildRect = visiblePeers[visiblePeers.length - 1].rect;
    let before = false;

    if (horizontalGrid && visiblePeers.length > 0) {
      const rows = [];
      for (const item of visiblePeers) {
        const r = item.rect;
        const lastRow = rows[rows.length - 1];
        if (lastRow) {
          const overlap = Math.min(lastRow.bottom, r.y + r.h) - Math.max(lastRow.top, r.y);
          if (overlap > Math.min(lastRow.maxH, r.h) * 0.2 || Math.abs((r.y + r.h / 2) - lastRow.centerY) <= 14) {
            lastRow.items.push(item);
            lastRow.top = Math.min(lastRow.top, r.y);
            lastRow.bottom = Math.max(lastRow.bottom, r.y + r.h);
            lastRow.maxH = Math.max(lastRow.maxH, r.h);
            lastRow.centerY = (lastRow.top + lastRow.bottom) / 2;
            continue;
          }
        }
        rows.push({ top: r.y, bottom: r.y + r.h, maxH: r.h, centerY: r.y + r.h / 2, items: [item] });
      }
      for (const row of rows) row.items.sort((a, b) => a.rect.x - b.rect.x);

      let chosenRow = rows[0];
      if (y > rows[rows.length - 1].bottom) {
        chosenRow = rows[rows.length - 1];
      } else {
        let bestDist = Infinity;
        for (const row of rows) {
          if (y >= row.top && y <= row.bottom) { chosenRow = row; break; }
          const dist = Math.abs(y - row.centerY);
          if (dist < bestDist) { bestDist = dist; chosenRow = row; }
        }
      }

      if (preferredChild && chosenRow.items.some((it) => it.child.id === preferredChild.id)) {
        const item = chosenRow.items.find((it) => it.child.id === preferredChild.id);
        before = x < item.rect.x + item.rect.w / 2;
        targetIndex = before ? item.origIdx : item.origIdx + 1;
        anchorChildRect = item.rect;
      } else {
        let matched = null;
        for (const item of chosenRow.items) {
          if (x < item.rect.x + item.rect.w / 2) {
            matched = item;
            break;
          }
        }
        if (matched) {
          before = true;
          targetIndex = matched.origIdx;
          anchorChildRect = matched.rect;
        } else {
          const lastItem = chosenRow.items[chosenRow.items.length - 1];
          before = false;
          targetIndex = lastItem.origIdx + 1;
          anchorChildRect = lastItem.rect;
        }
      }
    } else if (preferredChild && visiblePeers.some((it) => it.child.id === preferredChild.id)) {
      const peer = visiblePeers.find((it) => it.child.id === preferredChild.id);
      const center = horizontalBox ? peer.rect.x + peer.rect.w / 2 : peer.rect.y + peer.rect.h / 2;
      before = (horizontalBox ? x : y) < center;
      targetIndex = before ? peer.origIdx : peer.origIdx + 1;
      anchorChildRect = peer.rect;
    } else {
      let matched = null;
      for (const peer of visiblePeers) {
        const center = horizontalBox ? peer.rect.x + peer.rect.w / 2 : peer.rect.y + peer.rect.h / 2;
        if ((horizontalBox ? x : y) < center) {
          matched = peer;
          break;
        }
      }
      if (matched) {
        before = true;
        targetIndex = matched.origIdx;
        anchorChildRect = matched.rect;
      } else {
        const lastPeer = visiblePeers[visiblePeers.length - 1];
        before = false;
        targetIndex = lastPeer.origIdx + 1;
        anchorChildRect = lastPeer.rect;
      }
    }

    let isNoop = false;
    if (sameParent) {
      let effectiveIdx = targetIndex;
      if (srcInfo.index < effectiveIdx) effectiveIdx--;
      effectiveIdx = clampNum(effectiveIdx, 0, Math.max(0, children.length - 1));
      isNoop = effectiveIdx === srcInfo.index;
    }

    return {
      targetParentId: parentNode.id,
      targetParentType: parentNode.type,
      targetIndex,
      parentRect,
      horizontal: horizontalGrid || horizontalBox,
      anchorChildRect,
      before,
      isEmptyContainer: false,
      anchorPlacement: null,
      isNoop,
      placementKey: `${parentNode.id}:${targetIndex}`,
    };
  }

  const hitMeta = STUDIO_CATALOG[hit.node.type];
  if (hitMeta?.container && hit.node.id !== doc.root.id && (hit.node.children || []).length > 0 && hit.parent && canDropInto(hit.parent.id)) {
    const hr = hit.rect;
    const parentHoriz = (hit.parent.type === 'HBox' || hit.parent.type === 'Grid') && viewportTier !== 'compact';
    const edgeLimit = parentHoriz ? Math.min(10, Math.floor(hr.w * 0.18)) : Math.min(10, Math.floor(hr.h * 0.18));
    const nearEdge = edgeLimit >= 4 && (parentHoriz
      ? ((x - hr.x <= edgeLimit) || ((hr.x + hr.w) - x <= edgeLimit))
      : ((y - hr.y <= edgeLimit) || ((hr.y + hr.h) - y <= edgeLimit)));
    if (nearEdge) {
      return resolveSlotInParent(hit.parent, hit.node);
    }
  }

  if (hitMeta?.container && canDropInto(hit.node.id)) {
    return resolveSlotInParent(hit.node, null);
  }

  let branch = hit.node;
  let anc = hit.parent;
  while (anc) {
    if (canDropInto(anc.id)) {
      return resolveSlotInParent(anc, branch);
    }
    branch = anc;
    anc = parentMap.get(anc.id)?.parent || null;
  }

  if (canDropInto(doc.root.id)) {
    return resolveSlotInParent(doc.root, null);
  }
  return null;
}

export function buildDragPreviewDocument(baseDoc, payload, target, previewNode = null) {
  if (!baseDoc?.root || !payload || !target) return { ok: false, error: 'Invalid preview arguments.' };
  const targetParent = findNode(baseDoc.root, target.targetParentId);
  if (!targetParent) return { ok: false, error: 'Target parent not found.' };

  if (payload.kind === 'palette') {
    const fresh = previewNode ? cloneDoc(previewNode) : createNode(payload.type, baseDoc);
    if (targetParent.type === 'AnchorBox' && target.anchorPlacement?.anchorProps) {
      fresh.props = { ...(fresh.props || {}), ...target.anchorPlacement.anchorProps };
    }
    return insertNode(baseDoc, target.targetParentId, target.targetIndex, fresh);
  }

  if (payload.kind === 'move') {
    const moved = moveNode(baseDoc, payload.nodeId, target.targetParentId, target.targetIndex);
    if (!moved.ok) return moved;
    if (targetParent.type === 'AnchorBox' && target.anchorPlacement?.anchorProps) {
      return {
        ...updateNodeProps(moved.document, payload.nodeId, target.anchorPlacement.anchorProps),
        nodeId: payload.nodeId,
      };
    }
    const stripPatch = {};
    for (const k of ANCHOR_PROP_KEYS) stripPatch[k] = undefined;
    return {
      ...updateNodeProps(moved.document, payload.nodeId, stripPatch),
      nodeId: payload.nodeId,
    };
  }

  return { ok: false, error: 'Unsupported drag kind.' };
}
