#include "ComboBox.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QApplication>
#include <QResizeEvent>
#include <QtMath>
#include <QStringListModel>
#include <QProxyStyle>
#include <QScrollBar>

#include "design/CornerRadius.h"
#include "design/Animation.h"
#include "compatibility/QtCompat.h"
#include "components/collections/ListView.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/scrolling/ScrollBar.h"
#include "components/textfields/LineEdit.h"

namespace {
static constexpr int kPopupShadowMargin = ::Spacing::Standard;
static constexpr int kPopupContentInset = ::Spacing::XSmall / 2;
static constexpr int kPopupWindowMargin = 4;
static constexpr qreal kPopupTextOpticalOffsetY = -2.0;

// Suppress QStyle's PE_PanelLineEdit native panel — ComboBox paints its own bg
class TransparentLineEditStyle : public QProxyStyle {
public:
    void drawPrimitive(PrimitiveElement pe, const QStyleOption* opt,
                       QPainter* p, const QWidget* w = nullptr) const override {
        if (pe == PE_PanelLineEdit) return;
        QProxyStyle::drawPrimitive(pe, opt, p, w);
    }
};
}

namespace fluent::basicinput {

// ─── ComboBoxItemDelegate implementation. zh_CN: ComboBoxItemDelegate 实现 ──

ComboBoxItemDelegate::ComboBoxItemDelegate(FluentElement* themeHost, QAbstractItemView* view,
                                           QObject* parent)
    : QStyledItemDelegate(parent), m_themeHost(themeHost), m_view(view) {
}

void ComboBoxItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const {
    if (!index.isValid()) return;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    FluentElement::Colors colors{};
    FluentElement::Radius radius{};
    if (m_themeHost) {
        colors = m_themeHost->themeColors();
        radius = m_themeHost->themeRadius();
    }

    int itemRightInset = 5;
    if (m_view && m_view->verticalScrollBar() &&
        m_view->verticalScrollBar()->maximum() > m_view->verticalScrollBar()->minimum()) {
        if (auto* listView = qobject_cast<fluent::collections::ListView*>(m_view)) {
            if (auto* scrollBar = listView->verticalFluentScrollBar()) {
                itemRightInset += scrollBar->thickness();
            }
        }
    }
    QRectF bgRect = QRectF(option.rect).adjusted(5, 3, -itemRightInset, -3);
    const int cornerR = radius.control > 0 ? radius.control : 4;

    const bool isSelected = option.state & QStyle::State_Selected;
    const bool isHovered  = option.state & QStyle::State_MouseOver;
    const bool isPressed  = (option.state & QStyle::State_Sunken) && isHovered;
    const bool isEnabled  = option.state & QStyle::State_Enabled;

    QColor bgColor   = Qt::transparent;
    QColor textColor = colors.textPrimary;

    if (!isEnabled) {
        textColor = colors.textDisabled;
    } else if (isSelected && isPressed) {
        bgColor = colors.subtleTertiary;
    } else if (isSelected && isHovered) {
        bgColor = colors.subtleSecondary;
    } else if (isSelected) {
        bgColor = colors.subtleSecondary;
    } else if (isPressed) {
        bgColor = colors.subtleTertiary;
    } else if (isHovered) {
        bgColor = colors.subtleSecondary;
    }

    if (bgColor.alpha() > 0) {
        QPainterPath path;
        path.addRoundedRect(bgRect, cornerR, cornerR);
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawPath(path);
    }

    // ListView owns the selected indicator overlay. The delegate only paints the
    // row background and text; drawing another indicator here creates a double
    // blue pill in ComboBox flyouts.
    const int textLeft = 16;
    QRectF textRect = bgRect.adjusted(textLeft, 0, -8, 0);
    // Center the actual Segoe UI ink rather than its asymmetric ascent/descent line box.
    // zh_CN: 按 Segoe UI 字形墨迹居中，而不是按上下不对称的 ascent/descent 行框居中。
    textRect.translate(0, kPopupTextOpticalOffsetY);
    painter->setPen(textColor);
    painter->setFont(option.font);
    const QString text = index.data(Qt::DisplayRole).toString();
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                      painter->fontMetrics().elidedText(text, Qt::ElideRight, int(textRect.width())));

