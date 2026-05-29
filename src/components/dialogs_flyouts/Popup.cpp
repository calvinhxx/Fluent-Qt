#include "Popup.h"

#include <QPainter>
#include <QPainterPath>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include "compatibility/QtCompat.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayLightDismiss.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/foundation/overlay/OverlayWindow.h"

namespace fluent::dialogs_flyouts {

// ── 构造 / 析构 ──────────────────────────────────────────────────────────────

Popup::Popup(QWidget* parent) : QWidget(parent) {
    m_originalParent = parent;
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setFocusPolicy(Qt::StrongFocus);
    const int shadowMargin = ::fluent::overlay::defaultShadowMargin();
    setContentsMargins(shadowMargin, shadowMargin, shadowMargin, shadowMargin);
    resize(::fluent::overlay::outerSizeForVisibleCard(QSize(320, 160), shadowMargin));

    m_anim = new QPropertyAnimation(this, "popupProgress", this);
    connect(m_anim, &QPropertyAnimation::finished, this, [this]() {
        if (m_isClosing) finalizeClosed();
        else             finalizeOpened();
    });

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(0.0);
    setGraphicsEffect(m_opacityEffect);

    // 必须显式 hide：若父窗口 show() 时 Popup 没被 hide 过，Qt 会自动把它显现出来
    // （opacity=0 视觉不可见，但仍会拦截鼠标事件，导致按钮点击失效）
    hide();

    onThemeUpdated();
}

Popup::~Popup() {
    if (qApp)
        qApp->removeEventFilter(this);
    if (m_scrim) {
        m_scrim->hide();
        delete m_scrim;
        m_scrim = nullptr;
    }
}

// ── 主题 ─────────────────────────────────────────────────────────────────────

void Popup::onThemeUpdated() {
    update();
    if (m_scrim) {
        if (auto* fe = dynamic_cast<FluentElement*>(m_scrim.data()))
            fe->onThemeUpdated();
    }
}

// ── popupProgress ────────────────────────────────────────────────────────────

void Popup::setPopupProgress(double p) {
    if (qFuzzyCompare(m_popupProgress, p)) return;
    m_popupProgress = p;
    if (m_opacityEffect) m_opacityEffect->setOpacity(p);
    emit popupProgressChanged(p);
    update();
}

// ── topLevelWidget 推断 ──────────────────────────────────────────────────────

QWidget* Popup::originalParentTopLevel() const {
    return ::fluent::overlay::resolveOwningTopLevel(m_originalParent, parentWidget());
}

// ── setPosition ──────────────────────────────────────────────────────────────

void Popup::setPosition(QWidget* relativeTo, const QPoint& localPos) {
    if (!relativeTo) return;
    QWidget* top = relativeTo->window();
    m_targetPos = relativeTo->mapTo(top, localPos);
    m_positionSet = true;
}

// ── 位置计算（默认居中，子类可重写）──────────────────────────────────────────

QPoint Popup::computePosition() const {
    QWidget* top = originalParentTopLevel();
    if (!top) return pos();
    // 默认：在 topLevelWidget 中居中
    return QPoint((top->width() - width()) / 2,
                  (top->height() - height()) / 2);
}

// ── open / close ─────────────────────────────────────────────────────────────

void Popup::open() {
    if (m_isOpen && !m_isClosing) return;
    m_anim->stop();
    m_isClosing = false;

    QWidget* top = originalParentTopLevel();
    m_topLevel = top;
    ::fluent::overlay::attachToTopLevel(this, top);

    emit aboutToShow();

    ensureScrim();

    ensurePolished();
    if (layout()) layout()->activate();

    // layout 驱动尺寸
    if (layout()) {
        QSize hint = layout()->totalSizeHint();
        if (hint.isValid() && !hint.isEmpty())
            resize(hint);
    }

    // 定位：用户通过 setPosition() 设置过则用目标位置，否则居中。
    if (m_positionSet)
        move(::fluent::overlay::outerTopLeftForVisibleCard(m_targetPos));
    else
        move(computePosition());

    show();
    ::fluent::overlay::raiseOverlayStack(m_scrim, this);
    setFocus(Qt::PopupFocusReason);

    if (qApp)
        qApp->installEventFilter(this);

    if (!m_animationEnabled) {
        setPopupProgress(1.0);
        m_isOpen = true;
        emit isOpenChanged(true);
        emit opened();
        return;
    }

    m_popupProgress = 0.0;
    startEnterAnimation();
}

void Popup::close() {
    if (!m_isOpen && !m_anim->state()) {
        if (!isVisible()) return;
    }
    if (m_isClosing) return;

    m_anim->stop();
    m_isClosing = true;
    emit aboutToHide();

    if (qApp)
        qApp->removeEventFilter(this);

    if (!m_animationEnabled) {
        setPopupProgress(0.0);
        finalizeClosed();
        return;
    }

    startExitAnimation();
}

void Popup::setIsOpen(bool open) {
    if (open) this->open();
    else      this->close();
}

// ── 动画 ─────────────────────────────────────────────────────────────────────

void Popup::startEnterAnimation() {
    const auto& a = themeAnimation();
    m_anim->setDuration(a.normal);       // 与 Dialog 一致：250ms
    m_anim->setStartValue(m_popupProgress);
    m_anim->setEndValue(1.0);
    m_anim->setEasingCurve(a.entrance); // 与 Dialog 一致
    m_anim->start();
}

void Popup::startExitAnimation() {
    const auto& a = themeAnimation();
    m_anim->setDuration(a.normal);       // 与 Dialog 一致：250ms
    m_anim->setStartValue(m_popupProgress);
    m_anim->setEndValue(0.0);
    m_anim->setEasingCurve(a.exit);     // 与 Dialog 一致
    m_anim->start();
}

void Popup::finalizeOpened() {
    if (m_isOpen) return;
    m_isOpen = true;
    emit isOpenChanged(true);
    emit opened();
}

void Popup::finalizeClosed() {
    m_isClosing = false;
    hide();
    destroyScrim();
    if (m_isOpen) {
        m_isOpen = false;
        emit isOpenChanged(false);
    }
    emit closed();
}

// ── Scrim ────────────────────────────────────────────────────────────────────

void Popup::ensureScrim() {
    if (!m_modal) return;
    QWidget* top = m_topLevel ? m_topLevel.data() : originalParentTopLevel();
    if (!top) return;

    if (!m_scrim)
        m_scrim = new ::fluent::overlay::OverlayScrim(top, QStringLiteral("PopupScrim"));
    if (auto* scrim = dynamic_cast<::fluent::overlay::OverlayScrim*>(m_scrim.data()))
        scrim->setModalAndDim(true, m_dim);
    m_scrim->setGeometry(top->rect());
    m_scrim->show();
    ::fluent::overlay::raiseOverlayStack(m_scrim, this);
}

void Popup::destroyScrim() {
    if (!m_scrim) return;
    m_scrim->hide();
    m_scrim->deleteLater();
    m_scrim = nullptr;
}

// ── Light-dismiss / Escape ──────────────────────────────────────────────────

bool Popup::eventFilter(QObject* watched, QEvent* event) {
    if (event && event->type() == QEvent::Destroy && watched == m_topLevel) {
        if (qApp)
            qApp->removeEventFilter(this);
        m_topLevel = nullptr;
        m_scrim = nullptr;
        return false;
    }

    if (event && event->type() == QEvent::Resize && watched == m_topLevel) {
        if (m_scrim && m_topLevel)
            m_scrim->setGeometry(m_topLevel->rect());
        if (isVisible()) {
            if (!m_positionSet)
                move(computePosition());
            ::fluent::overlay::raiseOverlayStack(m_scrim, this);
        }
        return false;
    }

    if (!m_isOpen && !isVisible()) return false;

    const bool noAutoClose = m_closePolicy == ClosePolicy(NoAutoClose);
    if (::fluent::overlay::isEscapeKeyPress(event) &&
        ::fluent::overlay::allowsImplicitClose(noAutoClose, m_closePolicy & CloseOnEscape)) {
        close();
        event->accept();
        return true;
    }

    if (!::fluent::overlay::allowsImplicitClose(noAutoClose, m_closePolicy & CloseOnPressOutside)) return false;
    if (!event || event->type() != QEvent::MouseButtonPress) return false;

    auto* me = static_cast<QMouseEvent*>(event);
    const QPoint globalPos = fluentMouseGlobalPos(me);
    const QPoint local = mapFromGlobal(globalPos);
    if (::fluent::overlay::visibleCardContains(rect(), local)) return false;

    close();
    return false;
}

void Popup::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape && (m_closePolicy & CloseOnEscape)) {
        close();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ── 绘制 ─────────────────────────────────────────────────────────────────────

void Popup::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 仅做 opacity 动画（QGraphicsOpacityEffect 统一作用于背景与子控件）。

    const QRect contentRect = ::fluent::overlay::visibleCardRect(rect());

    // 阴影
    const auto& s = themeShadow(Elevation::High);
    const int r = themeRadius().overlay;
    {
        const int layers = 10;
        for (int i = 0; i < layers; ++i) {
            const double ratio = 1.0 - static_cast<double>(i) / layers;
            QColor sc = s.color;
            sc.setAlphaF(s.opacity * ratio * 0.35);
            painter.setPen(Qt::NoPen);
            painter.setBrush(sc);
            painter.drawRoundedRect(
                contentRect.adjusted(-i, -i, i, i).translated(0, 2),
                r + i, r + i);
        }
    }

    // 背景 + 边框
    const auto& colors = themeColors();
    painter.setBrush(colors.bgLayer);
    painter.setPen(colors.strokeDefault);
    painter.drawRoundedRect(contentRect, r, r);
}

} // namespace fluent::dialogs_flyouts
