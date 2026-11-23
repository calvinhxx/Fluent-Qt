import { STUDIO_CATALOG, STUDIO_CATEGORIES, findParent } from './studio-core.mjs';

export const AI_STYLE_PRESETS = {
  auto: { id: 'auto', theme: null, accentColor: null, hint: '' },
  'winui-light': {
    id: 'winui-light',
    theme: 'light',
    accentColor: '#0067C0',
    hint: 'Use WinUI 3 light design tokens (design:{"theme":"light","accentColor":"#0067C0"}), clean Layer cards and balanced spacing.',
  },
  'cyber-dark': {
    id: 'cyber-dark',
    theme: 'dark',
    accentColor: '#00B7C3',
    hint: 'Use dark telemetry style (design:{"theme":"dark","accentColor":"#00B7C3"}), sleek NavigationView rail, KPI cards, charts, and DataGrid status pills.',
  },
  'editorial-coral': {
    id: 'editorial-coral',
    theme: 'light',
    accentColor: '#D84A38',
    hint: 'Use warm editorial/gallery light style (design:{"theme":"light","accentColor":"#D84A38"}), GridView(mediaTheme) tiles and ImageViewer showcase.',
  },
  'creative-violet': {
    id: 'creative-violet',
    theme: 'dark',
    accentColor: '#8764B8',
    hint: 'Use dark creative studio style (design:{"theme":"dark","accentColor":"#8764B8"}), bespoke inline SVG artwork in ImageViewer and media cards.',
  },
  'emerald-finance': {
    id: 'emerald-finance',
    theme: 'light',
    accentColor: '#107C10',
    hint: 'Use clean finance/productivity light style (design:{"theme":"light","accentColor":"#107C10"}), structured KPI Grid and clean DataGrid.',
  },
};

const OMIT_SUMMARY_PROPS = new Set([
  'enabled',
  'wordWrap',
  'isClosable',
  'tabsClosable',
  'searchEnabled',
  'alphaEnabled',
  'delay',
  'interval',
  'minVisibleLines',
  'showButtons',
  'showIndicator',
  'iconSize',
  'helperText',
  'validationState',
  'validationMessage',
  'readOnly',
  'layoutMode',
  'expansionMode',
  'sourceHeader',
  'targetHeader',
  'maxRating',
  'lowerValue',
  'upperValue',
  'cameraDistance',
  'zoom',
  'posX',
  'posY',
  'posZ',
  'rotX',
  'rotY',
  'rotZ',
  'scale',
  'surfaceIntensity',
  'hoverLift',
  'particleCount',
  'animationEnabled',
  'interactive',
  'backgroundMode',
  'spatialEnabled',
  'pointerTrackingEnabled',
  'renderMode',
  'timeEnabled',
  'presetsVisible',
  'browseText',
  'hintText',
  'description',
  'header',
  'contextText',
  'listPaneWidth',
  'shimmerTemplate',
  'backdropEffect',
]);

const MIN_HEIGHT_TYPES = new Set([
  'NavigationView',
  'SplitView',
  'ListDetailView',
  'GridView',
  'DataGrid',
  'ImageViewer',
  'AreaChart',
  'LineChart',
  'BarChart',
]);

const COMPACT_SELECT_OPTIONS = {
  'Label.typography': 'Caption|Body|Subtitle|Title|TitleLarge',
  'Label.colorRole': 'Default|Secondary|Accent|Critical',
  'Card.appearance': 'Layer|LayerAlt|Canvas',
  'Button.fluentStyle': 'Standard|Accent|Subtle',
};

const COMPACT_KEY_ONLY_PROPS = new Set([
  'mediaTheme',
  'glyph',
  'fluentSize',
  'avatarSize',
  'presence',
  'status',
  'labelPosition',
]);

let cachedSystemPrompt = null;

export const PROMPT_CACHE_KEY = 'fluentqt_studio_v1';

