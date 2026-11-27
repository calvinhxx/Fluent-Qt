import { normalizeStudioLang, quotaMessages } from '../../../site/studio/studio-locale.mjs';
import { buildAiMessages } from '../../../site/studio/studio-ai-prompt.mjs';
import { findParent, validateDocument } from '../../../site/studio/studio-core.mjs';

function allowedOriginFor(request, env) {
  const origin = request.headers.get('Origin') || '';
  const configured = String(env.ALLOWED_ORIGINS || 'https://calvinhxx.github.io')
    .split(',')
    .map((s) => s.trim())
    .filter(Boolean);
  if (configured.includes('*')) return origin || '*';
  if (origin && configured.includes(origin)) return origin;
  return null;
}

function corsHeaders(allowedOrigin) {
  const headers = {
    'Access-Control-Allow-Methods': 'POST, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type',
    'Access-Control-Max-Age': '86400',
    Vary: 'Origin',
  };
  if (allowedOrigin) headers['Access-Control-Allow-Origin'] = allowedOrigin;
  return headers;
}

function jsonResponse(payload, status, allowedOrigin) {
  return new Response(JSON.stringify(payload), {
    status,
    headers: {
      'Content-Type': 'application/json; charset=utf-8',
      ...corsHeaders(allowedOrigin),
    },
  });
}

// One named object admits requests for the entire gateway, including across cold starts.
export class StudioQuota {
  constructor(ctx, env) {
    this.storage = ctx.storage;
    this.minute = Number(env.RATE_LIMIT_PER_MIN ?? 6);
    this.hourly = Number(env.RATE_LIMIT_PER_HOUR ?? 20);
    this.daily = Number(env.MAX_DAILY_REQUESTS ?? 800);
    if (![this.minute, this.hourly, this.daily].every(n => Number.isSafeInteger(n) && n >= 0)) {
      throw new Error('Invalid Studio quota configuration');
    }
    this.storage.sql.exec('CREATE TABLE IF NOT EXISTS requests (stamp INTEGER NOT NULL, ip TEXT NOT NULL)');
  }

  async fetch(request) {
    const ip = await request.text();
    const now = Date.now();
    const today = Math.floor(now / 86_400_000) * 86_400_000;
    const hour = now - 3_600_000;
    const reason = this.storage.transactionSync(() => {
      const sql = this.storage.sql;
      sql.exec('DELETE FROM requests WHERE stamp < ?', Math.min(today, hour));
      const [counts] = sql.exec(`
        SELECT count(*) FILTER (WHERE stamp >= ?) AS daily,
               count(*) FILTER (WHERE ip = ? AND stamp > ?) AS hourly,
               count(*) FILTER (WHERE ip = ? AND stamp > ?) AS minute
          FROM requests`, today, ip, hour, ip, now - 60_000);
      if (counts.daily >= this.daily) return 'quotaReached';
      if (counts.hourly >= this.hourly) return 'quotaHourly';
      if (counts.minute >= this.minute) return 'quotaMinute';
      sql.exec('INSERT INTO requests VALUES (?, ?)', now, ip);
      return '';
    });
    if (!reason) return new Response(null);
    return new Response(quotaMessages[reason].en, { status: 429, headers: { 'X-Studio-Error': reason } });
  }
}

async function verifyTurnstile(token, ip, env) {
  if (!env.TURNSTILE_SECRET_KEY) return true;
  if (typeof token !== 'string' || !token.trim()) return false;
  const body = new URLSearchParams({
    secret: env.TURNSTILE_SECRET_KEY,
    response: token.trim(),
    remoteip: ip,
  });
  const res = await fetch('https://challenges.cloudflare.com/turnstile/v0/siteverify', {
    method: 'POST',
    body,
  });
  if (!res.ok) return false;
  const data = await res.json().catch(() => null);
  return Boolean(data?.success);
}

async function runGroq(env, messages) {
  const model = env.GROQ_MODEL || 'llama-3.3-70b-versatile';
  const res = await fetch('https://api.groq.com/openai/v1/chat/completions', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      Authorization: `Bearer ${env.GROQ_API_KEY}`,
    },
    body: JSON.stringify({
      model,
      messages,
      temperature: 0.35,
      max_tokens: 2800,
      response_format: { type: 'json_object' },
    }),
  });
  if (!res.ok) throw new Error(`Groq HTTP ${res.status}`);
  const data = await res.json();
  const content = data?.choices?.[0]?.message?.content;
  if (!content) throw new Error('Groq returned empty output');
  return { provider: 'groq', model, content };
}

