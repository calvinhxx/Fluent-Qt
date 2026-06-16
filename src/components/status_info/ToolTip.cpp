#include "ToolTip.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/textfields/Label.h"
#include "design/Elevation.h"
#include "design/Typography.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace fluent::status_info {

using namespace fluent::textfields;

namespace {
constexpr qreal kHiddenOpacity = 0.0;
constexpr qreal kVisibleOpacity = 1.0;
constexpr qreal kOpacityEpsilon = 0.001;

// Transparent inset around the bubble so the layered shadow has room to spread
// (paintLayeredShadow grows ~11px); the bubble is drawn inside this margin.
// zh_CN: 气泡四周的透明内缩，给分层阴影留出扩散空间（约 11px），气泡绘制在该边距内部。
constexpr int kShadowMargin = 12;
}

ToolTip::ToolTip(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowOpacity(kHiddenOpacity);
    
    m_textBlock = new Label(this);
    // Keep the label background transparent so ToolTip::paintEvent owns it.
    // zh_CN: 确保标签背景透明，由 ToolTip 的 paintEvent 处理背景绘制。
    m_textBlock->setAttribute(Qt::WA_TranslucentBackground);
    m_textBlock->setStyleSheet("background-color: transparent;");
    
    // 1. Text style: Caption size, regular weight by default. zh_CN: 默认使用 Caption 字号，不加粗。
    QFont f = m_textBlock->font();
    f.setBold(false); 
    f.setPixelSize(Typography::FontSize::Caption);
    m_textBlock->setFont(f);
    m_textBlock->setAlignment(Qt::AlignCenter);

    // 2. Initialize the padding. Figma "Tooltip" spec: 8 horizontal, 5 top / 7 bottom
    //    (the asymmetric vertical padding optically centers the caption text).
    // zh_CN: 初始化内边距。Figma「Tooltip」规范：水平 8，上 5、下 7（不对称纵向内边距让文字视觉居中）。
    m_margins = QMargins(8, 5, 8, 7);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_textBlock);
    setLayout(layout);
    applyLayoutMargins();

    // 3. Initial colors. zh_CN: 初始颜色设置。
    const auto& c = themeColors();
    m_bgColor = c.bgLayer;  // Figma tooltip surface is near-white (#FCFCFC); bgLayer matches in both themes.
    m_borderColor = c.strokeDivider;
    m_textColor = c.textPrimary;
}

void ToolTip::setText(const QString& text) {
    m_textBlock->setText(text);
    adjustSize();
}

QString ToolTip::text() const {
    return m_textBlock->text();
}

void ToolTip::setMargins(const QMargins& margins) {
    if (m_margins != margins) {
        m_margins = margins;
        applyLayoutMargins();
        adjustSize();
        emit marginsChanged();
    }
}

int ToolTip::shadowMargin() const {
    return kShadowMargin;
}

void ToolTip::applyLayoutMargins() {
    // The layout margins fold the shadow inset together with the content padding so the
    // label sits inside the bubble and the bubble sits inside the transparent shadow band.
    // zh_CN: 布局边距把阴影内缩与内容内边距合并，使标签位于气泡内、气泡位于透明阴影带内。
    if (auto* l = layout())
        l->setContentsMargins(m_margins + fluent::overlay::uniformShadowMargins(kShadowMargin));
}

void ToolTip::setFont(const QFont& font) {
    QWidget::setFont(font);
    if (m_textBlock) {
        m_textBlock->setFont(font);
    }
    adjustSize();
}

void ToolTip::setAnimationEnabled(bool enabled) {
    if (m_animationEnabled == enabled) return;

    m_animationEnabled = enabled;
    if (!m_animationEnabled) {
        if (m_opacityAnimation) {
            m_opacityAnimation->stop();
        }
        if (m_hideOnAnimationFinished) {
            finishHideAnimation();
        } else {
            setWindowOpacity(isVisible() ? kVisibleOpacity : kHiddenOpacity);
        }
    }

    emit animationEnabledChanged(m_animationEnabled);
}

