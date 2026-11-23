import { isSafeStudioSvg, isStudioPhotoUrl } from './studio-media.mjs';
import { AI_STYLE_PRESETS, buildAiMessages, PROMPT_CACHE_KEY } from './studio-ai-prompt.mjs';
export { AI_STYLE_PRESETS, buildAiMessages, buildCatalogSummary, PROMPT_CACHE_KEY } from './studio-ai-prompt.mjs';
import {
  ANCHOR_CHILD_SCHEMA,
  MAX_DEPTH,
  MAX_NODES,
  STUDIO_CATALOG,
  cloneDoc,
  findNode,
  findParent,
  validateDocument,
} from './studio-core.mjs';
import { playgroundFields } from './studio-metadata.mjs';

export function providerOrigin(address) {
  try {
    const url = new URL(address);
    const local = ['localhost', '127.0.0.1', '[::1]'].includes(url.hostname);
    if (!url.username && !url.password && (url.protocol === 'https:' || (local && url.protocol === 'http:'))) {
      return url.origin;
    }
  } catch { /* The user can be partway through typing an address. */ }
  return '';
}

export function changeAiEndpoint(config, address) {
  const origin = providerOrigin(address);
  if (!origin || config.keyOrigin !== origin) config.apiKey = '';
  config.baseUrl = address;
  config.keyOrigin = origin;
}

export const AI_PROVIDER_PRESETS = {
  deepseek: { label: 'DeepSeek', baseUrl: 'https://api.deepseek.com/v1', model: 'deepseek-chat' },
  openrouter: { label: 'OpenRouter (Free)', baseUrl: 'https://openrouter.ai/api/v1', model: 'meta-llama/llama-3.3-70b-instruct:free' },
  openai: { label: 'OpenAI', baseUrl: 'https://api.openai.com/v1', model: 'gpt-4o-mini' },
  ollama: { label: 'Ollama (Local)', baseUrl: 'http://localhost:11434/v1', model: 'qwen2.5:7b' },
};

const TYPE_ALIASES = new Map([
  ['vboxlayout', 'VBox'],
  ['qvboxlayout', 'VBox'],
  ['column', 'VBox'],
  ['stack', 'VBox'],
  ['hboxlayout', 'HBox'],
  ['qhboxlayout', 'HBox'],
  ['row', 'HBox'],
  ['gridlayout', 'Grid'],
  ['qgridlayout', 'Grid'],
  ['anchorlayout', 'AnchorBox'],
  ['canvas', 'AnchorBox'],
  ['fluentcard', 'Card'],
  ['panel', 'Card'],
  ['groupbox', 'Card'],
  ['section', 'Card'],
  ['formfield', 'Field'],
  ['fluentfield', 'Field'],
  ['separator', 'Divider'],
  ['spacer', 'Divider'],
  ['pushbutton', 'Button'],
  ['qpushbutton', 'Button'],
  ['fluentbutton', 'Button'],
  ['switch', 'ToggleSwitch'],
  ['toggle', 'ToggleSwitch'],
  ['select', 'ComboBox'],
  ['dropdown', 'ComboBox'],
  ['qcombobox', 'ComboBox'],
  ['textbox', 'LineEdit'],
  ['textfield', 'LineEdit'],
  ['textinput', 'LineEdit'],
  ['input', 'LineEdit'],
  ['qlineedit', 'LineEdit'],
  ['password', 'PasswordBox'],
  ['passwordinput', 'PasswordBox'],
  ['spinbox', 'NumberBox'],
  ['qspinbox', 'NumberBox'],
  ['qdoublespinbox', 'NumberBox'],
  ['numberinput', 'NumberBox'],
  ['textarea', 'TextEdit'],
  ['qtextedit', 'TextEdit'],
  ['multilineedit', 'TextEdit'],
  ['searchbox', 'AutoSuggestBox'],
  ['heading', 'Label'],
  ['title', 'Label'],
  ['text', 'Label'],
  ['qlabel', 'Label'],
  ['table', 'DataGrid'],
  ['qtableview', 'DataGrid'],
  ['tableview', 'DataGrid'],
  ['progress', 'ProgressBar'],
  ['qprogressbar', 'ProgressBar'],
  ['spinner', 'ProgressRing'],
  ['loading', 'ProgressRing'],
  ['chart', 'LineChart'],
  ['tabs', 'TabView'],
  ['qtabwidget', 'TabView'],
  ['alert', 'InfoBar'],
  ['banner', 'InfoBar'],
  ['badge', 'InfoBadge'],
  ['rating', 'RatingControl'],
  ['datepicker', 'CalendarDatePicker'],
  ['upload', 'FileDropZone'],
  ['dropzone', 'FileDropZone'],
  ['navbar', 'NavigationView'],
  ['sidebar', 'NavigationView'],
  ['toolbar', 'CommandBar'],
  ['code', 'CodeBlock'],
]);