    painter->restore();
}

QSize ComboBoxItemDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return QSize(0, ::Spacing::ControlHeight::Large);
}

// ─── ComboBoxPopup implementation. zh_CN: ComboBoxPopup 实现 ────────────────

ComboBox::ComboBoxPopup::ComboBoxPopup(ComboBox* comboBox)
    : Flyout(comboBox), m_comboBox(comboBox) {
    setObjectName("ComboBoxPopup");
    setAnimationEnabled(false);
    setPlacement(fluent::dialogs_flyouts::Flyout::Auto);
    setAnchorOffset(comboBox ? comboBox->popupOffset() : ::Spacing::XSmall);
    setModal(false);
    setDim(false);
    setClosePolicy(ClosePolicy(CloseOnPressOutside | CloseOnEscape));

    m_listView = new fluent::collections::ListView(this);
    m_listView->setObjectName("ComboBoxPopupListView");
    m_listView->setBorderVisible(false);
    m_listView->setBackgroundVisible(true);
    m_listView->setSelectionMode(fluent::collections::ListView::ListSelectionMode::Single);
    m_listView->setSpacing(0);

    m_delegate = new ComboBoxItemDelegate(comboBox, m_listView, this);
    m_listView->setItemDelegate(m_delegate);
    m_listView->setFont(comboBox->themeFont(comboBox->fontRole()).toQFont());

    m_listView->setMouseTracking(true);
    m_listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    connect(m_listView, &fluent::collections::ListView::itemClicked, this, [this](int index) {
        m_comboBox->setCurrentIndex(index);
        if (m_comboBox->m_lineEdit) {
            m_comboBox->m_lineEdit->setText(m_comboBox->itemText(index));
        }
        m_comboBox->hidePopup();
    });

    connect(this, &ComboBoxPopup::closed, this, [this]() {
        if (m_comboBox) m_comboBox->onPopupHidden();
    });

    onThemeUpdated();
}

void ComboBox::ComboBoxPopup::showForComboBox() {
    m_listView->setModel(m_comboBox->model());
    m_listView->setFont(m_comboBox->themeFont(m_comboBox->fontRole()).toQFont());

    if (m_comboBox->currentIndex() >= 0) {
        m_listView->setSelectedIndex(m_comboBox->currentIndex());
    }

    const int itemCount = m_comboBox->count();
    const int itemH     = ::Spacing::ControlHeight::Large;
    const int maxVisible = qMin(itemCount, 6);
    const int rowsH     = maxVisible * itemH;
    const int sSize     = kPopupShadowMargin;
    const int cardInset = kPopupContentInset;
    const int cardW     = qMax(m_comboBox->width(), 120);
    const int cardH     = rowsH + cardInset * 2;
    const QSize totalSize = ::fluent::overlay::outerSizeForVisibleCard(QSize(cardW, cardH), sSize);

    setFixedSize(totalSize);
    setAnchorOffset(m_comboBox->m_popupOffset);
    setAnchor(m_comboBox);

    const QRect cardRect = ::fluent::overlay::visibleCardRect(rect(), sSize);
    m_listView->setGeometry(cardRect.adjusted(cardInset, cardInset, -cardInset, -cardInset));
    m_listView->clearMask();
    m_listView->refreshFluentScrollChrome();

    if (isOpen() || isVisible()) {
        move(computePosition());
        show();
        raise();
    } else {
        showAt(m_comboBox);
    }

    if (m_comboBox->currentIndex() >= 0) {
        m_listView->scrollTo(m_listView->model()->index(m_comboBox->currentIndex(), 0),
                             QAbstractItemView::PositionAtCenter);
    }
}

void ComboBox::ComboBoxPopup::onThemeUpdated() {
    Flyout::onThemeUpdated();
    QPalette pal = palette();
    pal.setColor(QPalette::Window, themeColors().bgLayer);
    setPalette(pal);

    if (m_comboBox) {
        m_listView->setFont(m_comboBox->themeFont(m_comboBox->fontRole()).toQFont());
    }
    if (m_listView && m_listView->viewport()) m_listView->viewport()->update();
}

