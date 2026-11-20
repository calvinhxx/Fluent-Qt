# Project site workflow

> **Status:** Current guide

<!-- docs-nav:top:start -->
[Documentation](../README.md) › [Development](README.md) › Gallery and site

[← Tooltip Usage](tooltip-usage.md) · [Contents](../SUMMARY.md) · [Development index](README.md)
<!-- docs-nav:top:end -->

The English homepage at `https://calvinhxx.github.io/Fluent-Qt/` is the
`x-default` entry. The other eight homepages use `zh-CN/`, `zh-TW/`, `ja/`,
`ko/`, `de/`, `ru/`, `es/`, and `fr/` under the same project root.
The [locale registry](../../tools/site/site_locales.py) maps each language to
its URL prefix, HTML language tag and root README. The language menu links
directly to the generated HTML. Each homepage supplies its own canonical URL,
reciprocal `hreflang` links, Open Graph metadata and JSON-LD.

The shared `404.html` uses the first path segment below the site root to choose
the language and home link. For example, `/Fluent-Qt/fr/api/de/missing` returns
to the French homepage, and `/Fluent-Qt/api/ja/missing` returns to the English
homepage. The same rule applies when serving a local preview at `/`.

The legacy `/Fluent-Qt/app/` path is retained as a no-index redirect to the
canonical `/Fluent-Qt/gallery/` page. Keep `site/app/index.html` when changing
the Pages layout so links from older posts and bookmarks continue to work.

## Editing

1. Edit the shared HTML structure in
   [`tools/site/index.template.html`](../../tools/site/index.template.html).
2. Edit homepage copy and runtime messages in the
   [homepage locale files](../../tools/site/locales/site/). Use the registry
   key for each filename, including `zh.json` for Simplified Chinese and
   `zh-TW.json` for Traditional Chinese. Keep keys and named placeholders
   such as `{platform}` aligned with `en.json`. The generator embeds the
   runtime messages that `site/site.js` reads.
3. Edit English and Simplified Chinese foundation and module guides, shared
   code examples and header references in
   [`documentation_guides.py`](../../tools/site/documentation_guides.py).
   For the other seven languages, edit the matching
   [guide locale file](../../tools/site/locales/guides/) using the same guide
   IDs and section order.
4. Generate the local homepages and documents.

   ```bash
   python3 tools/site/generate_localized_site.py
   python3 tools/site/generate_api_reference.py
   ```

The homepage generator writes `site/index.html`, one `site/<prefix>/index.html`
per additional language, `site/sitemap.xml`, and the support-tier redirects.
CI regenerates these files from their sources on each website build.
Website copies of the application icon, README hero, and home JPEG are also
untracked. `tools/site/site_assets.py` maps their existing URLs to canonical
files in `app/assets/` and `docs/assets/readme/`. The local server serves those
files directly, and the artifact builder copies their exact bytes into the
published site. Do not recreate copies under `site/` for source previews.
It versions the homepage script, stylesheet and Spatial controller by file
content, so returning browsers do not combine new markup with a cached
controller. A controller load failure shows a retry action instead of leaving
the example waiting.

The homepage shows screenshots of the CheckBox property panel. To refresh them
after a panel change, serve the site with its matching Gallery WASM build and
run `python3 tools/site/capture_playground_showcase.py` (requires Playwright,
Chromium and Pillow). It selects the checked state in the live panel and saves
both languages in Light, Dark and High Contrast. Use `--url` for a different
preview server.

The machine-readable API catalog is generated from the installed-header allowlist,
installed public headers, and the generated AI catalog. `site/api/catalog.json`
remains as a snapshot for quality tools. CI refreshes it and the AI catalog
from current source before validation; changing a public header or Gallery
sample does not require manually generating or committing catalog updates.
For local inspection, use:

```bash
python3 tools/site/build_site.py --prepare
```

`generate_api_reference.py` writes the component, guide, and reference pages;
`--catalog-only` limits it to the machine-readable catalog. Pages are not
tracked. The catalog does not depend on preview images, so it stays the
same whichever machine captured them.

### Component visual documentation