const SAFE_ID_RE = /^[A-Za-z_][A-Za-z_0-9]*$/;
const SAFE_KEY_RE = /^[A-Za-z_][A-Za-z_0-9]*$/;
const CONTROL_CHAR_RE = /[\u0000-\u0008\u000b\u000c\u000e-\u001f]/g;

function normalizeType(rawType) {
  if (typeof rawType !== 'string') return null;
  const trimmed = rawType.trim();
  if (Object.hasOwn(STUDIO_CATALOG, trimmed)) return trimmed;
  const stripped = trimmed.replace(/^(?:fluent::(?:\w+::)?|Fluent)/i, '');
  if (Object.hasOwn(STUDIO_CATALOG, stripped)) return stripped;
  const lower = trimmed.toLowerCase();
  const byLower = Object.keys(STUDIO_CATALOG).find((k) => k.toLowerCase() === lower);
  if (byLower) return byLower;
  return TYPE_ALIASES.get(lower) || TYPE_ALIASES.get(stripped.toLowerCase()) || null;
}

function cleanText(value, maxLen = 2000) {
  return String(value).replace(CONTROL_CHAR_RE, '').slice(0, maxLen);
}

function makeUniqueId(preferred, used, fallbackPrefix) {
  let base = typeof preferred === 'string' ? preferred.trim().replace(/[^A-Za-z_0-9]+/g, '_') : '';
  if (!base || !/^[A-Za-z_]/.test(base) || Object.hasOwn(Object.prototype, base) || base === 'prototype') {
    base = `${fallbackPrefix}_${base}`.replace(/^_+|_+$/g, '') || fallbackPrefix;
    if (!/^[A-Za-z_]/.test(base)) base = `${fallbackPrefix}_1`;
  }
  base = base.slice(0, 64);
  if (!used.has(base)) {
    used.add(base);
    return base;
  }
  let idx = 2;
  while (used.has(`${base}_${idx}`)) idx++;
  const unique = `${base}_${idx}`;
  used.add(unique);
  return unique;
}

const NAV_PAGE_CONTAINERS = new Set(['VBox', 'ScrollView', 'SplitView', 'ListDetailView', 'AnchorBox']);

function scanBalancedJsonObject(str, startIdx) {
  let depth = 0;
  let inString = false;
  let escaped = false;
  for (let i = startIdx; i < str.length; i++) {
    const ch = str[i];
    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (ch === '\\') {
        escaped = true;
      } else if (ch === '"') {
        inString = false;
      }
      continue;
    }
    if (ch === '"') {
      inString = true;
    } else if (ch === '{') {
      depth++;
    } else if (ch === '}') {
      depth--;
      if (depth === 0) {
        return { closed: true, endIdx: i, slice: str.slice(startIdx, i + 1) };
      }
    }
  }
  return { closed: false };
}

export function extractJsonFromText(rawText) {
  if (rawText && typeof rawText === 'object') {
    return { ok: true, data: rawText };
  }
  if (typeof rawText !== 'string') {
    return { ok: false, error: 'AI response is empty.' };
  }
  const trimmed = rawText.trim();
  if (!trimmed) {
    return { ok: false, error: 'AI response is empty.' };
  }
  try {
    return { ok: true, data: JSON.parse(trimmed) };
  } catch (_) {
    // Strip markdown fences or extract valid balanced JSON objects.
  }

  let bestValid = null;
  let bestValidLen = 0;
  for (const match of trimmed.matchAll(/```(?:json)?\s*([\s\S]*?)```/gi)) {
    const inner = match[1].trim();
    if (!inner) continue;
    try {
      const parsed = JSON.parse(inner);
      if (parsed && typeof parsed === 'object') {
        if (parsed.root || typeof parsed.type === 'string') {
          return { ok: true, data: parsed };
        }
        if (!bestValid || inner.length > bestValidLen) {
          bestValid = parsed;
          bestValidLen = inner.length;
        }
      }
    } catch (_) {
      // Continue to balanced-brace scan.
    }
  }

  let sawBrace = false;
  let sawUnclosed = false;
  let firstParseError = null;
  let pos = trimmed.indexOf('{');
  while (pos >= 0) {
    sawBrace = true;
    const scan = scanBalancedJsonObject(trimmed, pos);
    if (!scan.closed) {
      sawUnclosed = true;
      pos = trimmed.indexOf('{', pos + 1);
      continue;
    }
    try {
      const parsed = JSON.parse(scan.slice);
      if (parsed && typeof parsed === 'object') {
        if (parsed.root || typeof parsed.type === 'string') {
          return { ok: true, data: parsed };
        }
        if (!bestValid || scan.slice.length > bestValidLen) {
          bestValid = parsed;
          bestValidLen = scan.slice.length;
        }
      }
      pos = trimmed.indexOf('{', scan.endIdx + 1);
    } catch (err) {
      if (!firstParseError) firstParseError = err.message;
      pos = trimmed.indexOf('{', pos + 1);
    }
  }

  if (bestValid) {
    return { ok: true, data: bestValid };
  }
  if (firstParseError) {
    return { ok: false, error: `Invalid JSON: ${firstParseError}` };
  }
  if (sawBrace && sawUnclosed) {
    return { ok: false, error: 'Unclosed JSON object in AI output.' };
  }
  return { ok: false, error: 'No JSON object found in AI output.' };
}

