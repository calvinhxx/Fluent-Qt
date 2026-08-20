#!/usr/bin/env python3
"""Generate and validate static English and Simplified Chinese site pages."""

from __future__ import annotations

import argparse
import hashlib
import html
import json
import re
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
SITE_ROOT = ROOT / "site"
TEMPLATE_PATH = Path(__file__).with_name("index.template.html")
SITE_SCRIPT_PATH = SITE_ROOT / "site.js"
ERROR_PAGE_PATH = SITE_ROOT / "404.html"
BASE_URL = "https://calvinhxx.github.io/Fluent-Qt/"
REPOSITORY_URL = "https://github.com/calvinhxx/Fluent-Qt"
OG_IMAGE_URL = f"{BASE_URL}assets/og.png"

TEXT_PATTERN = re.compile(
    r'(?P<open><(?P<tag>[A-Za-z][A-Za-z0-9]*)[^>]*data-i18n="(?P<key>[^"]+)"[^>]*>)'
    r'(?P<content>.*?)'
    r'(?P<close></(?P=tag)>)',
    re.DOTALL,
)
START_TAG_PATTERN = re.compile(r"<(?!/|!)[^>]+>", re.DOTALL)


@dataclass(frozen=True)
class Locale:
    key: str
    html_lang: str
    output: Path
    canonical_url: str
    og_locale: str
    alternate_og_locale: str
    resource_prefix: str
    english_url: str
    chinese_url: str
    english_current: str
    chinese_current: str
    readme_url: str
    image_alt: str


LOCALES = (
    Locale(
        key="en",
        html_lang="en",
        output=SITE_ROOT / "index.html",
        canonical_url=BASE_URL,
        og_locale="en_US",
        alternate_og_locale="zh_CN",
        resource_prefix="",
        english_url="./",
        chinese_url="zh-CN/",
        english_current="page",
        chinese_current="false",
        readme_url=f"{REPOSITORY_URL}/blob/main/README.md",
        image_alt="Fluent-Qt Gallery project preview with native Qt Widgets controls.",
    ),
    Locale(
        key="zh",
        html_lang="zh-CN",
        output=SITE_ROOT / "zh-CN" / "index.html",
        canonical_url=f"{BASE_URL}zh-CN/",
        og_locale="zh_CN",
        alternate_og_locale="en_US",
        resource_prefix="../",
        english_url="../",
        chinese_url="./",
        english_current="false",
        chinese_current="page",
        readme_url=f"{REPOSITORY_URL}/blob/main/README.zh-CN.md",
        image_alt="Fluent-Qt Gallery 项目预览，展示原生 Qt Widgets 控件。",
    ),
)


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


def translations() -> dict[str, dict[str, str]]:
    source = SITE_SCRIPT_PATH.read_text(encoding="utf-8")
    declaration = "const translations = "
    start = source.find(declaration)
    end_marker = "\n};\n\nconst fallbackVersion"
    end = source.find(end_marker, start)
    if start < 0 or end < 0:
        fail("could not locate the translations object in site/site.js")

    object_start = source.find("{", start)
    literal = source[object_start : end + 2]
    literal = re.sub(
        r"^  (zh|en):",
        lambda match: f'  "{match.group(1)}":',
        literal,
        flags=re.MULTILINE,
    )
    try:
        parsed = json.loads(literal)
    except json.JSONDecodeError as error:
        fail(f"site/site.js translations are not JSON-compatible: {error}")

    if not isinstance(parsed, dict) or set(parsed) != {"en", "zh"}:
        fail("site/site.js must contain exactly the en and zh translation maps")
    if set(parsed["en"]) != set(parsed["zh"]):
        missing_en = sorted(set(parsed["zh"]) - set(parsed["en"]))
        missing_zh = sorted(set(parsed["en"]) - set(parsed["zh"]))
        fail(f"translation keys differ; missing_en={missing_en}, missing_zh={missing_zh}")
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
            tag = replace_attribute(tag, "href", locale.readme_url)
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
    if not prefix:
        return page

    attributes = (
        "src",
        "href",
        "data-theme-src-light",
        "data-theme-src-dark",
        "data-gallery-src",
    )
    attribute_names = "|".join(re.escape(name) for name in attributes)
    pattern = re.compile(
        rf'(?P<head>\b(?:{attribute_names})=")'
        r'(?P<url>(?:assets/|styles\.css|site\.js|gallery/)[^"]*)'
    )
    return pattern.sub(
        lambda match: f'{match.group("head")}{prefix}{match.group("url")}', page
    )


