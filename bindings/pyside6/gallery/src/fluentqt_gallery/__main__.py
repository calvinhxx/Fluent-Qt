"""Run ``python -m fluentqt_gallery``."""

from __future__ import annotations

import fluentqt

fluentqt.prepare_high_dpi_application()

from .app import main


if __name__ == "__main__":
    raise SystemExit(main())
