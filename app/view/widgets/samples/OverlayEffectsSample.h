#ifndef FLUENTQT_GALLERY_OVERLAY_EFFECTS_SAMPLE_H
#define FLUENTQT_GALLERY_OVERLAY_EFFECTS_SAMPLE_H

#include <FluentQt/FluentQt.h>
#include <QVBoxLayout>
#include <type_traits>
#include "SampleBuilders.h"

namespace fluent::gallery::samples {

// All three examples exercise the same public facade and input workflow.
// zh_CN: 三种示例使用相同公共接口和交互流程。
template <class Surface> QWidget* overlayEffectsSample(QWidget* parent, const QString& prefix, const QString& opener)
{
    using basicinput::Button;
    using basicinput::CheckBox;
    using basicinput::Slider;
    using textfields::Label;
    auto* root = verticalGroup(parent, 12);
    auto* layout = qobject_cast<QBoxLayout*>(root->layout());
    auto* open = new Button(opener, root);
    open->setObjectName(prefix + QStringLiteral(".OpenEffects"));
    open->setFluentStyle(Button::Accent);
    layout->addWidget(open, 0, Qt::AlignLeft);
    auto* hint = new Label(QStringLiteral("Open the overlay to try this action."), root);
    hint->setWordWrap(true);
    hint->setTextColorRole(Label::TextColorRole::Secondary);
    layout->addWidget(hint);
    auto* row = horizontalGroup(root, 12);
    auto* counter = new Button(QStringLiteral("Background action"), row);
    counter->setObjectName(prefix + QStringLiteral(".Background"));
    row->layout()->addWidget(counter);
    auto* status = new Label(QStringLiteral("Background clicks: 0"), row);
    status->setObjectName(prefix + QStringLiteral(".Status"));
    status->setTextColorRole(Label::TextColorRole::Secondary);
    row->layout()->addWidget(status);
    layout->addWidget(row);
    QObject::connect(counter, &Button::clicked, status, [status, count = 0]() mutable {
        status->setText(QStringLiteral("Background clicks: %1").arg(++count));
    });
    QObject::connect(open, &Button::clicked, root, [root, open, counter, hint, prefix] {
        auto* overlay = new Surface(root);
        overlay->setObjectName(prefix + QStringLiteral(".Effects"));
        overlay->setModal(true);
        overlay->setDim(true);
        overlay->setDimOpacity(0.12);
        overlay->setBackdropBlurRadius(8);
        overlay->setClosePolicy(Surface::CloseOnEscape);
        QWidget* body = overlay;
        if constexpr (std::is_same_v<Surface, collections::DrawerView>) {
            overlay->setEdge(collections::DrawerView::DrawerEdge::Right);
            overlay->setDrawerLength(320);
            overlay->setInteractive(false);
            body = new QWidget;
            overlay->setContentWidget(body, WidgetOwnership::Owned);
        } else {
            overlay->setThemeSource(open);
            overlay->setFixedWidth(400);
        }
        auto* content = new QVBoxLayout(body);
        content->setContentsMargins(28, 26, 28, 26);
        content->setSpacing(10);
        auto* title = new Label(QStringLiteral("Modal and backdrop effects"), body);
        title->setFluentTypography(Typography::FontRole::BodyStrong);
        title->setWordWrap(true);
        content->addWidget(title);
        auto* modal = new CheckBox(QStringLiteral("Modal"), body);
        modal->setObjectName(prefix + QStringLiteral(".Modal"));
        modal->setChecked(true);
        QObject::connect(modal, &QCheckBox::toggled, overlay, &Surface::setModal);
        const auto explainModal = [hint](bool enabled) {
            hint->setText(enabled
                    ? QStringLiteral("Modal on: background blocked.")
                    : QStringLiteral("Modal off: background available."));
        };
        QObject::connect(modal, &QCheckBox::toggled, hint, explainModal);
        explainModal(true);
        content->addWidget(modal);
        auto* dimEnabled = new CheckBox(QStringLiteral("Dim background"), body);
        dimEnabled->setObjectName(prefix + QStringLiteral(".DimEnabled"));
        dimEnabled->setChecked(true);
        QObject::connect(dimEnabled, &QCheckBox::toggled, overlay, &Surface::setDim);
        content->addWidget(dimEnabled);
        auto* outside = new CheckBox(QStringLiteral("Close on outside click"), body);
        outside->setObjectName(prefix + QStringLiteral(".CloseOutside"));
        QObject::connect(outside, &QCheckBox::toggled, overlay, [overlay](bool enabled) {
            typename Surface::ClosePolicy policy(Surface::CloseOnEscape);
            if (enabled)
                policy |= Surface::CloseOnPressOutside;
            overlay->setClosePolicy(policy);
        });
        content->addWidget(outside);
        auto* blurLabel = new Label(QStringLiteral("Blur: 8 px"), body);
        auto* blur = new Slider(Qt::Horizontal, body);
        blur->setObjectName(prefix + QStringLiteral(".Blur"));
        blur->setAccessibleName(QStringLiteral("Background blur radius"));
        blur->setRange(0, 24);
        blur->setValue(8);
        QObject::connect(blur, &QSlider::valueChanged, overlay, [overlay, blurLabel](int value) {
            overlay->setBackdropBlurRadius(value);
            blurLabel->setText(QStringLiteral("Blur: %1 px").arg(value));
        });
        content->addWidget(blurLabel);
        content->addWidget(blur);
        auto* dimLabel = new Label(QStringLiteral("Dim: 12%"), body);
        auto* dim = new Slider(Qt::Horizontal, body);
        dim->setObjectName(prefix + QStringLiteral(".Dim"));
        dim->setAccessibleName(QStringLiteral("Background dim opacity"));
        dim->setRange(0, 60);
        dim->setValue(12);
        QObject::connect(dim, &QSlider::valueChanged, overlay, [overlay, dimLabel](int value) {
            overlay->setDimOpacity(value / 100.0);
            dimLabel->setText(QStringLiteral("Dim: %1%").arg(value));
        });
        content->addWidget(dimLabel);
        content->addWidget(dim);
        auto* close = new Button(QStringLiteral("Close"), body);
        close->setObjectName(prefix + QStringLiteral(".Close"));
        QObject::connect(close, &Button::clicked, overlay, [overlay] { overlay->setIsOpen(false); });
        content->addWidget(close, 0, Qt::AlignRight);
        if constexpr (std::is_same_v<Surface, collections::DrawerView>)
            content->addStretch();
        QObject::connect(counter, &Button::clicked, overlay, &Surface::refreshBackdrop);
        QObject::connect(overlay, &Surface::closed, hint, [hint] {
            hint->setText(QStringLiteral("Open the overlay to try this action."));
        });
        QObject::connect(overlay, &Surface::closed, overlay, &QObject::deleteLater);
        overlay->open();
    });
    return root;
}

inline void overlayEffectsSourceFiles(GallerySample& sample, const QString& type)
{
    sample.usageSnippet = QStringLiteral(
        "// Full example includes the controls and background action.\n"
        "auto* overlay = new %1(this);\n"
        "overlay->setModal(true);\n"
        "overlay->setDim(true);\n"
        "overlay->setDimOpacity(0.12);\n"
        "overlay->setBackdropBlurRadius(8);\n"
        "overlay->setClosePolicy(%1::CloseOnEscape);\n"
        "overlay->open();\n")
                              .arg(type);
    sample.sourceFiles = {
        {QStringLiteral("Backdrop.cpp"), QStringLiteral("cpp"), sample.codeSnippet, sample.usageSnippet}};
}

} // namespace fluent::gallery::samples
#endif
