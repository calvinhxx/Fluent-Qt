#ifndef BUTTON_H
#define BUTTON_H

#include <QPushButton>
#include <QIcon>
#include <QEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QPainter>
#include <QStyleOptionButton>
#include <QPoint>
#include <QMargins>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

namespace fluent::basicinput {

/**
 * @brief Fluent-styled push button with token-driven painting and optional iconfont content.
 * zh_CN: 基于 Fluent 设计 token 自绘的 QPushButton，支持文本、图标字体和多种视觉状态。
 *
 * Button keeps Qt's QPushButton behavior while replacing the visual surface in
 * paintEvent(). Use it as the common base for button-like entry components
 * that do not need a more specialized Qt base class.
 * zh_CN: Button 保留 QPushButton 的交互语义，但通过 paintEvent 自绘视觉表面。
 * 不需要特殊 Qt 基类的按钮类入口组件，应优先以它作为通用基础。
 */
class Button : public QPushButton, public FluentElement, public QMLPlus {
    Q_OBJECT
    
    /**
     * @brief Visual treatment: Standard, Accent, or Subtle.
     * zh_CN: 按钮视觉风格：Standard(标准)、Accent(强调色)、Subtle(透明)。
     */
    Q_PROPERTY(ButtonStyle fluentStyle READ fluentStyle WRITE setFluentStyle NOTIFY fluentStyleChanged)
    /**
     * @brief Density token that controls height and padding, not the font.
     * zh_CN: 按钮密度 token，仅控制高度与内边距；字体仍通过 setFont()/font() 管理。
     */
    Q_PROPERTY(ButtonSize fluentSize READ fluentSize WRITE setFluentSize NOTIFY fluentSizeChanged)
    /**
     * @brief Content layout for text and icon placement.
     * zh_CN: 文本与图标的内容排列方式。
     */
    Q_PROPERTY(ButtonLayout fluentLayout READ fluentLayout WRITE setFluentLayout NOTIFY fluentLayoutChanged)
    /**
     * @brief Controls whether the focus visual frame is painted.
     * zh_CN: 控制是否绘制焦点视觉框。
     */
    Q_PROPERTY(bool focusVisual READ hasFocusVisual WRITE setFocusVisual NOTIFY focusVisualChanged)
    /**
     * @brief Overrides the interaction state for guided demos or visual checks.
     * zh_CN: 强制交互状态，主要用于引导、演示或 VisualCheck。
     */
    Q_PROPERTY(InteractionState interactionState READ interactionState WRITE setInteractionState NOTIFY interactionStateChanged)
    /**
     * @brief Uses the critical semantic color while hovered or pressed.
     * zh_CN: Hover/Pressed 状态使用 Critical 语义色，适合关闭、删除等危险操作。
     */
    Q_PROPERTY(bool criticalOnHover READ criticalOnHover WRITE setCriticalOnHover NOTIFY criticalOnHoverChanged)
    /**
     * @brief Per-corner radii; left/top/right/bottom map to top-left/top-right/bottom-right/bottom-left.
     * zh_CN: 四角圆角；left/top/right/bottom 分别对应左上、右上、右下、左下。
     */
    Q_PROPERTY(QMargins cornerRadii READ cornerRadii WRITE setCornerRadii RESET resetCornerRadii NOTIFY cornerRadiiChanged)
    /**
     * @brief Pixel offset applied to iconfont drawing; positive values move right/down.
     * zh_CN: iconfont 绘制偏移量，正值表示向右/向下移动，用于精细视觉对齐。
     */
    Q_PROPERTY(QPoint iconOffset READ iconOffset WRITE setIconOffset)
    /**
     * @brief Clockwise rotation applied to the painted icon in degrees.
     * zh_CN: 应用于自绘图标的顺时针旋转角度（度）。
     */
    Q_PROPERTY(qreal iconRotation READ iconRotation WRITE setIconRotation)
    /**
     * @brief Uniform scale applied to the painted iconfont glyph (1.0 = no scaling).
     * Drives WinUI-style press feedback (a brief scale-down) without touching geometry.
     * zh_CN: 作用于自绘 iconfont 字形的等比缩放（1.0 为不缩放），用于实现 WinUI 风格的按下反馈
     *（短暂缩小），且不改变几何布局。
     */
    Q_PROPERTY(qreal iconScale READ iconScale WRITE setIconScale)
    /**
     * @brief Painter-level opacity for the whole button surface (1.0 = fully opaque).
     * A lightweight fade that avoids QGraphicsOpacityEffect's offscreen compositing.
     * zh_CN: 作用于整个按钮表面的画笔级不透明度（1.0 为完全不透明）。轻量淡入淡出，
     * 避免 QGraphicsOpacityEffect 的离屏合成。
     */
    Q_PROPERTY(qreal contentOpacity READ contentOpacity WRITE setContentOpacity)

public:
    /**
     * @brief Button visual style resolved against Fluent design tokens.
     * zh_CN: 根据 Fluent 设计 token 解析的按钮视觉风格。
     */
    enum ButtonStyle { Standard, Accent, Subtle };
    Q_ENUM(ButtonStyle)

    /**
     * @brief Button density presets for padding and preferred height.
     * zh_CN: 控制内边距和推荐高度的按钮密度预设。
     */
    enum ButtonSize { Small, StandardSize, Large };
    Q_ENUM(ButtonSize)

    /**
     * @brief Text/icon composition mode.
     * zh_CN: 文本与图标的组合模式。
     */
    enum ButtonLayout { 
        TextOnly,       
        IconBefore,     
        IconOnly,       
        IconAfter       
    };
    Q_ENUM(ButtonLayout)