All nine homepages and root READMEs link to guides, component API pages,
header references, release history and property previews in the same language.
For example, the Japanese foundations guide at `ja/api/foundation/` links to
`ja/api/checkbox/?playground=1`. The directory indexes guide descriptions in
the current language and retains translated guide names as search aliases.
A searchable side directory contains foundation guides, optional modules,
component examples and nonvisual API references. On narrow screens, the directory folds above the
document. There is no separate card index; legacy `/api/` entries redirect to a
document and preserve the old language/search parameters.
The language-neutral `/api/` entry also honors the former
`fluent-qt-api-language` preference. An explicit `?lang=` takes precedence;
each localized `/<prefix>/api/` entry defaults to the language in its URL.
The document header has Home and Documentation links only. Language follows the
homepage's localized link; theme uses the same `fluent-qt-theme` preference,
including High Contrast. Gallery, releases and global language selection stay
on the homepage. Light/Dark controls inside an example change only its live
preview and native-image fallback. They do not change the document or save the
website theme. New document navigation follows the website theme again;
forced colors takes precedence over a local preview choice.
Examples start automatically when visible. The document, code, and native-image
fallback remain readable while the runtime loads or if it fails. Only a failed
load presents a Retry action. Site Gallery links all return to the homepage's
embedded Gallery; the runtime under `/gallery/` remains an internal loading URL
and a compatibility endpoint, not a second navigation destination.

`tools/site/component_inventory.py` joins the canonical catalog with
`tools/site/component-translations.json` for component names and concise Chinese
descriptions. The inventory fails when a component is missing editorial coverage.
`tools/site/component-docs.json` retains the curated Button and CheckBox state
recipes. C++ teaching code
comes from the generated AI catalog; Python code comes from
`app/data/gallery_python_snippets.json`. Edit the owning Gallery sample before
regenerating those catalogs. Each language's import appears inside its code
panel. Documents provide code copying without source downloads or build panels.
`tools/site/public_api.py` reads declarations from installed headers: constructors,
properties, methods, signals, enums and their values, aliases, fields and constants.
The full type-member inventory is used to generate HTML, not published again in
`site/api/catalog.json`. That lightweight catalog retains component metadata and
public-header links for existing tools.
It retains overloads, excludes private implementation, groups protected subclass
interfaces separately, and follows public FluentQt base classes. Qt base APIs
link to Qt's full member reference. Python-only wrapper methods use signatures
and English docstrings from the binding source. Maintain their translations in
`tools/site/locales/api/<language>.json`, including `zh.json` for Simplified Chinese.
Do not edit generated detail pages. `tools/site/component.template.html` owns
the shared reading shell, and `component-preview.template.html` owns the live example;
`tools/site/documentation_navigation.py` owns shared navigation, and
`tools/site/api-entry.template.html` owns the lightweight legacy redirects.
The same generator extracts the existing 64-pixel PNG from the Gallery's ICO
for document navigation, avoiding the full-resolution application artwork.

### Previews and local generation

Native previews under `site/assets/component-previews/` are not tracked. CI
captures them on Linux and publishes them with the pages.

Editing prose or code samples does not need a native Gallery.
`generate_api_reference.py` reuses local captures whose bytes still match the
manifest, even after their inputs change, and uses placeholders for missing
ones. It reports how many it reused or replaced. Placeholders show the preview's
"unavailable" state in the browser.

To capture current images, use a configured native Gallery build. Pass
`--executable` to use an existing build instead of the preset:

```bash
python3 tools/site/generate_component_previews.py --preset vcpkg-osx
python3 tools/site/generate_api_reference.py
python3 tools/site/test_generate_api_reference.py
```

To build the same artifact that CI publishes, run
`python3 tools/site/build_site.py --gallery /path/to/fluent_qt_gallery`. It
refreshes the AI and API catalogs, validates their source contracts, recaptures stale previews,
generates every page, and writes a verified site to `build/site-pages`.

