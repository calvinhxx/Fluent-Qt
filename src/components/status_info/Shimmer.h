#ifndef SHIMMER_H
#define SHIMMER_H

#include <QBasicTimer>
#include <QColor>
#include <QRectF>
#include <QVector>
#include <QWidget>

#include "../foundation/FluentElement.h"
#include "../foundation/QMLPlus.h"

class QPainter;
class QPaintEvent;
class QHideEvent;
class QResizeEvent;
class QShowEvent;
class QTimerEvent;

namespace fluent::status_info {

/**
 * @brief Painter helper for Fluent skeleton loading placeholders.
 * zh_CN: Fluent 骨架屏加载占位的绘制辅助类。
 *
 * Lets item delegates draw the same skeleton shapes as the Shimmer widget
 * without creating child widgets.
 * zh_CN: 供列表项委托等轻量绘制场景复用，无需创建子控件即可画出与 Shimmer 一致的骨架元素。
 */
class ShimmerPainter {
public:
    /**
     * @brief Shape used by a skeleton element.
     * zh_CN: 骨架元素使用的形状。
     */
    enum class Shape { Rectangle, RoundedRect, Circle, Line };

    /**
     * @brief Theme-resolved colors used by shimmer painting.
     * zh_CN: shimmer 绘制使用的主题解析颜色。
     */
    struct Palette {
        QColor baseColor;
        QColor highlightColor;
        QColor borderColor;
    };

    /**
     * @brief One skeleton shape in local target coordinates.
     * zh_CN: 局部目标坐标中的一个骨架形状。
     */
    struct Element {
        Shape shape = Shape::RoundedRect;
        QRectF rect;
        qreal radius = -1.0;

        Element() = default;
        Element(Shape elementShape, const QRectF& elementRect, qreal elementRadius = -1.0)
            : shape(elementShape), rect(elementRect), radius(elementRadius)
        {}
    };

    /**
     * @brief Resolves a shimmer palette from Fluent theme colors.
     * zh_CN: 从 Fluent 主题色解析 shimmer 调色板。
     */
    static Palette paletteFromTheme(const fluent::FluentElement::Colors& colors, bool enabled = true);

    /**
     * @brief Draws shimmer elements using a normalized animation progress.
     * zh_CN: 使用归一化动画进度绘制 shimmer 元素。
     */
    static void paint(QPainter* painter,
        const QVector<Element>& elements,
        const Palette& palette,
        qreal progress,
        bool animated = true);

    /**
     * @brief Skeleton template for a single image/card placeholder.
     * zh_CN: 单个图片/卡片占位的骨架模板。
     */
    static QVector<Element> imageCardElements(const QRectF& bounds, qreal radius = 4.0);

    /**
     * @brief Skeleton template for an avatar plus text row.
     * zh_CN: 头像加文本行的骨架模板。
     */
    static QVector<Element> avatarTextRowElements(const QRectF& bounds);

    /**
     * @brief Skeleton template for a multi-line text block.
     * zh_CN: 多行文本块的骨架模板。
     */
    static QVector<Element> textBlockElements(const QRectF& bounds, int lineCount = 3);
};

/**
 * @brief Fluent skeleton loading placeholder with animated shimmer.
 * zh_CN: 带流光动画的 Fluent 骨架屏占位控件。
 *
 * Displays placeholder shapes while content loads; set active to false or hide
 * the widget once data is ready.
 * zh_CN: 用于内容加载期间的骨架占位；数据就绪后将 active 设为 false 或隐藏控件即可。
 */
class Shimmer : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Whether the loading skeleton is active.
     * zh_CN: 加载骨架是否处于活动状态。
     */
    Q_PROPERTY(bool active READ isActive WRITE setActive NOTIFY activeChanged)
    /**
     * @brief Whether shimmer motion is enabled.
     * zh_CN: 是否启用 shimmer 动效。
     */
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
    /**
     * @brief Current normalized shimmer animation progress.
     * zh_CN: 当前归一化 shimmer 动画进度。
     */
    Q_PROPERTY(qreal shimmerProgress READ shimmerProgress WRITE setShimmerProgress NOTIFY shimmerProgressChanged)
    /**
     * @brief Full shimmer cycle duration in milliseconds.
     * zh_CN: 完整 shimmer 循环时长，单位为毫秒。
     */
    Q_PROPERTY(int cycleDuration READ cycleDuration WRITE setCycleDuration NOTIFY cycleDurationChanged)
    /**
     * @brief Built-in element template used when custom elements are not supplied.
     * zh_CN: 未提供自定义元素时使用的内置骨架模板。
     */
    Q_PROPERTY(ShimmerTemplate shimmerTemplate
        READ shimmerTemplate
        WRITE setShimmerTemplate
        NOTIFY shimmerTemplateChanged)

public:
    enum class ShimmerTemplate { Custom, ImageCard, AvatarTextRow, TextBlock };
    Q_ENUM(ShimmerTemplate)

    explicit Shimmer(QWidget* parent = nullptr);
    ~Shimmer() override;

    bool isActive() const { return m_active; }
    void setActive(bool active);

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    qreal shimmerProgress() const { return m_progress; }
    void setShimmerProgress(qreal progress);

    int cycleDuration() const { return m_cycleDuration; }
    void setCycleDuration(int durationMs);

    ShimmerTemplate shimmerTemplate() const { return m_template; }
    void setShimmerTemplate(ShimmerTemplate templateKind);

    QVector<ShimmerPainter::Element> elements() const { return m_customElements; }
    void setElements(const QVector<ShimmerPainter::Element>& elements);
    void clearElements();

    bool isAnimationRunning() const { return m_animationTimer.isActive(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void onThemeUpdated() override;

signals:
    void activeChanged(bool active);
    void animationEnabledChanged(bool enabled);
    void shimmerProgressChanged(qreal progress);
    void cycleDurationChanged(int durationMs);
    void shimmerTemplateChanged(ShimmerTemplate templateKind);
    void elementsChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QVector<ShimmerPainter::Element> effectiveElements() const;
    void updateTemplateElements();
    void updateThemePalette();
    void updateAnimationState();

    bool m_active = true;
    bool m_animationEnabled = true;
    qreal m_progress = 0.0;
    int m_cycleDuration = 0;
    ShimmerTemplate m_template = ShimmerTemplate::TextBlock;
    QVector<ShimmerPainter::Element> m_customElements;
    QVector<ShimmerPainter::Element> m_templateElements;
    ShimmerPainter::Palette m_palette;
    QBasicTimer m_animationTimer;
};

} // namespace fluent::status_info

#endif // SHIMMER_H
