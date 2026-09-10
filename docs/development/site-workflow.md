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

The ribbons use three depth layers, fading trails, and local pointer deflection.
Primary clicks on the background add a short pulse; links, controls, and touch
scrolling do not trigger it. Keep at most three pulses alive, cap the canvas at
2.5 million pixels, and use fewer particles with a 30 fps draw limit below 700 px.
Desktop drawing is capped at 60 fps. Avoid pairwise particle links or per-dot
blur filters: their cost grows quickly with density.

## Gallery screenshots

Use the current native Gallery for product images. Build `test_gallery_content_pages`
using the [build workflow](build-workflow.md), then capture each route in Light
and Dark. On macOS, for example:

```bash
env -u SKIP_VISUAL_TEST QT_QPA_PLATFORM=cocoa VISUAL_SNAPSHOT=1 \
  QT_SCALE_FACTOR=1 QT_FONT_DPI=96 GALLERY_PARITY_ROUTE=home GALLERY_PARITY_THEME=dark \
  ./build/vcpkg-osx/tests/gallery/test_gallery_content_pages \
  --gtest_filter=GalleryContentPagesTest.PythonParityVisualCheck
```

Use `home`, `button`, and `collections` for the website images. The shared helper
writes 1440×900 PNGs to `build/vcpkg-osx/visual/`. Review the captures before
replacing the matching Light/Dark assets in `site/assets/gallery/`.

The README, Gallery loading poster, and sharing card use designed compositions.
Keep the README's frame, spacing, and shadow; keep the sharing card's brand,
background, and angled window. Update their Gallery content through
[`promo-assets.html`](../../tools/site/promo-assets.html), then render:

```bash
python3 -m pip install "playwright==1.58.0"
python3 -m playwright install chromium
python3 tools/site/generate_promo_assets.py
```

The renderer uses the Light home capture and bundled Inter fonts. It produces
the 1600×900 README image and matching loading poster, plus a separate
1731×909 `site/assets/og.png`. Use `--output-root /tmp/fluentqt-promo-review`
to preview them before replacement. Do not copy a raw screenshot over these
outputs. Keep HTML dimensions and OG metadata aligned, and review the exported
artwork as well as its placement on the site. Run `generate_localized_site.py`
after rendering: it versions Gallery and sharing image URLs from their content,
so browsers and sharing previews fetch the updated assets.

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

## Search discovery

Use `Fluent-Qt` as the display name and `FluentQt` as its searchable alias.
Keep both spellings in the English and Chinese page metadata and README
introductions; the generated project JSON-LD declares `FluentQt` as
`alternateName`. Keep repository URLs and package identifiers unchanged.

GitHub repository search normally searches the name, description, and topics,
not the README. Keep `FluentQt (Fluent-Qt)` in the repository About description
and `fluentqt` in Topics. When the topic limit is reached, replace a redundant
generic topic instead of removing language or framework identifiers.
After updating About or Topics, compare these repository searches:

- `fluentqt user:calvinhxx`
- `fluent-qt user:calvinhxx`
- `fluentqt in:readme user:calvinhxx`

All three should include `calvinhxx/Fluent-Qt` once the search index refreshes.
See [GitHub repository search](https://docs.github.com/en/search-github/searching-on-github/searching-for-repositories).

After a Pages deployment, use the site's verified Google Search Console
property to inspect the English home, Chinese home, and API Explorer URLs.
Check the fetched page, indexing status, and Google's selected canonical;
submit `https://calvinhxx.github.io/Fluent-Qt/sitemap.xml` in the Sitemaps report.
Record the actual result rather than treating a checked-in sitemap as proof of
submission or indexing. Ownership verification requires the site's account;
do not invent verification tokens or assume that missing HTML verification
means the property is unverified.

Use the Performance report to distinguish brand queries (`FluentQt`,
`Fluent-Qt`) from discovery queries such as `Qt Widgets UI library` and
`PySide6 控件库`. Compare impressions and clicks over the same date range,
then review Gallery and installation-link activity in the configured site
analytics. Link clicks indicate interest, not completed installation or adoption.
See [Google's sitemap submission guide](https://developers.google.com/search/docs/crawling-indexing/sitemaps/build-sitemap);
submission does not guarantee indexing or ranking.

<!-- docs-nav:bottom:start -->
---
[← Tooltip Usage](tooltip-usage.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:bottom:end -->