The capture tool uses existing Gallery factories, named-widget actions and
`--capture-target gallerySamplePreviewWidget`. It checks selected, mixed,
disabled and focus states before capturing both themes. Its manifest records
image hashes, logical/pixel dimensions, platform, Qt, fonts, binary hash and a
digest of capture dependencies and environment settings. Library and Gallery
sources, headers, resources, build files and the capture workflow participate.
Qt version, scale, platform, rendering and locale settings invalidate reuse;
temporary display numbers do not. `--check` runs without Qt and rejects stale or
missing images; publication uses the same rule. It does not certify other
operating systems or approve visual baselines. Gallery card illustrations under `tools/gallery/artwork/` are not
component preview images.

When `build_site.py --gallery` is given an executable, it also compares the
binary hash before reusing captures, so a locally rebuilt Qt kit cannot silently
reuse images from the previous executable.

Successful captures are cached by source, executable and recipe so an interrupted
batch can resume. A capture that times out or exits with an error is retried
once before the batch fails. Larger examples use their own preview height; a capture fails
if the real sample is clipped. The manifest is published after the complete batch.

Preview and verify the static pages without requiring a WASM build. Generate
the homepage and documents first; a fresh clone does not contain them:

```bash
python3 tools/site/serve.py --static-only --port 8766
python3 tools/site/test_component_docs_browser.py --url http://127.0.0.1:8766/
```

The browser check requires Playwright and Chromium. It covers bilingual routes,
native images, themes, keyboard state tabs, exact code copying, absence of download panels,
Chinese search, narrow layout, JavaScript-disabled reading, unavailable-runtime
fallback, document assets below 300 KB (excluding the runtime), and active text
contrast. The website CI module generates and validates the pages before Pages
deploys its artifact.

To check live examples, build the WASM Gallery as described in the
[WebAssembly workflow](webassembly-workflow.md), then run
`python3 tools/site/test_component_docs_interactive.py`. The local server mounts
that build under `/gallery/`. The check follows the homepage-to-document journey,
uses physical pointer and keyboard input in the real Qt controls, switches themes
and states in one runtime, and verifies automatic startup, focus preservation,
viewport suspension and failed-load recovery. Its RadioButton edge checks use
Pillow to inspect captures at integer and fractional pixel ratios.
Run `python3 tools/site/test_component_focus_browser.py` to inspect all catalog
previews at 1x, 1.25x, 1.5x and 2x in Light/Dark. It follows real Tab navigation,
including focus delegated to child controls, and checks the repaired control
surfaces with pixel assertions. `--routes` and `--scales` restrict a local rerun.
Results record which examples expose a visible focus target; non-interactive
examples retain rendering coverage. This is browser evidence, not native
screen-reader or operating-system acceptance.
Run `python3 tools/site/test_component_catalog_browser.py` after broad catalog or
host changes. It checks both languages of every component document at desktop and
phone widths, compares navigation styles, and switches preview themes while the
homepage and another document stay open. It also switches every documented sample
between Light and Dark in one real Qt runtime, including cleanup before returning to Button. It
opens a dialog immediately after startup and verifies it remains above the preview
until closed.
`GalleryComponentPreview` owns the isolated browser host and reuses the catalog's
sample factories. It does not instantiate the Gallery shell or read its settings.
The document creates one iframe when its example enters the viewport, hides the
sample outside the viewport, and removes the iframe on navigation. The mounted
runtime retains input and dialog state while the reader visits the API section;
hiding the sample does not stop the application's event loop. Host messages validate
origin, source and session. While the sample is visible, the runtime sends a
`content-size` message with the height the sample needs at the current width.
The document keeps the running example between its minimum height (taller for
dialogs, menus and windows, whose overlays open beyond the sample) and the
smaller of 720 px and 80% of the viewport; a taller sample scrolls inside Qt.
Hosts that ignore the message keep the captured preview height. Capture
environment and runtime metadata belong in the capture manifest and verification records, not in the product copy.

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

The homepage links to the CheckBox property panel from **Property preview**.
The **Documentation** link opens the searchable document directory in the
current language.