const HEX_COLOR_RE = /^#[0-9a-fA-F]{6}$/;
function coercePropValue(meta, key, value, repairs) {
  if (!SAFE_KEY_RE.test(key) || Object.hasOwn(Object.prototype, key) || key === 'prototype') {
    return { ok: false };
  }
  const field = (meta.propSchema || []).find((f) => f.key === key)
    || ANCHOR_CHILD_SCHEMA.find((f) => f.key === key);
  const extra = playgroundFields[meta.type]?.[key];
  const hasDefault = Object.hasOwn(meta.defaultProps || {}, key);
  if (!field && !extra && !hasDefault) {
    return { ok: false };
  }
  const kind = field?.kind || extra?.type || typeof meta.defaultProps[key];
  const options = field?.options || extra?.options;

  if (value === null) {
    return (extra?.nullable || key === 'cornerRadius') ? { ok: true, value: null } : { ok: false };
  }

  if (meta.type === 'ImageViewer' && key === 'svg') {
    if (typeof value !== 'string') return { ok: false };
    const trimmed = value.trim();
    if (!isSafeStudioSvg(trimmed)) {
      repairs.push('dropped unsafe or invalid ImageViewer.svg');
      return { ok: false };
    }
    return { ok: true, value: trimmed };
  }

  if (meta.type === 'ImageViewer' && key === 'imageUrl') {
    if (typeof value !== 'string') return { ok: false };
    const trimmed = value.trim();
    if (!isStudioPhotoUrl(trimmed)) {
      repairs.push('dropped unsupported ImageViewer.imageUrl');
      return { ok: false };
    }
    return { ok: true, value: trimmed };
  }

  switch (kind) {
    case 'number': {
      const n = typeof value === 'number' ? value : Number(String(value).trim());
      if (!Number.isFinite(n)) return { ok: false };
      const bounded = field
        ? Math.max(field.min ?? -Infinity, Math.min(field.max ?? Infinity, n))
        : n;
      return { ok: true, value: bounded };
    }
    case 'boolean': {
      if (typeof value === 'boolean') return { ok: true, value };
      if (typeof value === 'string') {
        const low = value.trim().toLowerCase();
        if (low === 'true' || low === '1' || low === 'yes') return { ok: true, value: true };
        if (low === 'false' || low === '0' || low === 'no') return { ok: true, value: false };
      }
      return { ok: false };
    }
    case 'select':
    case 'enum': {
      if (typeof value !== 'string') return { ok: false };
      const str = value.trim();
      if (key === 'icon' || key === 'glyph') {
        return SAFE_ID_RE.test(str) && str.length <= 80 ? { ok: true, value: str } : { ok: false };
      }
      if (Array.isArray(options) && options.length > 0) {
        if (options.includes(str)) return { ok: true, value: str };
        const matched = options.find((opt) => opt.toLowerCase() === str.toLowerCase());
        if (matched) {
          repairs.push(`normalized ${meta.type}.${key} -> ${matched}`);
          return { ok: true, value: matched };
        }
        return { ok: false };
      }
      return SAFE_ID_RE.test(str) ? { ok: true, value: str } : { ok: false };
    }
    case 'numlist': {
      const arr = Array.isArray(value)
        ? value
        : (typeof value === 'string' ? value.split(/[\s,;]+/).filter(Boolean) : null);
      if (!arr) return { ok: false };
      const nums = arr.map((v) => Number(v)).filter(Number.isFinite);
      return nums.length ? { ok: true, value: nums.slice(0, 32) } : { ok: false };
    }
    case 'list':
    case 'string-list': {
      const arr = Array.isArray(value)
        ? value
        : (typeof value === 'string' ? value.split(/\r?\n|,/).map((s) => s.trim()).filter(Boolean) : null);
      if (!arr) return { ok: false };
      return { ok: true, value: arr.map((s) => cleanText(s, 160)).slice(0, 24) };
    }
    case 'multi-enum': {
      if (!Array.isArray(value)) return { ok: false };
      const valid = value.filter((v) => typeof v === 'string' && (!options || options.includes(v)));
      return { ok: true, value: valid };
    }
    case 'table': {
      if (!Array.isArray(value)) return { ok: false };
      const rows = value.slice(0, 32)
        .filter(Array.isArray)
        .map((row) => row.map((cell) => {
          if (cell === null || typeof cell === 'boolean' || Number.isFinite(cell)) return cell;
          return cleanText(cell ?? '', 160);
        }));
      return { ok: true, value: rows };
    }
    case 'text':
    case 'textarea':
    case 'string':
    case 'color':
    case 'date':
    case 'time':
    case 'datetime': {
      if (typeof value === 'object') return { ok: false };
      return { ok: true, value: cleanText(value, extra?.maximumLength || 2000) };
    }
    default:
      return { ok: false };
  }
}

