# Free Cloudflare Worker for Fluent-Qt Studio AI

> **Status:** Optional website deployment helper

Page Studio can send generation requests to this optional Worker. The default site has an empty gateway URL. The gateway tries the configured Groq and OpenRouter providers, then the Workers AI binding. Provider billing depends on your accounts and plans.

A single SQLite-backed Durable Object counts admitted requests across Worker instances. The defaults are six requests per IP per rolling minute, twenty per rolling hour and 800 across the gateway per UTC day. An admitted request counts even if an upstream provider fails. The gateway returns 503 if quota storage is unavailable. Wrangler creates the binding and applies its first migration during deployment.

## Deploy

```bash
cd tools/site/studio-ai-worker
npx wrangler login
npx wrangler deploy
```

Optional free-tier secrets (use unbilled accounts only):

```bash
npx wrangler secret put GROQ_API_KEY
npx wrangler secret put OPENROUTER_API_KEY
```

The built-in Studio client uses the gateway's request limits. Set `TURNSTILE_SECRET_KEY` only if a custom client also supplies a fresh `turnstileToken` with every request.

To allow local preview testing alongside production Pages, set
`ALLOWED_ORIGINS` in [wrangler.toml](wrangler.toml) to a comma-separated list:

```toml
ALLOWED_ORIGINS = "https://calvinhxx.github.io,http://127.0.0.1:8765"
```

## Connect to Page Studio

Paste the deployed `https://...` URL into `<meta name="fluent-studio-ai-endpoint" content="...">`
inside [site/studio/index.html](../../../site/studio/index.html), or enter it in
the **AI Config** panel during testing.

## Use OpenAI directly from Studio

Select **AI Config → Custom API → OpenAI** to send requests directly to
OpenAI. The preset fills in `https://api.openai.com/v1` and `gpt-4o-mini`.
To try GPT-6.2, enter `gpt-6.2-sol` or `gpt-6.2-astra` in **Model** and
supply your own OpenAI API key. This path works independently of the Worker.

With that base URL and either GPT-6.2 model, Studio uses `/v1/responses`
with `reasoning.effort` set to `medium` and omits `temperature`. The first
input item is a `developer` message containing the shared Studio rules,
catalog, and examples. The current design request follows in a `user`
message so the shared prefix is eligible for caching. See
[OpenAI's model guide](https://developers.openai.com/api/docs/guides/latest-model).