The Spatial section embeds the real C++ `spatial-view-scene` Gallery
sample through `gallery/?embed=site&showcase=spatial&render-scale=native`, not CSS
imitations or a second full Gallery shell. The browser launcher calls the same
sample factory, does not load Gallery preferences, and leaves rendering/input
to SpatialView. Theme and reduced-motion changes reach the actual Qt controls.

The QmlBridge section follows Spatial and describes the optional plugin's QML
support. Keep C++ and QML excerpts separate; link to the integration guide and
complete example for setup.

Localized headings must fit the viewport. Allow longer titles such as Spatial
to wrap on narrow screens.

The showcase starts when visible. Its controller pauses the Qt content when
offscreen or in a hidden tab; Stop unloads the iframe. Ready/error messages must
match the origin, frame, and current session. Keep explicit error and retry
states; never hide a failed runtime behind a simulated scene.
SpatialView and SpatialItem documents use the same local 3D controls as the
homepage. They enable perspective after the sample session activates, without
reading or writing Gallery settings. Reduced motion and High Contrast still
use the component's native 2D policy. Check both documents and mode changes with
`python3 tools/site/test_spatial_docs_browser.py` against the local site server.
The WASM build versions its JavaScript and binary together after linking; the
local preview server revalidates cached files on reload.

The hero retains a small desktop perspective tilt and returns to a flat image
on hover or keyboard focus. Mobile, reduced motion and High Contrast use a flat
capture. Keep screenshot pixels free of filters, reflective overlays and
narrow-screen cropping. The full-size link
opens the original 1440×900 capture in a keyboard-accessible dialog with scrolling,
Escape dismissal, and focus restoration. It follows the displayed theme and
falls back to an ordinary image link without JavaScript or dialog support.

## Voluntary support

The homepage's `#support` section presents WeChat Pay, Alipay, and Ko-fi as peer
payment channels. Simplified Chinese defaults to WeChat Pay. All other languages
default to Ko-fi unless a `?fuel=` tier parameter selects a CNY tier.
WeChat Pay and Alipay offer ¥19, ¥49, ¥99, and an any-amount option; each preset
selects its matching supplied fixed-amount code, and the any-amount option uses
the generic code. Selecting Ko-fi displays `site/assets/support/kofi.png` in the
same QR card frame and replaces the CNY tier controls with USD actions that open
the maintainer's official Ko-fi Tip Panel in a modal `<dialog>` or open the
creator page directly. Closing and reopening the dialog preserves the mounted
checkout and the supporter's selection; modifier clicks and no-script or
blocked-embed states fall back to the direct Ko-fi link. Download QR replacements
from the account's [QR Code page](https://ko-fi.com/QRCode).
The website does not create orders or confirm payment. WeChat Pay and Alipay
status remains in the visitor's WeChat or Alipay bill, and community support
retains the boundaries in [SUPPORT.md](../../SUPPORT.md).

Keep the full generic images at `site/assets/support/wechat.jpg`,
`site/assets/support/alipay.jpg`, and `site/assets/support/kofi.png`. Fixed-amount originals use the same directory
and the names `wechat-19.jpg`, `wechat-49.jpg`, `wechat-99.jpg`, `alipay-19.jpg`,
`alipay-49.jpg`, and `alipay-99.jpg`. CSS crops each image to the QR region inside
a white quiet zone without exposing payee name labels or raw uncropped poster links.
Lossless JPEG optimization must preserve the original pixels, metadata, and decoded QR content.
While a new image loads, hide the previous code and show a loading message; an
image failure must never fall back to a different amount.

When replacing an image, check its dimensions and crop coordinates in
`site/styles.css`. WeChat's fixed-amount images use a different crop from its
generic image. Asset URLs resolve beside the generic image so locale and
deployment subpaths work. Without JavaScript, the three generic cards and the
direct Ko-fi link remain available.

Before the deferred controller runs, the early page script masks the QR pixels
and shows the existing loading message. `DOMContentLoaded` releases that mask
after the controller has selected its code, or restores the generic-code fallback
if the main script cannot load or execute. With scripting disabled, the mask is
never applied. Missing controls or default selections leave the static codes
available; a support initialization exception must not stop Gallery, theme, or
other page initialization.