export function buildCatalogSummary() {
  const lines = [];
  for (const cat of STUDIO_CATEGORIES) {
    const defs = Object.values(STUDIO_CATALOG).filter((d) => d.category === cat.id);
    const entries = defs.map((d) => {
      const cap = d.container ? (d.maxChildren === 1 ? '(1)' : '(*)') : '';
      const schemaKeys = [];
      for (const f of d.propSchema || []) {
        if (OMIT_SUMMARY_PROPS.has(f.key)) continue;
        if (f.key === 'minHeight' && !MIN_HEIGHT_TYPES.has(d.type)) continue;
        if (f.key === 'displayMode' && d.type !== 'NavigationView') continue;
        const compactOpts = COMPACT_SELECT_OPTIONS[`${d.type}.${f.key}`];
        if (compactOpts) {
          schemaKeys.push(`${f.key}:${compactOpts}`);
        } else if (f.kind === 'select' && Array.isArray(f.options) && !COMPACT_KEY_ONLY_PROPS.has(f.key)) {
          schemaKeys.push(`${f.key}:${f.options.join('|')}`);
        } else {
          schemaKeys.push(f.key);
        }
      }
      if (d.type === 'Button' && !schemaKeys.includes('icon')) schemaKeys.push('icon');
      if (d.type === 'DataGrid' && !schemaKeys.includes('modelData')) schemaKeys.push('modelData');
      return schemaKeys.length ? `${d.type}${cap}[${schemaKeys.join(',')}]` : `${d.type}${cap}`;
    });
    lines.push(`${cat.id}: ${entries.join('; ')}`);
  }
  return lines.join('\n');
}

const FEW_SHOT_DASHBOARD = {
  schemaVersion: 1,
  design: { theme: 'dark', accentColor: '#00B7C3' },
  root: {
    id: 'root',
    type: 'VBox',
    props: { spacing: 12, padding: 14 },
    children: [
      {
        id: 'nav',
        type: 'NavigationView',
        props: { displayMode: 'Left', paneOpen: true, items: ['Grid|推理集群', 'Camera|模型库', 'Sync|队列', 'Settings|设置'], minHeight: 500 },
        children: [
          {
            id: 'p0',
            type: 'VBox',
            props: { spacing: 12, padding: 12 },
            children: [
              {
                id: 'hdr',
                type: 'HBox',
                props: { spacing: 10, align: 'Center' },
                children: [
                  { id: 'ico', type: 'FontIcon', props: { glyph: 'Shield' } },
                  { id: 'title', type: 'Label', props: { text: 'GPU 推理集群监控', typography: 'Title' } },
                  { id: 'q', type: 'AutoSuggestBox', props: { placeholderText: '搜索节点或模型...' } },
                  { id: 'av', type: 'Avatar', props: { name: 'Ops', presence: 'Online' } },
                  { id: 'sync', type: 'Button', props: { text: '同步', icon: 'Sync', fluentStyle: 'Accent' } },
                ],
              },
              {
                id: 'kpis',
                type: 'Grid',
                props: { columns: 2, spacing: 12 },
                children: [
                  {
                    id: 'k1',
                    type: 'Card',
                    props: { spacing: 4, padding: 12 },
                    children: [
                      { id: 'k1l', type: 'Label', props: { text: 'P95 延迟', typography: 'Caption', colorRole: 'Secondary' } },
                      { id: 'k1v', type: 'Label', props: { text: '38 ms', typography: 'TitleLarge', colorRole: 'Accent' } },
                      { id: 'k1b', type: 'InfoBadge', props: { displayMode: 'Value', value: 99, status: 'Success' } },
                    ],
                  },
                  {
                    id: 'k2',
                    type: 'Card',
                    props: { spacing: 4, padding: 12 },
                    children: [
                      { id: 'k2l', type: 'Label', props: { text: '显存利用率', typography: 'Caption', colorRole: 'Secondary' } },
                      { id: 'k2v', type: 'Label', props: { text: '76%', typography: 'TitleLarge' } },
                      { id: 'k2p', type: 'ProgressBar', props: { value: 76 } },
                    ],
                  },
                ],
              },
              { id: 'chart', type: 'AreaChart', props: { title: '吞吐 (tok/s)', seriesName: 'QPS', values: [420, 465, 510, 495, 548, 590], minHeight: 170 } },
              {
                id: 'grid',
                type: 'DataGrid',
                props: {
                  headers: ['节点', '负载', '延迟', '状态'],
                  modelData: [
                    ['h100-01', '72%', '34 ms', 'Online'],
                    ['h100-02', '64%', '29 ms', 'Healthy'],
                    ['a100-07', '91%', '84 ms', 'Warning'],
                  ],
                  minHeight: 168,
                },
              },
            ],
          },
        ],
      },
    ],
  },
};

