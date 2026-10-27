"""Shared navigation and a lightweight directory inside component documentation."""

import html
import os
from pathlib import Path
from urllib.parse import urlsplit

from generate_localized_site import documentation_entry

SITE = Path(__file__).resolve().parents[2] / "site"


def relative(path, page):
    url = urlsplit(path)
    result = os.path.relpath(SITE / url.path, (SITE / page).parent).replace(os.sep, "/")
    result = result.removesuffix("index.html")
    if url.path.endswith("/") and not result.endswith("/"):
        result += "/"
    return (result or "./") + ("?" + url.query if url.query else "") + ("#" + url.fragment if url.fragment else "")


def navigation(page, language):
    def link(name, label, target, active=False):
        current = ' aria-current="page"' if active else ''
        return f'<a data-shell-link="{name}" href="{html.escape(relative(target, page), quote=True)}"' \
               f'{current}>{html.escape(label)}</a>'

    home = "zh-CN/index.html" if language == "zh" else "index.html"
    links = [link("home", "官网" if language == "zh" else "Home", home),
             link("components", "文档" if language == "zh" else "Documentation", documentation_entry(language), True)]
    return f'''<header class="documentation-header"><div class="documentation-header-inner">
      <a class="documentation-brand" data-shell-link="brand" href="{relative(home, page)}"><img src="{relative('assets/app-icon-64.png', page)}" alt="" width="30" height="30"><span>Fluent-Qt</span></a>
      <nav class="documentation-nav" aria-label="{"网站导航" if language == "zh" else "Site navigation"}">{''.join(links)}</nav>
    </div></header>'''


def breadcrumb(page, language, title=None):
    home = relative("zh-CN/index.html" if language == "zh" else "index.html", page)
    directory = relative(documentation_entry(language), page)
    label = "文档" if language == "zh" else "Documentation"
    current = f'<a href="{directory}">{label}</a><span aria-hidden="true">/</span><span aria-current="page">{html.escape(title)}</span>' if title else f'<span aria-current="page">{label}</span>'
    return f'<nav class="documentation-breadcrumb" aria-label="{"当前位置" if language == "zh" else "Breadcrumb"}"><a href="{home}">{"官网" if language == "zh" else "Home"}</a><span aria-hidden="true">/</span>{current}</nav>'
