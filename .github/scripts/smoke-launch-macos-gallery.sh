#!/usr/bin/env bash

set -euo pipefail

if (( $# < 1 || $# > 2 )); then
  echo "usage: $(basename "$0") <build-directory> [seconds]" >&2
  exit 2
fi

build_dir="$1"
duration="${2:-10}"
if [[ ! "$duration" =~ ^[1-9][0-9]*$ ]]; then
  echo "smoke duration must be a positive integer: $duration" >&2
  exit 2
fi

gallery="$build_dir/app/Fluent-Qt Gallery.app/Contents/MacOS/Fluent-Qt Gallery"
if [[ ! -x "$gallery" ]]; then
  echo "Gallery executable was not found: $gallery" >&2
  exit 1
fi

log_dir="${RUNNER_TEMP:-${TMPDIR:-/tmp}}"
log_path="$log_dir/fluent-qt-gallery-smoke.log"
: >"$log_path"
export SPDLOG_LEVEL="${SPDLOG_LEVEL:-debug}"

"$gallery" >"$log_path" 2>&1 &
pid=$!

cleanup() {
  if [[ -n "${pid:-}" ]] && kill -0 "$pid" 2>/dev/null; then
    kill "$pid" 2>/dev/null || true
    wait "$pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

for (( elapsed = 0; elapsed < duration; elapsed++ )); do
  sleep 1
  if ! kill -0 "$pid" 2>/dev/null; then
    set +e
    wait "$pid"
    status=$?
    set -e
    pid=""
    cat "$log_path" >&2
    echo "Gallery exited during the macOS smoke launch with status $status." >&2
    exit 1
  fi
done

if ! grep -q "GalleryContentPresenter prewarm stopped" "$log_path"; then
  cat "$log_path" >&2
  echo "Gallery did not complete its startup prewarm during the smoke launch." >&2
  exit 1
fi

echo "Gallery completed startup prewarm and remained alive for ${duration}s."
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
pid=""
