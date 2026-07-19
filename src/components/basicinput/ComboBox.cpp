#include "ComboBox.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QApplication>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QWheelEvent>
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
static constexpr int kPopupItemOuterInset = 5;
static constexpr int kPopupItemTextLeftInset = 16;
static constexpr int kPopupItemTextRightInset = 8;
static constexpr qreal kPopupTextOpticalOffsetY = -2.0;
static constexpr int kClosedFieldTextFitClearance = ::Spacing::XSmall;

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

    int itemRightInset = kPopupItemOuterInset;
    if (m_view && m_view->verticalScrollBar() &&
        m_view->verticalScrollBar()->maximum() > m_view->verticalScrollBar()->minimum()) {
        if (auto* listView = qobject_cast<fluent::collections::ListView*>(m_view)) {
            if (auto* scrollBar = listView->verticalFluentScrollBar()) {
                itemRightInset += scrollBar->thickness();
            }
        }
    }
    QRectF bgRect = QRectF(option.rect).adjusted(kPopupItemOuterInset, 3,
                                                -itemRightInset, -3);
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
    QRectF textRect = bgRect.adjusted(kPopupItemTextLeftInset, 0,
                                      -kPopupItemTextRightInset, 0);
    // Center the actual UI-font ink rather than its asymmetric ascent/descent line box.
    // zh_CN: 按 UI 字体的字形墨迹居中，而不是按上下不对称的 ascent/descent 行框居中。
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
    // The Flyout card (Popup::paintEvent) already paints an opaque bgLayer surface rounded at the
    // overlay radius (8px). A second ListView background would also paint an opaque corner mask
    // rounded at only the control radius (4px) and inset just 2px, so its filled corners poke past
    // the card's 8px-rounded corners as white "dog-ears". Let the card be the single surface.
    // zh_CN: Flyout 卡片(Popup::paintEvent)已绘制按 overlay 圆角(8px)的不透明 bgLayer 表面。再让 ListView
    // 画背景会叠加一层按 control 圆角(4px)、仅内缩 2px 的不透明圆角遮罩,其填充的四角会超出卡片 8px 圆角,
    // 形成白色「狗耳」。让卡片成为唯一表面即可。
    m_listView->setBackgroundVisible(false);
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
    int widestText = 0;
    const QFontMetrics popupMetrics(m_listView->font());
    for (int index = 0; index < itemCount; ++index)
        widestText = qMax(widestText,
                          popupMetrics.horizontalAdvance(m_comboBox->itemText(index)));

    int scrollClearance = 0;
    if (itemCount > maxVisible) {
        scrollClearance = ::Spacing::XSmall;
        if (auto* scrollBar = m_listView->verticalFluentScrollBar())
            scrollClearance = qMax(scrollClearance, scrollBar->thickness());
    }
    const int textChrome = cardInset * 2
        + kPopupItemOuterInset * 2
        + kPopupItemTextLeftInset
        + kPopupItemTextRightInset
        + scrollClearance
        + ::Spacing::Standard;
    const int cardW = qMax(qMax(m_comboBox->width(), 120), widestText + textChrome);
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

    const QRect surface = ::fluent::overlay::overlaySurfaceRect(top);
    const int spaceBelow = surface.bottom() - anchor.bottom();
    const int spaceAbove = anchor.top() - surface.top();
    // Include the anchor gap and the surface safety margin in the fit test. If
    // only the card height is considered, a popup that is a few pixels too tall
    // is first placed below and then clamped upward across its owning field.
    // zh_CN: 适配判断必须计入锚点间距与表面安全边距；只比较卡片高度会让仅差数像素的
    // 弹层先向下打开，再被钳制回输入框上方并与其重叠。
    const int requiredSpace = anchorOffset() + cardH + kPopupWindowMargin;
    const bool fitsBelow = spaceBelow >= requiredSpace;
    const bool fitsAbove = spaceAbove >= requiredSpace;
    const bool placeAbove = !fitsBelow && (fitsAbove || spaceAbove > spaceBelow);

    QPoint cardTopLeft(anchor.left(), placeAbove
        ? anchor.top() - anchorOffset() - cardH
        : anchor.bottom() + 1 + anchorOffset());

    if (clampToWindow()) {
        cardTopLeft = ::fluent::overlay::clampCardTopLeft(
            cardTopLeft, cardSize, surface, kPopupWindowMargin);
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
    // QComboBox defaults to WheelFocus, which grants focus before wheelEvent and changes the
    // selection merely by hovering and scrolling. Fluent ComboBox accepts wheel selection only
    // after deliberate click/tab focus. zh_CN: QComboBox 默认 WheelFocus，会在 wheelEvent 前先抢
    // 焦点，导致仅悬停滚轮就切换选项；Fluent ComboBox 只接受点击/Tab 明确取得焦点后的滚轮输入。
    setFocusPolicy(Qt::StrongFocus);
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
    // QFontMetrics::horizontalAdvance() is the logical glyph advance. Leave a
    // small clearance so integer layout/raster rounding cannot make an item
    // that nominally fits cross the elision boundary on another platform.
    // zh_CN: 为字形逻辑宽度预留少量余量，避免不同平台的整数布局和栅格化舍入触发省略号。
    const int w = m_contentPaddingH + maxTextW + kClosedFieldTextFitClearance + chevronArea;
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

void ComboBox::wheelEvent(QWheelEvent* event) {
    const bool ownsFocus = hasFocus() || (m_lineEdit && m_lineEdit->hasFocus());
    if (!ownsFocus) {
        event->ignore();
        return;
    }
    QComboBox::wheelEvent(event);
}

void ComboBox::keyPressEvent(QKeyEvent* event) {
    const bool ownsFocus = hasFocus() || (m_lineEdit && m_lineEdit->hasFocus());
    if (!ownsFocus) {
        event->ignore();
        return;
    }
    QComboBox::keyPressEvent(event);
}

void ComboBox::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (!hasFocus())
            setFocus(Qt::MouseFocusReason);
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

    // Branch the closed-field surface on the active design language. The chevron and text
    // (drawn further down) are shared; only the field background/border differ per brand.
    // zh_CN: 闭合表面按当前设计语言分支绘制。下方的箭头与文字为共用部分,仅字段背景/边框按品牌区分。
    const DesignLanguage lang = themeDesignLanguage();
    // Theme-aware interaction veil (same idiom as Button): DARKEN light surfaces, LIGHTEN dark
    // ones so hover/press stay visible under both App themes. zh_CN: 主题感知交互薄层(与 Button 同):
    // 浅色面变暗、深色面变亮,使 hover/press 在明暗两主题下都可见。
    const bool darkTheme = effectiveTheme() == Dark;
    const auto veil = [darkTheme](int alpha) {
        return darkTheme ? QColor(255, 255, 255, alpha) : QColor(0, 0, 0, alpha);
    };

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

    // "Open" reads as focus for the closed field on every brand (popup up or line-edit focused).
    // zh_CN: 对闭合字段而言,"展开"等同于聚焦(弹层打开或输入框聚焦)。
    const bool fieldFocused = enabled && (lineEditFocused || m_popupVisible);

    if (lang == DesignMaterial) {
        // Material 3: outlined text-field look. Transparent/controlDefault fill, a 1 dp `outline`
        // (strokeStrong) at rest, a 2 dp `primary` (accentDefault) outline when focused/open. Hover
        // adds a within-bounds state-layer veil (never a halo beyond the widget). §4/§5.
        // zh_CN: Material 3:描边文本框外观。透明/controlDefault 填充,静息 1dp outline(strokeStrong),
        // 聚焦/展开时 2dp primary(accentDefault) 描边。Hover 叠加界内 state-layer 薄层(不画超出控件的光晕)。
        const qreal mR = qMax<qreal>(0.0, radius.control);
        QColor fill = enabled ? (m_pressed || m_popupVisible ? colors.controlDefault : QColor(Qt::transparent))
                              : colors.controlDisabled;

        QPainterPath bgPath;
        bgPath.addRoundedRect(r.adjusted(1.0, 1.0, -1.0, -1.0), mR, mR);
        painter.setPen(Qt::NoPen);
        if (fill != Qt::transparent) {
            painter.setBrush(fill);
            painter.drawPath(bgPath);
        }

        // State layer veil, clipped inside the field so it can't bleed into the surrounding band.
        // zh_CN: state-layer 薄层,裁剪在字段内,避免溢出到周围条带。
        if (enabled && !fieldFocused) {
            QColor stateLayer;
            if (m_pressed) stateLayer = veil(0x1A);       // 10%
            else if (m_hovered) stateLayer = veil(0x14);  // 8%
            if (stateLayer.isValid() && stateLayer.alpha() > 0) {
                painter.save();
                painter.setClipPath(bgPath);
                painter.setBrush(stateLayer);
                painter.drawPath(bgPath);
                painter.restore();
            }
        }

        // Outline: 1 dp strokeStrong at rest, 2 dp accent when focused/open. zh_CN: 描边:静息 1dp strokeStrong,聚焦/展开 2dp accent。
        const qreal sw = fieldFocused ? 2.0 : 1.0;
        const QColor outline = !enabled ? colors.strokeSecondary
                                        : (fieldFocused ? colors.accentDefault : colors.strokeStrong);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outline, sw));
        const qreal inset = sw / 2.0;
        painter.drawRoundedRect(r.adjusted(inset, inset, -inset, -inset), mR, mR);
    } else if (lang == DesignCupertino) {
        // macOS: bezel push-button look. Subtle fill + 1px strokeStrong hairline + small radius (~6)
        // + a faint 1px lower-edge drop shadow (matching the Button bezel). Pressed/open darken via the
        // theme-aware veil; focus/open gets an accent hairline. zh_CN: macOS:bezel 按钮外观。柔和填充 +
        // 1px strokeStrong 发丝边框 + 小圆角(~6) + 底缘 1px 柔和投影(与 Button bezel 一致)。按下/展开用主题感知
        // 薄层压暗;聚焦/展开使用强调色发丝边。
        const qreal mR = qMax<qreal>(0.0, qMin<qreal>(radius.control, 6.0));
        QColor fill = enabled ? colors.bgLayerAlt : colors.controlDisabled;

        QPainterPath bgPath;
        bgPath.addRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
        painter.setPen(Qt::NoPen);
        painter.setBrush(fill);
        painter.drawPath(bgPath);

        // Pressed/hover veil over the bezel keeps feedback visible on both light and dark.
        // zh_CN: bezel 上叠加按下/悬停薄层,使反馈在浅/深主题上都可见。
        if (enabled) {
            QColor stateLayer;
            if (m_pressed || m_popupVisible) stateLayer = veil(darkTheme ? 0x3A : 0x30);
            else if (m_hovered) stateLayer = veil(darkTheme ? 0x1C : 0x16);
            // NOTE: guard on isValid() too — a default-constructed QColor is INVALID but reports
            // alpha()==255 (Qt stores invalid as 0xFFFF alpha), so a bare alpha()>0 check would fire
            // at rest and setBrush(invalid) paints SOLID BLACK over the bezel. zh_CN: 必须同时判 isValid()
            // ——默认构造的 QColor 虽无效,alpha() 却返回 255(Qt 用 0xFFFF 存无效 alpha),裸 alpha()>0 会在静息
            // 命中,setBrush(无效色) 会把 bezel 整片涂成不透明黑。
            if (stateLayer.isValid() && stateLayer.alpha() > 0) {
                painter.save();
                painter.setClipPath(bgPath);
                painter.setBrush(stateLayer);
                painter.drawPath(bgPath);
                painter.restore();
            }
        }

        // Faint 1px drop shadow hugging the bottom inner edge — sells the raised bezel. zh_CN: 贴底缘内侧 1px 柔和投影——读出凸起 bezel。
        if (enabled && !m_pressed && !m_popupVisible) {
            painter.save();
            painter.setClipPath(bgPath);
            painter.setBrush(Qt::NoBrush);
            QColor shadow(0, 0, 0, darkTheme ? 0x3A : 0x1E);
            painter.setPen(QPen(shadow, 1.0));
            painter.drawRoundedRect(r.adjusted(1.0, 2.0, -1.0, -1.0), mR, mR);
            painter.restore();
        }

        // Hairline border: strokeStrong at rest, accent when focused/open. zh_CN: 发丝边框:静息 strokeStrong,聚焦/展开 accent。
        const QColor hairline = !enabled ? colors.strokeSecondary
                                         : (fieldFocused ? colors.accentDefault : colors.strokeStrong);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(hairline, 1.0));
        painter.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), mR, mR);
    } else {
        // DesignFluent (default): unchanged WinUI 3 treatment. zh_CN: 默认 Fluent,WinUI 3 处理不变。
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
    // Figma: 12 px symbol, color rgba(0,0,0,0.61) → textSecondary
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
            // Round the chevron hover chip to the control radius (was innerR in the Fluent path,
            // now computed locally so it is independent of the per-language field branch above).
            // zh_CN: 箭头悬停小块圆角取控件圆角(原 Fluent 路径用 innerR),此处本地计算,与上方分支解耦。
            const qreal chipR = qMax<qreal>(0.0, radius.control - 1.0);
            QPainterPath bp;
            bp.addRoundedRect(btnRect, chipR, chipR);
            painter.setPen(Qt::NoPen);
            painter.setBrush(chevronBg);
            painter.drawPath(bp);
        }
    }

    const QFont iconFont = Typography::Icons::font(m_chevronSize);
    const QString chevronGlyph = Typography::Icons::glyphForSize(
        m_chevronGlyph, m_chevronSize);

    // chevronOffset.x() = right padding, chevronOffset.y() = vertical offset
    QRectF chevronRect = QRectF(r).adjusted(0, 0, -m_chevronOffset.x(), 0);
    // Click animation: chevron bounces down like DropDownButton
    const qreal maxBounce = 3.0;
    qreal pressOffset = maxBounce * qSin(m_pressProgress * M_PI);
    chevronRect.translate(0, pressOffset + m_chevronOffset.y());

    painter.setPen(chevronColor);
    painter.setFont(iconFont);
    painter.drawText(chevronRect, Qt::AlignRight | Qt::AlignVCenter, chevronGlyph);
}

} // namespace fluent::basicinput
