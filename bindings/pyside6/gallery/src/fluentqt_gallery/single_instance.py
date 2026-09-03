"""Per-user single-instance coordination for the standalone Gallery app."""

from __future__ import annotations

from enum import IntEnum
import hashlib
import sys

import shiboken6
from PySide6.QtCore import (
    QByteArray,
    QDir,
    QElapsedTimer,
    QIODevice,
    QLockFile,
    QObject,
    QStandardPaths,
    QThread,
    QTimer,
    Signal,
)
from PySide6.QtNetwork import QLocalServer, QLocalSocket


_ACTIVATE_COMMAND = b"activate/1\n"
_ACTIVATED_REPLY = b"activated/1\n"
_EXISTING_INSTANCE_TIMEOUT_MS = 2000
_CONNECT_ATTEMPT_MS = 100
_CONNECT_RETRY_DELAY_MS = 20
_DISCONNECTED_SOCKET_DRAIN_MS = 20
_MAXIMUM_COMMAND_BYTES = 256


class StartResult(IntEnum):
    Primary = 0
    ExistingInstanceNotified = 1
    Error = 2


def _runtime_directory() -> str:
    path = QStandardPaths.writableLocation(
        QStandardPaths.StandardLocation.AppLocalDataLocation
    )
    if not path and QDir.homePath():
        path = QDir(QDir.homePath()).filePath(".fluent-qt")
    if not path:
        path = QDir.tempPath()
    return QDir(path).filePath("single-instance")


def _scoped_instance_name(application_id: str) -> str:
    user_scope = QStandardPaths.writableLocation(
        QStandardPaths.StandardLocation.AppLocalDataLocation
    )
    if not user_scope:
        user_scope = QDir.homePath()
    digest = hashlib.sha256(
        application_id.encode("utf-8")
        + b"\n"
        + user_scope.encode("utf-8")
    ).hexdigest()[:24]
    return "fluent-qt-gallery-{0}".format(digest)


def _allow_set_foreground_window(process_id: int) -> None:
    """Hand foreground permission to an existing Windows process."""

    if sys.platform != "win32":
        return
    try:
        import ctypes

        user32 = ctypes.WinDLL("user32", use_last_error=True)
        allow_foreground = user32.AllowSetForegroundWindow
        allow_foreground.argtypes = [ctypes.c_uint32]
        allow_foreground.restype = ctypes.c_int
        allow_foreground(process_id)
    except (AttributeError, OSError):
        return


def _allow_owner_foreground_activation(lock_file: QLockFile | None) -> None:
    if sys.platform != "win32" or lock_file is None:
        return
    try:
        owner_process_id, _host_name, _application_name = (
            lock_file.getLockInfo()
        )
        owner_process_id = int(owner_process_id)
    except (RuntimeError, TypeError, ValueError):
        return
    if owner_process_id <= 0 or owner_process_id > 0xFFFFFFFF:
        return
    _allow_set_foreground_window(owner_process_id)


