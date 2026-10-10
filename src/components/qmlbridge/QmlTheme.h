#ifndef FLUENTQT_QMLBRIDGE_QMLTHEME_H
#define FLUENTQT_QMLBRIDGE_QMLTHEME_H

#include <QColor>
#include <QObject>

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Exposes the host's Fluent tokens to QML as the read-only fluentTheme object.
 * zh_CN: 通过只读 fluentTheme 对象向 QML 提供宿主的 Fluent token。
 * QML content opts into these tokens; existing Quick controls are not restyled automatically.
 * zh_CN: QML 内容自行绑定这些 token；不会自动重设现有 Quick 控件的样式。
 */
class QmlTheme : public QObject {
    Q_OBJECT
    Q_PROPERTY(QColor background READ background NOTIFY changed)
    Q_PROPERTY(QColor surface READ surface NOTIFY changed)
    Q_PROPERTY(QColor surfaceAlt READ surfaceAlt NOTIFY changed)
    Q_PROPERTY(QColor text READ text NOTIFY changed)
    Q_PROPERTY(QColor secondaryText READ secondaryText NOTIFY changed)
    Q_PROPERTY(QColor accent READ accent NOTIFY changed)
    Q_PROPERTY(QColor onAccent READ onAccent NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(bool dark READ isDark NOTIFY changed)
    Q_PROPERTY(bool animationsEnabled READ areAnimationsEnabled NOTIFY changed)
    Q_PROPERTY(QString fontFamily READ fontFamily NOTIFY changed)
    Q_PROPERTY(int fontPixelSize READ fontPixelSize NOTIFY changed)
    Q_PROPERTY(int cornerRadius READ cornerRadius NOTIFY changed)
    Q_PROPERTY(int transitionDuration READ transitionDuration NOTIFY changed)
public:
    QColor background() const;
    QColor surface() const;
    /** @brief Returns the alternate Fluent layer color for inset surfaces.
     * zh_CN: 返回用于内嵌表面的 Fluent 交替层颜色。 */
    QColor surfaceAlt() const;
    QColor text() const;
    QColor secondaryText() const;
    QColor accent() const;
    QColor onAccent() const;
    QColor border() const;
    bool isDark() const;
    bool areAnimationsEnabled() const;
    QString fontFamily() const;
    int fontPixelSize() const;
    int cornerRadius() const;
    int transitionDuration() const;

signals:
    void changed();

private:
    friend class QmlView;
    explicit QmlTheme(QmlView* view);
    QmlView* m_view;
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLTHEME_H
