#include "SplitButton.h"
#include "components/basicinput/private/MenuButtonAccessibility_p.h"
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionButton>
#include <QVariantAnimation>
#include <QtMath>

namespace fluent::basicinput {

namespace {

int splitButtonTextWidth(const QFontMetrics& fm, const QString& text)
{
    if (text.isEmpty())
        return 0;
    // Ink can extend past horizontalAdvance; reserve the largest metric so sizeHint
    // matches painted glyphs (same gap rule as DropDownButton chevron reserve).
    // zh_CN: 墨水可能超出 horizontalAdvance;取最大度量保证 sizeHint 与绘制一致
    // (与 DropDownButton 预留 chevron 区同理)。
    const int advance = fm.horizontalAdvance(text);
    const int bounds = fm.boundingRect(text).width();
    const int tight = fm.tightBoundingRect(text).width();
    return qMax(advance, qMax(bounds, tight));
}

} // namespace

SplitButton::SplitButton(const QString& text, QWidget* parent)
    : Button(text, parent) {
    detail::initializeMenuButtonAccessibility(this);
    setMouseTracking(true);
    m_pressAnimation = new QVariantAnimation(this);
    connect(m_pressAnimation, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                m_pressProgress = value.toReal();
                update();
            });
    connect(m_pressAnimation, &QVariantAnimation::finished, this, [this]() {
        m_pressProgress = 0.0;
        m_animatedPart = None;
        update();
    });
}

SplitButton::~SplitButton() {
    if (m_menu)
        disconnect(m_menu.data(), nullptr, this, nullptr);
}

void SplitButton::startPressAnimation(SplitPart part) {
    if (!m_pressAnimation || part == None)
        return;
    m_animatedPart = part;
    m_pressAnimation->stop();
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
    m_pressAnimation->setStartValue(0.0);
    m_pressAnimation->setEndValue(1.0);
    m_pressAnimation->start();
}

void SplitButton::setMenu(QMenu* menu) {
    if (m_menu == menu)
        return;

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

void SplitButton::setOpen(bool open) {
    if (m_isOpen == open)
        return;
    m_isOpen = open;
    update();
    detail::notifyMenuButtonOpenAccessibility(this);
    emit openChanged();
}

void SplitButton::setSecondaryWidth(int width) {
    if (m_secondaryWidth != width) {
        m_secondaryWidth = width;
        updateGeometry();
        update();
        emit secondaryWidthChanged();
    }
}

void SplitButton::mouseMoveEvent(QMouseEvent* event) {
    SplitPart part = getPartAt(event->pos());
    if (m_hoverPart != part) {
        m_hoverPart = part;
        update();
    }
    Button::mouseMoveEvent(event);
}

void SplitButton::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressPart = getPartAt(event->pos());
        startPressAnimation(m_pressPart);
        update();
        if (m_pressPart == Secondary) {
            event->accept();
            return;
        }
    }
    Button::mousePressEvent(event);
}

void SplitButton::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        const SplitPart releasePart = getPartAt(event->pos());

        if (m_pressPart == Secondary) {
            event->accept();
            if (releasePart == Secondary && m_menu)
                detail::showMenuButtonMenu(this);
            m_pressPart = None;
            update();
            return;
        }

        if (m_pressPart == Primary && releasePart != Primary) {
            // Releasing over the secondary segment cancels the primary
            // command instead of letting QPushButton treat the whole widget
            // as one hit target. zh_CN: 主区按下后若在二级区释放，则取消主命令。
            m_pressPart = None;
            setDown(false);
            update();
            event->accept();
            return;
        }

        m_pressPart = None;
        update();
    }
    Button::mouseReleaseEvent(event);
}

void SplitButton::keyPressEvent(QKeyEvent* event)
{
    const bool altDown = event->key() == Qt::Key_Down
        && event->modifiers().testFlag(Qt::AltModifier);
    const bool f4 = event->key() == Qt::Key_F4
        && event->modifiers() == Qt::NoModifier;
    if (m_menu && (altDown || f4)) {
        detail::showMenuButtonMenu(this);
        event->accept();
        return;
    }
    Button::keyPressEvent(event);
}

