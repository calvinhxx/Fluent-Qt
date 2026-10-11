"""Source transactions and durable review history, independent of Qt."""

import json
from pathlib import Path
import uuid

from fluent_qt_preview_client import write_json
from .source import digest, replace_source


class EditHistory:
    def __init__(self, source, directory):
        self.source = Path(source).resolve()
        self.directory = Path(directory).resolve()
        self.directory.mkdir(parents=True, exist_ok=True)
        self.index = self.directory / "history.json"
        self.entries = []
        if self.index.exists():
            data = json.loads(self.index.read_text(encoding="utf-8"))
            if data.get("source") != str(self.source):
                raise ValueError("This history belongs to another scene.")
            self.entries = data["entries"]

    def save(self):
        write_json(self.index, {"source": str(self.source), "entries": self.entries})

    @property
    def pending(self):
        return next((entry for entry in reversed(self.entries) if entry["status"] == "pending"), None)

    @property
    def latest(self):
        return self.entries[-1] if self.entries else None

    @property
    def undoable(self):
        return next((entry for entry in reversed(self.entries) if entry["status"] == "accepted"), None)

    def begin(self, before, after, *, region, title, changes):
        if self.pending:
            raise RuntimeError("Keep or revert the current preview before applying another edit.")
        identity = uuid.uuid4().hex
        folder = self.directory / identity
        folder.mkdir()
        (folder / "before.py").write_bytes(before)
        (folder / "after.py").write_bytes(after)
        entry = {"id": identity, "region": region, "title": title, "changes": dict(changes),
                 "before_revision": digest(before), "after_revision": digest(after), "status": "pending"}
        # Persist recovery data before modifying the source file.
        self.entries.append(entry)
        self.save()
        try:
            replace_source(self.source, before, after)
        except BaseException:
            entry["status"] = "failed"
            self.save()
            raise
        return entry

    def contents(self, entry, version):
        identity = entry["id"]
        if len(identity) != 32 or any(char not in "0123456789abcdef" for char in identity):
            raise ValueError("Invalid history identity.")
        content = (self.directory / identity / (version + ".py")).read_bytes()
        if digest(content) != entry[version + "_revision"]:
            raise RuntimeError("History source snapshot has changed.")
        return content

    def accept(self):
        entry = self.pending
        if not entry:
            raise RuntimeError("There is no pending edit.")
        if self.source.read_bytes() != self.contents(entry, "after"):
            raise RuntimeError("Source changed outside this edit. End the comparison without overwriting it.")
        entry["status"] = "accepted"
        self.save()
        return entry

    def restore(self, *, undo=False):
        entry = self.undoable if undo and not self.pending else self.pending if not undo else None
        if not entry:
            raise RuntimeError("There is no matching edit to undo.")
        replace_source(self.source, self.contents(entry, "after"), self.contents(entry, "before"))
        entry["status"] = "undone" if undo else "reverted"
        self.save()
        return entry

    def dismiss(self):
        """End a conflicted comparison without writing the source."""
        if not self.pending:
            raise RuntimeError("There is no pending edit.")
        self.pending["status"] = "dismissed"
        self.save()
