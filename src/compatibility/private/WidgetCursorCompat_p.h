#pragma once

#include <QCursor>
#include <QEvent>
#include <QObject>
#include <QPointer>
#include <QWidget>
#include <memory>
#include <optional>
#include <utility>

namespace compatibility::detail {

// One owner publishes hover and drag feedback to a receiver. Qt delivers
// CursorChange synchronously, including writes of the already visible shape.
class ScopedWidgetCursor final : public QObject {
public:
    explicit ScopedWidgetCursor(QWidget* receiver) : m_receiver(receiver) {}
    ~ScopedWidgetCursor() override { clear(); }

    void apply(Qt::CursorShape shape)
    {
        if (!m_receiver || m_yielded || m_shape == shape)
            return;
        if (!m_shape) {
            if (m_receiver->testAttribute(Qt::WA_SetCursor))
                m_saved = m_receiver->cursor();
            m_receiver->installEventFilter(this);
        }
        m_shape = shape;
        // Some window backends send more than one notification for a top-level write.
        const QPointer<ScopedWidgetCursor> alive(this);
        ++m_writeDepth;
        m_receiver->setCursor(shape);
        if (alive)
            --m_writeDepth;
    }

    void clear()
    {
        m_yielded = false;
        if (!m_shape)
            return;
        m_shape.reset();
        const auto saved = std::exchange(m_saved, std::nullopt);
        if (!m_receiver)
            return;
        m_receiver->removeEventFilter(this);
        if (saved)
            m_receiver->setCursor(*saved);
        else
            m_receiver->unsetCursor();
    }

    QWidget* receiver() const { return m_receiver.data(); }

private:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_receiver && event->type() == QEvent::CursorChange) {
            if (m_writeDepth && m_receiver->testAttribute(Qt::WA_SetCursor) && m_shape == m_receiver->cursor().shape())
                return false;
            m_shape.reset();
            m_saved.reset();
            m_yielded = true;
            m_receiver->removeEventFilter(this);
        }
        return false;
    }

    QPointer<QWidget> m_receiver;
    std::optional<QCursor> m_saved;
    std::optional<Qt::CursorShape> m_shape;
    bool m_yielded = false;
    int m_writeDepth = 0;
};

// Fixed controls and idle regions need no observer. Retain it after the
// first hover to avoid allocating again on each boundary crossing.
inline void updateWidgetCursor(std::unique_ptr<ScopedWidgetCursor>& feedback,
    QWidget* receiver,
    std::optional<Qt::CursorShape> shape = std::nullopt)
{
    if (!shape) {
        if (feedback)
            feedback->clear();
        return;
    }
    if (!feedback || feedback->receiver() != receiver)
        feedback = std::make_unique<ScopedWidgetCursor>(receiver);
    feedback->apply(*shape);
}

inline bool interruptsPointerGesture(QEvent::Type type)
{
    return type == QEvent::Hide || type == QEvent::WindowDeactivate || type == QEvent::UngrabMouse;
}

} // namespace compatibility::detail