QPoint ComboBox::ComboBoxPopup::computePosition() const {
    if (!m_comboBox || !m_comboBox->window()) return Flyout::computePosition();

    QWidget* top = m_comboBox->window();
    const int shadow = kPopupShadowMargin;
    const QSize cardSize = ::fluent::overlay::visibleCardSize(size(), shadow);
    const int cardW = cardSize.width();
    const int cardH = cardSize.height();
    const QRect anchor(m_comboBox->mapTo(top, QPoint(0, 0)), m_comboBox->size());

    const int spaceBelow = top->height() - (anchor.bottom() + 1);
    const int spaceAbove = anchor.top();
    const bool placeAbove = spaceBelow < cardH && spaceAbove > spaceBelow;

    QPoint cardTopLeft(anchor.left(), placeAbove
        ? anchor.top() - anchorOffset() - cardH
        : anchor.bottom() + 1 + anchorOffset());

    if (clampToWindow()) {
        cardTopLeft = ::fluent::overlay::clampCardTopLeft(cardTopLeft, cardSize, top->rect(), kPopupWindowMargin);
    }

    return ::fluent::overlay::outerTopLeftForVisibleCard(cardTopLeft, shadow);
}

bool ComboBox::ComboBoxPopup::eventFilter(QObject* watched, QEvent* event) {
    if (event && event->type() == QEvent::MouseButtonPress && m_comboBox) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint comboLocal = m_comboBox->mapFromGlobal(fluentMouseGlobalPos(mouseEvent));
        const bool pressOnOwner = m_comboBox->rect().contains(comboLocal);
        const bool pressInsidePopup = ::fluent::overlay::visibleCardContains(rect(), mapFromGlobal(fluentMouseGlobalPos(mouseEvent)), kPopupShadowMargin);
        if (pressOnOwner && !pressInsidePopup) {
            m_comboBox->m_ignoreNextPopupPress = true;
        }
    }

    return Flyout::eventFilter(watched, event);
}

// ─── ComboBox implementation. zh_CN: ComboBox 主体实现 ─────────────────────

ComboBox::ComboBox(QWidget* parent) : QComboBox(parent) {
    setAttribute(Qt::WA_Hover);
    setFont(themeFont(m_fontRole).toQFont());
    setFixedHeight(::Spacing::ControlHeight::Standard);

    initAnimation();
    onThemeUpdated();
}

ComboBox::~ComboBox() {
    delete m_popup;
}

void ComboBox::initAnimation() {
    m_pressAnimation = new QPropertyAnimation(this, "pressProgress", this);
    m_pressAnimation->setDuration(themeAnimation().slow);
    m_pressAnimation->setEasingCurve(themeAnimation().decelerate);
}

void ComboBox::setFontRole(const QString& role) {
    if (m_fontRole == role) return;
    m_fontRole = role;
    setFont(themeFont(m_fontRole).toQFont());
    updateGeometry();
    emit fontRoleChanged();
    update();
}

void ComboBox::setContentPaddingH(int px) {
    if (m_contentPaddingH == px) return;
    m_contentPaddingH = px;
    updateGeometry();
    emit layoutChanged();
    update();
}

void ComboBox::setContentPaddingV(int px) {
    if (m_contentPaddingV == px) return;
    m_contentPaddingV = px;
    updateGeometry();
    emit layoutChanged();
    update();
}

void ComboBox::setChevronGlyph(const QString& glyph) {
    if (m_chevronGlyph == glyph) return;
    m_chevronGlyph = glyph;
    emit chevronChanged();
    update();
}

void ComboBox::setChevronSize(int size) {
    if (m_chevronSize == size) return;
    m_chevronSize = size;
    emit chevronChanged();
    update();
}

void ComboBox::setChevronOffset(const QPoint& offset) {
    if (m_chevronOffset == offset) return;
    m_chevronOffset = offset;
    emit chevronChanged();
    update();
}

