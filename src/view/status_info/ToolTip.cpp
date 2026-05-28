#include "ToolTip.h"
#include "view/textfields/Label.h"
#include "design/Typography.h"
#include <QPainter>
#include <QPropertyAnimation>
#include <QVBoxLayout>

namespace view::status_info {

using namespace view::textfields;

namespace {
constexpr qreal kHiddenOpacity = 0.0;
constexpr qreal kVisibleOpacity = 1.0;
constexpr qreal kOpacityEpsilon = 0.001;
}

ToolTip::ToolTip(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowOpacity(kHiddenOpacity);
    
    m_textBlock = new Label(this);
    // 确保标签背景透明，以便由 ToolTip 的 paintEvent 处理背景绘制
    m_textBlock->setAttribute(Qt::WA_TranslucentBackground);
    m_textBlock->setStyleSheet("background-color: transparent;");
    
    // 1. 设置 ToolTip 文本样式: 默认使用 Caption 字号，不加粗
    QFont f = m_textBlock->font();
    f.setBold(false); 
    f.setPixelSize(Typography::FontSize::Caption);
    m_textBlock->setFont(f);
    m_textBlock->setAlignment(Qt::AlignCenter);

    // 2. 初始化内边距
    m_margins = QMargins(themeSpacing().small, themeSpacing().xSmall, 
                        themeSpacing().small, themeSpacing().xSmall);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(m_margins);
    layout->addWidget(m_textBlock);
    
    setLayout(layout);
    
    // 3. 初始颜色设置
    const auto& c = themeColors();
    m_bgColor = c.bgSolid; 
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
        if (layout()) {
            layout()->setContentsMargins(m_margins);
        }
        adjustSize();
        emit marginsChanged();
    }
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
    m_bgColor = c.bgSolid; 
    m_borderColor = c.strokeDivider;
    m_textColor = c.textPrimary;
    update();
}

void ToolTip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const auto& r = themeRadius();
    
    // 1. 绘制背景和边框
    p.setBrush(m_bgColor);
    p.setPen(QPen(m_borderColor, 1));
    p.drawRoundedRect(rect().adjusted(0,0,-1,-1), r.overlay, r.overlay);
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

} // namespace view::status_info