void ToolTip::setVisible(bool visible) {
    if (!m_animationEnabled) {
        if (m_opacityAnimation) {
            m_opacityAnimation->stop();
        }
        m_hideOnAnimationFinished = false;
        setWindowOpacity(visible ? kVisibleOpacity : kHiddenOpacity);
        QWidget::setVisible(visible);
        return;
    }

    if (visible) {
        startShowAnimation();
    } else {
        startHideAnimation();
    }
}

void ToolTip::onThemeUpdated() {
    const auto& c = themeColors();
    m_bgColor = c.bgLayer;  // Figma tooltip surface is near-white (#FCFCFC); bgLayer matches in both themes.
    m_borderColor = c.strokeDivider;
    m_textColor = c.textPrimary;
    update();
}

void ToolTip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& r = themeRadius();
    const QRect bubble = fluent::overlay::visibleCardRect(rect(), kShadowMargin);

    // 1. Soft drop shadow, drawn with the shared overlay painter so it matches flyouts/menus;
    //    Medium elevation keeps it subtle, as Figma's tooltip shadow is light.
    // zh_CN: 柔和投影，复用浮层共享绘制器，与 flyout/menu 一致；用 Medium 层级保持轻盈，贴合 Figma 工具提示的浅阴影。
    fluent::overlay::paintLayeredShadow(p, bubble, r.control, themeShadow(Elevation::Medium));

    // 2. Background and border. The Figma "Tooltip" uses the 4px control radius,
    //    not the 8px overlay radius used by dialogs/flyouts.
    // zh_CN: 绘制背景和边框。Figma「Tooltip」用 4px 控件圆角，而非 dialog/flyout 的 8px 浮层圆角。
    p.setBrush(m_bgColor);
    p.setPen(QPen(m_borderColor, 1));
    p.drawRoundedRect(QRectF(bubble).adjusted(0.5, 0.5, -0.5, -0.5), r.control, r.control);
}

void ToolTip::ensureOpacityAnimation() {
    if (m_opacityAnimation) return;

    m_opacityAnimation = new QPropertyAnimation(this, "windowOpacity", this);
    connect(m_opacityAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_hideOnAnimationFinished) {
            finishHideAnimation();
            return;
        }
        setWindowOpacity(kVisibleOpacity);
    });
}

void ToolTip::startShowAnimation() {
    ensureOpacityAnimation();

    m_hideOnAnimationFinished = false;
    m_opacityAnimation->stop();

    const qreal startOpacity = isVisible() ? windowOpacity() : kHiddenOpacity;
    setWindowOpacity(startOpacity);
    QWidget::setVisible(true);

    if (startOpacity >= kVisibleOpacity - kOpacityEpsilon) {
        setWindowOpacity(kVisibleOpacity);
        return;
    }

    const auto anim = themeAnimation();
    m_opacityAnimation->setDuration(anim.fast);
    m_opacityAnimation->setEasingCurve(anim.decelerate);
    m_opacityAnimation->setStartValue(startOpacity);
    m_opacityAnimation->setEndValue(kVisibleOpacity);
    m_opacityAnimation->start();
}

void ToolTip::startHideAnimation() {
    if (!isVisible()) {
        if (m_opacityAnimation) {
            m_opacityAnimation->stop();
        }
        m_hideOnAnimationFinished = false;
        setWindowOpacity(kHiddenOpacity);
        return;
    }

    ensureOpacityAnimation();

    m_hideOnAnimationFinished = true;
    m_opacityAnimation->stop();

    const qreal startOpacity = windowOpacity();
    if (startOpacity <= kHiddenOpacity + kOpacityEpsilon) {
        finishHideAnimation();
        return;
    }

    const auto anim = themeAnimation();
    m_opacityAnimation->setDuration(anim.fast);
    m_opacityAnimation->setEasingCurve(anim.accelerate);
    m_opacityAnimation->setStartValue(startOpacity);
    m_opacityAnimation->setEndValue(kHiddenOpacity);
    m_opacityAnimation->start();
}

void ToolTip::finishHideAnimation() {
    m_hideOnAnimationFinished = false;
    setWindowOpacity(kHiddenOpacity);
    QWidget::setVisible(false);
}

} // namespace fluent::status_info