void ComboBox::setPopupOffset(int offset) {
    if (m_popupOffset == offset) return;
    m_popupOffset = offset;
    emit layoutChanged();
}

void ComboBox::setPressProgress(qreal p) {
    m_pressProgress = p;
    update();
}

void ComboBox::onThemeUpdated() {
    setFont(themeFont(m_fontRole).toQFont());
    if (m_popup) {
        m_popup->onThemeUpdated();
    }
    if (m_lineEdit) {
        applyLineEditStyle();
    }
    update();
}

QSize ComboBox::sizeHint() const {
    const auto& sp = themeSpacing();
    QFontMetrics fm(font());
    // Find widest item
    int maxTextW = 80; // Figma: min width 80px
    for (int i = 0; i < count(); ++i) {
        int w = fm.horizontalAdvance(itemText(i));
        maxTextW = qMax(maxTextW, w);
    }
    // chevron area: offset.x + icon size + gap
    const int chevronArea = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
    const int w = m_contentPaddingH + maxTextW + chevronArea;
    const int h = sp.controlHeight.standard;
    return QSize(w, h);
}

// ── Editable ─────────────────────────────────────────────────────────────────

void ComboBox::setEditable(bool editable) {
    if (editable && !m_lineEdit) {
        m_lineEdit = new fluent::textfields::LineEdit(this);
        m_lineEdit->setClearButtonEnabled(false);
        m_lineEdit->setFontRole(m_fontRole);
        m_lineEdit->setContentMargins(QMargins(0, 0, 0, 0));
        m_lineEdit->setFrameVisible(false);
        auto* style = new TransparentLineEditStyle();
        style->setParent(m_lineEdit);
        m_lineEdit->setStyle(style);
        m_lineEdit->setFocusPolicy(Qt::ClickFocus);
        m_lineEdit->installEventFilter(this);
        applyLineEditStyle();
        setMouseTracking(true);
        layoutLineEdit();

        if (currentIndex() >= 0)
            m_lineEdit->setText(currentText());

        m_lineEdit->show();

        connect(m_lineEdit, &fluent::textfields::LineEdit::returnPressed, this, [this]() {
            validateLineEditText();
            hidePopup();
        });

        connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int idx) {
                    if (m_lineEdit && idx >= 0)
                        m_lineEdit->setText(itemText(idx));
                });
    } else if (!editable && m_lineEdit) {
        m_lineEdit->removeEventFilter(this);
        setMouseTracking(false);
        m_chevronHovered = false;
        delete m_lineEdit;
        m_lineEdit = nullptr;
    }
    update();
}

void ComboBox::layoutLineEdit() {
    if (!m_lineEdit) return;
    const int chevronAreaW = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
    const int gap = ::Spacing::Gap::Tight;
    QRect textRect = rect().adjusted(m_contentPaddingH, m_contentPaddingV,
                                     -(chevronAreaW + gap), -m_contentPaddingV);
    m_lineEdit->setGeometry(textRect);
}

void ComboBox::applyLineEditStyle() {
    if (!m_lineEdit) return;
    m_lineEdit->setFontRole(m_fontRole);
    m_lineEdit->onThemeUpdated();
}

void ComboBox::validateLineEditText() {
    if (!m_lineEdit) return;
    int idx = findText(m_lineEdit->text(), Qt::MatchFixedString);
    if (idx >= 0) {
        setCurrentIndex(idx);
    } else {
        // Revert to previous valid text
        m_lineEdit->setText(currentIndex() >= 0 ? currentText() : QString());
    }
}

void ComboBox::resizeEvent(QResizeEvent* event) {
    QComboBox::resizeEvent(event);
    layoutLineEdit();
}

// ── Popup ────────────────────────────────────────────────────────────────────

void ComboBox::showPopup() {
    if (m_popupVisible) return;
    m_popupVisible = true;

    if (!m_popup)
        m_popup = new ComboBoxPopup(this);

    m_popup->showForComboBox();
    update();
}

