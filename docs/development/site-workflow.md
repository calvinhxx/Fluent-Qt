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

## Validation

Run the same freshness check used by the Pages workflow:

```bash
python3 tools/site/generate_localized_site.py --check
```

The check requires matching translation keys, static localized text and
attributes, valid JSON-LD and sitemap XML, canonical URLs, reciprocal
`hreflang` links, the URL-owned 404 language contract, the legacy Gallery
redirect, and the current CMake project version in structured data.
The Pages workflow also verifies that both localized pages and `sitemap.xml`
are present before deployment.

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