For local verification, open `/zh-CN/#support` and `/#support`. Check all eight
CNY amount/method combinations, the Ko-fi channel and modal checkout, arrow-key
selection, visible focus, narrow layouts, and Light, Dark, and High Contrast.
Check rapid selection and image-error recovery. Decode the rendered codes and
compare them with the supplied originals. Also check delayed and failed
main-script loading and the no-script fallback. Run the focused controller checks:

```bash
node --test tools/site/test_support.mjs
```

The website CI module runs this test before uploading the generated site.
The workflow boundary check requires it to remain unconditional.

Browser checks and QR decoding do not verify the payment app's displayed amount,
continued code validity, repeat payments, or settlement. Those require a separate
manual check in the payment app.

The repository's [funding configuration](../../.github/FUNDING.yml) lists four
custom URLs, one per tier. GitHub's Sponsor dialog shows custom links as bare
URLs without labels or styling, so each one is a short path that names its tier:
`/fuel/iced-coffee/`, `/fuel/context-window/`, `/fuel/max-reasoning/`, and
`/fuel/freestyle/`. `generate_localized_site.py` writes these redirect pages from
`SUPPORT_TIERS`. Each opens `?fuel=<tier>#support` in the supported language
matching the browser preference. An unsupported language or a browser with
JavaScript disabled goes to the English homepage. The support controller selects the named CNY tier on WeChat Pay and greets the
visitor until they change the selection; unknown values keep each locale's
default channel and copy. GitHub accepts at most four custom links.
The separate `ko_fi` field identifies the same account as the tip panel and
direct link. When changing the recipient, check the panel, link and QR code. The
localized README links lead to their respective homepage support sections.

For publication, verify the codes in both payment apps, publish the website
through the existing `main`-branch Pages workflow, and confirm the public
`#support` section and images load. The funding configuration must be on the
repository's default branch, with Settings → General → Features → Sponsorships
enabled. Follow the public Sponsor button through to the page after deployment.
Local previews and a file on a maintenance branch do not enable that public entry.

## Gallery screenshots

Use the current **Windows native Gallery with 3D enabled and Mica** for README and first-party
website product images. Build `fluent_qt_gallery` using the
[build workflow](build-workflow.md). Capture the real composed window in Light
and Dark at 1440×900, with the Windows caption buttons visible. Keep third-party
case-study images in their original platform and do not add simulated window
chrome or a painted Mica background.

In Gallery Settings, enable **3D Gallery**, select Mica as the window effect,
and choose the required theme. Verify that 3D is active, not a fallback; the
Spatial navigation category alone does not establish this. Keep the window active
and unmaximized, wait for the splash and navigation
animations to finish, and move the pointer outside the captured window. Use
an OS-composited, lossless PNG capture, not a compressed automation preview or
`QWidget::grab()`: Mica is supplied by the desktop compositor. For example,
copy the active window with Alt+Print Screen and save it as PNG in Paint without
resizing. Check the saved file for cursor indicators and transient overlays. The
`PythonParityVisualCheck` and `FoundationVisualCheck` fixtures deliberately use
`BackdropEffect::Solid` for deterministic test snapshots; they are not the source
for these marketing images. Restore any capture-only changes to local Gallery
preferences afterward.

Capture Home in both themes for the homepage. Save the frames as
`site/assets/gallery/gallery-home-real.png` and `gallery-home-dark.png`.
Keep `docs/assets/readme/gallery-home.jpg` in sync with the Light capture.
Publication copies it to `site/assets/gallery-home.jpg`.

