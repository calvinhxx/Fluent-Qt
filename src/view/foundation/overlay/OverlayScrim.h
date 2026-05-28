#ifndef OVERLAYSCRIM_H
#define OVERLAYSCRIM_H

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <cmath>

#include "view/foundation/FluentElement.h"

namespace view::overlay {

/**
 * @brief Same-window scrim used behind modal or drawer-style overlays.
 * zh_CN: 用于模态或抽屉类同窗口浮层背后的遮罩层。
 *
 * OverlayScrim provides dim painting and input interception so overlay hosts can
 * separate backdrop behavior from the visible card or drawer content.
 * zh_CN: OverlayScrim 提供遮罩绘制和输入拦截，使浮层宿主可以将背景行为与可见卡片
 * 或抽屉内容分离。
 */
class OverlayScrim : public QWidget, public FluentElement {
public:
    explicit OverlayScrim(QWidget* parent = nullptr, const QString& objectName = QString())
        : QWidget(parent)
    {
        if (!objectName.isEmpty())
            setObjectName(objectName);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        setModalAndDim(true, true);
    }

    void setModalAndDim(bool modal, bool dim)
    {
        m_modal = modal;
        m_dim = dim;
        setAttribute(Qt::WA_TransparentForMouseEvents, !m_modal);
        update();
    }

    void setOpacityProgress(qreal progress)
    {
        const qreal normalized = qBound<qreal>(0.0, progress, 1.0);
        if (std::abs(m_opacityProgress - normalized) <= 0.0001)
            return;
        m_opacityProgress = normalized;
        update();
    }

    void onThemeUpdated() override
    {
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        if (!m_dim)
            return;

        QPainter painter(this);
        const auto smoke = themeSmoke();
        QColor color = smoke.baseColor;
        color.setAlphaF(smoke.opacity * m_opacityProgress);
        painter.fillRect(rect(), color);
    }

    void mousePressEvent(QMouseEvent* event) override { handlePointerEvent(event); }
    void mouseReleaseEvent(QMouseEvent* event) override { handlePointerEvent(event); }
    void mouseMoveEvent(QMouseEvent* event) override { handlePointerEvent(event); }
    void wheelEvent(QWheelEvent* event) override { handlePointerEvent(event); }

private:
    void handlePointerEvent(QEvent* event)
    {
        if (m_modal)
            event->accept();
        else
            event->ignore();
    }

    bool m_modal = true;
    bool m_dim = true;
    qreal m_opacityProgress = 1.0;
};

} // namespace view::overlay

#endif // OVERLAYSCRIM_H
