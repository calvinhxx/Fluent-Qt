"""Incrementally rebuild C++ previews, retaining the running UI on build failure."""

from __future__ import annotations

import os
from contextlib import contextmanager
from pathlib import Path
import signal
import subprocess
import time

from fluent_qt_preview_client import PreviewClient, write_json


EXTENSIONS = {".cpp", ".c", ".cc", ".h", ".hpp", ".ui", ".qrc", ".qml", ".cmake",
              ".png", ".svg", ".jpg", ".json", ".ttf", ".otf"}
IGNORED_DIRECTORIES = {".git", "__pycache__", "node_modules", "CMakeFiles"}


def fingerprint(paths):
    result = {}

    def selected_file(name):
        return os.path.splitext(name)[1].lower() in EXTENSIONS or name == "CMakeLists.txt"

    def scan(directory):
        try:
            with os.scandir(directory) as entries:
                for entry in entries:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            if entry.name not in IGNORED_DIRECTORIES:
                                scan(entry.path)
                        elif selected_file(entry.name) and entry.is_file():
                            metadata = entry.stat()
                            result[entry.path] = (metadata.st_mtime_ns, metadata.st_size, metadata.st_ino)
                    except FileNotFoundError:
                        pass  # An editor may replace a file atomically during the scan.
        except FileNotFoundError:
            pass

    for selected in paths:
        selected = Path(selected).expanduser().resolve()
        if selected.is_dir():
            scan(selected)
        elif selected_file(selected.name):
            try:
                metadata = selected.stat()
                result[str(selected)] = (metadata.st_mtime_ns, metadata.st_size, metadata.st_ino)
            except FileNotFoundError:
                pass
    return result


@contextmanager
def watcher_lock(directory):
    """Keep one watcher per directory; the OS releases the lock after a crash."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    with (directory / "watcher.lock").open("a+b") as stream:
        stream.seek(0, os.SEEK_END)
        if stream.tell() == 0:
            stream.write(b"\0")
            stream.flush()
        stream.seek(0)
        try:
            if os.name == "posix":
                import fcntl
                fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
            elif os.name == "nt":
                import msvcrt
                msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
            else:
                raise RuntimeError("Preview watch locking is unsupported on this platform.")
        except OSError as error:
            raise RuntimeError("Another watcher owns this session directory.") from error
        try:
            yield
        finally:
            stream.seek(0)
            if os.name == "posix":
                fcntl.flock(stream, fcntl.LOCK_UN)
            else:
                msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


def stop_process(process, *, group=False):
    if process is None or process.poll() is not None:
        return
    if group and os.name == "posix":
        os.killpg(process.pid, signal.SIGTERM)
    else:
        process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        if group and os.name == "posix":
            os.killpg(process.pid, signal.SIGKILL)
        else:
            process.kill()
        process.wait(timeout=5)


def run_watch(command, build_command, directory, paths, project_root, *,
              poll_interval=1.0, ready_timeout=60.0):
    with watcher_lock(directory):
        return _run_watch(command, build_command, directory, paths, project_root,
                          poll_interval=poll_interval, ready_timeout=ready_timeout)


def _run_watch(command, build_command, directory, paths, project_root, *,
               poll_interval, ready_timeout):
    client = PreviewClient(directory)
    # The host's QLockFile is authoritative. A slow inspect reply must never be
    # interpreted as proof that a session directory is available.
    child = build = None
    owned_session = False
    failure = ""
    generation = 1
    snapshot = fingerprint(paths)
    pending_at = None

    def status(state, **extra):
        write_json(client.directory / "watch.json", {"state": state, "generation": generation, **extra})
        print("[preview] " + state + (": " + str(extra) if extra else ""), flush=True)

    try:
        previous = None
        try:
            previous = client.metadata()["session_id"]
        except RuntimeError:
            pass
        child = subprocess.Popen(command, cwd=project_root)
        client.wait_ready(timeout=ready_timeout, after=previous, process=child)
        owned_session = True
        status("ready", session_dir=str(client.directory))
        next_scan = time.monotonic()
        while child.poll() is None:
            now = time.monotonic()
            if now >= next_scan:
                changed = fingerprint(paths)
                next_scan = time.monotonic() + poll_interval
                if changed != snapshot:
                    snapshot = changed
                    pending_at = time.monotonic()
            if build is not None and build.poll() is not None:
                result = build.returncode
                build = None
                if result != 0:
                    status("build_failed", exit_code=result, previous_preview_retained=True)
                else:
                    metadata = client.metadata()
                    try:
                        state = client.request("capture_state")
                    except RuntimeError:
                        state = {"widgets": []}
                    stop_process(child)
                    child = subprocess.Popen(command, cwd=project_root)
                    client.wait_ready(timeout=ready_timeout, after=metadata["session_id"], process=child)
                    restored = client.request("restore_state", state)
                    generation += 1
                    status("ready", restored_values=restored["restored"])
            if build is None and pending_at is not None and time.monotonic() - pending_at >= 0.35:
                pending_at = None
                status("building", previous_preview_retained=True)
                build = subprocess.Popen(build_command, cwd=project_root,
                                         start_new_session=(os.name == "posix"))
            time.sleep(0.1)
        return child.returncode
    except KeyboardInterrupt:
        return 130
    except (OSError, RuntimeError, ValueError) as error:
        failure = str(error)
        raise
    finally:
        stop_process(build, group=True)
        stop_process(child)
        try:
            metadata = client.metadata()
            if child is not None and metadata.get("pid") == child.pid:
                write_json(client.directory / "session.json", {**metadata, "status": "closed"})
        except (OSError, RuntimeError, ValueError):
            pass
        if owned_session:
            status("stopped", **({"error": failure} if failure else {}))
