#include "Dialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QLayout>
#include <QPointer>
#include <QWindow>
#include "design/Material.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayShadow.h"
#include "components/foundation/private/SurfacePainter_p.h"
#include "components/foundation/overlay/OverlayWindow.h"
#include "compatibility/QtCompat.h"

namespace fluent::dialogs_flyouts {

namespace {

void refreshFluentDescendants(QWidget* root)
{
    if (!root)
        return;

    const auto widgets = root->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* widget : widgets) {
        if (auto* fluentWidget = dynamic_cast<FluentElement*>(widget))
            fluentWidget->onThemeUpdated();
        refreshFluentDescendants(widget);
    }
}

} // namespace

Dialog::Dialog(QWidget *parent) : QDialog(parent) {
    const Qt::WindowFlags surfaceFlags =
        usesSameWindowSurfaceBackend()
            ? (Qt::Widget | Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::NoDropShadowWindowHint)
            : (Qt::Dialog | Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::NoDropShadowWindowHint);
    setWindowFlags(surfaceFlags);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);
    if (usesSameWindowSurfaceBackend()) {
        m_sameWindowOpacityEffect = new QGraphicsOpacityEffect(this);
        m_sameWindowOpacityEffect->setOpacity(1.0);
        setGraphicsEffect(m_sameWindowOpacityEffect);
    }

    m_animation = new QPropertyAnimation(this, "animationProgress", this);

    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_isClosing) {
            // Exit finished: done() hides the window first, then size/opacity
            // restore so the Dialog can be reused.
            // zh_CN: 退场完成：先 done() 隐藏窗口，再恢复尺寸/透明度以便复用。
            m_isClosing   = false;
            m_isAnimating = false;
            const auto targetSize = m_targetSize;
            m_targetSize  = QSize();
            QDialog::done(m_closingResult);
            // The window is hidden; the compositor never sees the restore below.
            // zh_CN: 窗口已隐藏，compositor 看不到下面的恢复操作。
            if (!targetSize.isEmpty())
                resize(targetSize);
            setMinimumSize(m_savedMinSize);
            setMaximumSize(m_savedMaxSize);
            setSurfaceOpacity(1.0);
        } else {
            // Entrance finished: restore the size constraints. zh_CN: 进场完成：恢复尺寸约束。
            resize(m_targetSize);
            setMinimumSize(m_savedMinSize);
            setMaximumSize(m_savedMaxSize);
            m_isAnimating = false;
            m_targetSize  = QSize();
        }
    });

    onThemeUpdated();
}

Dialog::~Dialog() {
    // Tear the smoke overlay down synchronously: the Dialog may be destroyed
    // before the smoke fade-out finishes (e.g. a stack ContentDialog popped right
    // after exec()). m_smokeAnim is parented to the Dialog and dies with it, so
    // finished never fires and the overlay (parented to parentWidget) would leak
    // as an orphan; destroy it here immediately.
    // zh_CN: 同步清理烟雾蒙层：Dialog 可能在 smoke 淡出完成前被销毁（如栈上
    // ContentDialog，exec() 返回后立即出栈）。m_smokeAnim 以 Dialog 为父对象，
    // 随之销毁后 finished 不再触发，overlay（父为 parentWidget）将孤儿残留，
    // 故在此立即销毁。
    if (m_smokeOverlay) {
        m_smokeOverlay->hide();
        delete m_smokeOverlay;
        m_smokeOverlay = nullptr;
    }
}

void Dialog::setAnimationProgress(double p) {
    m_animationProgress = p;

    // Opacity-only animation: resizing would relayout children, which do not
    // scale proportionally and would shift during the animation.
    // zh_CN: 只做 opacity 动画。resize 会触发布局重排，子控件不等比缩放，
    // 动画期间会错位。
    if (m_isAnimating) {
        setSurfaceOpacity(p);
    }

    update();
}

// ── Public show entry points. zh_CN: 公开显示入口 ────────────────────────────────

