#include "DropDownButton.h"
#include "components/basicinput/private/MenuButtonAccessibility_p.h"
#include <QEasingCurve>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOptionButton>
#include <QtMath>

namespace fluent::basicinput {

DropDownButton::DropDownButton(const QString& text, QWidget* parent)
    : Button(detail::prepareMenuButtonAccessibility(text), parent) {
    initAnimation();
}

DropDownButton::DropDownButton(QWidget* parent)
    : Button(detail::prepareMenuButtonAccessibility(parent)) {
    initAnimation();
}

void DropDownButton::initAnimation() {
    if (m_pressAnimation) return;
    m_pressAnimation = new QPropertyAnimation(
        this, "pressProgress", this);
    // Global motion tokens: slow contrast with a decelerate curve.
    // zh_CN: 使用全局动画规范——慢速对比效果 + 减速曲线。
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
}

void DropDownButton::setMenu(QMenu* menu) {
    if (m_menu == menu) return;

    const bool hadMenu = m_menu != nullptr;
    if (m_menu)
        disconnect(m_menu.data(), nullptr, this, nullptr);
    setOpen(false);
    m_menu = menu;
    if (m_menu) {
        connect(m_menu, &QMenu::aboutToShow, this, [this]() { setOpen(true); });
        connect(m_menu, &QMenu::aboutToHide, this, [this]() { setOpen(false); });
        connect(m_menu, &QObject::destroyed, this, [this]() {
            setOpen(false);
            detail::notifyMenuButtonMenuAccessibility(this, true);
            emit menuChanged();
        });
        setOpen(m_menu->isVisible());
    }
    detail::notifyMenuButtonMenuAccessibility(
        this, hadMenu != (m_menu != nullptr));
    emit menuChanged();
}

void DropDownButton::setOpen(bool open) {
    if (m_isOpen == open) return;
    m_isOpen = open;
    update();
    detail::notifyMenuButtonOpenAccessibility(this);
    emit openChanged();
}

void DropDownButton::setChevronGlyph(const QString& glyph) {
    if (m_chevronGlyph == glyph) return;
    m_chevronGlyph = glyph;
    update();
    emit chevronChanged();
}

void DropDownButton::setIconFontFamily(const QString& family) {
    if (m_iconFontFamily == family) return;
    m_iconFontFamily = family;
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronSize(int size) {
    if (m_chevronSize == size) return;
    m_chevronSize = size;
    updateGeometry();
    update();
    emit chevronChanged();
}

void DropDownButton::setChevronOffset(const QPoint& offset) {
    if (m_chevronOffset == offset) return;
    m_chevronOffset = offset;
    updateGeometry();
    update();
    emit chevronChanged();
}

void DropDownButton::setPressProgress(qreal value) {
    qreal clamped = std::clamp(value, 0.0, 1.0);
    if (qFuzzyCompare(m_pressProgress, clamped))
        return;
    m_pressProgress = clamped;
    update();
}

QSize DropDownButton::sizeHint() const {
    QSize size = Button::sizeHint();
    size.rwidth() += chevronReserveWidth();
    return size;
}

QSize DropDownButton::minimumSizeHint() const {
    return sizeHint();
}

QRectF DropDownButton::contentPaintRect(const QRectF& surfaceRect) const {
    const qreal reserve = qMin(surfaceRect.width(),
                               static_cast<qreal>(chevronReserveWidth()));
    return surfaceRect.adjusted(0, 0, -reserve, 0);
}

int DropDownButton::chevronReserveWidth() const {
    const auto& spacing = themeSpacing();
    const int gap = fluentSize() == Small ? spacing.gap.tight : spacing.gap.normal;
    return gap + qMax(0, m_chevronSize) + qMax(0, m_chevronOffset.x());
}

void DropDownButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        // Clicking plays one press-down-and-rebound animation, independent of
        // the open state.
        // zh_CN: 点击触发一次“向下压+回弹”动画（与 open 状态无关）。
        if (m_pressAnimation) {
            m_pressAnimation->stop();
            m_pressAnimation->setStartValue(0.0);
            m_pressAnimation->setEndValue(1.0);
            m_pressAnimation->start();
        }

        if (m_menu) {
            // Keep menu interaction asynchronous so callers are not blocked by
            // a nested event loop. zh_CN: 菜单交互保持异步，避免嵌套事件循环
            // 阻塞调用方。
            event->accept();
            detail::showMenuButtonMenu(this);
            return;
        }
    }
    Button::mousePressEvent(event);
}