void SplitButton::leaveEvent(QEvent* event) {
    m_hoverPart = None;
    m_pressPart = None;
    update();
    Button::leaveEvent(event);
}

SplitButton::SplitPart SplitButton::getPartAt(const QPoint& pos) const {
    if (!rect().contains(pos)) return None;

    // Use the configured drop-down zone width. zh_CN: 使用配置的下拉区域宽度。
    const QRect logicalSecondary(width() - m_secondaryWidth, 0,
                                 m_secondaryWidth, height());
    const QRect secondary =
        QStyle::visualRect(layoutDirection(), rect(), logicalSecondary);
    if (secondary.contains(pos))
        return Secondary;
    return Primary;
}

int SplitButton::primaryTrailingInset() const
{
    const auto& spacing = themeSpacing();
    return (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;
}

QRectF SplitButton::primaryContentRect(const QRectF& primaryRect) const
{
    const qreal inset = qMin(primaryRect.width(), static_cast<qreal>(primaryTrailingInset()));
    return layoutDirection() == Qt::RightToLeft
        ? primaryRect.adjusted(inset, 0, 0, 0)
        : primaryRect.adjusted(0, 0, -inset, 0);
}

QSize SplitButton::sizeHint() const {
    const auto& spacing = themeSpacing();
    QFontMetrics fm(font());

    const int hPadding = (fluentSize() == Small) ? spacing.small
                         : (fluentSize() == Large ? spacing.standard : spacing.padding.controlH);
    const int vPadding = (fluentSize() == Small) ? spacing.gap.tight
                         : (fluentSize() == Large ? spacing.small : spacing.padding.controlV);
    const int iconGap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;

    const bool iconOnly = fluentLayout() == IconOnly;
    const QString txt = iconOnly ? QString() : text();
    const bool hasIconFont = !iconGlyph().isEmpty();
    const int txtWidth = splitButtonTextWidth(fm, txt);
    const int iconWidth = hasIconFont ? iconPixelSize() : 0;
    const int contentWidth = txtWidth + iconWidth
        + ((!txt.isEmpty() && hasIconFont) ? iconGap : 0);
    const int primaryWidth = qMax(Button::sizeHint().width(), contentWidth + hPadding * 2);
    // Trailing inset is for text clearance only; icon-only already centers in the primary zone.
    // zh_CN: 尾缘 inset 只服务于文字避让；纯图标已在主区内居中，不必再加宽。
    const int trailing = iconOnly ? 0 : primaryTrailingInset();

    return QSize(primaryWidth + m_secondaryWidth + trailing,
                 fm.height() + vPadding * 2);
}

QSize SplitButton::minimumSizeHint() const {
    return sizeHint();
}

void SplitButton::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const auto& colors = themeColorsRef();
    const auto& spacing = themeSpacing();
    const auto& radius = themeRadius();

    // 1. Size parameters. zh_CN: 尺寸参数。
    int sWidth = m_secondaryWidth;
    int sepMargin = spacing.gap.tight;

    // WinUI keeps the chevron on its native 12 px optical drawing at both
    // standard and compact control densities.
    // zh_CN: WinUI 在常规与紧凑密度下都使用原生 12 px chevron 字形。
    const int chevronSize = Typography::IconSize::Compact;

    QRectF fullRect = rect();
    const QRect logicalPrimaryRect(0, 0, width() - sWidth, height());
    const QRect logicalSecondaryRect(width() - sWidth, 0, sWidth, height());
    const QRectF primaryRect =
        QStyle::visualRect(layoutDirection(), rect(), logicalPrimaryRect);
    const QRectF secondaryRect =
        QStyle::visualRect(layoutDirection(), rect(), logicalSecondaryRect);
    const qreal dividerX = layoutDirection() == Qt::RightToLeft
        ? sWidth
        : width() - sWidth;

    const bool checked = isChecked();
    const bool accentLike = (fluentStyle() == Accent || checked);

    // Colors resolved per-branch below. textColor/chevronColor are shared by the content paint.
    // zh_CN: 颜色按分支解析;textColor/chevronColor 供下方内容绘制共用。
    QColor textColor = accentLike ? colors.textOnAccent : colors.textPrimary;
    QColor chevronColor = textColor;
    if (!isEnabled()) {
        textColor = colors.textDisabled;
        chevronColor = colors.textDisabled;
    }

    // Fluent surface treatment. zh_CN: Fluent 表面处理。

        // 2. State colors. zh_CN: 确定状态颜色。
        QColor baseBg;

        if (fluentStyle() == Accent || checked) {
            baseBg = colors.accentDefault;
            textColor = colors.textOnAccent;
        } else {
            baseBg = colors.controlDefault;
            textColor = colors.textPrimary;
        }

        if (!isEnabled()) {
            baseBg = colors.controlDisabled;
            textColor = colors.textDisabled;
        }
        chevronColor = textColor;

        // 3. Paint the shared background. zh_CN: 绘制整体背景。
        painter.setPen(Qt::NoPen);
        painter.setBrush(baseBg);
        painter.drawRoundedRect(fullRect, radius.control, radius.control);

        // 4. Paint per-zone highlights. zh_CN: 绘制分区域高亮。
        if (isEnabled()) {
            auto drawHighlight = [&](const QRectF& r, SplitPart part) {
                QColor highlight;
                if (m_pressPart == part) {
                    highlight = (fluentStyle() == Accent || checked) ? colors.accentTertiary : colors.controlTertiary;
                } else if (m_hoverPart == part) {
                    highlight = (fluentStyle() == Accent || checked) ? colors.accentSecondary : colors.controlSecondary;
                } else {
                    return;
                }
                painter.setBrush(highlight);
                painter.save();
                painter.setClipRect(r);
                painter.drawRoundedRect(fullRect, radius.control, radius.control);
                painter.restore();
            };

            drawHighlight(primaryRect, Primary);
            drawHighlight(secondaryRect, Secondary);
        }

        // 5. Paint the divider with token colors. zh_CN: 绘制分割线（使用 Token 颜色）。
        if (isEnabled()) {
            // Divider: lighter on Accent, standard stroke on Standard.
            // zh_CN: 分割线——Accent 风格下变淡，Standard 风格用标准边框色。
            QColor sepColor = (fluentStyle() == Accent || checked) ? colors.strokeDivider : colors.strokeDefault;
            painter.setPen(QPen(sepColor, 1));
            painter.drawLine(QPointF(dividerX, sepMargin),
                             QPointF(dividerX, height() - sepMargin));
        }

    // 6. Paint the primary content (text/icon). zh_CN: 绘制主内容。
    painter.setPen(textColor);
    painter.setFont(font());
    // Each zone sinks independently to suggest its own press feedback.
    // zh_CN: 两侧独立计算下沉偏移，模拟各自的点击触感。
    constexpr qreal kPi = 3.14159265358979323846;
    const qreal rebound = qSin(qBound<qreal>(0.0, m_pressProgress, 1.0) * kPi);
    const double primaryOffset = (m_pressPart == Primary ? 0.5 : 0.0)
        + (m_animatedPart == Primary ? rebound * 2.0 : 0.0);
    const double secondaryOffset = (m_pressPart == Secondary ? 0.5 : 0.0)
        + (m_animatedPart == Secondary ? rebound * 2.0 : 0.0);

    QString txt = (fluentLayout() == IconOnly) ? "" : text();
    bool hasIconFont = !iconGlyph().isEmpty();
    int gap = (fluentSize() == Small) ? spacing.gap.tight : spacing.gap.normal;

    int iconWidth = hasIconFont ? iconPixelSize() : 0;

    // Text layouts keep a trailing inset before the divider (DropDownButton pattern). Icon-only
    // centers in the full primary zone — applying that inset there slides the glyph left and
    // leaves an awkward gap before the chevron.
    // zh_CN: 带文字布局在分割线前保留尾缘间距(对齐 DropDownButton)。纯图标则在完整主区内居中——
    // 若再套该 inset 会把字形挤向左侧，与箭头之间留下别扭空隙。
    const bool iconOnly = fluentLayout() == IconOnly;
    const QRectF layoutRect = iconOnly ? primaryRect : primaryContentRect(primaryRect);
    const int hPadding = (fluentSize() == Small) ? spacing.small
                         : (fluentSize() == Large ? spacing.standard : spacing.padding.controlH);
    const bool rightToLeft = layoutDirection() == Qt::RightToLeft;
    double cursorX = rightToLeft ? layoutRect.right() : layoutRect.left();
    if (iconOnly && hasIconFont) {
        cursorX = layoutRect.left() + (layoutRect.width() - iconWidth) / 2.0;
    } else {
        cursorX += rightToLeft ? -hPadding : hPadding;
    }

    if (hasIconFont) {
        const bool usesFluentIcons = iconFontFamily() == Typography::FontFamily::FluentIcons;
        const qreal iconX = (!iconOnly && rightToLeft)
            ? cursorX - iconWidth
            : cursorX;
        QRectF iconRect(iconX, primaryRect.top() + primaryOffset,
                        iconWidth, primaryRect.height());
        if (usesFluentIcons) {
            Typography::Icons::paintGlyph(
                painter, iconRect, iconGlyph(), iconPixelSize(), Qt::AlignCenter);
        } else {
            QFont iconFont(iconFontFamily());
            iconFont.setPixelSize(iconPixelSize());
            painter.setFont(iconFont);
            painter.drawText(iconRect, Qt::AlignCenter, iconGlyph());
            painter.setFont(font());
        }
        if (fluentLayout() != IconOnly) {
            cursorX += rightToLeft
                ? -(iconWidth + gap)
                : iconWidth + gap;
        }
    }

    if (!txt.isEmpty()) {
        const QRectF textRect = rightToLeft
            ? QRectF(layoutRect.left(),
                     primaryRect.top() + primaryOffset,
                     qMax<qreal>(0.0, cursorX - layoutRect.left()),
                     primaryRect.height())
            : QRectF(cursorX,
                     primaryRect.top() + primaryOffset,
                     qMax<qreal>(0.0, layoutRect.right() - cursorX),
                     primaryRect.height());
        painter.save();
        painter.setClipRect(layoutRect);
        painter.drawText(textRect,
                         QStyle::visualAlignment(layoutDirection(),
                                                 Qt::AlignLeft | Qt::AlignVCenter),
                         txt);
        painter.restore();
    }

    // 7. Paint the chevron; it also sinks 0.5px while pressed. zh_CN: 绘制下拉箭头，按下时同样下沉 0.5px。
    if (m_animatedPart == Secondary && rebound > 0.0)
        chevronColor.setAlphaF(chevronColor.alphaF() * (1.0 - 0.25 * rebound));
    painter.setPen(chevronColor);
    Typography::Icons::paintGlyph(
        painter,
        secondaryRect.translated(0, secondaryOffset),
        Typography::Icons::ChevronDown,
        chevronSize,
        Qt::AlignCenter);

    // 8. Focus ring.
    //    zh_CN: 焦点框。
    if (hasFocus() && isEnabled()) {
        QColor focusColor = colors.textSecondary;
        focusColor.setAlpha(120);
        painter.setPen(QPen(focusColor, 1.0));
        painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(fullRect.adjusted(1.5, 1.5, -1.5, -1.5),
                                    radius.control - 1, radius.control - 1);
        }
}

} // namespace fluent::basicinput