export function sanitizeAiDocument(rawInput) {
  const extracted = extractJsonFromText(rawInput);
  if (!extracted.ok) return extracted;
  let candidate = extracted.data;
  if (candidate && typeof candidate === 'object' && candidate.document && typeof candidate.document === 'object') {
    candidate = candidate.document;
  }
  if (!candidate || typeof candidate !== 'object') {
    return { ok: false, error: 'AI output must be a JSON object.' };
  }

  let rawRoot = candidate.root;
  if (!rawRoot && typeof candidate.type === 'string') {
    rawRoot = candidate;
  }
  if (!rawRoot || typeof rawRoot !== 'object') {
    return { ok: false, error: 'Document must contain a root object.' };
  }

  const repairs = [];
  const rawRootType = normalizeType(rawRoot.type);
  if (!rawRootType || !STUDIO_CATALOG[rawRootType].container) {
    repairs.push('wrapped non-container root in VBox');
    rawRoot = {
      id: 'root',
      type: 'VBox',
      props: { spacing: 14, padding: 18 },
      children: rawRootType ? [rawRoot] : [],
    };
  }

  const usedIds = new Set();
  let nodeCount = 0;

  function maxSubtreeDepth(node) {
    if (!node || typeof node !== 'object') return 0;
    const ch = Array.isArray(node.children) ? node.children : [];
    let best = 0;
    for (const c of ch) {
      best = Math.max(best, maxSubtreeDepth(c));
    }
    return 1 + best;
  }

  function sanitizeNode(raw, depth, isRoot = false, parentType = null) {
    if (!raw || typeof raw !== 'object' || depth > MAX_DEPTH || nodeCount >= MAX_NODES) {
      return null;
    }
    const rawType = normalizeType(raw.type);
    if (!rawType) {
      repairs.push(`dropped unsupported type "${String(raw.type)}"`);
      return null;
    }
    let type = rawType;
    let demotedCard = false;
    if (type === 'Card' && parentType === 'Card') {
      type = 'VBox';
      demotedCard = true;
    }
    const meta = STUDIO_CATALOG[type];
    nodeCount++;
    const id = isRoot
      ? (usedIds.add('root'), 'root')
      : makeUniqueId(raw.id, usedIds, type.toLowerCase());
    if (demotedCard) {
      repairs.push(`demoted nested Card "${id}" to VBox`);
    }

    const props = { ...structuredClone(meta.defaultProps || {}) };
    if (raw.props && typeof raw.props === 'object' && !Array.isArray(raw.props)) {
      for (const [k, v] of Object.entries(raw.props)) {
        const coerced = coercePropValue(meta, k, v, repairs);
        if (coerced.ok) {
          props[k] = coerced.value;
        }
      }
    }
    if (demotedCard && typeof props.padding === 'number') {
      props.padding = Math.min(props.padding, 10);
    }

    const node = { id, type, props };
    if (meta.container) {
      let rawChildren = Array.isArray(raw.children) ? raw.children : [];
      if (meta.maxChildren === 1 && rawChildren.length > 1) {
        if (type === 'SpatialItem') {
          repairs.push('wrapped multiple SpatialItem children in Card');
          rawChildren = [{ id: `${id}_card`, type: 'Card', props: { appearance: 'Layer', borderVisible: true, spacing: 10, padding: 14 }, children: rawChildren }];
        } else {
          repairs.push(`trimmed ${type} children to 1`);
          rawChildren = rawChildren.slice(0, 1);
        }
      }
      const children = [];
      for (const childRaw of rawChildren) {
        if (nodeCount >= MAX_NODES) break;
        const sanitizedChild = sanitizeNode(childRaw, depth + 1, false, type);
        if (sanitizedChild) children.push(sanitizedChild);
      }
      if (
        type === 'NavigationView'
        && children.length > 1
        && !children.every((child) => NAV_PAGE_CONTAINERS.has(child.type))
        && nodeCount < MAX_NODES
        && (depth + 1 + Math.max(0, ...children.map(maxSubtreeDepth))) <= MAX_DEPTH
      ) {
        const pageId = makeUniqueId(`${id}_page`, usedIds, 'page');
        nodeCount++;
        repairs.push(`wrapped NavigationView "${id}" children into VBox page "${pageId}"`);
        node.children = [{
          id: pageId,
          type: 'VBox',
          props: { spacing: 12, padding: 14 },
          children,
        }];
      } else {
        node.children = children;
      }
    } else if (Array.isArray(raw.children) && raw.children.length > 0) {
      repairs.push(`dropped children on leaf ${type}`);
    }
    return node;
  }

  const root = sanitizeNode(rawRoot, 1, true, null);
  if (!root) {
    return { ok: false, error: 'Failed to sanitize root container.' };
  }
  const doc = { schemaVersion: 1, root };
  if (candidate.design && typeof candidate.design === 'object' && !Array.isArray(candidate.design)) {
    const cleanDesign = {};
    if (typeof candidate.design.theme === 'string') {
      const th = candidate.design.theme.trim().toLowerCase();
      if (th === 'light' || th === 'dark') cleanDesign.theme = th;
      else repairs.push('dropped invalid design.theme');
    }
    if (typeof candidate.design.accentColor === 'string') {
      const acc = candidate.design.accentColor.trim();
      if (HEX_COLOR_RE.test(acc)) cleanDesign.accentColor = acc.toUpperCase();
      else repairs.push('dropped invalid design.accentColor');
    }
    if (Object.keys(cleanDesign).length > 0) doc.design = cleanDesign;
  }
  const check = validateDocument(doc);
  if (!check.ok) {
    return { ok: false, error: check.error };
  }
  return { ok: true, document: doc, count: check.count, repairs };
}

