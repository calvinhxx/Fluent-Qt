#include "Dialog.h"
#include <QPainter>
#include <QMouseEvent>
#include <QLayout>
#include <QPointer>
#include "design/Material.h"

namespace view::dialogs_flyouts {

Dialog::Dialog(QWidget *parent) : QDialog(parent) {
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::CustomizeWindowHint | Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setContentsMargins(m_shadowSize, m_shadowSize, m_shadowSize, m_shadowSize);

    m_animation = new QPropertyAnimation(this, "animationProgress", this);

    connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
        if (m_isClosing) {
            // 退场完成：先 done() 隐藏窗口，再恢复尺寸/透明度以便 Dialog 复用
            m_isClosing   = false;
            m_isAnimating = false;
            const auto targetSize = m_targetSize;
            m_targetSize  = QSize();
            QDialog::done(m_closingResult);
            // 窗口已隐藏，compositor 看不到下面的恢复操作
            if (!targetSize.isEmpty())
                resize(targetSize);
            setMinimumSize(m_savedMinSize);
            setMaximumSize(m_savedMaxSize);
            setWindowOpacity(1.0);
        } else {
            // 进场完成：恢复尺寸约束
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
    // 同步清理烟雾蒙层：Dialog 可能在 smoke 淡出未完成时被销毁
    // （如栈上 ContentDialog，exec() 返回后立即出栈）。
    // m_smokeAnim 以 Dialog 为 parent，随 Dialog 销毁而死，finished 不再触发，
    // 导致 overlay（parent=parentWidget）孤儿残留。因此此处立即销毁。
    if (m_smokeOverlay) {
        m_smokeOverlay->hide();
        delete m_smokeOverlay;
        m_smokeOverlay = nullptr;
    }
}

void Dialog::setAnimationProgress(double p) {
    m_animationProgress = p;

    // 只做 opacity 动画。
    // resize 会触发 layout 重排，子控件不会等比例缩放，导致动画期间 UI 错位。
    if (m_isAnimating) {
        setWindowOpacity(p);
    }

    update();
}

// ── 公开显示入口 ─────────────────────────────────────────────────────────────────

void Dialog::open() {
    if (m_smokeEnabled) showSmokeOverlay();
    if (m_animationEnabled && !isVisible()) {
        m_isAnimating       = true;
        m_animationProgress = 0.0;
        setWindowOpacity(0.0);          // compositor 看不到第一帧
    }
    QDialog::open();
}

int Dialog::exec() {
    if (m_smokeEnabled) showSmokeOverlay();
    if (m_animationEnabled && !isVisible()) {
        m_isAnimating       = true;
        m_animationProgress = 0.0;
        setWindowOpacity(0.0);
    }
    int result = QDialog::exec();
    hideSmokeOverlay();
    return result;
}

// ── 显示事件 ─────────────────────────────────────────────────────────────────

void Dialog::showEvent(QShowEvent *event) {
    // 蒙层模式：居中于父窗口
    if (m_smokeEnabled && parentWidget()) {
        QPoint center = parentWidget()->mapToGlobal(parentWidget()->rect().center());
        move(center.x() - width() / 2, center.y() - height() / 2);
    }

    QDialog::showEvent(event);

    if (!m_animationEnabled || !m_isAnimating) {
        m_animationProgress = 1.0;
        m_isAnimating = false;
        setWindowOpacity(1.0);
        return;
    }

    m_isClosing = false;

    // Widget 已在窗口体系中 — 递归 polish + 字体初始化
    ensurePolished();
    for (auto* w : findChildren<QWidget*>()) {
        w->ensurePolished();
        if (auto* fe = dynamic_cast<FluentElement*>(w))
            fe->onThemeUpdated();
        if (w->layout()) w->layout()->activate();
    }
    if (layout()) layout()->activate();

    // 仅 opacity 动画 —— 不再 resize，避免子控件错位
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

// ── 隐藏事件 ─────────────────────────────────────────────────────────────────

void Dialog::hideEvent(QHideEvent* event) {
    hideSmokeOverlay();
    QDialog::hideEvent(event);
}

// ── 关闭 ─────────────────────────────────────────────────────────────────────

void Dialog::done(int r) {
    if (!m_animationEnabled) {
        // macOS Core Animation 会在 hide() 时对当前帧播放系统窗口消失动画。
        // 先将窗口设为全透明，避免内容 "闪缩"。
        setWindowOpacity(0.0);
        QDialog::done(r);
        setWindowOpacity(1.0);   // 恢复，以便 Dialog 复用
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

// ── 鼠标拖拽 ─────────────────────────────────────────────────────────────────

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

// ── Smoke 蒙层管理 ──────────────────────────────────────────────────────────

void Dialog::showSmokeOverlay() {
    if (!parentWidget() || !parentWidget()->isVisible()) return;

    // 若 overlay 不存在则创建；若存在（可能正在淡出中）则复用并反向
    if (!m_smokeOverlay) {
        m_smokeOverlay = new SmokeOverlay(parentWidget());
        const auto& smoke = themeSmoke();
        QColor c = smoke.baseColor;
        c.setAlphaF(smoke.opacity);
        m_smokeOverlay->setColor(c);
        m_smokeOverlay->setGeometry(parentWidget()->rect());
        m_smokeOverlay->setProgress(0.0);
        m_smokeOverlay->show();
        m_smokeOverlay->raise();
    }

    // 懒创建动画
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

    // 淡出完成后销毁 overlay
    QPointer<SmokeOverlay> guard(m_smokeOverlay);
    connect(m_smokeAnim, &QPropertyAnimation::finished, this, [this, guard]() {
        if (!m_smokeFadingOut) return;  // 中途被 showSmokeOverlay 反向
        m_smokeFadingOut = false;
        if (guard) guard->deleteLater();
        if (m_smokeOverlay == guard.data()) m_smokeOverlay = nullptr;
    }, Qt::SingleShotConnection);

    m_smokeAnim->start();
}

void Dialog::onThemeUpdated() {
    update();
    if (m_smokeOverlay) {
        const auto& smoke = themeSmoke();
        QColor c = smoke.baseColor;
        c.setAlphaF(smoke.opacity);
        m_smokeOverlay->setColor(c);
    }
}

// ── 绘制 ─────────────────────────────────────────────────────────────────────

void Dialog::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const QRect contentRect = rect().adjusted(m_shadowSize, m_shadowSize, -m_shadowSize, -m_shadowSize);
    drawShadow(painter, contentRect);

    const auto& colors = themeColors();
    const int r = themeRadius().overlay;
    painter.setBrush(colors.bgLayer);
    painter.setPen(colors.strokeDefault);
    painter.drawRoundedRect(contentRect, r, r);
}

void Dialog::drawShadow(QPainter& painter, const QRect& contentRect) {
    const auto& s = themeShadow(Elevation::High);
    const int layers     = 10;
    const int spreadStep = 1;
    const int r          = themeRadius().overlay;

    for (int i = 0; i < layers; ++i) {
        const double ratio = 1.0 - static_cast<double>(i) / layers;
        QColor sc = s.color;
        sc.setAlphaF(s.opacity * ratio * 0.35);

        painter.setPen(Qt::NoPen);
        painter.setBrush(sc);

        const int spread  = i * spreadStep;
        const int offsetY = 2;
        painter.drawRoundedRect(
            contentRect.adjusted(-spread, -spread, spread, spread).translated(0, offsetY),
            r + spread, r + spread);
    }
}

} // namespace view::dialogs_flyouts