def structured_data(locale: Locale, values: dict[str, str], version: str) -> str:
    gallery_description = (
        "Interactive C++ WebAssembly Gallery for evaluating Fluent-Qt controls in a browser."
        if locale.key == "en"
        else "可在浏览器中体验 Fluent-Qt 控件的交互式 C++ WebAssembly Gallery。"
    )
    graph: dict[str, Any] = {
        "@context": "https://schema.org",
        "@graph": [
            {
                "@type": "SoftwareSourceCode",
                "@id": f"{BASE_URL}#project",
                "name": "Fluent-Qt",
                "url": locale.canonical_url,
                "description": values["meta.description"],
                "codeRepository": REPOSITORY_URL,
                "programmingLanguage": "C++",
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
                "description": gallery_description,
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
    token_values = {
        "HTML_LANG": locale.html_lang,
        "SITE_LANGUAGE": locale.key,
        "META_TITLE": values["meta.title"],
        "META_DESCRIPTION": values["meta.description"],
        "CANONICAL_URL": locale.canonical_url,
        "OG_LOCALE": locale.og_locale,
        "OG_ALTERNATE_LOCALE": locale.alternate_og_locale,
        "OG_IMAGE_ALT": locale.image_alt,
        "STRUCTURED_DATA": structured_data(locale, values, version),
        "EN_URL": locale.english_url,
        "ZH_URL": locale.chinese_url,
        "EN_CURRENT": locale.english_current,
        "ZH_CURRENT": locale.chinese_current,
    }
    page = template
    for token, value in token_values.items():
        replacement = value if token == "STRUCTURED_DATA" else html.escape(value, quote=True)
        page = page.replace(f"{{{{{token}}}}}", replacement)

    page = localize_text(page, values, locale)
    page = localize_attributes(page, values, locale)
    page = prefix_resources(page, locale.resource_prefix)
    if re.search(r"{{[A-Z0-9_]+}}", page):
        fail(f"unresolved template token in {locale.output}")
    return page.rstrip() + "\n"


def render_sitemap() -> str:
    return f'''<?xml version="1.0" encoding="UTF-8"?>
<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9"
        xmlns:xhtml="http://www.w3.org/1999/xhtml">
  <url>
    <loc>{BASE_URL}</loc>
    <xhtml:link rel="alternate" hreflang="en" href="{BASE_URL}" />
    <xhtml:link rel="alternate" hreflang="zh-CN" href="{BASE_URL}zh-CN/" />
    <xhtml:link rel="alternate" hreflang="x-default" href="{BASE_URL}" />
  </url>
  <url>
    <loc>{BASE_URL}zh-CN/</loc>
    <xhtml:link rel="alternate" hreflang="en" href="{BASE_URL}" />
    <xhtml:link rel="alternate" hreflang="zh-CN" href="{BASE_URL}zh-CN/" />
    <xhtml:link rel="alternate" hreflang="x-default" href="{BASE_URL}" />
  </url>
</urlset>
'''


def validate_page(page: str, locale: Locale, values: dict[str, str]) -> None:
    requirements = (
        f'<html lang="{locale.html_lang}" data-site-language="{locale.key}">',
        f"<title>{html.escape(values['meta.title'])}</title>",
        f'<link rel="canonical" href="{locale.canonical_url}">',
        '<link rel="alternate" hreflang="en"',
        '<link rel="alternate" hreflang="zh-CN"',
        '<link rel="alternate" hreflang="x-default"',
        f'<meta property="og:url" content="{locale.canonical_url}">',
        f'<meta property="og:locale" content="{locale.og_locale}">',
        '<script type="application/ld+json">',
    )
    for requirement in requirements:
        if requirement not in page:
            fail(f"{locale.output} is missing SEO contract: {requirement}")

    json_ld_match = re.search(
        r'<script type="application/ld\+json">\s*(.*?)\s*</script>', page, re.DOTALL
    )
    if not json_ld_match:
        fail(f"{locale.output} is missing JSON-LD")
    try:
        json.loads(json_ld_match.group(1).replace("<\\/", "</"))
    except json.JSONDecodeError as error:
        fail(f"{locale.output} contains invalid JSON-LD: {error}")

    expected_links = (
        f'href="{locale.english_url}" lang="en" hreflang="en"',
        f'href="{locale.chinese_url}" lang="zh-CN" hreflang="zh-CN"',
    )
    for expected in expected_links:
        if expected not in page:
            fail(f"{locale.output} is missing language link: {expected}")


def validate_error_page() -> None:
    try:
        page = ERROR_PAGE_PATH.read_text(encoding="utf-8")
    except OSError as error:
        fail(f"cannot read {ERROR_PAGE_PATH}: {error}")

    requirements = (
        '<html lang="en" data-site-language="en">',
        '<base href="/Fluent-Qt/">',
        'location.pathname) ? "zh" : "en"',
        'href="zh-CN/" data-lang-zh',
        'href="./" data-lang-en',
    )
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
    outputs: list[tuple[Path, str]] = []

    for locale in LOCALES:
        page = render_page(source, locale, localized_values[locale.key], version)
        validate_page(page, locale, localized_values[locale.key])
        outputs.append((locale.output, page))

    sitemap = render_sitemap()
    try:
        ET.fromstring(sitemap)
    except ET.ParseError as error:
        fail(f"generated sitemap is invalid XML: {error}")
    outputs.extend(
        (
            (SITE_ROOT / "sitemap.xml", sitemap),
        )
    )

    valid = all(write_or_check(path, content, arguments.check) for path, content in outputs)
    if not valid:
        fail("localized site outputs are stale; run tools/site/generate_localized_site.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
