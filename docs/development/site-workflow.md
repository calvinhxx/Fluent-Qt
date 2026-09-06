# Project site workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Gallery and site

[← Tooltip Usage](tooltip-usage.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:top:end -->

The GitHub Pages site uses static language-specific HTML so crawlers and users
receive one stable language at each URL:

- `https://calvinhxx.github.io/Fluent-Qt/` — English and `x-default`
- `https://calvinhxx.github.io/Fluent-Qt/zh-CN/` — Simplified Chinese

Do not restore browser-language redirects or change the page language in
JavaScript. Language switching uses ordinary links, and each page owns its
canonical URL, reciprocal `hreflang` annotations, localized Open Graph data,
and localized JSON-LD.

The shared `404.html` follows the requested URL: missing paths below `zh-CN/`
render Chinese, while all other missing paths render English. It must not read
browser-language state, and its assets and home links resolve from the
`/Fluent-Qt/` project root even for deeply nested missing URLs.

The legacy `/Fluent-Qt/app/` path is retained as a no-index redirect to the
canonical `/Fluent-Qt/gallery/` page. Keep `site/app/index.html` when changing
the Pages layout so links from older posts and bookmarks continue to work.

## Editing

1. Edit the shared HTML structure in
   [`tools/site/index.template.html`](../../tools/site/index.template.html).
2. Edit English and Chinese strings together in the `translations` object in
   [`site/site.js`](../../site/site.js).
3. Regenerate committed outputs:

   ```bash
   python3 tools/site/generate_localized_site.py
   ```

The generator owns `site/index.html`, `site/zh-CN/index.html`, and
`site/sitemap.xml`. Do not edit those files directly.

The API Explorer catalog is generated from the installed-header allowlist,
installed public headers, and the generated AI catalog. After any of those
inputs change, regenerate it instead of editing the JSON directly:

```bash
python3 tools/site/generate_api_reference.py
```

The hero's optional Canvas effect lives in
[`site/hero-particles.js`](../../site/hero-particles.js). Keep the animation
confined to the hero and its colors in `site/styles.css`. It pauses outside the
viewport, in hidden tabs, and when the user pauses it. Reduced motion retains a
static frame; High Contrast and forced colors remove the decoration. The page
must remain usable if the module or Canvas is unavailable.

## Validation

Check the generated pages and scripts:

```bash
python3 tools/site/generate_localized_site.py --check
python3 tools/site/generate_api_reference.py --check
node --check site/site.js
node --test tools/site/test_hero_particles.mjs
```

The check requires matching translation keys, static localized text and
attributes, valid JSON-LD and sitemap XML, canonical URLs, reciprocal
`hreflang` links, the URL-owned 404 language contract, the legacy Gallery
redirect, and the current CMake project version in structured data.
The pull-request planning job runs both freshness checks before merge. The
Pages workflow repeats them and also verifies that both localized pages and
`sitemap.xml` are present before deployment.

The motion tests use Node's built-in test runner (Node 20+) and cover visibility,
pause persistence, system preferences, pointer input, resize limits, and page
cleanup. They do not replace browser review: inspect both languages in Light,
Dark, and High Contrast, check the mobile layout, and operate the pause control
with the keyboard.

After deployment, verify both language URLs and submit `sitemap.xml` to the
configured search-engine webmaster tools. Search Console ownership and sitemap
submission are external operations and are not performed by repository CI.

GitHub project Pages are served below `/Fluent-Qt/`, but the robots exclusion
protocol reads only the origin-root `https://calvinhxx.github.io/robots.txt`.
This repository therefore does not publish a misleading project-path
`robots.txt`; manage the origin-root file in the owning user-site repository.

<!-- docs-nav:bottom:start -->
---
[← Tooltip Usage](tooltip-usage.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