export function applyRefinedSubtree(baseDoc, targetNodeId, rawInput) {
  if (!baseDoc?.root || !targetNodeId) {
    return { ok: false, error: 'Missing base document or target node id.' };
  }
  const extracted = extractJsonFromText(rawInput);
  if (!extracted.ok) return extracted;
  let candidate = extracted.data;
  if (candidate && typeof candidate === 'object' && candidate.document && typeof candidate.document === 'object') {
    candidate = candidate.document;
  }
  if (!candidate || typeof candidate !== 'object') {
    return { ok: false, error: 'AI output must be a JSON object.' };
  }

  if (candidate.root || candidate.id === baseDoc.root.id) {
    const page = candidate.root ? candidate : { root: candidate };
    const check = validateDocument(page);
    if (!check.ok) return check;
    candidate = findNode(page.root, targetNodeId);
    if (!candidate) return { ok: false, error: 'The response omitted the selected node.' };
  }
  if (candidate.id && candidate.id !== targetNodeId) {
    return { ok: false, error: 'The response refers to a different selected node.' };
  }

  const cloned = cloneDoc(baseDoc);
  const found = findParent(cloned.root, targetNodeId);
  if (!found?.parent) return { ok: false, error: 'The selected node no longer exists.' };

  // Normalize only the replacement. Sibling ids, properties and page design belong to the user.
  const patch = sanitizeAiDocument({ root: {
    id: 'root', type: 'VBox', children: [{ ...candidate, id: targetNodeId }],
  } });
  const replacement = patch.document?.root.children[0];
  if (!replacement) return { ok: false, error: 'Invalid replacement for the selected node.' };
  replacement.id = targetNodeId;
  found.parent.children[found.index] = replacement;
  const result = validateDocument(cloned);
  if (!result.ok) return result;
  return { ...result, document: cloned, focusNodeId: targetNodeId, repairs: patch.repairs };
}

function combineSignals(userSignal, timeoutMs = 30000) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(new Error('Request timed out')), timeoutMs);
  const onAbort = () => controller.abort(userSignal?.reason);
  if (userSignal) {
    if (userSignal.aborted) controller.abort(userSignal.reason);
    else userSignal.addEventListener('abort', onAbort, { once: true });
  }
  return {
    signal: controller.signal,
    cleanup: () => {
      clearTimeout(timer);
      if (userSignal) userSignal.removeEventListener('abort', onAbort);
    },
  };
}

function isReasoningFamilyModel(model) {
  return /(?:^|\/)(?:o\d|gpt-[5-9]|gpt-\d{2,}|deepseek-reasoner|r1\b)/i.test(String(model || '').trim());
}