void DropDownButton::keyPressEvent(QKeyEvent* event)
{
    const bool altDown = event->key() == Qt::Key_Down
        && event->modifiers().testFlag(Qt::AltModifier);
    const bool f4 = event->key() == Qt::Key_F4
        && event->modifiers() == Qt::NoModifier;
    const bool primaryOpen =
        (event->key() == Qt::Key_Space
         || event->key() == Qt::Key_Return
         || event->key() == Qt::Key_Enter)
        && event->modifiers() == Qt::NoModifier;
    if (m_menu && (altDown || f4 || primaryOpen)) {
        detail::showMenuButtonMenu(this);
        event->accept();
        return;
    }
    Button::keyPressEvent(event);
}

void DropDownButton::paintEvent(QPaintEvent* event) {
    // 1. Lock the pressed look while the menu is open. zh_CN: 菜单开启时锁定为按下状态。
    InteractionState oldState = interactionState();
    if (m_isOpen) {
        const_cast<DropDownButton*>(this)->setInteractionState(Pressed);
    }

    // 2. Let the base class paint the plain button. zh_CN: 调用基类绘制基础按钮。
    Button::paintEvent(event);

    // 3. Restore the state. zh_CN: 恢复状态。
    if (m_isOpen) {
        const_cast<DropDownButton*>(this)->setInteractionState(oldState);
    }

    // 4. Paint the chevron glyph. zh_CN: 绘制 Chevron 图标。
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    // Icon font with the size unchanged. zh_CN: 设置图标字体（字号保持不变）。
    const bool usesFluentIcons = m_iconFontFamily == Typography::FontFamily::FluentIcons;
    QFont iconFont = usesFluentIcons
        ? Typography::Icons::font(m_chevronSize)
        : QFont(m_iconFontFamily);
    if (!usesFluentIcons)
        iconFont.setPixelSize(m_chevronSize);
    painter.setFont(iconFont);
    const qreal pressEffect = qSin(m_pressProgress * M_PI);

    // Icon color reuses Button's semantic colors with a subtle pressed tweak. zh_CN: 图标颜色复用 Button 的语义色，并提供轻微按压反馈。
    const auto& colors = themeColorsRef();
    // Filled surface matches Button's rule (Accent, or a checked Standard): the chevron then needs the
    // on-accent color to stay legible; otherwise it follows the brand's neutral/secondary text.
    // zh_CN: 填充表面与 Button 规则一致(Accent 或选中的 Standard):此时箭头需用 on-accent 色才清晰;否则随品牌的中性/次要文字色。
    QColor textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
    } else {
    // Accent uses textOnAccent; other styles use textPrimary.
            // zh_CN: Accent 使用 textOnAccent，其它样式使用 textPrimary。
            textColor = (fluentStyle() == Accent) ? colors.textOnAccent : colors.textPrimary;
        if (pressEffect > 0.0) {
            // 1.0 → 0.5 for a clear pressed feel. Multiply existing alpha (do not
            // force opaque 255) so secondary tokens stay consistent.
            // zh_CN: 1.0 → 0.5，明显的按压感。乘在已有 alpha 上（不要写成不透明 255），
            // 与次要色 token 保持一致。
            const qreal alphaFactor = 1.0 - 0.5 * pressEffect;
            textColor.setAlphaF(textColor.alphaF() * alphaFactor);
        }
    }
    
    painter.setPen(textColor);

    // Paint the glyph: it dips down along Y with the animation then rebounds,
    // plus the developer offset; chevronOffset.x() is the right-edge padding and
    // chevronOffset.y() the vertical tweak. Snap the dip to whole pixels so the
    // compact optical chevron stays sharp while pressed.
    // zh_CN: 绘制图标——按动画进度沿 Y 轴下移后弹回，再叠加自定义偏移；
    // chevronOffset.x() 为右缘间距，chevronOffset.y() 为垂直微调。下沉取整像素，
    // 避免紧凑光学箭头在按下时发虚。
    // Dedicated chevron slot at the trailing edge (SplitButton pattern), not a
    // full-width AlignRight band. zh_CN: 尾缘独立箭头槽（对齐 SplitButton），非整行 AlignRight。
    const qreal maxOffset = 3.0;
    const qreal pressOffset = qRound(maxOffset * pressEffect);
    const QRectF bounds = QRectF(rect());
    QRectF chevronSlot(
        bounds.right() - m_chevronOffset.x() - m_chevronSize,
        bounds.center().y() - m_chevronSize * 0.5,
        m_chevronSize,
        m_chevronSize);
    chevronSlot.translate(0, pressOffset + m_chevronOffset.y());
    if (usesFluentIcons) {
        Typography::Icons::paintGlyph(
            painter, chevronSlot, m_chevronGlyph, m_chevronSize, Qt::AlignCenter);
    } else {
        painter.drawText(chevronSlot, Qt::AlignCenter, m_chevronGlyph);
    }
}

} // namespace fluent::basicinput