void ComboBox::hidePopup() {
    if (!m_popupVisible) return;
    m_popupVisible = false;
    m_pressed = false;

    if (m_popup)
        m_popup->close();

    update();
    QComboBox::hidePopup();
}

// Private helper called from the popup close lifecycle.
void ComboBox::onPopupHidden() {
    const bool needsUpdate = m_popupVisible || m_pressed;
    m_popupVisible = false;
    m_pressed = false;
    if (needsUpdate) update();
}

// ── Input events. zh_CN: 输入事件 ────────────────────────────────────────────

void ComboBox::enterEvent(FluentEnterEvent* event) {
    m_hovered = true;
    update();
    QComboBox::enterEvent(event);
}

void ComboBox::leaveEvent(QEvent* event) {
    m_hovered = false;
    m_chevronHovered = false;
    update();
    QComboBox::leaveEvent(event);
}

void ComboBox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (m_ignoreNextPopupPress) {
            m_ignoreNextPopupPress = false;
            m_pressed = false;
            update();
            event->accept();
            return;
        }

        m_pressed = true;
        // Fire-and-forget bounce animation (0→1, qSin gives 0→peak→0)
        m_pressAnimation->stop();
        m_pressAnimation->setStartValue(0.0);
        m_pressAnimation->setEndValue(1.0);
        m_pressAnimation->start();

        // Toggle popup ourselves — base class has its own popup management
        // that conflicts with our custom popup
        if (m_popupVisible)
            hidePopup();
        else
            showPopup();
    }
    event->accept();
}

void ComboBox::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_pressed = false;
        update();
    }
    event->accept();
}

void ComboBox::mouseMoveEvent(QMouseEvent* event) {
    if (m_lineEdit) {
        const int chevronAreaW = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
        bool over = event->pos().x() >= width() - chevronAreaW;
        if (over != m_chevronHovered) {
            m_chevronHovered = over;
            update();
        }
    }
    QComboBox::mouseMoveEvent(event);
}

