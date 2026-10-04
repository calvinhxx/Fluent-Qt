#ifndef FLUENTWINDOWBACKDROPX11PROTOCOL_P_H
#define FLUENTWINDOWBACKDROPX11PROTOCOL_P_H

#include <QtGlobal>

namespace compatibility::detail {

// A server error is still a reply to the ordered request. A broken transport
// or a call that never sent a request provides no submission evidence.
inline bool x11ClientRoundTripCompleted(bool requestSent, bool replyReceived,
                                        bool protocolErrorReceived, bool connectionFailed)
{
    return requestSent && !connectionFailed && (replyReceived || protocolErrorReceived);
}

class X11BackdropBackgroundPreparation {
public:
    void invalidate()
    {
        m_nativeId = 0;
        m_creationOrdered = false;
        m_applied = false;
    }

    template <typename OrderCreation, typename ApplyBackground>
    bool prepare(quint32 nativeId, quint32 opaqueRgb, bool transparent, OrderCreation orderCreation,
                 ApplyBackground applyBackground)
    {
        if (!nativeId)
            return false;
        if (m_nativeId != nativeId) {
            invalidate();
            m_nativeId = nativeId;
        }
        // A transparent compositor surface always uses zero, so theme/focus
        // changes cannot require another native write in that mode.
        const quint32 color = transparent ? 0 : opaqueRgb;
        if (m_applied && m_color == color && m_transparent == transparent)
            return true;
        if (!m_creationOrdered) {
            if (!orderCreation())
                return false;
            m_creationOrdered = true;
        }
        // Cache only a checked successful write. BadWindow and other protocol
        // errors must stay retryable after Qt completes a surface recreation.
        if (!applyBackground(color, transparent))
            return false;
        m_color = color;
        m_transparent = transparent;
        m_applied = true;
        return true;
    }

private:
    quint32 m_nativeId = 0;
    quint32 m_color = 0;
    bool m_transparent = false;
    bool m_creationOrdered = false;
    bool m_applied = false;
};

} // namespace compatibility::detail

#endif // FLUENTWINDOWBACKDROPX11PROTOCOL_P_H