void Dialog::setThemeSource(QWidget* source) {
    if (m_themeSource == source)
        return;
    m_themeSource = source;
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
}

bool Dialog::syncThemeOverrideFromSource() {
    QWidget* source = m_themeSource ? m_themeSource.data() : parentWidget();
    return ::fluent::overlay::syncInheritedThemeOverride(this, source);
}

void Dialog::open() {
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    prepareSameWindowSurfaceSize();
    if (m_smokeEnabled) showSmokeOverlay();
    syncNativeTransientParent();
    if (m_animationEnabled && !isVisible()) {
        m_isAnimating       = true;
        m_animationProgress = 0.0;
        setSurfaceOpacity(0.0);          // The compositor never shows frame one. zh_CN: compositor 看不到第一帧。
    }
    // QDialog::open() always switches the dialog to WindowModal. Preserve an explicitly requested
    // application-modal window by using the non-blocking QWidget show path instead.
    // zh_CN: QDialog::open() 会强制切为 WindowModal；显式请求 ApplicationModal 时改走非阻塞 show。
    if (windowModality() == Qt::ApplicationModal)
        QDialog::show();
    else
        QDialog::open();
    raise();
    if (usesSameWindowSurfaceBackend())
        setFocus(Qt::ActiveWindowFocusReason);
    else
        activateWindow();
}

int Dialog::exec() {
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    prepareSameWindowSurfaceSize();
    if (m_smokeEnabled) showSmokeOverlay();
    syncNativeTransientParent();
    if (m_animationEnabled && !isVisible()) {
        m_isAnimating       = true;
        m_animationProgress = 0.0;
        setSurfaceOpacity(0.0);
    }
    int result = QDialog::exec();
    hideSmokeOverlay();
    return result;
}

// ── Show events. zh_CN: 显示事件 ─────────────────────────────────────────────

