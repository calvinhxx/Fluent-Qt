#ifndef FLUENTQT_COMPONENTS_STATUS_INFO_AVATAR_H
#define FLUENTQT_COMPONENTS_STATUS_INFO_AVATAR_H

#include <QColor>
#include <QMetaType>
#include <QPixmap>
#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QPaintEvent;
class QResizeEvent;

namespace fluent::status_info {

class InfoBadge;

/**
 * @brief Fluent identity image with initials fallback and optional presence status.
 * zh_CN: 带首字母回退与可选在线状态的 Fluent 身份头像。
 *
 * Avatar keeps all user-facing text caller-owned. The name is used only for
 * initials generation and accessibility, while an explicit initials value or
 * pixmap overrides the visual fallback.
 * zh_CN: Avatar 的可见文本均由调用方提供。姓名只用于生成首字母和辅助功能名称；
 * 显式首字母或位图会覆盖对应的视觉回退。
 */
class Avatar : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString initials READ initials WRITE setInitials
                   NOTIFY initialsChanged)
    Q_PROPERTY(QPixmap image READ image WRITE setImage NOTIFY imageChanged)
    Q_PROPERTY(AvatarShape shape READ shape WRITE setShape NOTIFY shapeChanged)
    Q_PROPERTY(AvatarSize avatarSize READ avatarSize WRITE setAvatarSize
                   NOTIFY avatarSizeChanged)
    Q_PROPERTY(PresenceStatus presence READ presence WRITE setPresence
                   NOTIFY presenceChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor
                   WRITE setBackgroundColor NOTIFY backgroundColorChanged)
    Q_PROPERTY(QColor foregroundColor READ foregroundColor
                   WRITE setForegroundColor NOTIFY foregroundColorChanged)

public:
    /**
     * @brief Avatar outline shape.
     * zh_CN: 头像外轮廓形状。
     */
    enum class AvatarShape {
        Circular,
        Square
    };
    Q_ENUM(AvatarShape)

    /**
     * @brief Token-aligned avatar size presets in device-independent pixels.
     * zh_CN: 以设备无关像素表示、与 token 对齐的头像尺寸预设。
     */
    enum class AvatarSize {
        Small = 24,
        Medium = 32,
        Large = 40,
        ExtraLarge = 56
    };
    Q_ENUM(AvatarSize)

    /**
     * @brief Presence state rendered by the composed InfoBadge.
     * zh_CN: 由内部组合的 InfoBadge 呈现的在线状态。
     */
    enum class PresenceStatus {
        None,
        Available,
        Away,
        Busy,
        DoNotDisturb,
        Offline
    };
    Q_ENUM(PresenceStatus)

    explicit Avatar(QWidget* parent = nullptr);
    explicit Avatar(const QString& name, QWidget* parent = nullptr);

    QString name() const { return m_name; }
    void setName(const QString& name);

    QString initials() const { return m_initials; }
    void setInitials(const QString& initials);

    QPixmap image() const { return m_image; }
    void setImage(const QPixmap& image);

    AvatarShape shape() const { return m_shape; }
    void setShape(AvatarShape shape);

    AvatarSize avatarSize() const { return m_avatarSize; }
    void setAvatarSize(AvatarSize size);

    PresenceStatus presence() const { return m_presence; }
    void setPresence(PresenceStatus presence);

    QColor backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const QColor& color);

    QColor foregroundColor() const { return m_foregroundColor; }
    void setForegroundColor(const QColor& color);

    /**
     * @brief Returns explicit initials or the generated name fallback.
     * zh_CN: 返回显式首字母，或根据姓名生成的回退值。
     */
    QString effectiveInitials() const;

    InfoBadge* presenceBadge() const { return m_presenceBadge; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void nameChanged(const QString& name);
    void initialsChanged(const QString& initials);
    void imageChanged(const QPixmap& image);
    void shapeChanged(AvatarShape shape);
    void avatarSizeChanged(AvatarSize size);
    void presenceChanged(PresenceStatus presence);
    void backgroundColorChanged(const QColor& color);
    void foregroundColorChanged(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    int avatarExtent() const;
    QColor effectiveBackgroundColor() const;
    QColor effectiveForegroundColor(const QColor& background) const;
    QColor surroundingSurfaceColor() const;
    void updatePresenceBadge();
    void updateFixedExtent();

    QString m_name;
    QString m_initials;
    QPixmap m_image;
    AvatarShape m_shape = AvatarShape::Circular;
    AvatarSize m_avatarSize = AvatarSize::Medium;
    PresenceStatus m_presence = PresenceStatus::None;
    QColor m_backgroundColor;
    QColor m_foregroundColor;
    InfoBadge* m_presenceBadge = nullptr;
};

} // namespace fluent::status_info

Q_DECLARE_METATYPE(fluent::status_info::Avatar::AvatarShape)
Q_DECLARE_METATYPE(fluent::status_info::Avatar::AvatarSize)
Q_DECLARE_METATYPE(fluent::status_info::Avatar::PresenceStatus)

#endif // FLUENTQT_COMPONENTS_STATUS_INFO_AVATAR_H