const FEW_SHOT_MEDIA = {
  schemaVersion: 1,
  design: { theme: 'light', accentColor: '#D84A38' },
  root: {
    id: 'root',
    type: 'VBox',
    props: { spacing: 10, padding: 14 },
    children: [
      {
        id: 'hdr',
        type: 'HBox',
        props: { spacing: 10, align: 'Center' },
        children: [
          { id: 'ic', type: 'FontIcon', props: { glyph: 'Camera' } },
          { id: 'title', type: 'Label', props: { text: '全部相片', typography: 'Title' } },
          { id: 'q', type: 'AutoSuggestBox', props: { placeholderText: '搜索相片或标签...' } },
          { id: 'imp', type: 'Button', props: { text: '导入', icon: 'Add', fluentStyle: 'Accent' } },
        ],
      },
      { id: 'cmd', type: 'CommandBar', props: { items: ['Folder|相册', 'Heart|收藏', 'Filter|筛选', 'Share|分享'] } },
      {
        id: 'body',
        type: 'SplitView',
        props: { ratio: 54, minHeight: 400 },
        children: [
          {
            id: 'tiles',
            type: 'GridView',
            props: {
              headerText: '夏季精选',
              mediaTheme: 'Landscape',
              items: ['海岸日落 | RAW · 4K', '山间步道 | HEIC', '湖畔清晨 | 广角', '街角咖啡 | 35mm'],
              minHeight: 320,
            },
          },
          {
            id: 'detail',
            type: 'Card',
            props: { spacing: 8, padding: 12 },
            children: [
              { id: 'pv', type: 'ImageViewer', props: { mediaTheme: 'Landscape', minHeight: 220 } },
              { id: 'cap', type: 'Label', props: { text: '海岸日落.jpg', typography: 'Subtitle' } },
              {
                id: 'acts',
                type: 'HBox',
                props: { spacing: 8, align: 'Start' },
                children: [
                  { id: 'fav', type: 'Button', props: { text: '收藏', icon: 'Heart' } },
                  { id: 'ed', type: 'Button', props: { text: '调色', icon: 'Brush', fluentStyle: 'Accent' } },
                ],
              },
            ],
          },
        ],
      },
    ],
  },
};