The current set was recaptured on 2026-09-29 from a local Release build of Gallery
1.9.1 (rendering sources now recorded in `143bdbfa`) with Qt 6.9.3, at
1440×900 with 3D and Mica enabled. The retired component showcase images
remain available in the [v1.11.4 site assets](https://github.com/calvinhxx/Fluent-Qt/tree/v1.11.4/site/assets).
Home retains its live particle background. The PNGs preserve the native capture
pixels; do not resize or stretch source material. A 4K display is not required.
Use Home for the hero, README, loading poster, and sharing card, not a Spatial
component demonstration. Compare old and new material at the same display size;
image dimensions alone do not establish text sharpness.

The README, Gallery loading poster, and sharing card use designed compositions.
Keep the README's frame, spacing, and shadow; keep the sharing card's brand,
background, and angled window. Update their Gallery content through
[`promo-assets.html`](../../tools/site/promo-assets.html), then render:

```bash
python3 -m pip install "playwright==1.58.0" Pillow
python3 -m playwright install chromium
python3 tools/site/generate_promo_assets.py
```

The renderer uses the Light home capture and the generated FluentQt text fonts
from `res/`; it does not need the upstream font input cache. It downsamples
the README's embedded capture once with high-quality filtering at its final
1232×770 size, leaving the source PNG untouched. This does not remove softness
already present in the native 3D rendering. It produces
the 1600×900 README image (copied to the matching loading poster during
publication), plus a separate
1731×909 `site/assets/og.png`. Use `--output-root /tmp/fluentqt-promo-review`
to preview them before replacement. Do not copy a raw screenshot over these
outputs. Keep HTML dimensions and OG metadata aligned, and review the exported
artwork as well as its placement on the site. Run `generate_localized_site.py`
after rendering: it versions Gallery and sharing image URLs from their content,
so browsers and sharing previews fetch the updated assets.

## Validation

Check the tracked catalog, the generated pages and the scripts:

```bash
python3 tools/site/generate_api_reference.py --check
python3 tools/site/generate_localized_site.py --check
python3 tools/site/test_generate_localized_site.py
python3 .github/scripts/validate-ci-workflow-boundaries.py
node --check site/site.js
node --test tools/site/test_hero_particles.mjs
node --test tools/site/test_spatial_showcase.mjs
```

`generate_api_reference.py --check` verifies the tracked API catalog without Qt
or previews. The homepage generator fails on mismatched translation keys,
static localized text and attributes, invalid JSON-LD or sitemap XML, canonical
URLs, reciprocal `hreflang` links, the URL-owned 404 language contract, the
legacy Gallery redirect, and the current CMake project version in structured
data.
The pull-request planning job regenerates and checks the AI and API catalogs. The
[website CI module](ci-workflow.md) generates the homepage, documents and
previews on every pull request and on `main`, then uploads a verified artifact.
Packaging reuses the file list from the document generator to exclude retired routes.
Pages deploys only that artifact after checking its commit and file hashes, and
verifies all nine homepage paths and `sitemap.xml` during assembly. After
building the website locally, verify its file hashes and commit receipt with
the Pages command.

```bash
python3 tools/site/build_site.py --verify build/site-pages --commit "$(git rev-parse HEAD)"
```

The motion tests use Node's built-in test runner (Node 20+) and cover visibility,
pause persistence, system preferences, pointer input, resize limits, and page
cleanup. Inspect the localized homepages in Light, Dark, and High Contrast,
check the mobile layout, and operate the pause control
with the keyboard.
Also start the WASM Spatial sample, adjust its real camera Slider and switch
2D/3D. Check offscreen suspension, theme changes, Stop/restart and error handling.
The WASM browser smoke verifies the shared sample and writes Light/Dark captures
under `build/wasm/showcase-evidence/`. Open the full-size Home capture in both themes, check its natural dimensions,
and verify Escape/Close restores focus to the opener. At narrow widths, the page
must not scroll horizontally; only the full-size image viewer may do so.

For a local preview with the built runtime mounted at the same URL as Pages:

```bash
python3 tools/site/serve.py --port 8765
```

This serves the generated website from `site/` and `/gallery/` from `build/wasm/app/`, without
copying compiled binaries into tracked assets. Build the `wasm` Gallery first;
a site-only `http.server --directory site` does not contain the runtime.

After deployment, verify the homepage URLs in the locale registry and submit `sitemap.xml` to the
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
property to inspect the English home, Chinese home, and component document URLs.
Check the fetched page, indexing status, and Google's selected canonical;
submit `https://calvinhxx.github.io/Fluent-Qt/sitemap.xml` in the Sitemaps report.
Record the actual result rather than treating a generated sitemap as proof of
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