void Dialog::showEvent(QShowEvent *event) {
    if (syncThemeOverrideFromSource())
        onThemeUpdated();
    syncNativeTransientParent();

    // Smoke mode: center over the parent window. zh_CN: 蒙层模式：居中于父窗口。
    if (usesSameWindowSurfaceBackend()) {
        positionSameWindowSurface();
    } else if (m_smokeEnabled && parentWidget()) {
        const QRect surface = ::fluent::overlay::overlaySurfaceRect(parentWidget());
        const QPoint center = parentWidget()->mapToGlobal(surface.center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    }
    QDialog::showEvent(event);
    if (m_smokeOverlay)
        m_smokeOverlay->raise();
    raise();

    if (!m_animationEnabled || !m_isAnimating) {
        m_animationProgress = 1.0;
        m_isAnimating = false;
        setSurfaceOpacity(1.0);
        return;
    }

    m_isClosing = false;

    // The widget is in the window hierarchy: polish recursively and seed fonts.
    // zh_CN: Widget 已在窗口体系中 — 递归 polish + 字体初始化。
    ensurePolished();
    for (auto* w : findChildren<QWidget*>()) {
        w->ensurePolished();
        if (auto* fe = dynamic_cast<FluentElement*>(w))
            fe->onThemeUpdated();
        if (w->layout()) w->layout()->activate();
    }
    if (layout()) layout()->activate();

    // Opacity only; no resize, so children never shift. zh_CN: 仅 opacity 动画，不再 resize，避免子控件错位。
    m_targetSize   = size();
    m_savedMinSize = minimumSize();
    m_savedMaxSize = maximumSize();

    const auto& anim = themeAnimation();
    m_animation->stop();
    m_animation->setDuration(anim.normal);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setEasingCurve(anim.entrance);
    m_animation->start();
}

// ── Hide events. zh_CN: 隐藏事件 ─────────────────────────────────────────────

void Dialog::hideEvent(QHideEvent* event) {
    hideSmokeOverlay();
    QDialog::hideEvent(event);
}

bool Dialog::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_smokeOverlay && event) {
        const QEvent::Type type = event->type();
        if (type == QEvent::MouseButtonPress
            || type == QEvent::MouseButtonDblClick
            || type == QEvent::MouseButtonRelease) {
            if (isVisible()) {
                raise();
                activateWindow();
            }
            event->accept();
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void Dialog::syncNativeTransientParent() {
    if (usesSameWindowSurfaceBackend())
        return;

    QWidget* owner = parentWidget() ? parentWidget()->window() : nullptr;
    if (!owner)
        return;

    owner->winId();
    winId();
    QWindow* dialogWindow = windowHandle();
    QWindow* ownerWindow = owner->windowHandle();
    if (dialogWindow && ownerWindow && dialogWindow->transientParent() != ownerWindow)
        dialogWindow->setTransientParent(ownerWindow);
}

bool Dialog::usesSameWindowSurfaceBackend() const {
    return ::fluent::overlay::linuxDesktopUsesSameWindowSurfaces() && parentWidget();
}

void Dialog::prepareSameWindowSurfaceSize() {
    if (!usesSameWindowSurfaceBackend())
        return;

    ensurePolished();
    for (auto* child : findChildren<QWidget*>())
        child->ensurePolished();
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }

    const int collapsedHeight = m_shadowSize * 2 + 48;
    if (height() > collapsedHeight)
        return;

    QSize preferred = sizeHint();
    if (!minimumSizeHint().isEmpty())
        preferred = preferred.expandedTo(minimumSizeHint());
    preferred = preferred.expandedTo(minimumSize());
    if (!preferred.isValid() || preferred.isEmpty())
        return;

    QSize next(qMax(width(), preferred.width()), qMax(height(), preferred.height()));
    next = next.expandedTo(minimumSize());
    next = next.boundedTo(maximumSize());
    if (next != size())
        resize(next);
}

void Dialog::positionSameWindowSurface() {
    QWidget* owner = parentWidget();
    if (!owner)
        return;

    QRect surface = ::fluent::overlay::overlaySurfaceRect(owner);
    if (surface.isEmpty())
        surface = owner->rect();

    const QRect current(pos(), size());
    const bool defaultPlacement = pos().isNull();
    const bool outsideSurface = !surface.contains(current);
    QPoint next = pos();
    if (m_smokeEnabled || defaultPlacement || outsideSurface) {
        next = QPoint(surface.left() + (surface.width() - width()) / 2,
                      surface.top() + (surface.height() - height()) / 2);
    }

    if (width() <= surface.width())
        next.setX(qBound(surface.left(), next.x(), surface.right() - width() + 1));
    else
        next.setX(surface.left());
    if (height() <= surface.height())
        next.setY(qBound(surface.top(), next.y(), surface.bottom() - height() + 1));
    else
        next.setY(surface.top());

    if (next != pos())
        move(next);
}

void Dialog::setSurfaceOpacity(qreal opacity) {
    const qreal clamped = qBound<qreal>(0.0, opacity, 1.0);
    if (m_sameWindowOpacityEffect) {
        m_sameWindowOpacityEffect->setOpacity(clamped);
        return;
    }
    setWindowOpacity(clamped);
}

// ── Closing. zh_CN: 关闭 ─────────────────────────────────────────────────────

void Dialog::done(int r) {
    if (!m_animationEnabled) {
        // macOS Core Animation plays its own dismiss animation on the last frame
        // at hide(); make the window fully transparent first to avoid a flash-shrink.
        // zh_CN: macOS Core Animation 会在 hide() 时对当前帧播放系统消失动画；
        // 先将窗口设为全透明，避免内容“闪缩”。
        setSurfaceOpacity(0.0);
        QDialog::done(r);
        setSurfaceOpacity(1.0);   // Restore for Dialog reuse. zh_CN: 恢复，以便复用。
        return;
    }

    if (m_isClosing) return;
    m_isClosing     = true;
    m_closingResult = r;

    m_animation->stop();

    if (!m_isAnimating) {
        m_targetSize        = size();
        m_savedMinSize      = minimumSize();
        m_savedMaxSize      = maximumSize();
        setMinimumSize(QSize(0, 0));
        setMaximumSize(QSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX));
        m_isAnimating       = true;
        m_animationProgress = 1.0;
    }

    const auto& anim = themeAnimation();
    m_animation->setDuration(anim.normal);
    m_animation->setStartValue(m_animationProgress);
    m_animation->setEndValue(0.0);
    m_animation->setEasingCurve(anim.exit);
    m_animation->start();
}