function getStaticSystemPrompt() {
  if (cachedSystemPrompt) return cachedSystemPrompt;
  const catalog = buildCatalogSummary();
  cachedSystemPrompt = [
    'Fluent-Qt Desktop UI Architect. Output ONLY 1 minified single-line JSON matching Studio schema.',
    '',
    'Design & Craft Rules:',
    '1. Design: optional root "design":{"theme":"light"|"dark","accentColor":"#RRGGBB"} by domain: Photo/Editorial->light,#D84A38; AI/DevOps->dark,#00B7C3; Music/Creative->dark,#8764B8; Finance/Productivity->light,#107C10|#0067C0.',
    '2. Icons: NavigationView.items & CommandBar.items accept "Icon|Label" (e.g. "Grid|Overview","Camera|Photos"). Button.icon & FontIcon.glyph use: Home,Grid,List,Camera,Video,Music,Headphones,Play,Pause,Heart,FavoriteStar,Folder,File,Document,Cloud,Upload,Download,Search,Settings,Edit,Delete,Share,Filter,Sync,Refresh,Brush,Color,Shield,Lock,Mail,Calendar,Clock,People,World,Info,Warning,CheckMark,Add.',
    '3. Media & Data: GridView sets mediaTheme(Landscape|Coffee|Architecture|City|Portrait|Technology|Abstract|Music)+"Title | Subtitle" items; ImageViewer sets mediaTheme OR safe inline svg("<svg viewBox=\\"0 0 640 360\\" xmlns=\\"http://www.w3.org/2000/svg\\">...</svg>", NO script/foreignObject/on*/http(s)://); DataGrid ALWAYS sets DataGrid.headers+DataGrid.modelData(English status words Online,Healthy,Verified,Ready,Warning,Pending,Offline for pills).',
    '4. Shells: NavigationView children->wrap EACH page in 1 VBox; sidebar->ListDetailView(listPaneWidth:204) or SplitView(max 2 panes,never 3 narrow cols); hero+detail->SplitView(ratio:54). Wrap Card Buttons in HBox(align:"Start"|"Center"); max 1 Accent Button/section; NEVER nest Card in Card.',
    '5. Budget: Match prompt lang; 18-42 nodes(depth<=7); omit defaults; zero prose.',
    '6. Node: {"id":/^[A-Za-z_][A-Za-z_0-9]*$/,"type":type,"props":{...},"children":[...]}. (*)multi,(1)<=1,leaves:no children.',
    '',
    'Catalog:',
    catalog,
    '',
    'Ex1:',
    JSON.stringify(FEW_SHOT_DASHBOARD),
    'Ex2:',
    JSON.stringify(FEW_SHOT_MEDIA),
  ].join('\n');
  return cachedSystemPrompt;
}

export function buildAiMessages({
  prompt,
  lang = 'zh',
  mode = 'create',
  currentDoc = null,
  targetNodeId = null,
  stylePreset = 'auto',
} = {}) {
  const system = getStaticSystemPrompt();
  const preset = AI_STYLE_PRESETS[stylePreset] || AI_STYLE_PRESETS.auto;
  const presetLine = preset.hint ? `Style preset: ${preset.hint}` : '';
  const cleanReq = String(prompt || '').trim();

  if (mode === 'refine' && currentDoc?.root && targetNodeId && targetNodeId !== currentDoc.root.id) {
    const info = findParent(currentDoc.root, targetNodeId);
    if (info && info.parent) {
      const parentMeta = STUDIO_CATALOG[info.parent.type];
      const subJson = JSON.stringify(info.node);
      const user = [
        `Mode: refine selected node subtree (lang=${lang}).`,
        `Target node id: "${targetNodeId}" inside parent ${info.parent.type}(maxChildren=${parentMeta?.maxChildren ?? '*'}).`,
        ...(presetLine ? [presetLine] : []),
        `Selected node JSON: ${subJson}`,
        `Request: ${cleanReq}`,
        `Output ONLY 1 minified JSON object for the updated node (keeping id="${targetNodeId}").`,
      ].join('\n');
      return [
        { role: 'system', content: system },
        { role: 'user', content: user },
      ];
    }
  }

  if (mode === 'iterate' && currentDoc?.root) {
    const docJson = JSON.stringify(currentDoc);
    const user = [
      `Mode: iterate current desktop page (lang=${lang}).`,
      ...(presetLine ? [presetLine] : []),
      `Current document JSON: ${docJson}`,
      `Request: ${cleanReq}`,
      'Apply the changes cleanly while preserving existing design tokens, ids, and unaffected panels. Output ONLY 1 minified JSON document.',
    ].join('\n');
    return [
      { role: 'system', content: system },
      { role: 'user', content: user },
    ];
  }

  const user = [
    `Mode: create new desktop page (lang=${lang}).`,
    ...(presetLine ? [presetLine] : []),
    `Request: ${cleanReq}`,
  ].join('\n');

  return [
    { role: 'system', content: system },
    { role: 'user', content: user },
  ];
}
