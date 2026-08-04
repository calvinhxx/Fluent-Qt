"""GitHub release checker used by the Python Gallery settings page."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum
import json
import platform
import re

import fluentqt
from PySide6.QtCore import QObject, QUrl, Signal
from PySide6.QtNetwork import (
    QNetworkAccessManager,
    QNetworkReply,
    QNetworkRequest,
)


_LATEST_RELEASE_API = (
    "https://api.github.com/repos/calvinhxx/Fluent-Qt/releases/latest"
)


class UpdateStatus(Enum):
    UpToDate = "up-to-date"
    UpdateAvailable = "update-available"
    Error = "error"


@dataclass
class UpdateResult:
    status: UpdateStatus = UpdateStatus.Error
    current_version: str = ""
    latest_version: str = ""
    release_name: str = ""
    release_url: QUrl | None = None
    asset_url: QUrl | None = None
    asset_name: str = ""
    message: str = ""


def normalized_version(version: str) -> str:
    normalized = version.strip()
    if normalized[:1].lower() == "v":
        normalized = normalized[1:]
    normalized = normalized.split("-", 1)[0]
    match = re.match(r"^\d+(?:\.\d+){0,2}", normalized)
    return match.group(0) if match else ""


def compare_versions(left: str, right: str) -> int:
    def parts(value: str) -> list[int]:
        normalized = normalized_version(value)
        return [int(part) for part in normalized.split(".") if part]

    left_parts = parts(left)
    right_parts = parts(right)
    for index in range(3):
        left_value = left_parts[index] if index < len(left_parts) else 0
        right_value = right_parts[index] if index < len(right_parts) else 0
        if left_value != right_value:
            return -1 if left_value < right_value else 1
    return 0


def platform_key() -> str:
    machine = platform.machine().lower()
    arm64 = "arm" in machine or "aarch64" in machine
    system = platform.system()
    if system == "Windows":
        return "windows-arm64" if arm64 else "windows-x64"
    if system == "Darwin":
        return "macos-arm64" if arm64 else "macos-x64"
    if system == "Linux":
        return "linux-arm64" if arm64 else "linux-x64"
    return "release-page"


def platform_display_name() -> str:
    return {
        "windows-arm64": "Windows ARM64",
        "windows-x64": "Windows x64",
        "macos-arm64": "macOS Apple Silicon",
        "macos-x64": "macOS Intel",
        "linux-arm64": "Linux ARM64",
        "linux-x64": "Linux x64",
    }.get(platform_key(), "this platform")


def _asset_matches_platform(name: str, key: str) -> bool:
    folded = name.lower()
    if ".sha256" in folded:
        return False
    if key == "windows-arm64":
        return "windows-arm64" in folded and folded.endswith(".exe")
    if key == "windows-x64":
        return (
            "windows-x64" in folded
            and folded.endswith(".exe")
            and "qt5.15" not in folded
        )
    if key == "macos-arm64":
        return "darwin-arm64" in folded and folded.endswith(".dmg")
    if key == "macos-x64":
        return (
            "darwin-x86_64" in folded
            and folded.endswith(".dmg")
            and "qt5.15" not in folded
        )
    if key == "linux-arm64":
        return "linux-arm64" in folded and folded.endswith(".deb")
    if key == "linux-x64":
        return "linux-x86_64" in folded and folded.endswith(".deb")
    return False


class GalleryUpdateChecker(QObject):
    """Python equivalent of the C++ Gallery UpdateChecker."""

    checkStarted = Signal()
    checkFinished = Signal(object)

    def __init__(self, parent: QObject | None = None) -> None:
        super().__init__(parent)
        self._network = QNetworkAccessManager(self)
        self._reply: QNetworkReply | None = None

    def is_checking(self) -> bool:
        return self._reply is not None

    def current_version(self) -> str:
        version = str(fluentqt.__version__).strip()
        return version or "0.0.0"

    def platform_label(self) -> str:
        return platform_display_name()

    def check_for_updates(self) -> None:
        if self._reply is not None:
            return
        request = QNetworkRequest(QUrl(_LATEST_RELEASE_API))
        request.setHeader(
            QNetworkRequest.UserAgentHeader,
            "Fluent-Qt-Gallery/{0}".format(self.current_version()),
        )
        request.setRawHeader(b"Accept", b"application/vnd.github+json")
        request.setAttribute(
            QNetworkRequest.RedirectPolicyAttribute,
            QNetworkRequest.NoLessSafeRedirectPolicy,
        )
        self._reply = self._network.get(request)
        self._reply.finished.connect(self._handle_reply_finished)
        self.checkStarted.emit()

    def parse_release_payload(self, payload: bytes) -> UpdateResult:
        result = UpdateResult(current_version=self.current_version())
        try:
            release = json.loads(payload.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            result.message = "GitHub release response could not be parsed."
            return result
        if not isinstance(release, dict):
            result.message = "GitHub release response could not be parsed."
            return result

        result.latest_version = normalized_version(str(release.get("tag_name", "")))
        result.release_name = str(release.get("name", ""))
        result.release_url = QUrl(str(release.get("html_url", "")))
        if not result.latest_version or not result.release_url.isValid():
            result.message = "Latest release metadata is incomplete."
            return result

        if compare_versions(result.latest_version, result.current_version) <= 0:
            result.status = UpdateStatus.UpToDate
            result.message = "You are on the latest version ({0}).".format(
                result.current_version
            )
            return result

        key = platform_key()
        for asset in release.get("assets", []):
            if not isinstance(asset, dict):
                continue
            name = str(asset.get("name", ""))
            if not _asset_matches_platform(name, key):
                continue
            url = QUrl(str(asset.get("browser_download_url", "")))
            if url.isValid():
                result.asset_name = name
                result.asset_url = url
                break

        result.status = UpdateStatus.UpdateAvailable
        if result.asset_url is not None and result.asset_url.isValid():
            result.message = "Version {0} is available for {1}.".format(
                result.latest_version, self.platform_label()
            )
        else:
            result.message = (
                "Version {0} is available. Open the release page to download it."
            ).format(result.latest_version)
        return result

    def _handle_reply_finished(self) -> None:
        reply = self._reply
        self._reply = None
        if reply is None:
            return

        result = UpdateResult(current_version=self.current_version())
        status = reply.attribute(QNetworkRequest.HttpStatusCodeAttribute)
        http_status = int(status) if status is not None else 0
        if reply.error() != QNetworkReply.NoError:
            result.message = reply.errorString()
        elif http_status >= 400:
            result.message = (
                "GitHub release request failed with HTTP {0}.".format(http_status)
            )
        else:
            result = self.parse_release_payload(bytes(reply.readAll()))
        reply.deleteLater()
        self.checkFinished.emit(result)


__all__ = [
    "GalleryUpdateChecker",
    "UpdateResult",
    "UpdateStatus",
    "compare_versions",
    "normalized_version",
    "platform_display_name",
    "platform_key",
]
