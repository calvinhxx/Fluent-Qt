#include "ParticlesSamples.h"
#include <QMarginsF>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <FluentQt/Particles.h>
#include "components/basicinput/Button.h"
#include "components/basicinput/ComboBox.h"
#include "components/basicinput/Slider.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {
using particles::ParticleBackdrop;
using basicinput::Button;
using basicinput::ComboBox;
using basicinput::Slider;
using textfields::Label;
using samples::makeSample;

QVector<GallerySample> particleBackdropSamples()
{
    return {
        makeSample(
            QStringLiteral("particle-backdrop-basic"), QStringLiteral("Flowing ribbons"),
            QStringLiteral("A quiet animated surface follows the current theme and motion preference."),
            QStringLiteral(
                "auto* backdrop = new ParticleBackdrop(this);\n"
                "backdrop->setEffect(ParticleBackdrop::FlowingRibbons);\n"
                "backdrop->setBackgroundMode(ParticleBackdrop::Solid);\n"
                "backdrop->setParticleCount(240);\n"
                "backdrop->setMaximumFrameRate(30);\n"
                "backdrop->setMinimumHeight(240);\n"
                "backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);\n"),
            [](QWidget* parent) {
                auto* backdrop = new ParticleBackdrop(parent);
                backdrop->setEffect(ParticleBackdrop::FlowingRibbons);
                backdrop->setBackgroundMode(ParticleBackdrop::Solid);
                backdrop->setParticleCount(240);
                backdrop->setMaximumFrameRate(30);
                backdrop->setMinimumHeight(240);
                backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                return backdrop;
            },
            true),
        makeSample(
            QStringLiteral("particle-backdrop-content"), QStringLiteral("Floating dots behind content"),
            QStringLiteral("Soft drifting lights leave room for content. Native controls retain "
                           "their normal input behavior."),
            QStringLiteral(
                "auto* backdrop = new ParticleBackdrop(this);\n"
                "backdrop->setEffect(ParticleBackdrop::FloatingDots);\n"
                "backdrop->setBackgroundMode(ParticleBackdrop::Solid);\n"
                "backdrop->setMaximumFrameRate(30);\n"
                "backdrop->setFadeMargins(QMarginsF(260, 0, 0, 0));\n"
                "backdrop->setMinimumHeight(280);\n"
                "backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);\n"
                "auto* layout = new QVBoxLayout(backdrop);\n"
                "layout->setContentsMargins(24, 24, 24, 24);\n"
                "auto* title = new Label(QStringLiteral(\"Make room for a little motion\"), "
                "backdrop);\n"
                "title->setFluentTypography(Typography::FontRole::Subtitle);\n"
                "title->setWordWrap(true);\n"
                "layout->addWidget(title);\n"
                "layout->addStretch();\n"
                "auto* ripple = new Button(QStringLiteral(\"Create a ripple\"), backdrop);\n"
                "ripple->setObjectName(QStringLiteral(\"particleRipple\"));\n"
                "ripple->setFluentStyle(Button::Accent);\n"
                "layout->addWidget(ripple, 0, Qt::AlignLeft);\n"
                "QObject::connect(ripple, &Button::clicked, backdrop, [backdrop] {\n"
                "    backdrop->triggerRipple(QPointF(backdrop->width() * .72, backdrop->height() * "
                ".42));\n"
                "});\n"),
            [](QWidget* parent) {
                auto* backdrop = new ParticleBackdrop(parent);
                backdrop->setEffect(ParticleBackdrop::FloatingDots);
                backdrop->setBackgroundMode(ParticleBackdrop::Solid);
                backdrop->setMaximumFrameRate(30);
                backdrop->setFadeMargins(QMarginsF(260, 0, 0, 0));
                backdrop->setMinimumHeight(280);
                backdrop->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                auto* layout = new QVBoxLayout(backdrop);
                layout->setContentsMargins(24, 24, 24, 24);
                auto* title = new Label(QStringLiteral("Make room for a little motion"), backdrop);
                title->setFluentTypography(Typography::FontRole::Subtitle);
                title->setWordWrap(true);
                layout->addWidget(title);
                layout->addStretch();
                auto* ripple = new Button(QStringLiteral("Create a ripple"), backdrop);
                ripple->setObjectName(QStringLiteral("particleRipple"));
                ripple->setFluentStyle(Button::Accent);
                layout->addWidget(ripple, 0, Qt::AlignLeft);
                QObject::connect(ripple, &Button::clicked, backdrop, [backdrop] {
                    backdrop->triggerRipple(QPointF(backdrop->width() * .72, backdrop->height() * .42));
                });
                return backdrop;
            },
            true),
        makeSample(
            QStringLiteral("particle-backdrop-interaction"), QStringLiteral("Explore particle effects"),
            QStringLiteral("Switch between ribbons, floating dots and a starfield. Move the pointer, "
                           "create a ripple, adjust the speed or pause any effect."),
            QStringLiteral(
                "auto* panel = new QWidget(this);\n"
                "panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);\n"
                "auto* layout = new QVBoxLayout(panel);\n"
                "layout->setContentsMargins(0, 0, 0, 0);\n"
                "layout->setSpacing(12);\n"
                "auto* backdrop = new ParticleBackdrop(panel);\n"
                "backdrop->setEffect(ParticleBackdrop::Starfield);\n"
                "backdrop->setBackgroundMode(ParticleBackdrop::Solid);\n"
                "backdrop->setInteractive(true);\n"
                "backdrop->setObjectName(QStringLiteral(\"particleBackdrop\"));\n"
                "backdrop->setMinimumHeight(300);\n"
                "layout->addWidget(backdrop);\n"
                "auto* effect = new ComboBox(panel);\n"
                "effect->setObjectName(QStringLiteral(\"particleEffect\"));\n"
                "effect->setAccessibleName(QStringLiteral(\"Particle effect\"));\n"
                "effect->addItem(QStringLiteral(\"Flowing ribbons\"), "
                "int(ParticleBackdrop::FlowingRibbons));\n"
                "effect->addItem(QStringLiteral(\"Floating dots\"), "
                "int(ParticleBackdrop::FloatingDots));\n"
                "effect->addItem(QStringLiteral(\"Starfield\"), "
                "int(ParticleBackdrop::Starfield));\n"
                "effect->setCurrentIndex(2);\n"
                "layout->addWidget(effect);\n"
                "QObject::connect(effect, qOverload<int>(&ComboBox::currentIndexChanged), "
                "backdrop,\n"
                "                 [backdrop, effect](int) {\n"
                "    "
                "backdrop->setEffect(static_cast<ParticleBackdrop::Effect>(effect->currentData()."
                "toInt()));\n"
                "});\n"
                "auto* speed = new Slider(Qt::Horizontal, panel);\n"
                "speed->setObjectName(QStringLiteral(\"particleSpeed\"));\n"
                "speed->setRange(25, 200);\n"
                "speed->setValue(100);\n"
                "speed->setAccessibleName(QStringLiteral(\"Particle speed\"));\n"
                "layout->addWidget(speed);\n"
                "QObject::connect(speed, &QSlider::valueChanged, backdrop,\n"
                "                 [backdrop](int value) { backdrop->setSpeed(value / 100.0); });\n"
                "auto* pause = new Button(QStringLiteral(\"Pause motion\"), panel);\n"
                "pause->setObjectName(QStringLiteral(\"particlePause\"));\n"
                "layout->addWidget(pause, 0, Qt::AlignLeft);\n"
                "QObject::connect(pause, &Button::clicked, backdrop, [backdrop, pause] {\n"
                "    backdrop->setAnimationEnabled(!backdrop->isAnimationEnabled());\n"
                "    pause->setText(backdrop->isAnimationEnabled() ? QStringLiteral(\"Pause "
                "motion\")\n"
                "                                                 : QStringLiteral(\"Resume "
                "motion\"));\n"
                "});\n"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                panel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
                auto* layout = new QVBoxLayout(panel);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(12);
                auto* backdrop = new ParticleBackdrop(panel);
                backdrop->setEffect(ParticleBackdrop::Starfield);
                backdrop->setBackgroundMode(ParticleBackdrop::Solid);
                backdrop->setInteractive(true);
                backdrop->setObjectName(QStringLiteral("particleBackdrop"));
                backdrop->setMinimumHeight(300);
                layout->addWidget(backdrop);
                auto* effect = new ComboBox(panel);
                effect->setObjectName(QStringLiteral("particleEffect"));
                effect->setAccessibleName(QStringLiteral("Particle effect"));
                effect->addItem(QStringLiteral("Flowing ribbons"), int(ParticleBackdrop::FlowingRibbons));
                effect->addItem(QStringLiteral("Floating dots"), int(ParticleBackdrop::FloatingDots));
                effect->addItem(QStringLiteral("Starfield"), int(ParticleBackdrop::Starfield));
                effect->setCurrentIndex(2);
                layout->addWidget(effect);
                QObject::connect(effect, qOverload<int>(&ComboBox::currentIndexChanged), backdrop,
                    [backdrop, effect](int) {
                        backdrop->setEffect(static_cast<ParticleBackdrop::Effect>(effect->currentData().toInt()));
                    });
                auto* speed = new Slider(Qt::Horizontal, panel);
                speed->setObjectName(QStringLiteral("particleSpeed"));
                speed->setRange(25, 200);
                speed->setValue(100);
                speed->setAccessibleName(QStringLiteral("Particle speed"));
                layout->addWidget(speed);
                QObject::connect(speed, &QSlider::valueChanged, backdrop, [backdrop](int value) {
                    backdrop->setSpeed(value / 100.0);
                });
                auto* pause = new Button(QStringLiteral("Pause motion"), panel);
                pause->setObjectName(QStringLiteral("particlePause"));
                layout->addWidget(pause, 0, Qt::AlignLeft);
                QObject::connect(pause, &Button::clicked, backdrop, [backdrop, pause] {
                    backdrop->setAnimationEnabled(!backdrop->isAnimationEnabled());
                    pause->setText(backdrop->isAnimationEnabled()
                            ? QStringLiteral("Pause motion")
                            : QStringLiteral("Resume motion"));
                });
                return panel;
            },
            true)};
}

} // namespace
QVector<GallerySample> particlesSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("particle-backdrop"))
        return particleBackdropSamples();
    return {};
}
} // namespace fluent::gallery