bool ComboBox::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_lineEdit) {
        if (event->type() == QEvent::FocusIn) {
            m_lineEdit->selectAll();
            update();
        } else if (event->type() == QEvent::FocusOut) {
            validateLineEditText();
            update();
        }
    }
    return QComboBox::eventFilter(watched, event);
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void ComboBox::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const auto& colors = themeColors();
    const auto& radius = themeRadius();

    const bool enabled = isEnabled();
    const QRectF r(rect());

    // ── Background ───────────────────────────────────────────────────────
    // Figma: Outer 1px padding + 4px radius, inner base 3px radius
    // Outer wrapper
    QColor outerBg = Qt::transparent;
    const bool lineEditFocused = m_lineEdit && m_lineEdit->hasFocus();
    if (!enabled) {
        outerBg = colors.controlDisabled;
    } else if (m_popupVisible) {
        outerBg = colors.controlTertiary;
    } else if (m_pressed) {
        outerBg = colors.controlTertiary;
    } else if (lineEditFocused) {
        outerBg = colors.controlDefault;
    } else if (m_hovered) {
        outerBg = colors.controlSecondary;
    } else {
        outerBg = colors.controlDefault;
    }

    // Draw the control background with 1px inset for border
    const qreal outerR = radius.control; // 4px
    const qreal innerR = outerR - 1;     // 3px

    // Fill background
    QPainterPath bgPath;
    bgPath.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), outerR, outerR);
    painter.setPen(Qt::NoPen);
    painter.setBrush(outerBg);
    painter.drawPath(bgPath);

    // ── Border ───────────────────────────────────────────────────────────
    // Figma: border rgba(0,0,0,0.06) → strokeDefault
    QColor borderColor = colors.strokeDefault;
    if (!enabled) {
        borderColor = colors.strokeDefault;
    } else if (m_popupVisible) {
        borderColor = colors.strokeDefault;
    }

    // Bottom accent stroke when focused/open (WinUI 3 pattern)
    if (lineEditFocused && enabled) {
        // Draw normal border first
        painter.setPen(QPen(borderColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), outerR, outerR);

        // Accent bottom border (2px)
        const qreal accentH = 2.0;
        QRectF bottomRect(r.left() + 0.5, r.bottom() - accentH - 0.5,
                          r.width() - 1.0, accentH);
        QPainterPath bp;
        bp.addRoundedRect(bottomRect, innerR, innerR);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.accentDefault);
        painter.drawPath(bp);
    } else {
        // Normal border
        painter.setPen(QPen(borderColor, 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), outerR, outerR);

        // Bottom edge gradient (WinUI 3 ControlElevation): slightly darker at bottom
        if (enabled && !m_pressed) {
            const qreal accentH = 1.0;
            QRectF bottomRect(r.left() + 1, r.bottom() - accentH - 0.5,
                              r.width() - 2, accentH);
            QPainterPath bp;
            bp.addRoundedRect(bottomRect, 1.0, 1.0);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.strokeSecondary);
            painter.drawPath(bp);
        }
    }

    // ── Text ─────────────────────────────────────────────────────────────
    // Figma: text 14px, color rgba(0,0,0,0.9) → textPrimary
    QColor textColor = enabled ? colors.textPrimary : colors.textDisabled;

    // Chevron area calculation
    const int chevronAreaW = m_chevronOffset.x() + m_chevronSize + ::Spacing::Gap::Tight;
    QRectF textRect = r.adjusted(m_contentPaddingH, m_contentPaddingV,
                                 -(chevronAreaW), -m_contentPaddingV);
    // Figma: pb-[2px] on text wrapper
    textRect.adjust(0, 0, 0, -2);

    // In editable mode, QLineEdit handles text display
    if (!m_lineEdit) {
        painter.setPen(textColor);
        painter.setFont(font());
        const QString text = currentText();
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(text, Qt::ElideRight, int(textRect.width())));
    }

    // ── Chevron ──────────────────────────────────────────────────────────
    // Figma: Segoe Fluent Icons 12px, color rgba(0,0,0,0.61) → textSecondary
    QColor chevronColor = enabled ? colors.textSecondary : colors.textDisabled;
    if (m_pressProgress > 0.0 && enabled) {
        qreal alphaFactor = 1.0 - 0.5 * m_pressProgress;
        chevronColor.setAlphaF(chevronColor.alphaF() * alphaFactor);
    }

    // Editable mode: draw chevron button hover/press background
    if (m_lineEdit && enabled) {
        QColor chevronBg = Qt::transparent;
        if (m_chevronHovered && m_pressed) {
            chevronBg = colors.subtleTertiary;
        } else if (m_chevronHovered) {
            chevronBg = colors.subtleSecondary;
        }
        if (chevronBg.alpha() > 0) {
            // Compact hover rect around the chevron icon with inset from edges
            const qreal pad = 4.0;   // padding around icon
            const qreal btnW = m_chevronSize + pad * 2;
            const qreal btnH = m_chevronSize + pad * 2;
            const qreal btnX = r.right() - m_chevronOffset.x() - m_chevronSize - pad;
            const qreal btnY = r.center().y() - btnH / 2.0;
            QRectF btnRect(btnX, btnY, btnW, btnH);
            QPainterPath bp;
            bp.addRoundedRect(btnRect, innerR, innerR);
            painter.setPen(Qt::NoPen);
            painter.setBrush(chevronBg);
            painter.drawPath(bp);
        }
    }

    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(m_chevronSize);

    // chevronOffset.x() = right padding, chevronOffset.y() = vertical offset
    QRectF chevronRect = QRectF(r).adjusted(0, 0, -m_chevronOffset.x(), 0);
    // Click animation: chevron bounces down like DropDownButton
    const qreal maxBounce = 3.0;
    qreal pressOffset = maxBounce * qSin(m_pressProgress * M_PI);
    chevronRect.translate(0, pressOffset + m_chevronOffset.y());

    painter.setPen(chevronColor);
    painter.setFont(iconFont);
    painter.drawText(chevronRect, Qt::AlignRight | Qt::AlignVCenter, m_chevronGlyph);
}

} // namespace fluent::basicinput
