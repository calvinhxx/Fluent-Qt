#!/usr/bin/env python3
"""Check website themes and generated release documentation without loading WASM."""

from __future__ import annotations

import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import subprocess
import sys
import threading
from urllib.parse import parse_qs, urlsplit

from documentation_guides import guide_ui
from serve import SiteHandler
from site_locales import LOCALES, locale_by_key

PROJECT_ROOT = Path(__file__).resolve().parents[2]


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


class QuietSiteHandler(SiteHandler):
    log_message = QuietHandler.log_message


def prepare_site_theme_contract(project_root: Path) -> None:
    # Homepages are generated artifacts. A fresh checkout has neither locale;
    # refresh both even if an earlier local run left stale files behind.
    subprocess.run(
        [sys.executable, str(project_root / "tools/site/generate_localized_site.py")],
        cwd=project_root,
        check=True,
    )


def check_release_anchor_navigation(page: object, site_url: str) -> None:
    """Direct links reveal their target; subsequent filters own the resulting URL."""
    base = site_url + 'api/releases/'

    def open_page(suffix):
        page.goto(base + suffix, wait_until='domcontentloaded')
        page.wait_for_selector('[data-release-filters]:not([hidden])')

    def selected_scope():
        return page.locator('[data-release-scope-filter]:checked').input_value()

    def assert_revealed(anchor):
        if not page.locator(f'[id="{anchor}"]').is_visible():
            raise RuntimeError(f'Direct release link hid its target: {anchor}')
        if selected_scope() != 'all' or urlsplit(page.url).query or urlsplit(page.url).fragment != anchor:
            raise RuntimeError(f'Revealing a hidden target lost the anchor or retained conflicting filters: {anchor}')
        page.wait_for_function('''id => {
            const rect = document.getElementById(id).getBoundingClientRect();
            return rect.top >= 0 && rect.top < innerHeight;
        }''', arg=anchor)

    for anchor in ('v1.7.1', 'gallery-private-qt-headers'):
        # Exercise both a hidden version and a hidden group inside a visible version.
        open_page('?scope=uilib#' + anchor)
        assert_revealed(anchor)
        page.reload(wait_until='domcontentloaded')
        page.wait_for_selector('[data-release-filters]:not([hidden])')
        assert_revealed(anchor)
        page.locator('[data-release-scope-filter][value="uilib"] + span').click()
        if urlsplit(page.url).fragment or page.locator(f'[id="{anchor}"]').is_visible():
            raise RuntimeError(f'Switching scope retained a hidden release anchor: {anchor}')
        page.reload(wait_until='domcontentloaded')
        page.wait_for_selector('[data-release-filters]:not([hidden])')
        if (selected_scope() != 'uilib' or urlsplit(page.url).fragment
                or page.locator(f'[id="{anchor}"]').is_visible()):
            raise RuntimeError('Reloading a filtered release URL restored a stale anchor or cleared its scope')

    open_page('?scope=uilib&from=1.11.3&change=missing&breaking=1#v1.7.1')
    assert_revealed('v1.7.1')

    # A matching anchor must not clear useful filters, including on refresh.
    open_page('?scope=uilib&change=ComboBox#v1.11.3')
    page.reload(wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if (selected_scope() != 'uilib' or page.locator('[data-release-query]').input_value() != 'ComboBox'
            or urlsplit(page.url).fragment != 'v1.11.3'
            or not page.locator('[id="v1.11.3"]').is_visible()):
        raise RuntimeError('A visible version anchor cleared matching filters on reload')
    page.locator('[data-release-query]').fill('LineEdit')
    if urlsplit(page.url).fragment != 'v1.11.3':
        raise RuntimeError('Changing to another matching query cleared a visible anchor')
    page.locator('[data-release-query]').fill('')
    page.locator('[data-release-scope-filter][value="gallery"] + span').click()
    if urlsplit(page.url).fragment != 'v1.11.3':
        raise RuntimeError('Switching to another matching scope cleared a visible anchor')
    page.reload(wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if (selected_scope() != 'gallery' or urlsplit(page.url).fragment != 'v1.11.3'
            or not page.locator('[id="v1.11.3"]').is_visible()):
        raise RuntimeError('Reloading lost the matching scope or visible anchor')

    for key, value in (('change', 'no-such-component'), ('from', '1.11.3'), ('breaking', '1')):
        open_page('?scope=uilib#v1.11.3')
        if key == 'change':
            page.locator('[data-release-query]').fill(value)
        elif key == 'from':
            page.locator('[data-release-from]').select_option(value)
        else:
            page.locator('[data-release-breaking]').check()
        if urlsplit(page.url).fragment or page.locator('[id="v1.11.3"]').is_visible():
            raise RuntimeError(f'{key} filter retained a hidden version anchor')
        page.reload(wait_until='domcontentloaded')
        page.wait_for_selector('[data-release-filters]:not([hidden])')
        params = parse_qs(urlsplit(page.url).query)
        restored = (page.locator('[data-release-query]').input_value() if key == 'change'
                    else page.locator('[data-release-from]').input_value() if key == 'from'
                    else '1' if page.locator('[data-release-breaking]').is_checked() else '')
        if (params.get(key) != [value] or restored != value or selected_scope() != 'uilib'
                or urlsplit(page.url).fragment or page.locator('[id="v1.11.3"]').is_visible()):
            raise RuntimeError(f'Reloading lost the {key} filter')

    # Same-document navigation still reveals a migration excluded by the active scope.
    open_page('?scope=uilib')
    page.goto(base + '?scope=uilib#gallery-private-qt-headers', wait_until='domcontentloaded')
    assert_revealed('gallery-private-qt-headers')

    for anchor in ('missing-version', 'bad%ZZ', 'content'):
        open_page('?scope=uilib#' + anchor)
        page.locator('[data-release-scope-filter][value="gallery"] + span').click()
        if selected_scope() != 'gallery' or urlsplit(page.url).fragment != anchor:
            raise RuntimeError('Filtering changed an unknown or unrelated page anchor')


def check_release_documents(page: object, site_url: str, language: str, output: Path) -> None:
    """Exercise the generated reading surface without requesting a Gallery runtime."""
    # The homepage checks leave an explicit Light preference; this pass follows
    # the emulated system theme in its isolated browser context.
    page.evaluate("localStorage.removeItem('fluent-qt-theme')")
    response = page.goto(site_url + 'api/releases/', wait_until='domcontentloaded')
    if response is None or not response.ok:
        raise RuntimeError('Missing generated release history')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    all_count = page.locator('[data-release-entry]').count()
    def choose_scope(scope):
        page.locator(f'[data-release-scope-filter][value="{scope}"] + span').click()

    library = page.locator('[id="v1.11.1"] [data-release-scope="uilib"]')
    gallery = page.locator('[id="v1.11.1"] [data-release-scope="gallery"]')
    choose_scope('uilib')
    if not library.is_visible() or gallery.is_visible() or page.locator('#gallery-private-qt-headers').is_visible():
        raise RuntimeError('UILib view leaked Gallery summaries or migration requirements')
    page.locator('[data-release-breaking]').check()
    if page.locator('[id="v1.11.1"]').is_visible() or not page.locator('[id="v1.7.0"]').is_visible():
        raise RuntimeError('Migration filtering ignored the selected scope')
    choose_scope('gallery')
    if not gallery.is_visible() or library.is_visible() or page.locator('[id="v1.7.0"]').is_visible():
        raise RuntimeError('Gallery migration view leaked library content')
    page.locator('[data-release-from]').select_option('1.11.0')
    page.reload(wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if not page.locator('[value="gallery"][data-release-scope-filter]').is_checked() or not gallery.is_visible():
        raise RuntimeError('Reloading lost the audience or migration filters')
    page.locator('[data-release-query]').fill('qt6-base-private-dev')
    choose_scope('uilib')
    if not page.locator('[data-release-empty]').is_visible():
        raise RuntimeError('Gallery migration search terms leaked into UILib results')
    page.locator('[data-release-filters] button[type="reset"]').click()
    all_scope = page.locator('[value="all"][data-release-scope-filter]')
    all_scope.focus()
    page.keyboard.press('ArrowRight')
    if not page.locator('[value="uilib"][data-release-scope-filter]').is_checked():
        raise RuntimeError('Scope filters do not support native radio keyboard navigation')
    page.locator('[data-release-filters] button[type="reset"]').click()
    page.locator('[data-release-breaking]').check()
    page.locator('[data-release-from]').select_option('1.6.4')
    if page.locator('[id="v1.5.0"]').is_visible() or not page.locator('[id="v1.7.0"]').is_visible():
        raise RuntimeError('Upgrade range filtering is incorrect')
    page.locator('[data-release-filters] button[type="reset"]').click()
    if not all_scope.is_checked():
        raise RuntimeError('Reset did not restore the All scope')
    if page.locator('[data-release-entry]:visible').count() != all_count:
        raise RuntimeError('Clearing release filters did not restore every record')
    page.locator('[data-release-query]').fill('ParticleBackdrop')
    if not page.locator('[id="v1.8.3"]').is_visible():
        raise RuntimeError('Component class names must match their documentation identifiers')
    page.reload(wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if page.locator('[data-release-query]').input_value() != 'ParticleBackdrop':
        raise RuntimeError('Reloading lost the release query')
    if page.locator('[data-directory-search]').input_value():
        raise RuntimeError('Release search leaked into the documentation directory')
    page.locator('[data-release-query]').fill('no-such-component-123')
    if not page.locator('[data-release-empty]').is_visible():
        raise RuntimeError('Release history is missing its empty state')
    page.locator('[data-release-filters] button[type="reset"]').click()
    for width in (1280, 390):
        page.set_viewport_size({'width': width, 'height': 800})
        if page.evaluate('document.documentElement.scrollWidth > innerWidth'):
            raise RuntimeError(f'Release history overflows at {width}px')
        for theme in ('light', 'dark'):
            page.emulate_media(color_scheme=theme)
            page.wait_for_function('theme => document.documentElement.dataset.theme === theme', arg=theme)
            page.screenshot(path=str(output / f'releases-{language}-{theme}-{width}.png'), full_page=False)
    page.goto(site_url + 'api/releases/#v1.11.3', wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    page.screenshot(path=str(output / f'releases-{language}-grouped-mobile.png'), full_page=False)
    page.goto(site_url + 'api/releases/?scope=uilib#gallery-private-qt-headers', wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if not page.locator('#gallery-private-qt-headers').is_visible():
        raise RuntimeError('A scoped URL hid an existing migration anchor')
    page.goto(site_url + 'api/releases/?scope=unknown', wait_until='domcontentloaded')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if page.locator('[data-release-entry]:visible').count() != all_count or not all_scope.is_checked():
        raise RuntimeError('Unknown scopes must fall back to All')
    page.goto(site_url + 'api/roadmap/', wait_until='domcontentloaded')
    page.wait_for_url(site_url + 'api/releases/')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if page.locator('a[href*="/roadmap/"]').count():
        raise RuntimeError('Retired roadmap must not appear in the documentation navigation')
    page.goto(site_url + 'api/?q=roadmap', wait_until='domcontentloaded')
    page.wait_for_url(site_url + 'api/releases/')
    page.wait_for_selector('[data-release-filters]:not([hidden])')
    if page.locator('[data-directory-search]').input_value():
        raise RuntimeError('The legacy roadmap alias must not filter the documentation directory')
    # This guide shares the API shell without starting a component's live preview.
    page.goto(site_url + 'api/foundation-iconography/', wait_until='domcontentloaded')
    if page.locator('.document-version-note a').count():
        page.locator('.document-version-note a').click()
        page.wait_for_selector('[data-release-filters]:not([hidden])')
        if page.locator('[data-release-query]').input_value() != 'foundation-iconography':
            raise RuntimeError('The API notice lost its related release filter')
        anchor = urlsplit(page.url).fragment
        if not page.locator(f'[data-release-entry][id="{anchor}"]').is_visible():
            raise RuntimeError('The API notice did not reach its pending change')
    check_release_anchor_navigation(page, site_url)


def check_directory_hierarchy(page: object, site_url: str, *, language: str = "en") -> None:
    """Keep each document discoverable through its owning branch and search."""
    cases = [
        ('releases', ['project']),
        ('foundation', ['foundation']),
        ('foundation-iconography', ['foundation', 'category-foundation']),
        ('module-particles', ['modules', 'category-particles']),
        ('module-spatial', ['modules', 'category-spatial']),
        ('module-qmlbridge', ['modules', 'category-qml-bridge']),
        ('reference/particles-session', ['reference']),
    ]
    # The previous release check also exercises the compact layout.
    page.set_viewport_size({'width': 1280, 'height': 800})
    for route, expected in cases:
        page.goto(site_url + 'api/' + route + '/', wait_until='domcontentloaded')
        roots = page.locator('.directory-groups > [data-directory-group]').evaluate_all(
            'groups => groups.map(group => group.dataset.directoryGroup)')
        if roots != ['project', 'foundation', 'components', 'modules', 'reference']:
            raise RuntimeError(f'Unexpected documentation sections: {roots}')
        current = page.locator('.directory-groups a[aria-current="page"]')
        if current.count() != 1 or not current.is_visible():
            raise RuntimeError(f'The current document is not exposed in its branch: {route}')
        ancestors = current.evaluate('''link => {
            const groups = [];
            for (let parent = link.parentElement; parent; parent = parent.parentElement) {
                if (parent.hasAttribute('data-directory-group')) groups.unshift(parent.dataset.directoryGroup);
            }
            return groups;
        }''')
        if ancestors != expected:
            raise RuntimeError(f'Wrong documentation branch for {route}: {ancestors}')

    page.goto(site_url + 'api/releases/', wait_until='domcontentloaded')
    components = page.locator('[data-directory-group="components"]')
    components.locator(':scope > summary').click()
    basic = page.locator('[data-directory-group="category-basic-input"]')
    basic.locator(':scope > summary').focus()
    basic.locator(':scope > summary').press('Enter')
    if not page.locator('[data-component-id="button"]').is_visible():
        raise RuntimeError('Keyboard expansion did not expose nested components')
    before = page.locator('[data-directory-group]').evaluate_all('groups => groups.map(g => g.open)')
    search = page.locator('[data-directory-search]')
    for query, component in (('ParticleBackdrop', 'particle-backdrop'), ('FontIcon', 'font-icon'), ('ChartView', 'chart-view')):
        search.fill(query)
        if not page.locator(f'[data-component-id="{component}"]').is_visible():
            raise RuntimeError(f'Search did not expand the matched component branch: {query}')
    ui = guide_ui(language)
    search.fill('no-such-document-xyz')
    if page.locator('[data-directory-group]:not([hidden])').count():
        raise RuntimeError('Empty search results left empty branches visible')
    status_text = page.locator('[data-directory-status]').text_content()
    if status_text != ui['noResults']:
        raise RuntimeError(f'Unexpected empty search status in {language}: {status_text!r} != {ui["noResults"]!r}')
    search.press('Escape')
    after = page.locator('[data-directory-group]').evaluate_all('groups => groups.map(g => g.open)')
    if before != after:
        raise RuntimeError('Clearing search lost the reader\'s expanded branches')


def check_playground_hover_contrast(preview: object) -> None:
    preview.hover()
    ratio = preview.evaluate("""button => {
        if (!button.matches(':hover')) throw new Error('Pointer missed the property preview button');
        for (const transition of button.getAnimations()) {
            if (['color', 'background-color'].includes(transition.transitionProperty)) transition.finish();
        }
        const ctx = document.createElement('canvas').getContext('2d');
        function luminance(color) {
            ctx.fillStyle = color;
            ctx.fillRect(0, 0, 1, 1);
            const [r, g, b] = ctx.getImageData(0, 0, 1, 1).data;
            function linear(byte) {
                const channel = byte / 255;
                return channel <= 0.04045 ? channel / 12.92 : ((channel + 0.055) / 1.055) ** 2.4;
            }
            return 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b);
        }
        const style = getComputedStyle(button);
        const fg = luminance(style.color), bg = luminance(style.backgroundColor);
        return (Math.max(fg, bg) + 0.05) / (Math.min(fg, bg) + 0.05);
    }""")
    if ratio < 4.5:
        raise RuntimeError(f"Property preview hover contrast is {ratio:.2f}, expected at least 4.5")


def run_site_theme_contract(browser: object, output: Path, *, site_root: Path | None = None) -> dict:
    if site_root is None:
        prepare_site_theme_contract(PROJECT_ROOT)
        # The static contract must never load Gallery; no WASM build is needed.
        handler = partial(QuietSiteHandler, wasm_root=output / "no-wasm-runtime")
    else:
        site_root = site_root.resolve()
        for loc in LOCALES:
            name = loc.output_relative
            if not (site_root / name).is_file():
                raise RuntimeError(f"Website artifact is missing {name}: {site_root}")
        handler = partial(QuietHandler, directory=str(site_root))
    active_root = site_root or (PROJECT_ROOT / "site")
    output.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", 0), handler)
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    results = []
    try:
        for loc in LOCALES:
            language, locale = loc.html_lang, loc.url_prefix
            page = browser.new_page(viewport={"width": 1280, "height": 800})
            errors, runtime_requests = [], []
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.on("request", lambda request: runtime_requests.append(request.url)
                    if urlsplit(request.url).path.startswith("/gallery/")
                    or urlsplit(request.url).path.endswith(".wasm") else None)
            try:
                page.route("https://api.github.com/**", lambda route: route.abort())
                page.emulate_media(color_scheme="light", forced_colors="none")
                site_url = f"http://127.0.0.1:{server.server_port}/{locale}"
                response = page.goto(site_url, wait_until="domcontentloaded")
                if response is None or not response.ok:
                    raise RuntimeError(f"Could not load the website theme contract for {language}")
                page.wait_for_selector("html[data-i18n-ready]", timeout=10_000)

                preview = page.locator(".hero-playground-action")
                preview.wait_for(state="visible")
                if language == "en":  # All localized homepages share the button stylesheet.
                    check_playground_hover_contrast(preview)

                for width in (960, 1024, 1280):
                    page.set_viewport_size({"width": width, "height": 800})
                    menu = page.locator(".menu-button")
                    if menu.is_visible():
                        menu.click()
                    github = page.locator(".github-link")
                    bounds = github.bounding_box()
                    if not bounds or bounds["x"] + bounds["width"] > width:
                        raise RuntimeError(f"Navigation exceeds {width}px in {language}")
                    switcher = page.locator(".language-switch")
                    switcher.locator("summary").click()
                    if switcher.get_attribute("open") is None:
                        raise RuntimeError(f"Language dropdown failed to open in {language}")
                    menu_bounds = switcher.locator(".language-menu").bounding_box()
                    if not menu_bounds or menu_bounds["x"] < 0 or menu_bounds["x"] + menu_bounds["width"] > width:
                        raise RuntimeError(f"Language dropdown overflows {width}px in {language}: {menu_bounds}")
                    page.keyboard.press("Escape")
                    if switcher.get_attribute("open") is not None:
                        raise RuntimeError(f"Language dropdown failed to close on Escape in {language}")
                    if menu.is_visible():
                        menu.click()

                def active_theme() -> str:
                    return page.evaluate("document.documentElement.dataset.theme")

                def stored_theme() -> str | None:
                    return page.evaluate("localStorage.getItem('fluent-qt-theme')")

                toggle = page.locator("[data-theme-toggle]")
                if active_theme() != "light":
                    raise RuntimeError("Website did not start in the emulated light theme")
                toggle.click()
                if active_theme() != "dark" or stored_theme() != "dark":
                    raise RuntimeError("Website light-to-dark theme transition failed")
                toggle.click()
                if active_theme() != "high-contrast" or stored_theme() != "high-contrast":
                    raise RuntimeError("Website dark-to-high-contrast transition failed")
                gallery_theme = page.evaluate(
                    "new URL(galleryUrl()).searchParams.get('host-theme')"
                )
                if gallery_theme != "high-contrast":
                    raise RuntimeError("Website Gallery URL omitted the high-contrast host theme")

                page.reload(wait_until="domcontentloaded")
                page.wait_for_selector("html[data-i18n-ready]", timeout=10_000)
                if active_theme() != "high-contrast":
                    raise RuntimeError("Website did not restore the high-contrast preference")
                page.locator("[data-theme-toggle]").click()
                if active_theme() != "light" or stored_theme() != "light":
                    raise RuntimeError("Website high-contrast-to-light transition failed")

                page.emulate_media(forced_colors="active")
                page.wait_for_function(
                    "document.documentElement.dataset.theme === 'high-contrast'"
                )
                if not page.locator("[data-theme-toggle]").is_disabled():
                    raise RuntimeError("Website theme control stayed active during forced-colors")
                if stored_theme() != "light":
                    raise RuntimeError("forced-colors overwrote the stored website preference")
                page.emulate_media(forced_colors="none")
                page.wait_for_function("document.documentElement.dataset.theme === 'light'")
                if page.locator("[data-theme-toggle]").is_disabled():
                    raise RuntimeError("Website theme control did not recover after forced-colors")
                for width in (1280, 390, 320):
                    page.set_viewport_size({"width": width, "height": 800})
                    page.goto(site_url + "#top")
                    page.wait_for_selector("html[data-i18n-ready]")
                    if page.evaluate("document.documentElement.scrollWidth > innerWidth"):
                        raise RuntimeError(f"Website overflows horizontally in {language} at {width}px")
                    # The section clips overflow, so page width alone misses a cut-off title.
                    if not page.locator("#spatial-title").evaluate("""title => {
                        const text = document.createRange();
                        text.selectNodeContents(title);
                        const section = title.closest('section').getBoundingClientRect();
                        return [...text.getClientRects()].every(r => r.left >= section.left && r.right <= section.right);
                    }"""):
                        raise RuntimeError(f"Spatial title is clipped in {language} at {width}px")
                    page.wait_for_function("""width =>
                        (getComputedStyle(document.querySelector('.hero-window')).transform === 'none') === (width < 900)
                    """, arg=width)
                    page.wait_for_function("getComputedStyle(document.querySelector('.hero-message')).opacity === '1'")
                    if page.locator('[data-showcase-frame]').get_attribute('src'):
                        raise RuntimeError("Website eagerly loaded the opt-in Spatial runtime")
                    page.screenshot(path=str(output / f"{language}-{width}.png"))

                # A locale code deeper in a missing URL must not change its language or home link.
                for mount in ("", "Fluent-Qt/"):
                    site_base = f"http://127.0.0.1:{server.server_port}/{mount}"
                    nested = "de" if locale else "ja"
                    not_found_url = f"{site_base}{locale}api/{nested}/missing-page-check"
                    page.route(
                        not_found_url,
                        lambda route: route.fulfill(
                            status=404,
                            content_type="text/html; charset=utf-8",
                            body=(active_root / "404.html").read_text(encoding="utf-8"),
                        ),
                    )
                    page.goto(not_found_url, wait_until="domcontentloaded")
                    if page.locator("html").get_attribute("lang") != language:
                        raise RuntimeError(f"404.html did not resolve lang={language} from URL {not_found_url}")
                    home_url = page.locator("[data-404-home]").evaluate("link => link.href")
                    if home_url != site_base + locale:
                        raise RuntimeError(f"404.html links {not_found_url} to the wrong homepage: {home_url}")
                    page.unroute(not_found_url)

                release_checked = "not_run"
                directory_checked = "not_run"
                has_guide = (active_root / locale / "api/foundation/index.html").is_file()
                if site_root is not None or has_guide:
                    check_release_documents(page, site_url, language, output)
                    release_checked = "passed"
                    fallback = browser.new_page(java_script_enabled=False)
                    try:
                        fallback.goto(site_url + 'api/releases/', wait_until='domcontentloaded')
                        entry_count = fallback.locator('[data-release-entry]').count()
                        if not entry_count or fallback.locator('[data-release-entry]:visible').count() != entry_count:
                            raise RuntimeError('No-JavaScript reading lost release entries')
                        for scope in ('uilib', 'gallery'):
                            if not fallback.locator(f'[id="v1.11.1"] [data-release-scope="{scope}"]').is_visible():
                                raise RuntimeError('No-JavaScript reading hid a release scope')
                        if not fallback.locator('#gallery-private-qt-headers').is_visible():
                            raise RuntimeError('No-JavaScript reading hid migration instructions')
                    finally:
                        fallback.close()
                    check_directory_hierarchy(page, site_url, language=loc.key)
                    directory_checked = "passed"
                if runtime_requests:
                    raise RuntimeError(f"Static website eagerly requested WASM ({language}): {runtime_requests}")
                if site_root is not None or has_guide:
                    page.route("**/gallery/**", lambda route: route.fulfill(status=503, content_type="text/html", body="<h1>Unavailable</h1>"))
                    page.locator('[data-component-id="button"]').click()
                    page.wait_for_url(site_url + 'api/button/')
                    actual_html_lang = page.locator('html').get_attribute('lang')
                    if actual_html_lang != language:
                        raise RuntimeError(f"Clicking Button from {loc.key} switched html lang to {actual_html_lang!r}")
                    actual_home = page.locator('[data-shell-link="home"]').text_content()
                    if actual_home != guide_ui(loc.key)['home']:
                        raise RuntimeError(f"Unexpected localized home label after clicking Button in {loc.key}: {actual_home!r}")
                if errors:
                    raise RuntimeError(f"Website page errors ({language}): {errors}")
                results.append({"language": language, "themes": ["light", "dark", "high-contrast"],
                                "forced_colors": "passed", "widths": [1280, 390, 320], "runtime_requests": [],
                                "release_documents": release_checked,
                                "directory_hierarchy": directory_checked})
            finally:
                page.close()
    finally:
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)
    return {"status": "passed", "source": str(active_root), "pages": results}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--site-root", type=Path, help="Validate an already generated website artifact")
    parser.add_argument("--output", type=Path, default=PROJECT_ROOT / "build/site-theme-browser")
    args = parser.parse_args()
    from playwright.sync_api import sync_playwright

    with sync_playwright() as playwright:
        browser = playwright.chromium.launch(headless=True)
        try:
            result = run_site_theme_contract(browser, args.output, site_root=args.site_root)
        finally:
            browser.close()
    (args.output / "result.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Website browser theme contract passed: {len(LOCALES)} languages, three themes, forced colors, wide/narrow, no WASM")


if __name__ == "__main__":
    main()