function resolveSdkModel({ createOpenAI, createOpenAICompatible }, { baseUrl, apiKey, model, fetchImpl }) {
  let clean = String(baseUrl || '').trim().replace(/\/+$/, '');
  if (/^https?:\/\/(?:api\.openai\.com|api\.deepseek\.com|localhost:\d+|127\.0\.0\.1:\d+)$/i.test(clean)) {
    clean = `${clean}/v1`;
  }
  const explicitChat = /\/chat\/completions$/i.test(clean);
  const explicitResponses = /\/responses$/i.test(clean);
  const baseURL = clean.replace(/\/(?:chat\/completions|responses)$/i, '');
  const options = { baseURL, fetch: (input, init) => fetchImpl.call(globalThis, input, init) };
  if (explicitResponses || providerOrigin(baseURL) === 'https://api.openai.com') {
    const openai = createOpenAI({ ...options, apiKey });
    return explicitChat
      ? { modelInstance: openai.chat(model), isOpenAi: true, api: 'chat' }
      : { modelInstance: openai.responses(model), isOpenAi: true, api: 'responses' };
  }
  const compat = createOpenAICompatible({ ...options, name: 'custom', apiKey: apiKey || undefined, includeUsage: true });
  return { modelInstance: compat.chatModel(model), isOpenAi: false, api: 'chat' };
}

function normalizeSdkUsage(u) {
  if (!u || typeof u !== 'object') return null;
  const inputTokens = Number(u.inputTokens ?? 0);
  const outputTokens = Number(u.outputTokens ?? 0);
  const cachedTokens = Number(
    u.inputTokenDetails?.cacheReadTokens
      ?? u.cachedInputTokens
      ?? u.raw?.prompt_cache_hit_tokens
      ?? 0,
  );
  const safeIn = Number.isFinite(inputTokens) ? inputTokens : 0;
  const safeOut = Number.isFinite(outputTokens) ? outputTokens : 0;
  if (safeIn <= 0 && safeOut <= 0) return null;
  const safeCached = Number.isFinite(cachedTokens) ? Math.min(Math.max(0, cachedTokens), safeIn) : 0;
  return {
    inputTokens: safeIn,
    outputTokens: safeOut,
    cachedTokens: safeCached,
    cacheHitRate: safeIn > 0 ? Math.round((safeCached / safeIn) * 100) : 0,
  };
}

function mergeUsage(u1, u2) {
  if (!u1) return u2 || null;
  if (!u2) return u1;
  const inputTokens = (u1.inputTokens || 0) + (u2.inputTokens || 0);
  const outputTokens = (u1.outputTokens || 0) + (u2.outputTokens || 0);
  const cachedTokens = (u1.cachedTokens || 0) + (u2.cachedTokens || 0);
  return {
    inputTokens,
    outputTokens,
    cachedTokens,
    cacheHitRate: inputTokens > 0 ? Math.round((cachedTokens / inputTokens) * 100) : 0,
  };
}

function formatSdkError(err) {
  if (typeof err?.statusCode === 'number' || err?.responseBody) {
    const status = err.statusCode;
    let msg = String(err.message || '').trim();
    if (err.responseBody) {
      try {
        const parsed = JSON.parse(err.responseBody);
        const bodyMsg = parsed?.error?.message || parsed?.message;
        if (typeof bodyMsg === 'string' && bodyMsg.trim()) {
          msg = bodyMsg.trim();
        }
      } catch (_) {
        // Keep SDK error message when body is non-JSON.
      }
    }
    if (status) {
      return msg.startsWith(`HTTP ${status}:`) ? msg : `HTTP ${status}: ${msg.slice(0, 240)}`;
    }
    return msg || String(err);
  }
  return err?.message || String(err);
}

async function runSdkStreamOnce({
  sdk: { streamText, Output },
  modelInstance,
  isOpenAi,
  api,
  modelName,
  system,
  messages,
  signal,
  useJsonOutput,
  onProgress,
}) {
  const reasoningModel = isReasoningFamilyModel(modelName);
  const openaiOptions = {
    promptCacheKey: PROMPT_CACHE_KEY,
    ...(reasoningModel
      ? {
          reasoningEffort: 'medium',
          ...(api === 'responses' ? { reasoningSummary: null } : {}),
        }
      : {}),
  };

  let capturedError = null;
  const result = streamText({
    model: modelInstance,
    system,
    messages,
    ...(useJsonOutput ? { output: Output.json() } : {}),
    maxOutputTokens: 8192,
    maxRetries: 0,
    abortSignal: signal,
    ...(reasoningModel ? {} : { temperature: 0.35 }),
    ...(isOpenAi ? { providerOptions: { openai: openaiOptions } } : {}),
    onError: ({ error }) => {
      capturedError = error;
    },
  });

  let text = '';
  let finishReason = 'other';
  let sdkUsage = null;
  let thinkingNotified = false;

  for await (const part of result.fullStream) {
    if (part.type === 'reasoning-delta') {
      if (!thinkingNotified && typeof onProgress === 'function') {
        thinkingNotified = true;
        onProgress({ phase: 'thinking', chars: text.length, widgets: 0 });
      }
    } else if (part.type === 'text-delta') {
      text += part.text;
      if (typeof onProgress === 'function') {
        const widgets = (text.match(/"type"\s*:/g) || []).length;
        onProgress({ phase: 'generating', chars: text.length, widgets });
      }
    } else if (part.type === 'finish') {
      finishReason = part.finishReason || 'other';
      sdkUsage = part.totalUsage ?? null;
    } else if (part.type === 'error') {
      throw part.error;
    }
  }

  if (capturedError) {
    throw capturedError;
  }

  const usage = normalizeSdkUsage(sdkUsage);
  const truncated = finishReason === 'length';
  if (!text.trim()) {
    if (truncated) return { text: '', truncated: true, usage };
    throw new Error('Model returned an empty completion.');
  }
  return { text, truncated, usage };
}

