"""Executable Python counterparts of the particle backdrop samples."""

from textwrap import dedent

from .native_samples import register_source_samples


def _script(body: str) -> str:
    return (
        "import fluentqt\n"
        "from PySide6.QtCore import QMarginsF, QPointF, Qt\n"
        "from PySide6.QtWidgets import QSizePolicy, QVBoxLayout, QWidget\n\n"
        + dedent(body).strip() + "\n"
    )


register_source_samples(
    "particle-backdrop",
    ("ParticleBackdrop",),
    {
        "particle-backdrop-basic": (
            "backdrop",
            _script("""
                backdrop = fluentqt.ParticleBackdrop()
                backdrop.setEffect(fluentqt.ParticleBackdrop.Effect.FlowingRibbons)
                backdrop.setBackgroundMode(fluentqt.ParticleBackdrop.BackgroundMode.Solid)
                backdrop.setParticleCount(240)
                backdrop.setMaximumFrameRate(30)
                backdrop.setMinimumHeight(240)
                backdrop.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
            """),
        ),
        "particle-backdrop-content": (
            "backdrop",
            _script("""
                backdrop = fluentqt.ParticleBackdrop()
                backdrop.setEffect(fluentqt.ParticleBackdrop.Effect.FloatingDots)
                backdrop.setBackgroundMode(fluentqt.ParticleBackdrop.BackgroundMode.Solid)
                backdrop.setMaximumFrameRate(30)
                backdrop.setFadeMargins(QMarginsF(260, 0, 0, 0))
                backdrop.setMinimumHeight(280)
                backdrop.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
                layout = QVBoxLayout(backdrop)
                layout.setContentsMargins(24, 24, 24, 24)
                title = fluentqt.Label("Make room for a little motion", backdrop)
                title.setFluentTypography(fluentqt.FontRole.Subtitle)
                title.setWordWrap(True)
                layout.addWidget(title)
                layout.addStretch()
                ripple = fluentqt.Button("Create a ripple", backdrop)
                ripple.setObjectName("particleRipple")
                ripple.setFluentStyle(fluentqt.Button.ButtonStyle.Accent)
                layout.addWidget(ripple, 0, Qt.AlignLeft)
                ripple.clicked.connect(lambda: backdrop.triggerRipple(
                    QPointF(backdrop.width() * .72, backdrop.height() * .42)
                ))
            """),
        ),
        "particle-backdrop-interaction": (
            "panel",
            _script("""
                panel = QWidget()
                panel.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
                layout = QVBoxLayout(panel)
                layout.setContentsMargins(0, 0, 0, 0)
                layout.setSpacing(12)
                backdrop = fluentqt.ParticleBackdrop(panel)
                backdrop.setEffect(fluentqt.ParticleBackdrop.Effect.Starfield)
                backdrop.setBackgroundMode(fluentqt.ParticleBackdrop.BackgroundMode.Solid)
                backdrop.setInteractive(True)
                backdrop.setObjectName("particleBackdrop")
                backdrop.setMinimumHeight(300)
                layout.addWidget(backdrop)
                effect = fluentqt.ComboBox(panel)
                effect.setObjectName("particleEffect")
                effect.setAccessibleName("Particle effect")
                effect.addItem("Flowing ribbons", fluentqt.ParticleBackdrop.Effect.FlowingRibbons)
                effect.addItem("Floating dots", fluentqt.ParticleBackdrop.Effect.FloatingDots)
                effect.addItem("Starfield", fluentqt.ParticleBackdrop.Effect.Starfield)
                effect.setCurrentIndex(2)
                layout.addWidget(effect)
                effect.currentIndexChanged.connect(
                    lambda _index: backdrop.setEffect(effect.currentData())
                )
                speed = fluentqt.Slider(Qt.Horizontal, panel)
                speed.setObjectName("particleSpeed")
                speed.setRange(25, 200)
                speed.setValue(100)
                speed.setAccessibleName("Particle speed")
                layout.addWidget(speed)
                speed.valueChanged.connect(lambda value: backdrop.setSpeed(value / 100.0))
                pause = fluentqt.Button("Pause motion", panel)
                pause.setObjectName("particlePause")
                layout.addWidget(pause, 0, Qt.AlignLeft)

                def toggle_motion():
                    backdrop.setAnimationEnabled(not backdrop.isAnimationEnabled())
                    pause.setText("Pause motion" if backdrop.isAnimationEnabled() else "Resume motion")

                pause.clicked.connect(toggle_motion)
            """),
        ),
    },
)