    /**
     * @brief Paint-state override used before falling back to real hover/press state.
     * zh_CN: 绘制状态覆盖值；为 Rest 时再回退到真实 hover/press 状态。
     */
    enum InteractionState { 
        Rest,           
        Hover,          
        Pressed,        
        Disabled        
    };
    Q_ENUM(InteractionState)

    explicit Button(const QString& text, QWidget* parent = nullptr);
    explicit Button(QWidget* parent = nullptr);

    // Fluent property accessors.
    // zh_CN: Fluent 属性访问接口。
    ButtonStyle fluentStyle() const { return m_style; }
    void setFluentStyle(ButtonStyle style);

    ButtonSize fluentSize() const { return m_size; }
    void setFluentSize(ButtonSize size);

    ButtonLayout fluentLayout() const { return m_layout; }
    void setFluentLayout(ButtonLayout layout);

    bool hasFocusVisual() const { return m_focusVisual; }
    void setFocusVisual(bool focus);

    InteractionState interactionState() const { return m_interactionState; }
    void setInteractionState(InteractionState state);

    bool criticalOnHover() const { return m_criticalOnHover; }
    void setCriticalOnHover(bool enabled);

    QMargins cornerRadii() const;
    void setCornerRadii(const QMargins& radii);
    void resetCornerRadii();

    QPoint iconOffset() const { return m_iconOffset; }
    void setIconOffset(const QPoint& offset);

    qreal iconRotation() const { return m_iconRotation; }
    void setIconRotation(qreal rotation);

    qreal iconScale() const { return m_iconScale; }
    void setIconScale(qreal scale);

    qreal contentOpacity() const { return m_contentOpacity; }
    void setContentOpacity(qreal opacity);

    void onThemeUpdated() override { update(); }

    // Size hints include Fluent padding, text, and iconfont content.
    // zh_CN: 尺寸提示会综合 Fluent 内边距、文本和 iconfont 内容。
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /**
     * @brief Sets an iconfont glyph that is drawn directly during paintEvent().
     * zh_CN: 设置在 paintEvent() 中直接绘制的 iconfont 图标字符。
     *
     * @param glyph Icon glyph, usually from Typography::Icons.
     * @param pixelSize Icon slot and native optical design size; defaults to
     * WinUI's standard 16 px control icon.
     * @param family Icon font family; defaults to FluentQt Icons.
     */
    void setIconGlyph(const QString& glyph,
                      int pixelSize = Typography::IconSize::Standard,
                      const QString& family = Typography::FontFamily::FluentIcons);

    void setIconGlyph(QChar glyph,
                      int pixelSize = Typography::IconSize::Standard,
                      const QString& family = Typography::FontFamily::FluentIcons) {
        setIconGlyph(QString(glyph), pixelSize, family);
    }

signals:
    void fluentStyleChanged();
    void fluentSizeChanged();
    void fluentLayoutChanged();
    void focusVisualChanged();
    void interactionStateChanged();
    void criticalOnHoverChanged();
    void cornerRadiiChanged();

protected:
    // Paints the full Fluent button surface instead of delegating to QStyle.
    // zh_CN: 自绘完整 Fluent 按钮表面，而不是交给 QStyle 绘制。
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief Returns the rectangle used to lay out painted icon and text content.
     * zh_CN: 返回用于布局自绘图标与文本内容的区域。
     */
    virtual QRectF contentPaintRect(const QRectF& surfaceRect) const;

    // Exposes iconfont details to specialized button subclasses.
    // zh_CN: 向特殊按钮子类暴露 iconfont 信息，便于自定义绘制。
    QString iconGlyph() const { return m_iconGlyph; }
    QString iconFontFamily() const { return m_iconFontFamily; }
    int iconPixelSize() const { return m_iconPixelSize; }

    // State changes only invalidate painting; QPushButton keeps interaction semantics.
    // zh_CN: 状态变化只触发重绘；交互语义仍由 QPushButton 保持。
    void focusInEvent(QFocusEvent* event) override { QPushButton::focusInEvent(event); update(); }
    void focusOutEvent(QFocusEvent* event) override { QPushButton::focusOutEvent(event); update(); }

    void enterEvent(FluentEnterEvent* event) override { QPushButton::enterEvent(event); update(); }
    void leaveEvent(QEvent* event) override { QPushButton::leaveEvent(event); update(); }
    void mousePressEvent(QMouseEvent* event) override { QPushButton::mousePressEvent(event); update(); }
    void mouseReleaseEvent(QMouseEvent* event) override { QPushButton::mouseReleaseEvent(event); update(); }

private:
    ButtonStyle m_style = Standard;
    ButtonSize m_size = StandardSize;
    ButtonLayout m_layout = TextOnly;
    InteractionState m_interactionState = Rest;
    bool m_focusVisual = false;
    bool m_criticalOnHover = false;
    bool m_hasCustomCornerRadii = false;
    QMargins m_cornerRadii;
    QPoint m_iconOffset {0, 0}; // Fine-tunes iconfont centering for glyphs with uneven metrics.
    qreal m_iconRotation = 0.0;
    qreal m_iconScale = 1.0;    // Press-feedback scale for the painted glyph; 1.0 = no scaling.
    qreal m_contentOpacity = 1.0; // Painter-level fade for the whole surface; 1.0 = opaque.
    
    // Iconfont state used for crisp text rendering instead of pixmap conversion.
    // zh_CN: iconfont 状态用于直接文本绘制，避免 pixmap 转换导致模糊。
    QString m_iconGlyph;        // iconfont 字符
    QString m_iconFontFamily;   // iconfont 字体家族
    int m_iconPixelSize = 0;    // iconfont 像素大小
};

} // namespace fluent::basicinput

#endif // BUTTON_H