async function runOpenRouterFree(env, messages, origin) {
  const model = env.OPENROUTER_MODEL || 'meta-llama/llama-3.3-70b-instruct:free';
  const res = await fetch('https://openrouter.ai/api/v1/chat/completions', {
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      Authorization: `Bearer ${env.OPENROUTER_API_KEY}`,
      'HTTP-Referer': origin || 'https://calvinhxx.github.io/Fluent-Qt/',
      'X-Title': 'Fluent-Qt Page Studio',
    },
    body: JSON.stringify({
      model,
      messages,
      temperature: 0.35,
      max_tokens: 2800,
    }),
  });
  if (!res.ok) throw new Error(`OpenRouter HTTP ${res.status}`);
  const data = await res.json();
  const content = data?.choices?.[0]?.message?.content;
  if (!content) throw new Error('OpenRouter returned empty output');
  return { provider: 'openrouter-free', model, content };
}

async function runCloudflareAi(env, messages) {
  if (!env.AI || typeof env.AI.run !== 'function') {
    throw new Error('Workers AI binding "AI" is not configured');
  }
  const model = env.CF_AI_MODEL || '@cf/meta/llama-3.1-8b-instruct';
  const output = await env.AI.run(model, {
    messages,
    temperature: 0.35,
    max_tokens: 2600,
  });
  const content = typeof output === 'string'
    ? output
    : (output?.response ?? output?.result?.response ?? JSON.stringify(output));
  if (!content) throw new Error('Workers AI returned empty output');
  return { provider: 'cloudflare-workers-ai', model, content };
}

export default {
  async fetch(request, env) {
    const allowedOrigin = allowedOriginFor(request, env);
    if (request.method === 'OPTIONS') {
      if (!allowedOrigin) return new Response('Forbidden origin', { status: 403 });
      return new Response(null, { status: 204, headers: corsHeaders(allowedOrigin) });
    }
    if (!allowedOrigin) {
      return jsonResponse({ ok: false, error: 'Origin not allowed' }, 403, null);
    }
    if (request.method !== 'POST') {
      return jsonResponse({ ok: false, error: 'Method not allowed' }, 405, allowedOrigin);
    }

    let body;
    try {
      body = await request.json();
    } catch (_) {
      return jsonResponse({ ok: false, error: 'Invalid JSON request body' }, 400, allowedOrigin);
    }

    const ip = request.headers.get('CF-Connecting-IP') || 'unknown';
    const turnstileOk = await verifyTurnstile(body?.turnstileToken, ip, env);
    if (!turnstileOk) {
      return jsonResponse({ ok: false, error: 'Anti-bot verification failed.' }, 403, allowedOrigin);
    }

    const prompt = typeof body?.prompt === 'string' ? body.prompt.trim() : '';
    if (!prompt || prompt.length > 1200) {
      return jsonResponse({ ok: false, error: 'Prompt must be 1 to 1200 characters.' }, 400, allowedOrigin);
    }
    const mode = body.mode === 'replace' ? 'create' : (body.mode ?? 'create');
    const lang = normalizeStudioLang(body.lang) || 'zh';
    const currentDoc = body?.currentDoc && typeof body.currentDoc === 'object' ? body.currentDoc : null;
    if (currentDoc && JSON.stringify(currentDoc).length > 40_000) {
      return jsonResponse({ ok: false, error: 'currentDoc exceeds 40 KB limit' }, 413, allowedOrigin);
    }

    if (!['create', 'iterate', 'refine'].includes(mode)
        || (mode !== 'create' && !validateDocument(currentDoc).ok)
        || (mode === 'refine' && !findParent(currentDoc.root, body.targetNodeId)?.parent)) {
      return jsonResponse({ ok: false, error: 'Invalid edit mode, document or selected node.' }, 400, allowedOrigin);
    }

    const messages = buildAiMessages({ prompt, mode, currentDoc, lang,
      targetNodeId: body.targetNodeId, stylePreset: body.stylePreset });
    try {
      const quota = env.STUDIO_QUOTA.get(env.STUDIO_QUOTA.idFromName('global'));
      const result = await quota.fetch('https://quota.internal/admit', { method: 'POST', body: ip });
      if (!result.ok) {
        const errorCode = result.headers.get('X-Studio-Error');
        return jsonResponse({ ok: false, error: await result.text(), ...(errorCode && { errorCode }) },
          result.status, allowedOrigin);
      }
    } catch {
      return jsonResponse({ ok: false, error: 'Free quota service unavailable.' }, 503, allowedOrigin);
    }

    const runners = [];
    if (env.GROQ_API_KEY) runners.push(() => runGroq(env, messages));
    if (env.OPENROUTER_API_KEY) runners.push(() => runOpenRouterFree(env, messages, allowedOrigin));
    runners.push(() => runCloudflareAi(env, messages));

    for (const run of runners) {
      try {
        const result = await run();
        return jsonResponse({ ok: true, ...result }, 200, allowedOrigin);
      } catch { /* Try the next configured provider. */ }
    }
    return jsonResponse({ ok: false, error: 'Generation providers are temporarily unavailable.' }, 503, allowedOrigin);
  },
};
