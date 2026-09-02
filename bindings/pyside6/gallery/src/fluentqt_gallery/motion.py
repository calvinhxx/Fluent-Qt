"""Motion-policy helpers for finite Gallery-owned transitions."""

from __future__ import annotations

from collections.abc import Callable
from dataclasses import dataclass
from typing import Any

import fluentqt
from PySide6.QtCore import QAbstractAnimation, QVariantAnimation


_settling_active_transitions = False


@dataclass
class _ActiveTransition:
    animation: QVariantAnimation
    full_duration_ms: int
    complete_disabled: Callable[[], None]
    deletion_policy: Any | None


_active_transitions: dict[int, _ActiveTransition] = {}


def _forget_transition(key: int) -> None:
    _active_transitions.pop(key, None)


def _track_transition(record: _ActiveTransition) -> None:
    animation = record.animation
    key = id(animation)
    _active_transitions[key] = record
    if getattr(animation, "_gallery_motion_tracking_connected", False):
        return
    animation._gallery_motion_tracking_connected = True
    animation.finished.connect(lambda key=key: _forget_transition(key))
    animation.destroyed.connect(
        lambda *_unused, key=key: _forget_transition(key)
    )


def _delete_later(animation: QVariantAnimation) -> None:
    try:
        animation.deleteLater()
    except RuntimeError:
        pass


def effective_motion_duration(full_duration_ms: int) -> int:
    """Return the policy-adjusted duration for a decorative transition."""

    duration = max(0, int(full_duration_ms))
    return fluentqt.motion_policy().resolvedDuration(duration)


def start_finite_transition(
    animation: QVariantAnimation,
    full_duration_ms: int,
    *,
    complete_disabled: Callable[[], None],
    deletion_policy: Any | None = None,
) -> bool:
    """Start a finite transition or synchronously settle Disabled motion."""

    animation.stop()
    duration = effective_motion_duration(full_duration_ms)
    if duration == 0:
        complete_disabled()
        if deletion_policy is not None:
            _delete_later(animation)
        return False

    animation.setDuration(duration)
    _track_transition(
        _ActiveTransition(
            animation,
            max(0, int(full_duration_ms)),
            complete_disabled,
            deletion_policy,
        )
    )
    if deletion_policy is None:
        animation.start()
    else:
        animation.start(deletion_policy)
    return True


def settle_active_transitions(mode=None) -> None:
    """Converge running Gallery transitions after a policy change."""

    global _settling_active_transitions
    if _settling_active_transitions:
        return

    policy = fluentqt.motion_policy()
    mode = policy.mode() if mode is None else fluentqt.MotionMode(mode)

    _settling_active_transitions = True
    try:
        for key, record in tuple(_active_transitions.items()):
            if _active_transitions.get(key) is not record:
                continue
            animation = record.animation
            try:
                if animation.state() == QAbstractAnimation.State.Stopped:
                    _forget_transition(key)
                    continue
                if mode == fluentqt.MotionMode.Full:
                    animation.setDuration(record.full_duration_ms)
                    continue
                if mode == fluentqt.MotionMode.Disabled:
                    _forget_transition(key)
                    animation.stop()
                    record.complete_disabled()
                    if record.deletion_policy is not None:
                        _delete_later(animation)
                    continue

                reduced_remaining = policy.resolvedDuration(
                    record.full_duration_ms
                )
                remaining = max(0, animation.duration() - animation.currentTime())
                if remaining > reduced_remaining:
                    animation.setDuration(
                        animation.currentTime()
                        + reduced_remaining
                    )
            except RuntimeError:
                _forget_transition(key)
    finally:
        _settling_active_transitions = False


fluentqt.motion_policy().modeChanged.connect(settle_active_transitions)


__all__ = [
    "effective_motion_duration",
    "settle_active_transitions",
    "start_finite_transition",
]
