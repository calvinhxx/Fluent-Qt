"""Exercise automatic diagnostics and a real browser ZIP download."""
from __future__ import annotations

import json
import zipfile
from pathlib import Path

from wasm_gallery_ready import wait_for_gallery_ready


def run_diagnostics_contract(browser: object, base_url: str, artifact_dir: Path, *, theme: str | None = None) -> None:
    from playwright.sync_api import expect

    artifact_dir.mkdir(parents=True, exist_ok=True)
    page = browser.new_page(viewport={"width": 1280, "height": 1200}, device_scale_factor=2,
                            accept_downloads=True)
    errors = []
    page.on("pageerror", lambda error: errors.append(str(error)))

    def control():
        return page.locator('[id$=".galleryFeedbackButton"][aria-hidden="false"]')

    try:
        theme_query = f"&embed=site&host-theme={theme}" if theme else ""
        page.goto(f"{base_url}/app/index.html?route=text-edit&window-mode=maximized&wasm-smoke=interaction-state{theme_query}",
                  wait_until="domcontentloaded")
        wait_for_gallery_ready(page)
        page.locator("#qt-shadow-container .qt-window-a11y-container:visible "
                     "> button.hidden-visually-read-by-screen-reader").first.evaluate("button => button.click()")
        expect(control()).to_have_count(1)
        # Keep the real navigation URL, but do not contact or upload to GitHub during a test.
        page.context.route("https://github.com/calvinhxx/Fluent-Qt/issues/new**",
                           lambda route: route.fulfill(status=200, body="Issue form test destination"))
        control().focus()
        popup_events = []
        page.on("popup", lambda popup: popup_events.append(popup))
        # Download handoff is not proof of a saved file: cancellation must not
        # produce a filesystem-success claim or open GitHub automatically.
        with page.expect_download(timeout=20000) as cancelled:
            page.keyboard.press("Enter")
        cancelled.value.cancel()
        state = page.locator('[id$=".galleryFeedbackArtifactState"][aria-hidden="false"]')
        expect(state).to_have_text("Download started")
        assert not popup_events
        page.locator('[id$=".galleryFeedbackClose"][aria-hidden="false"]').focus()
        page.keyboard.press("Enter")
        expect(state).to_have_count(0)
        control().focus()
        with page.expect_download(timeout=20000) as download:
            page.keyboard.press("Enter")
        expect(state).to_have_text("Download started")
        action = page.locator('[id$=".galleryFeedbackAction"][aria-hidden="false"]')
        expect(action).to_have_count(1)
        expect(action).to_have_text("Open GitHub issue")
        assert not popup_events, "Preparing the report must not open a browser tab"
        page.screenshot(path=str(artifact_dir / "feedback-ready.png"))
        action.focus()
        with page.expect_popup(timeout=20000) as popup:
            page.keyboard.press("Enter")
        issue = popup.value
        issue.wait_for_load_state("domcontentloaded")
        from urllib.parse import parse_qs, urlparse
        fields = parse_qs(urlparse(issue.url).query)
        assert fields["template"] == ["bug_report.yml"]
        assert "evidence" in fields and "environment" in fields
        issue.close()
        path = artifact_dir / "fluentqt-report.zip"
        download.value.save_as(path)
        with zipfile.ZipFile(path) as archive:
            assert archive.testzip() is None
            report = json.loads(archive.read("report.json"))
        assert report["schema_version"] == 1
        assert report["recording"] is True
        assert report["log_messages_included"] is True
        assert report["environment"]["platform"] == "wasm"
        assert report["events"]
        assert len(report["events"]) <= report["limits"]["events"]
        assert report["session_id"]
        assert report["limits"]["session_ms"] == 0
        for snapshot in (report["initial"], report["final"]):
            assert len(snapshot["widgets"]) <= report["limits"]["widgets_per_snapshot"]
            assert all("text" not in widget and "object_name" not in widget for widget in snapshot["widgets"])
        page.mouse.move(1000, 1000)
        page.mouse.wheel(0, 1500)
        page.screenshot(path=str(artifact_dir / "report-preview.png"))
        expect(control()).to_be_enabled()
        assert report["gallery"]["last_content_route"] == "text-edit"
        assert report["crash_files_included"] == 0
        assert not errors, errors
        (artifact_dir / "result.json").write_text(json.dumps({
            "status": "pass", "checks": ["automatic-capture", "real-zip-download",
                "bounded-report", "text-exclusion", "prefilled-issue-navigation", "single-feedback-button",
                "anchored-feedback-navigation", "cancelled-download-status"], "page_errors": errors,
        }, indent=2) + "\n")
    except Exception as error:
        page.screenshot(path=str(artifact_dir / "failure.png"))
        (artifact_dir / "result.json").write_text(json.dumps({"status": "fail", "error": str(error),
                                                              "page_errors": errors}, indent=2) + "\n")
        raise
    finally:
        page.close()
