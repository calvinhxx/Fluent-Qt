#!/usr/bin/env python3
"""Shared locale registry for Fluent-Qt's website, guides, and README links."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(frozen=True)
class Locale:
    key: str
    html_lang: str
    url_prefix: str
    native_name: str
    short_label: str
    og_locale: str
    readme_file: str
    default_support_method: str
    playground_art_lang: str
    og_image_alt: str
    gallery_schema_description: str

    @property
    def resource_prefix(self) -> str:
        return "" if not self.url_prefix else "../"

    @property
    def output_relative(self) -> str:
        return f"{self.url_prefix}index.html"

    @property
    def api_url(self) -> str:
        return "api/foundation/"

    @property
    def releases_url(self) -> str:
        return "api/releases/"

    @property
    def playground_url(self) -> str:
        return "api/checkbox/?playground=1"

    def canonical_url(self, base_url: str) -> str:
        return f"{base_url}{self.url_prefix}"

    def readme_url(self, repository_url: str) -> str:
        return f"{repository_url}/blob/main/{self.readme_file}"


LOCALES: tuple[Locale, ...] = (
    Locale(
        key="en",
        html_lang="en",
        url_prefix="",
        native_name="English",
        short_label="EN",
        og_locale="en_US",
        readme_file="README.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="Fluent-Qt Gallery project preview with native Qt Widgets controls.",
        gallery_schema_description=(
            "Interactive C++ WebAssembly Gallery for evaluating Fluent-Qt controls in a browser."
        ),
    ),
    Locale(
        key="zh",
        html_lang="zh-CN",
        url_prefix="zh-CN/",
        native_name="简体中文",
        short_label="简",
        og_locale="zh_CN",
        readme_file="README.zh-CN.md",
        default_support_method="wechat",
        playground_art_lang="zh",
        og_image_alt="Fluent-Qt Gallery 项目预览，展示原生 Qt Widgets 控件。",
        gallery_schema_description=(
            "可在浏览器中体验 Fluent-Qt 控件的交互式 C++ WebAssembly Gallery。"
        ),
    ),
    Locale(
        key="zh-TW",
        html_lang="zh-TW",
        url_prefix="zh-TW/",
        native_name="繁體中文",
        short_label="繁",
        og_locale="zh_TW",
        readme_file="README.zh-TW.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="Fluent-Qt Gallery 專案預覽，展示原生 Qt Widgets 控制項。",
        gallery_schema_description=(
            "可在瀏覽器中體驗 Fluent-Qt 控制項的互動式 C++ WebAssembly Gallery。"
        ),
    ),
    Locale(
        key="ja",
        html_lang="ja",
        url_prefix="ja/",
        native_name="日本語",
        short_label="JA",
        og_locale="ja_JP",
        readme_file="README.ja.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="ネイティブ Qt Widgets コントロールを表示する Fluent-Qt Gallery プロジェクトのプレビュー。",
        gallery_schema_description=(
            "ブラウザ上で Fluent-Qt コントロールを試せるインタラクティブな C++ WebAssembly Gallery。"
        ),
    ),
    Locale(
        key="ko",
        html_lang="ko",
        url_prefix="ko/",
        native_name="한국어",
        short_label="KO",
        og_locale="ko_KR",
        readme_file="README.ko.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="네이티브 Qt Widgets 컨트롤을 보여 주는 Fluent-Qt Gallery 프로젝트 미리 보기.",
        gallery_schema_description=(
            "브라우저에서 Fluent-Qt 컨트롤을 직접 체험할 수 있는 대화형 C++ WebAssembly Gallery입니다."
        ),
    ),
    Locale(
        key="de",
        html_lang="de",
        url_prefix="de/",
        native_name="Deutsch",
        short_label="DE",
        og_locale="de_DE",
        readme_file="README.de.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="Vorschau auf das Fluent-Qt Gallery-Projekt mit nativen Qt Widgets-Steuerelementen.",
        gallery_schema_description=(
            "Interaktive C++-WebAssembly-Gallery zum Ausprobieren von Fluent-Qt-Steuerelementen im Browser."
        ),
    ),
    Locale(
        key="ru",
        html_lang="ru",
        url_prefix="ru/",
        native_name="Русский",
        short_label="RU",
        og_locale="ru_RU",
        readme_file="README.ru.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="Предпросмотр проекта Fluent-Qt Gallery с нативными элементами управления Qt Widgets.",
        gallery_schema_description=(
            "Интерактивная C++ WebAssembly Gallery для проверки элементов управления Fluent-Qt в браузере."
        ),
    ),
    Locale(
        key="es",
        html_lang="es",
        url_prefix="es/",
        native_name="Español",
        short_label="ES",
        og_locale="es_ES",
        readme_file="README.es.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="Vista previa del proyecto Fluent-Qt Gallery con controles nativos de Qt Widgets.",
        gallery_schema_description=(
            "Galería interactiva en C++ y WebAssembly para probar los controles de Fluent-Qt en el navegador."
        ),
    ),
    Locale(
        key="fr",
        html_lang="fr",
        url_prefix="fr/",
        native_name="Français",
        short_label="FR",
        og_locale="fr_FR",
        readme_file="README.fr.md",
        default_support_method="kofi",
        playground_art_lang="en",
        og_image_alt="Aperçu du projet Fluent-Qt Gallery avec des contrôles natifs Qt Widgets.",
        gallery_schema_description=(
            "Galerie interactive C++ WebAssembly pour évaluer les contrôles Fluent-Qt dans le navigateur."
        ),
    ),
)

LOCALE_KEYS: tuple[str, ...] = tuple(locale.key for locale in LOCALES)
LOCALE_BY_KEY: dict[str, Locale] = {locale.key: locale for locale in LOCALES}


def locale_by_key(key: str) -> Locale:
    return LOCALE_BY_KEY[key]


def locale_url_prefix(language: str) -> str:
    return LOCALE_BY_KEY[language].url_prefix


def relative_locale_url(from_locale: Locale, to_locale: Locale) -> str:
    if from_locale.key == to_locale.key:
        return "./"
    return f"{from_locale.resource_prefix}{to_locale.url_prefix}" or "./"