class GallerySingleInstance(QObject):
    """Own one Gallery process per user and forward activation requests."""

    activationRequested = Signal()

    def __init__(
        self,
        application_id: str,
        parent: QObject | None = None,
        *,
        runtime_directory: str | None = None,
    ) -> None:
        super().__init__(parent)
        self.setObjectName("gallerySingleInstance")
        self._application_id = str(application_id).strip()
        self._runtime_directory = (
            str(runtime_directory) if runtime_directory is not None else ""
        )
        self._server_name = ""
        self._lock_file_path = ""
        self._error_string = ""
        self._lock_file: QLockFile | None = None
        self._server: QLocalServer | None = None
        self._accepted_sockets: dict[int, QLocalSocket] = {}
        self._start_result = StartResult.Error
        self._started = False

    @property
    def is_primary(self) -> bool:
        return self._start_result == StartResult.Primary

    @property
    def server_name(self) -> str:
        return self._server_name

    @property
    def error_string(self) -> str:
        return self._error_string

    def start(self) -> StartResult:
        if self._started:
            return self._start_result
        self._started = True
        if not self._application_id:
            self._error_string = "The Gallery application ID is empty."
            return self._start_result

        runtime_path = self._runtime_directory or _runtime_directory()
        if not runtime_path or not QDir().mkpath(runtime_path):
            self._error_string = (
                "Cannot create the Gallery runtime directory: {0}".format(
                    runtime_path
                )
            )
            return self._start_result

        self._server_name = _scoped_instance_name(self._application_id)
        self._lock_file_path = QDir(runtime_path).filePath(
            self._server_name + ".lock"
        )
        lock_file = QLockFile(self._lock_file_path)
        lock_file.setStaleLockTime(0)
        self._lock_file = lock_file

        if not lock_file.tryLock(0):
            _allow_owner_foreground_activation(lock_file)
            if self._notify_existing_instance(
                _EXISTING_INSTANCE_TIMEOUT_MS
            ):
                self._start_result = StartResult.ExistingInstanceNotified
                return self._start_result
            # The owner may have exited during the connection retry window.
            if not lock_file.tryLock(0):
                self._error_string = (
                    "Another Gallery instance owns the lock, but its "
                    "activation endpoint is unavailable."
                )
                return self._start_result

        # Only the lock owner removes a stale endpoint, matching the C++ app.
        QLocalServer.removeServer(self._server_name)
        server = QLocalServer(self)
        server.setSocketOptions(
            QLocalServer.SocketOption.UserAccessOption
        )
        server.newConnection.connect(self._accept_pending_connections)
        if not server.listen(self._server_name):
            self._error_string = (
                "Cannot listen on the Gallery activation endpoint: {0}".format(
                    server.errorString()
                )
            )
            server.deleteLater()
            self._release_primary_ownership()
            return self._start_result
        self._server = server
        self._start_result = StartResult.Primary
        return self._start_result

    def close(self) -> None:
        self._release_primary_ownership()

    def _notify_existing_instance(self, timeout_ms: int) -> bool:
        timer = QElapsedTimer()
        timer.start()
        deadline = max(1, int(timeout_ms))
        while timer.elapsed() < deadline:
            socket = QLocalSocket()
            socket.connectToServer(
                self._server_name, QIODevice.OpenModeFlag.ReadWrite
            )
            remaining = deadline - timer.elapsed()
            if socket.waitForConnected(
                min(_CONNECT_ATTEMPT_MS, max(1, remaining))
            ):
                written = socket.write(_ACTIVATE_COMMAND)
                socket.flush()
                delivered = written == len(_ACTIVATE_COMMAND)
                if delivered and socket.bytesToWrite() > 0:
                    socket.waitForBytesWritten(
                        min(100, max(1, deadline - timer.elapsed()))
                    )
                reply_remaining = min(100, deadline - timer.elapsed())
                acknowledged = delivered and (
                    socket.bytesAvailable() > 0
                    or socket.waitForReadyRead(max(1, reply_remaining))
                ) and bytes(socket.readAll()).startswith(_ACTIVATED_REPLY)
                socket.disconnectFromServer()
                # A flushed command means a live local endpoint accepted the
                # activation request.  The reply is a best-effort confirmation:
                # on Windows, PySide's named-pipe wrapper can deliver the
                # command while delaying readyRead until disconnection.
                if acknowledged or delivered:
                    return True
            socket.abort()
            QThread.msleep(_CONNECT_RETRY_DELAY_MS)
        return False

    def _accept_pending_connections(self) -> None:
        server = self._server
        if server is None:
            return
        while server.hasPendingConnections():
            socket = server.nextPendingConnection()
            if socket is None:
                continue
            socket.setParent(self)
            self._accepted_sockets[id(socket)] = socket
            socket.readyRead.connect(
                lambda current=socket: self._process_socket(current)
            )
            socket.disconnected.connect(
                lambda current=socket: self._socket_disconnected(current)
            )
            # Drain once immediately: on Windows named pipes the peer can
            # disconnect before PySide delivers the queued readyRead callback.
            self._process_socket(socket)

    def _socket_disconnected(self, socket: QLocalSocket) -> None:
        if not shiboken6.isValid(socket):
            self._accepted_sockets.pop(id(socket), None)
            return
        self._process_socket(socket)
        if not shiboken6.isValid(socket):
            self._accepted_sockets.pop(id(socket), None)
            return
        if socket.property("galleryInstanceCleanupScheduled"):
            return
        socket.setProperty("galleryInstanceCleanupScheduled", True)
        # Keep the wrapper and native socket alive for one short drain window.
        # Windows ARM64 can queue readyRead after disconnected; deleting here
        # would make that callback observe an already-destroyed C++ object.
        QTimer.singleShot(
            _DISCONNECTED_SOCKET_DRAIN_MS,
            lambda current=socket: self._finish_disconnected_socket(current),
        )

    def _finish_disconnected_socket(self, socket: QLocalSocket) -> None:
        if shiboken6.isValid(socket):
            self._process_socket(socket)
        self._accepted_sockets.pop(id(socket), None)
        if shiboken6.isValid(socket):
            socket.deleteLater()

    def _process_socket(self, socket: QLocalSocket) -> None:
        if not shiboken6.isValid(socket):
            self._accepted_sockets.pop(id(socket), None)
            return
        if socket.property("galleryInstanceHandled"):
            return
        previous = socket.property("galleryInstanceCommand")
        command = bytes(previous) if isinstance(previous, QByteArray) else b""
        command += bytes(socket.readAll())
        if len(command) > _MAXIMUM_COMMAND_BYTES:
            socket.setProperty("galleryInstanceHandled", True)
            socket.disconnectFromServer()
            return
        newline = command.find(b"\n")
        if newline < 0:
            socket.setProperty(
                "galleryInstanceCommand", QByteArray(command)
            )
            return

        socket.setProperty("galleryInstanceHandled", True)
        if command[:newline].strip() == b"activate/1":
            socket.write(_ACTIVATED_REPLY)
            socket.flush()
            self.activationRequested.emit()
            # The client owns connection shutdown after it has read (or timed
            # out waiting for) the best-effort acknowledgement.
            return
        socket.disconnectFromServer()

    def _release_primary_ownership(self) -> None:
        lock_file = self._lock_file
        if self._server is not None:
            self._server.close()
            self._server.deleteLater()
            self._server = None
        accepted_sockets = tuple(self._accepted_sockets.values())
        self._accepted_sockets.clear()
        for socket in accepted_sockets:
            if not shiboken6.isValid(socket):
                continue
            try:
                socket.readyRead.disconnect()
            except (RuntimeError, TypeError):
                pass
            try:
                socket.disconnected.disconnect()
            except (RuntimeError, TypeError):
                pass
            socket.abort()
            socket.deleteLater()
        if self._server_name and lock_file is not None and lock_file.isLocked():
            QLocalServer.removeServer(self._server_name)
        if lock_file is not None and lock_file.isLocked():
            lock_file.unlock()
        if self._start_result == StartResult.Primary:
            self._start_result = StartResult.Error


__all__ = ["GallerySingleInstance", "StartResult"]