async function runSdkStream(params) {
  try {
    return await runSdkStreamOnce({ ...params, useJsonOutput: true });
  } catch (err) {
    const status = err?.statusCode;
    const detail = `${err?.message || ''} ${err?.responseBody || ''}`;
    if (!params.isOpenAi && status === 400 && /response_format|json_object/i.test(detail)) {
      return runSdkStreamOnce({ ...params, useJsonOutput: false });
    }
    throw err;
  }
}

export async function generateStudioPageWithAi({
  prompt,
  lang = 'zh',
  mode = 'create',
  currentDoc = null,
  targetNodeId = null,
  stylePreset = 'auto',
  config = {},
  fetchImpl = globalThis.fetch,
  signal,
  onProgress,
} = {}) {
  const cleanPrompt = String(prompt || '').trim();
  if (!cleanPrompt) {
    return { ok: false, error: lang === 'zh' ? '请先输入你想生成的页面描述。' : 'Please enter a page description first.' };
  }

  const canRefine = mode === 'refine' && currentDoc?.root && targetNodeId
    && targetNodeId !== currentDoc.root.id && Boolean(findNode(currentDoc.root, targetNodeId));
  if (mode === 'refine' && !canRefine) {
    return { ok: false, error: lang === 'zh' ? '请先选中要调整的控件。' : 'Select a component to refine.' };
  }
  const effectiveMode = canRefine ? 'refine' : (mode === 'iterate' && currentDoc?.root ? 'iterate' : 'create');
  const presetObj = AI_STYLE_PRESETS[stylePreset] || AI_STYLE_PRESETS.auto;
  const parseOutput = (raw) => {
    const res = effectiveMode === 'refine'
      ? applyRefinedSubtree(currentDoc, targetNodeId, raw)
      : sanitizeAiDocument(raw);
    if (res.ok && res.document && effectiveMode !== 'refine') {
      if (!res.document.design && presetObj.theme) {
        res.document.design = { theme: presetObj.theme, accentColor: presetObj.accentColor };
      } else if (!res.document.design && effectiveMode === 'iterate' && currentDoc?.design) {
        res.document.design = structuredClone(currentDoc.design);
      }
      if (!res.focusNodeId) res.focusNodeId = res.document.root.id;
    }
    return res;
  };

  const provider = config.provider === 'custom' ? 'custom' : 'auto';
  if (provider === 'custom') {
    const baseUrl = String(config.baseUrl || '').trim();
    const model = String(config.model || '').trim();
    const apiKey = String(config.apiKey || '').trim();
    const origin = providerOrigin(baseUrl);
    if (!origin || !model) {
      return { ok: false, needsConfig: true, error: lang === 'zh' ? '请填写 HTTPS 服务地址（本机可用 HTTP）与模型名称。' : 'Enter an HTTPS endpoint (HTTP for localhost) and model.' };
    }
    const isLocalHost = ['localhost', '127.0.0.1', '[::1]'].includes(new URL(origin).hostname);
    if (apiKey && config.keyOrigin !== undefined && config.keyOrigin !== origin) {
      return { ok: false, needsConfig: true, error: lang === 'zh' ? '请为当前服务重新输入密钥。' : 'Enter a key for this endpoint.' };
    }
    if (!apiKey && !isLocalHost) {
      return { ok: false, needsConfig: true, error: lang === 'zh' ? '请填写 API Key（本地 Ollama 可留空）。' : 'Please enter an API Key (optional for localhost Ollama).' };
    }
    if (typeof fetchImpl !== 'function') {
      return { ok: false, error: 'Fetch API unavailable.' };
    }

    const [{ content: systemPrompt }, userMsg] = buildAiMessages({
      prompt: cleanPrompt,
      lang,
      mode: effectiveMode,
      currentDoc,
      targetNodeId,
      stylePreset,
    });
    const req = combineSignals(signal, 90000);
    try {
      const sdk = await import('./studio-ai-sdk.mjs');
      const { modelInstance, isOpenAi, api } = resolveSdkModel(sdk, { baseUrl, apiKey, model, fetchImpl });
      const first = await runSdkStream({
        sdk,
        modelInstance,
        isOpenAi,
        api,
        modelName: model,
        system: systemPrompt,
        messages: [userMsg],
        signal: req.signal,
        onProgress,
      });
      let totalUsage = first.usage;
      let parsed = parseOutput(first.text);
      if (!parsed.ok) {
        if (first.truncated || /Unclosed JSON/i.test(parsed.error || '')) {
          return {
            ok: false,
            truncated: true,
            usage: totalUsage,
            error: lang === 'zh'
              ? '模型输出在 JSON 闭合前被截断（已跳过自动重试以避免重复扣费）。请简化描述或检查模型输出上限。'
              : 'Model output was truncated before closing JSON (skipped costly auto-retry). Try a shorter description or check the model output limit.',
          };
        }
        if (first.text.length <= 8000) {
          const retryMessages = [
            userMsg,
            { role: 'assistant', content: first.text.slice(0, 2000) },
            { role: 'user', content: `Your previous output failed validation (${parsed.error}). Reply with ONLY a corrected minified JSON object matching the schema.` },
          ];
          const second = await runSdkStream({
            sdk,
            modelInstance,
            isOpenAi,
            api,
            modelName: model,
            system: systemPrompt,
            messages: retryMessages,
            signal: req.signal,
            onProgress,
          });
          totalUsage = mergeUsage(totalUsage, second.usage);
          parsed = parseOutput(second.text);
          if (!parsed.ok && (second.truncated || /Unclosed JSON/i.test(parsed.error || ''))) {
            return {
              ok: false,
              truncated: true,
              usage: totalUsage,
              error: lang === 'zh'
                ? '模型输出在 JSON 闭合前被截断。请简化描述或检查模型输出上限。'
                : 'Model output was truncated before closing JSON. Try a shorter description or check the model output limit.',
            };
          }
        }
      }
      if (!parsed.ok) return { ...parsed, usage: totalUsage };
      return { ...parsed, mode: effectiveMode, source: 'custom', model, usage: totalUsage };
    } catch (err) {
      if (err?.name === 'AbortError' && signal?.aborted) {
        return { ok: false, canceled: true, error: lang === 'zh' ? '已取消生成' : 'Generation canceled' };
      }
      return { ok: false, error: formatSdkError(err) };
    } finally {
      req.cleanup();
    }
  }

  const workerUrl = String(config.workerUrl || '').trim();
  if (!providerOrigin(workerUrl)) {
    return {
      ok: false,
      needsConfig: true,
      error: lang === 'zh'
        ? '请先在下方「AI 配置」中填写自定义 API Key / Ollama 或免费 Worker 地址。'
        : 'Configure a custom API key / Ollama or free Worker URL in AI Config below.',
    };
  }
  if (typeof fetchImpl !== 'function') {
    return { ok: false, error: 'Fetch API unavailable.' };
  }

  const req = combineSignals(signal, 30000);
  try {
    const res = await fetchImpl(workerUrl, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        prompt: cleanPrompt,
        mode: effectiveMode === 'create' ? 'replace' : effectiveMode,
        currentDoc: effectiveMode === 'create' ? null : currentDoc,
        targetNodeId: effectiveMode === 'refine' ? targetNodeId : null,
        stylePreset,
        lang,
        ...(config.turnstileToken ? { turnstileToken: config.turnstileToken } : {}),
      }),
      signal: req.signal,
    });
    const data = await res.json().catch(() => null);
    if (!res.ok) {
      return {
        ok: false,
        rateLimited: res.status === 429,
        error: data?.error || (res.status === 429
          ? (lang === 'zh' ? '今日免费云端体验额度已用完，请切换「自定义 API」填写自己的 Key 或 Ollama。' : 'Free cloud quota reached; switch to Custom API to use your own key or Ollama.')
          : `Worker HTTP ${res.status}`),
      };
    }
    const payload = data?.document ?? data?.content ?? data;
    const parsed = parseOutput(payload);
    if (!parsed.ok) {
      return { ok: false, error: parsed.error || 'Worker returned invalid document.' };
    }
    return {
      ...parsed,
      mode: effectiveMode,
      source: 'worker',
      model: data?.model || data?.provider || 'Cloudflare AI',
    };
  } catch (err) {
    if (err?.name === 'AbortError' && signal?.aborted) {
      return { ok: false, canceled: true, error: lang === 'zh' ? '已取消生成' : 'Generation canceled' };
    }
    return { ok: false, error: err?.message || String(err) };
  } finally {
    req.cleanup();
  }
}