// ── Mouse dragging. zh_CN: 鼠标拖拽 ──────────────────────────────────────────

void Dialog::mousePressEvent(QMouseEvent *event) {
    if (m_dragEnabled && event->button() == Qt::LeftButton) {
        m_dragPosition = fluentMouseGlobalPos(event) - frameGeometry().topLeft();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    QDialog::mousePressEvent(event);
}

void Dialog::mouseMoveEvent(QMouseEvent *event) {
    if (cursor().shape() == Qt::ClosedHandCursor) {
        move(fluentMouseGlobalPos(event) - m_dragPosition);
        event->accept();
    }
    QDialog::mouseMoveEvent(event);
}

void Dialog::mouseReleaseEvent(QMouseEvent *event) {
    if (cursor().shape() == Qt::ClosedHandCursor) unsetCursor();
    QDialog::mouseReleaseEvent(event);
}

// ── Smoke overlay management. zh_CN: Smoke 蒙层管理 ─────────────────────────

void Dialog::showSmokeOverlay() {
    if (!parentWidget() || !parentWidget()->isVisible()) return;

    // Create the overlay when missing; reuse and reverse it when it exists
    // (it may be mid fade-out).
    // zh_CN: overlay 不存在则创建；存在（可能正在淡出）则复用并反向。
    const int surfaceRadius = qRound(::fluent::overlay::overlaySurfaceRadius(parentWidget()));
    if (!m_smokeOverlay) {
        m_smokeOverlay = new SmokeOverlay(parentWidget());
        const auto& smoke = themeSmoke();
        QColor c = smoke.baseColor;
        c.setAlphaF(smoke.opacity);
        m_smokeOverlay->setColor(c);
        m_smokeOverlay->setGeometry(::fluent::overlay::overlaySurfaceRect(parentWidget()));
        m_smokeOverlay->setSurfaceRadius(surfaceRadius);
        m_smokeOverlay->setProgress(0.0);
        m_smokeOverlay->installEventFilter(this);
        m_smokeOverlay->show();
        m_smokeOverlay->raise();
    } else {
        m_smokeOverlay->setGeometry(::fluent::overlay::overlaySurfaceRect(parentWidget()));
        m_smokeOverlay->setSurfaceRadius(surfaceRadius);
        m_smokeOverlay->raise();
    }
    raise();

    // Lazily create the animation. zh_CN: 懒创建动画。
    if (!m_smokeAnim) {
        m_smokeAnim = new QPropertyAnimation(this);
        m_smokeAnim->setPropertyName("progress");
    }
    m_smokeAnim->stop();
    m_smokeAnim->setTargetObject(m_smokeOverlay);
    const auto& anim = themeAnimation();
    m_smokeAnim->setDuration(anim.normal);
    m_smokeAnim->setStartValue(m_smokeOverlay->progress());
    m_smokeAnim->setEndValue(1.0);
    m_smokeAnim->setEasingCurve(anim.entrance);
    m_smokeFadingOut = false;
    m_smokeAnim->start();
}

void Dialog::hideSmokeOverlay() {
    if (!m_smokeOverlay || m_smokeFadingOut) return;

    if (!m_smokeAnim) {
        m_smokeAnim = new QPropertyAnimation(this);
        m_smokeAnim->setPropertyName("progress");
    }
    m_smokeAnim->stop();
    m_smokeAnim->setTargetObject(m_smokeOverlay);
    const auto& anim = themeAnimation();
    m_smokeAnim->setDuration(anim.normal);
    m_smokeAnim->setStartValue(m_smokeOverlay->progress());
    m_smokeAnim->setEndValue(0.0);
    m_smokeAnim->setEasingCurve(anim.exit);
    m_smokeFadingOut = true;

    // Destroy the overlay after the fade-out completes. zh_CN: 淡出完成后销毁 overlay。
    QPointer<SmokeOverlay> guard(m_smokeOverlay);
    const auto finishSmokeFadeOut = [this, guard]() {
        if (!m_smokeFadingOut) return;  // Reversed mid-flight by showSmokeOverlay. zh_CN: 中途被 showSmokeOverlay 反向。
        m_smokeFadingOut = false;
        if (guard) guard->deleteLater();
        if (m_smokeOverlay == guard.data()) m_smokeOverlay = nullptr;
    };
    fluentConnectSingleShot(m_smokeAnim, &QPropertyAnimation::finished, this, finishSmokeFadeOut);

    m_smokeAnim->start();
}

void Dialog::onThemeUpdated() {
    update();
    refreshFluentDescendants(this);
    if (m_smokeOverlay) {
        const auto& smoke = themeSmoke();
        QColor c = smoke.baseColor;
        c.setAlphaF(smoke.opacity);
        m_smokeOverlay->setColor(c);
    }
}

// ── Painting. zh_CN: 绘制 ────────────────────────────────────────────────────

void Dialog::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!usesSameWindowSurfaceBackend()) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    const QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);
    drawShadow(painter, contentRect);

    const auto& colors = themeColors();
    const int r = themeRadius().overlay;
    const DesignLanguage lang = themeDesignLanguage();

    // Per design language only the card SURFACE fill split and the BORDER pen differ — the
    // shadow / smoke-dim above is shared. zh_CN: 按设计语言仅「卡片表面填充」与「边框画笔」不同——
    // 上方阴影 / 蒙层逻辑全部共享。
    fluent::painting::RoundedSurfacePaint surface;
    surface.fill = colors.bgLayer;
    surface.radius = r;
    if (lang == DesignMaterial) {
        // Material 3 dialog: a single tonal surface, NO border stroke — elevation is conveyed by the
        // shadow alone. zh_CN: Material 3 对话框:单一色调表面、无边框描边——高度仅由阴影表达。
        surface.border = Qt::transparent;
    } else if (lang == DesignCupertino) {
        // macOS alert/sheet: a crisp 1px hairline edge using the stronger neutral stroke. zh_CN:
        // macOS 警告/sheet:用更强的中性描边绘制清晰的 1px 发丝边缘。
        surface.border = colors.strokeStrong;
    } else {
        // DesignFluent (default): unchanged WinUI overlay stroke. zh_CN: 默认 Fluent,WinUI 浮层描边不变。
        surface.border = colors.strokeDefault;
    }
    fluent::painting::paintRoundedSurface(painter, QRectF(contentRect), surface);
}

void Dialog::drawShadow(QPainter& painter, const QRect& contentRect) {
    // Medium, not High: the High dark-theme shadow is blur 8 / alpha 0.50, which — stacked on the
    // smoke dim — reads as a heavy dark halo around the card. Medium (blur 4 / alpha 0.40) keeps the
    // card lifted without the bruise. zh_CN: 用 Medium 而非 High：High 暗色阴影是 blur 8 / alpha 0.50,叠加蒙层
    // 后在卡片周围形成沉重暗晕。Medium(blur 4 / alpha 0.40)保留悬浮感又不发黑。
    ::fluent::overlay::paintLayeredShadow(painter, contentRect, themeRadius().overlay,
                                          themeShadow(Elevation::Medium));
}

} // namespace fluent::dialogs_flyouts
