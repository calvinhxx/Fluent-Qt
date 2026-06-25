#include "ToolTip.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/textfields/Label.h"
#include "design/Elevation.h"
#include "design/Typography.h"
#include <QEvent>
#include <QGuiApplication>
#include <QPainter>
#include <QPalette>
#include <QPropertyAnimation>
#include <QScreen>
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
constexpr int kTargetGap = 4;
constexpr char kAttachedToolTipName[] = "FluentAttachedToolTip";
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

ToolTip* ToolTip::attach(QWidget* target, const QString& text, Placement placement)
{
    if (!target)
        return nullptr;

    auto* toolTip = target->findChild<ToolTip*>(QString::fromLatin1(kAttachedToolTipName),
                                                Qt::FindDirectChildrenOnly);
    if (!toolTip) {
        toolTip = new ToolTip(target);
        toolTip->setObjectName(QString::fromLatin1(kAttachedToolTipName));
    }
    toolTip->setText(text);
    toolTip->setTarget(target, placement);
    return toolTip;
}

void ToolTip::setText(const QString& text) {
    m_textBlock->setText(text);
    if (m_target)
        m_target->setToolTip(text);
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
    // Color via the label's OWN style sheet rather than its palette: a ToolTip can sit under an ancestor
    // style sheet (the gallery sample card installs QStyleSheetStyle over its subtree and ignores child
    // palettes), which would drop a palette WindowText color and render the text near-black in dark
    // theme. A style-sheet color always wins. zh_CN: 用标签自身样式表上色而非 palette：ToolTip 可能位于带样式表的
    // 祖先下（画廊示例卡会安装 QStyleSheetStyle 并忽略子 palette），palette 颜色会被丢弃，深色主题里文字变近黑；样式表颜色始终生效。
    m_textBlock->setStyleSheet(QStringLiteral("color: rgba(%1, %2, %3, %4); background: transparent;")
                                   .arg(m_textColor.red()).arg(m_textColor.green())
                                   .arg(m_textColor.blue()).arg(m_textColor.alpha()));
    update();
}

bool ToolTip::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != m_target || !event)
        return QWidget::eventFilter(watched, event);

    switch (event->type()) {
    case QEvent::ToolTip:
        if (!text().isEmpty() && m_target && m_target->isEnabled()) {
            positionForTarget();
            show();
            raise();
        }
        return true;
    case QEvent::Leave:
    case QEvent::Hide:
    case QEvent::MouseButtonPress:
    case QEvent::WindowDeactivate:
        hide();
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
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

void ToolTip::setTarget(QWidget* target, Placement placement)
{
    if (m_target == target && m_placement == placement)
        return;
    if (m_target)
        m_target->removeEventFilter(this);
    m_target = target;
    m_placement = placement;
    if (m_target) {
        m_target->setToolTip(text());
        m_target->installEventFilter(this);
    }
}

void ToolTip::positionForTarget()
{
    if (!m_target)
        return;

    adjustSize();
    const QSize outerSize = sizeHint().expandedTo(size());
    const QSize cardSize(qMax(0, outerSize.width() - 2 * kShadowMargin),
                         qMax(0, outerSize.height() - 2 * kShadowMargin));
    const QRect targetRect(m_target->mapToGlobal(QPoint(0, 0)), m_target->size());

    auto cardTopLeft = [&](Placement placement) {
        switch (placement) {
        case Below:
            return QPoint(targetRect.center().x() - cardSize.width() / 2,
                          targetRect.bottom() + 1 + kTargetGap);
        case Left:
            return QPoint(targetRect.left() - kTargetGap - cardSize.width(),
                          targetRect.center().y() - cardSize.height() / 2);
        case Right:
            return QPoint(targetRect.right() + 1 + kTargetGap,
                          targetRect.center().y() - cardSize.height() / 2);
        case Above:
        default:
            return QPoint(targetRect.center().x() - cardSize.width() / 2,
                          targetRect.top() - kTargetGap - cardSize.height());
        }
    };

    QPoint visibleTopLeft = cardTopLeft(m_placement);
    QScreen* screen = QGuiApplication::screenAt(targetRect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    if (screen) {
        const QRect available = screen->availableGeometry();
        const QRect preferred(visibleTopLeft, cardSize);
        if (!available.contains(preferred)) {
            Placement fallback = m_placement;
            if (m_placement == Above)
                fallback = Below;
            else if (m_placement == Below)
                fallback = Above;
            else if (m_placement == Left)
                fallback = Right;
            else
                fallback = Left;
            const QPoint fallbackTopLeft = cardTopLeft(fallback);
            if (available.contains(QRect(fallbackTopLeft, cardSize)))
                visibleTopLeft = fallbackTopLeft;
        }
        visibleTopLeft.setX(qBound(available.left(),
                                   visibleTopLeft.x(),
                                   qMax(available.left(), available.right() - cardSize.width() + 1)));
        visibleTopLeft.setY(qBound(available.top(),
                                   visibleTopLeft.y(),
                                   qMax(available.top(), available.bottom() - cardSize.height() + 1)));
    }
    move(visibleTopLeft - QPoint(kShadowMargin, kShadowMargin));
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
