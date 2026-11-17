#ifndef FLUENTQT_QMLBRIDGE_QMLTHEME_H
#define FLUENTQT_QMLBRIDGE_QMLTHEME_H

#include <QColor>
#include <QObject>

namespace fluent::qmlbridge {
class QmlView;

/**
 * @brief Exposes host Fluent design tokens to QML as the read-only fluentTheme context object.
 * QML content binds to these tokens explicitly; standard Qt Quick controls are not restyled automatically.
 * zh_CN: 通过只读的 fluentTheme 上下文对象向 QML 提供宿主的 Fluent 设计 token。
 * 仅作用于显式绑定这些属性的 QML 内容，不会自动更改标准 Qt Quick 控件的样式。
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
    /** @brief Alternate Fluent surface color for inset areas. zh_CN: 用于内嵌区域的交替表面颜色。 */
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
    /** @brief Emitted when any host theme token or motion setting changes. zh_CN: 任一宿主主题 token 或动效设置变化时触发。 */
    void changed();

private:
    friend class QmlView;
    explicit QmlTheme(QmlView* view);
    QmlView* m_view;
};
} // namespace fluent::qmlbridge
#endif // FLUENTQT_QMLBRIDGE_QMLTHEME_H
