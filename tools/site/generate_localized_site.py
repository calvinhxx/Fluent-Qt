#!/usr/bin/env python3
"""Generate and validate static localized website pages."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Any

from docs_i18n import EXTRA_LOCALE_KEYS
from site_assets import source_path
from site_locales import LOCALES, Locale, relative_locale_url
from studio_i18n import studio_runtime_payload, validate_studio_locales


ROOT = Path(__file__).resolve().parents[2]
SITE_ROOT = ROOT / "site"
LOCALES_DIR = Path(__file__).with_name("locales") / "site"
TEMPLATE_PATH = Path(__file__).with_name("index.template.html")
ERROR_PAGE_PATH = SITE_ROOT / "404.html"
LEGACY_GALLERY_REDIRECT_PATH = SITE_ROOT / "app" / "index.html"
BASE_URL = "https://calvinhxx.github.io/Fluent-Qt/"
REPOSITORY_URL = "https://github.com/calvinhxx/Fluent-Qt"
# GitHub's Sponsor dialog shows custom funding links as bare URLs, so each tier
# gets a short path; site.js selects the tier named by the `fuel` parameter.
SUPPORT_TIERS = ("iced-coffee", "context-window", "max-reasoning", "freestyle")

RUNTIME_I18N_KEYS: tuple[str, ...] = (
    "a11y.systemHighContrastTheme",
    "a11y.useDarkTheme",
    "a11y.useHighContrastTheme",
    "a11y.useLightTheme",
    "a11y.openMenu",
    "a11y.closeMenu",
    "gallery.waitingTitle",
    "gallery.waitingDetail",
    "gallery.loadingTitle",
    "gallery.loadingDetail",
    "gallery.slowTitle",
    "gallery.slowDetail",
    "gallery.readyTitle",
    "gallery.readyDetail",
    "gallery.errorTitle",
    "gallery.errorDetail",
    "gallery.retryAction",
    "copy.copy",
    "copy.copied",
    "copy.announcement",
    "copy.failed",
    "support.kofiMobileHelp",
    "support.mobileHelp",
    "support.kofiInstruction",
    "support.linkedAnyInstruction",
    "support.linkedInstruction",
    "support.anyInstruction",
    "support.instruction",
    "support.any",
    "support.tier19",
    "support.tier49",
    "support.tier99",
    "downloads.macChoice",
    "downloads.openRelease",
    "downloads.downloadFor",
    "spatial.runTitle",
    "spatial.runCopy",
    "spatial.stopped",
    "spatial.reload",
)


def versioned_asset_url(path: str) -> str:
    digest = hashlib.sha256(source_path(path, SITE_ROOT).read_bytes()).hexdigest()[:12]
    return f"{path}?v={digest}"


OG_IMAGE_URL = f"{BASE_URL}{versioned_asset_url('assets/og.png')}"

TEXT_PATTERN = re.compile(
    r'(?P<open><(?P<tag>[A-Za-z][A-Za-z0-9]*)[^>]*data-i18n="(?P<key>[^"]+)"[^>]*>)'
    r'(?P<content>.*?)'
    r'(?P<close></(?P=tag)>)',
    re.DOTALL,
)
START_TAG_PATTERN = re.compile(r"<(?!/|!)[^>]+>", re.DOTALL)
PLACEHOLDER_PATTERN = re.compile(r"\{[a-z]+\}")


def fail(message: str) -> None:
    print(f"error: {message}", file=sys.stderr)
    raise SystemExit(1)


def project_version() -> str:
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(
        r"project\s*\(\s*FluentQt\s+VERSION\s+([0-9]+\.[0-9]+\.[0-9]+)",
        cmake,
    )
    if not match:
        fail("could not read the FluentQt version from CMakeLists.txt")
    return match.group(1)


def documentation_entry(language: str) -> str:
    from documentation_guides import guide_path
    return guide_path("foundation", language).removesuffix("index.html")


def translations() -> dict[str, dict[str, str]]:
    parsed: dict[str, dict[str, str]] = {}
    for locale in LOCALES:
        path = LOCALES_DIR / f"{locale.key}.json"
        if not path.is_file():
            fail(f"missing site translation file: {path}")
        try:
            values = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            fail(f"{path} is not valid JSON: {error}")
        if not isinstance(values, dict) or not all(
            isinstance(key, str) and isinstance(value, str) for key, value in values.items()
        ):
            fail(f"{path} must map string keys to string values")
        parsed[locale.key] = values

    baseline = parsed["en"]
    baseline_keys = set(baseline)
    for key in RUNTIME_I18N_KEYS:
        if key not in baseline_keys:
            fail(f"missing runtime i18n key in en.json: {key}")

    for locale in LOCALES:
        values = parsed[locale.key]
        if set(values) != baseline_keys:
            missing = sorted(baseline_keys - set(values))
            extra = sorted(set(values) - baseline_keys)
            fail(f"translation keys differ for {locale.key}; missing={missing}, extra={extra}")
        for key, en_value in baseline.items():
            value = values[key]
            if en_value and not value.strip():
                fail(f"empty {locale.key} translation for {key}")
            if not en_value and value:
                fail(f"{locale.key} translation for {key} must stay empty")
            if set(PLACEHOLDER_PATTERN.findall(value)) != set(PLACEHOLDER_PATTERN.findall(en_value)):
                fail(f"placeholder mismatch in {locale.key} for {key}")
    return parsed


def replace_attribute(tag: str, name: str, value: str) -> str:
    escaped = html.escape(value, quote=True)
    pattern = re.compile(rf'(\s{re.escape(name)}=")[^"]*(")')
    if pattern.search(tag):
        return pattern.sub(lambda match: f"{match.group(1)}{escaped}{match.group(2)}", tag, count=1)
    insertion = " />" if tag.endswith("/>") else ">"
    return tag[: -len(insertion)] + f' {name}="{escaped}"' + insertion


def localize_attributes(page: str, values: dict[str, str], locale: Locale) -> str:
    key_attributes = {
        "data-i18n-aria-label": "aria-label",
        "data-i18n-alt": "alt",
        "data-i18n-title": "title",
    }

    def transform(tag_match: re.Match[str]) -> str:
        tag = tag_match.group(0)
        for key_attribute, target_attribute in key_attributes.items():
            key_match = re.search(rf'\b{re.escape(key_attribute)}="([^"]+)"', tag)
            if not key_match:
                continue
            key = key_match.group(1)
            if key not in values:
                fail(f"missing {locale.key} translation for {key}")
            tag = replace_attribute(tag, target_attribute, values[key])

        if "data-readme-link" in tag:
            tag = replace_attribute(tag, "href", locale.readme_url(REPOSITORY_URL))
        if 'name="support-method"' in tag:
            value_match = re.search(r'\bvalue="([^"]+)"', tag)
            tag = re.sub(r"\schecked\b", "", tag)
            if value_match and value_match.group(1) == locale.default_support_method:
                insertion = " />" if tag.endswith("/>") else ">"
                tag = tag[: -len(insertion)] + " checked" + insertion
        return tag

    return START_TAG_PATTERN.sub(transform, page)


def localize_text(page: str, values: dict[str, str], locale: Locale) -> str:
    seen: set[str] = set()

    def transform(match: re.Match[str]) -> str:
        key = match.group("key")
        if key not in values:
            fail(f"missing {locale.key} translation for {key}")
        if "<" in match.group("content"):
            fail(f"data-i18n element for {key} must contain text only")
        seen.add(key)
        return f'{match.group("open")}{html.escape(values[key])}{match.group("close")}'

    localized = TEXT_PATTERN.sub(transform, page)
    template_keys = set(re.findall(r'data-i18n="([^"]+)"', page))
    if seen != template_keys:
        fail(f"not every data-i18n element was localized for {locale.key}")
    return localized


def prefix_resources(page: str, prefix: str) -> str:
    attributes = (
        "src",
        "href",
        "data-theme-src-light",
        "data-theme-src-dark",
        "data-theme-src-contrast",
        "data-gallery-src",
        "data-showcase-src",
        "data-showcase-controller",
    )
    attribute_names = "|".join(re.escape(name) for name in attributes)
    pattern = re.compile(
        rf'(?P<head>\b(?:{attribute_names})=")'
        r'(?P<url>(?:assets/|styles\.css|site\.js|spatial-showcase\.js|gallery/|api/|studio/)[^"]*)'
    )

    def replace(match: re.Match[str]) -> str:
        url = match.group("url")
        path = url.split("?", 1)[0]
        if path in {"styles.css", "site.js", "spatial-showcase.js"}:
            url = versioned_asset_url(path)
        elif url.startswith("assets/playground/") or (
            url.endswith(".png") and (url.startswith("assets/gallery/") or url == "assets/app-icon.png")
        ):
            url = versioned_asset_url(url)
        return f'{match.group("head")}{prefix}{url}'

    return pattern.sub(replace, page)


def alternate_links_markup() -> str:
    lines = [
        f'    <link rel="alternate" hreflang="{target.html_lang}" href="{target.canonical_url(BASE_URL)}">'
        for target in LOCALES
    ]
    lines.append(f'    <link rel="alternate" hreflang="x-default" href="{BASE_URL}">')
    return "\n".join(lines)


def og_alternate_locales_markup(locale: Locale) -> str:
    return "\n".join(
        f'    <meta property="og:locale:alternate" content="{other.og_locale}">'
        for other in LOCALES
        if other.key != locale.key
    )


def render_language_switch(locale: Locale, values: dict[str, str]) -> str:
    label = html.escape(values["a11y.language"], quote=True)
    items = []
    for target in LOCALES:
        href = html.escape(relative_locale_url(locale, target), quote=True)
        current = "page" if target.key == locale.key else "false"
        event = f'language_{target.key.lower().replace("-", "_")}'
        items.append(
            f'                <a href="{href}" lang="{target.html_lang}" hreflang="{target.html_lang}" '
            f'aria-current="{current}" data-analytics-event="{event}">'
            f'<span>{html.escape(target.native_name)}</span>'
            f'<small>{html.escape(target.short_label)}</small></a>'
        )
    menu = "\n".join(items)
    return (
        '            <details class="language-switch">\n'
        f'              <summary aria-label="{label}" data-i18n-aria-label="a11y.language">\n'
        '                <svg class="language-globe" viewBox="0 0 24 24" aria-hidden="true">'
        '<circle cx="12" cy="12" r="9"/>'
        '<path d="M3.6 9h16.8M3.6 15h16.8M12 3a13.5 13.5 0 0 1 0 18M12 3a13.5 13.5 0 0 0 0 18"/>'
        "</svg>\n"
        f'                <span class="language-code">{html.escape(locale.short_label)}</span>\n'
        '                <svg class="language-chevron" viewBox="0 0 16 16" aria-hidden="true">'
        '<path d="M4.5 6.25 8 9.75l3.5-3.5"/></svg>\n'
        "              </summary>\n"
        '              <div class="language-menu" role="list">\n'
        f"{menu}\n"
        "              </div>\n"
        "            </details>"
    )


def runtime_i18n_json(values: dict[str, str]) -> str:
    subset = {key: values[key] for key in RUNTIME_I18N_KEYS}
    return json.dumps(subset, ensure_ascii=False).replace("</", "<\\/")


def structured_data(locale: Locale, values: dict[str, str], version: str) -> str:
    graph: dict[str, Any] = {
        "@context": "https://schema.org",
        "@graph": [
            {
                "@type": "SoftwareSourceCode",
                "@id": f"{BASE_URL}#project",
                "name": "Fluent-Qt",
                "alternateName": "FluentQt",
                "url": locale.canonical_url(BASE_URL),
                "description": values["meta.description"],
                "codeRepository": REPOSITORY_URL,
                "programmingLanguage": ["C++", "Python"],
                "runtimePlatform": ["Qt 5.15+", "Qt 6.2+"],
                "version": version,
                "license": f"{REPOSITORY_URL}/blob/main/LICENSE",
                "image": OG_IMAGE_URL,
                "inLanguage": locale.html_lang,
                "sameAs": [
                    REPOSITORY_URL,
                    "https://pypi.org/project/FluentQt/",
                ],
            },
            {
                "@type": "WebApplication",
                "@id": f"{BASE_URL}gallery/#application",
                "name": "Fluent-Qt C++ Web Gallery",
                "url": f"{BASE_URL}gallery/",
                "description": locale.gallery_schema_description,
                "applicationCategory": "DeveloperApplication",
                "operatingSystem": "Web browser",
                "isAccessibleForFree": True,
                "inLanguage": ["en", "zh-CN"],
            },
        ],
    }
    encoded = json.dumps(graph, ensure_ascii=False, indent=2).replace("</", "<\\/")
    return "\n".join(f"      {line}" for line in encoded.splitlines())


def render_page(
    template: str,
    locale: Locale,
    values: dict[str, str],
    version: str,
) -> str:
    canonical = locale.canonical_url(BASE_URL)
    token_values = {
        "HTML_LANG": locale.html_lang,
        "SITE_LANGUAGE": locale.key,
        "PLAYGROUND_ART_LANG": locale.playground_art_lang,
        "META_TITLE": values["meta.title"],
        "META_DESCRIPTION": values["meta.description"],
        "CANONICAL_URL": canonical,
        "ALTERNATE_LINKS": alternate_links_markup(),
        "OG_LOCALE": locale.og_locale,
        "OG_ALTERNATE_LOCALES": og_alternate_locales_markup(locale),
        "OG_IMAGE_URL": OG_IMAGE_URL,
        "OG_IMAGE_ALT": locale.og_image_alt,
        "STRUCTURED_DATA": structured_data(locale, values, version),
        "LANGUAGE_SWITCH": render_language_switch(locale, values),
        "RUNTIME_I18N": runtime_i18n_json(values),
    }
    raw_tokens = {
        "STRUCTURED_DATA",
        "ALTERNATE_LINKS",
        "OG_ALTERNATE_LOCALES",
        "LANGUAGE_SWITCH",
        "RUNTIME_I18N",
    }
    page = template
    for token, value in token_values.items():
        replacement = value if token in raw_tokens else html.escape(value, quote=True)
        page = page.replace(f"{{{{{token}}}}}", replacement)

    page = localize_text(page, values, locale)
    page = localize_attributes(page, values, locale)
    page = prefix_resources(page, locale.resource_prefix)
    # Resolve locale-owned routes after prefixing shared root assets.
    page = page.replace("{{API_URL}}", locale.api_url)
    page = page.replace("{{RELEASES_URL}}", locale.releases_url)
    page = page.replace("{{PLAYGROUND_URL}}", locale.playground_url)
    page = page.replace("{{STUDIO_URL}}", locale.studio_url)
    if re.search(r"{{[A-Z0-9_]+}}", page):
        fail(f"unresolved template token in {locale.output_relative}")
    return page.rstrip() + "\n"


def render_sitemap() -> str:
    from component_inventory import config
    from documentation_guides import guides, guide_path
    from api_sections import reference_headers, reference_path
    from generate_api_reference import generate_reference

    entries = []
    home_alternates = "\n".join(
        f'    <xhtml:link rel="alternate" hreflang="{target.html_lang}" href="{target.canonical_url(BASE_URL)}" />'
        for target in LOCALES
    ) + f'\n    <xhtml:link rel="alternate" hreflang="x-default" href="{BASE_URL}" />'
    for loc in LOCALES:
        entries.append(f'''  <url>
    <loc>{loc.canonical_url(BASE_URL)}</loc>
{home_alternates}
  </url>''')

    for item in guides():
        guide_urls = {
            loc.key: BASE_URL + guide_path(item["id"], loc.key).removesuffix("index.html")
            for loc in LOCALES
        }
        guide_alternates = "\n".join(
            f'    <xhtml:link rel="alternate" hreflang="{loc.html_lang}" href="{guide_urls[loc.key]}" />'
            for loc in LOCALES
        ) + f'\n    <xhtml:link rel="alternate" hreflang="x-default" href="{guide_urls["en"]}" />'
        for loc in LOCALES:
            entries.append(f'''  <url>
    <loc>{guide_urls[loc.key]}</loc>
{guide_alternates}
  </url>''')

    paths = ["api/" + component["id"] + "/" for component in config()]
    paths += ["api/releases/"]
    paths += [
        reference_path(item["source"]).removesuffix("index.html")
        for item in reference_headers(generate_reference())
    ]
    for path in paths:
        doc_alternates = "\n".join(
            f'    <xhtml:link rel="alternate" hreflang="{loc.html_lang}" href="{BASE_URL}{loc.url_prefix}{path}" />'
            for loc in LOCALES
        ) + f'\n    <xhtml:link rel="alternate" hreflang="x-default" href="{BASE_URL}{path}" />'
        for loc in LOCALES:
            entries.append(f'''  <url>
    <loc>{BASE_URL}{loc.url_prefix}{path}</loc>
{doc_alternates}
  </url>''')

    url_entries = "\n".join(entries)
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"
        xmlns:xhtml="http://www.w3.org/1999/xhtml">
{url_entries}
</urlset>
'''


def validate_page(page: str, locale: Locale, values: dict[str, str]) -> None:
    canonical = locale.canonical_url(BASE_URL)
    requirements = [
        f'<html lang="{locale.html_lang}" data-site-language="{locale.key}">',
        f"<title>{html.escape(values['meta.title'])}</title>",
        f'<link rel="canonical" href="{canonical}">',
        '<link rel="alternate" hreflang="x-default"',
        f'<meta property="og:url" content="{canonical}">',
        f'<meta property="og:locale" content="{locale.og_locale}">',
        '<script type="application/ld+json">',
        '<script type="application/json" id="site-i18n">',
    ]
    for other in LOCALES:
        requirements.append(
            f'<link rel="alternate" hreflang="{other.html_lang}" href="{other.canonical_url(BASE_URL)}">'
        )
    for requirement in requirements:
        if requirement not in page:
            fail(f"{locale.output_relative} is missing SEO contract: {requirement}")

    json_ld_match = re.search(
        r'<script type="application/ld\+json">\s*(.*?)\s*</script>', page, re.DOTALL
    )
    if not json_ld_match:
        fail(f"{locale.output_relative} is missing JSON-LD")
    try:
        json.loads(json_ld_match.group(1).replace("<\\/", "</"))
    except json.JSONDecodeError as error:
        fail(f"{locale.output_relative} contains invalid JSON-LD: {error}")

    i18n_match = re.search(
        r'<script type="application/json" id="site-i18n">(.*?)</script>', page, re.DOTALL
    )
    if not i18n_match:
        fail(f"{locale.output_relative} is missing runtime i18n JSON")
    try:
        runtime_map = json.loads(i18n_match.group(1).replace("<\\/", "</"))
    except json.JSONDecodeError as error:
        fail(f"{locale.output_relative} contains invalid runtime i18n JSON: {error}")
    if set(runtime_map) != set(RUNTIME_I18N_KEYS):
        fail(f"{locale.output_relative} runtime i18n keys mismatch")

    for target in LOCALES:
        href = html.escape(relative_locale_url(locale, target), quote=True)
        expected = f'href="{href}" lang="{target.html_lang}" hreflang="{target.html_lang}"'
        if expected not in page:
            fail(f"{locale.output_relative} is missing language link: {expected}")

    section_labels = re.findall(r'<p class="section-index"[^>]*>([^<]*)</p>', page)
    for number, label in enumerate(section_labels, start=1):
        if not label.startswith(f"{number:02d} / "):
            fail(
                f"{locale.output_relative} section {number:02d} has a missing or out-of-order index: {label}"
            )


def validate_error_page() -> None:
    try:
        page = ERROR_PAGE_PATH.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read {ERROR_PAGE_PATH}: {error}")

    requirements = [
        '<html lang="en" data-site-language="en">',
        '<base href="/Fluent-Qt/">',
        "location.pathname",
        'href="./"',
    ]
    for loc in LOCALES:
        if loc.url_prefix:
            requirements.append(loc.url_prefix)
            requirements.append(loc.html_lang)
    for requirement in requirements:
        if requirement not in page:
            fail(f"site/404.html is missing URL-language contract: {requirement}")

    for forbidden in (
        'localStorage.getItem("fluent-qt-language")',
        "navigator.language",
        "navigator.languages",
    ):
        if forbidden in page:
            fail(f"site/404.html must not infer language from browser state: {forbidden}")


def render_support_redirect(tier: str) -> str:
    """Open the homepage support section with one tier selected."""
    target = f"../../?fuel={tier}#support"
    return f'''<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="0; url={target}">
  <link rel="canonical" href="{BASE_URL}">
  <title>Support Fluent-Qt</title>
  <script>
    const navLang = (navigator.language || "").toLowerCase();
    if (/^zh-(tw|hk|mo|hant)\\b/.test(navLang)) location.replace("../../zh-TW/?fuel={tier}#support");
    else if (navLang.startsWith("zh")) location.replace("../../zh-CN/?fuel={tier}#support");
    else {{
      const prefix = {{"ja": "ja/", "ko": "ko/", "de": "de/", "ru": "ru/", "es": "es/", "fr": "fr/"}}[navLang.split("-")[0]];
      if (prefix) location.replace(`../../${{prefix}}?fuel={tier}#support`);
    }}
  </script>
</head>
<body><p><a href="{target}">Support Fluent-Qt</a></p></body>
</html>
'''


def validate_legacy_gallery_redirect() -> None:
    try:
        page = LEGACY_GALLERY_REDIRECT_PATH.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read {LEGACY_GALLERY_REDIRECT_PATH}: {error}")

    requirements = (
        '<link rel="canonical" href="https://calvinhxx.github.io/Fluent-Qt/gallery/">',
        '<meta http-equiv="refresh" content="0; url=../gallery/">',
        'new URL("../gallery/", window.location.href)',
        "target.search = window.location.search",
        "target.hash = window.location.hash",
        "window.location.replace(target)",
        '<a href="../gallery/">Open the Fluent-Qt Gallery</a>',
    )
    for requirement in requirements:
        if requirement not in page:
            fail(
                "site/app/index.html is missing legacy Gallery redirect contract: "
                f"{requirement}"
            )


def output_digest(content: str) -> str:
    return hashlib.sha256(content.encode("utf-8")).hexdigest()[:12]


def write_or_check(path: Path, expected: str, check: bool) -> bool:
    current = path.read_text(encoding="utf-8") if path.is_file() else None
    if current == expected:
        print(f"ok {path.relative_to(ROOT)} {output_digest(expected)}")
        return True
    if check:
        print(f"stale {path.relative_to(ROOT)}", file=sys.stderr)
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(expected, encoding="utf-8")
    print(f"wrote {path.relative_to(ROOT)} {output_digest(expected)}")
    return True


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Fail when committed generated files differ from the template",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    source = TEMPLATE_PATH.read_text(encoding="utf-8")
    localized_values = translations()
    version = project_version()
    validate_error_page()
    validate_legacy_gallery_redirect()
    validate_studio_locales()
    outputs: list[tuple[Path, str]] = []

    for locale in LOCALES:
        page = render_page(source, locale, localized_values[locale.key], version)
        validate_page(page, locale, localized_values[locale.key])
        outputs.append((SITE_ROOT / locale.output_relative, page))

    sitemap = render_sitemap()
    try:
        ET.fromstring(sitemap)
    except ET.ParseError as error:
        fail(f"generated sitemap is invalid XML: {error}")
    outputs.append((SITE_ROOT / "sitemap.xml", sitemap))
    outputs.extend(
        (SITE_ROOT / "fuel" / tier / "index.html", render_support_redirect(tier))
        for tier in SUPPORT_TIERS
    )
    outputs.extend(
        (SITE_ROOT / "studio" / "locales" / f"{lang}.json", studio_runtime_payload(lang))
        for lang in EXTRA_LOCALE_KEYS
    )

    studio_locales_dir = SITE_ROOT / "studio" / "locales"
    expected_studio_files = {f"{lang}.json" for lang in EXTRA_LOCALE_KEYS}
    stale_studio_files = (
        [p for p in sorted(studio_locales_dir.glob("*.json")) if p.name not in expected_studio_files]
        if studio_locales_dir.is_dir()
        else []
    )
    valid = all(write_or_check(path, content, arguments.check) for path, content in outputs)
    for stale in stale_studio_files:
        if arguments.check:
            print(f"stale {stale.relative_to(ROOT)}", file=sys.stderr)
            valid = False
        else:
            stale.unlink()
    if not valid:
        fail("localized site outputs are stale; run tools/site/generate_localized_site.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
