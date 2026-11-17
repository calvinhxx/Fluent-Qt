#ifndef BUTTON_H
#define BUTTON_H

#include "compatibility/InputEventCompat.h"
#include <QPushButton>
#include <QIcon>
#include <QEvent>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QFont>
#include <QPainter>
#include <QStyleOptionButton>
#include <QPoint>
#include <QMargins>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "design/Typography.h"

namespace fluent::basicinput {

class ButtonContentPainter;

/**
 * @brief Fluent-styled push button with token-driven painting and optional iconfont content.
 * zh_CN: 基于 Fluent 设计 token 自绘的 QPushButton，支持文本、图标字体和多种视觉状态。
 *
 * Keeps QPushButton interaction behavior while painting the control in paintEvent().
 * zh_CN: 保留 QPushButton 原生交互，通过 paintEvent() 自定义外观绘制。
 */
class Button : public QPushButton, public FluentElement, public QMLPlus {
    Q_OBJECT
    friend class ButtonContentPainter;

    /**
     * @brief Visual treatment: Standard, Accent, or Subtle.
     * zh_CN: 按钮视觉风格：Standard(标准)、Accent(强调色)、Subtle(透明)。
     */
    Q_PROPERTY(ButtonStyle fluentStyle READ fluentStyle WRITE setFluentStyle NOTIFY fluentStyleChanged)
    /**
     * @brief Density token that controls height and padding, not typography.
     * zh_CN: 按钮密度 token，仅控制高度与内边距；字体由 fontRole 或 setFont() 管理。
     */
    Q_PROPERTY(ButtonSize fluentSize READ fluentSize WRITE setFluentSize NOTIFY fluentSizeChanged)
    /**
     * @brief Fluent typography role used while the button follows theme fonts.
     * zh_CN: 按钮跟随主题字体时使用的 Fluent 排版角色。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Foreground role for unfilled, unchecked buttons; filled and disabled keep contrast colors.
     * zh_CN: 非填充、未选中按钮的文字颜色角色；填充和禁用状态仍使用各自对比色。
     */
    Q_PROPERTY(TextColorRole textColorRole READ textColorRole WRITE setTextColorRole NOTIFY textColorRoleChanged)
    /**
     * @brief Content layout for text and icon placement.
     * zh_CN: 文本与图标的内容排列方式。
     */
    Q_PROPERTY(ButtonLayout fluentLayout READ fluentLayout WRITE setFluentLayout NOTIFY fluentLayoutChanged)
    /** @brief Qt icon with the same default-layout promotion as icon glyphs.
     * zh_CN: Qt 图标采用与字形图标相同的默认布局切换规则。 */
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon NOTIFY iconChanged)
    /**
     * @brief Controls whether the focus visual frame is painted.
     * zh_CN: 控制是否绘制焦点视觉框。
     */
    Q_PROPERTY(bool focusVisual READ hasFocusVisual WRITE setFocusVisual NOTIFY focusVisualChanged)
    /**
     * @brief Overrides the interaction state for guided demos or visual checks.
     * zh_CN: 强制交互状态，主要用于引导、演示或可视化检查。
     */
    Q_PROPERTY(InteractionState interactionState
        READ interactionState
        WRITE setInteractionState
        NOTIFY interactionStateChanged)
    /**
     * @brief Uses the critical error color while hovered or pressed.
     * zh_CN: 悬停或按下时使用危险操作配色（Critical），适合关闭、删除等操作。
     */
    Q_PROPERTY(bool criticalOnHover READ criticalOnHover WRITE setCriticalOnHover NOTIFY criticalOnHoverChanged)
    /**
     * @brief Per-corner radii; left/top/right/bottom map to top-left/top-right/bottom-right/bottom-left.
     * zh_CN: 四角圆角；left/top/right/bottom 分别对应左上、右上、右下、左下。
     */
    Q_PROPERTY(QMargins cornerRadii
        READ cornerRadii
        WRITE setCornerRadii
        RESET resetCornerRadii
        NOTIFY cornerRadiiChanged)
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
     * Applications can animate this property for press feedback; Button does not start a scale animation itself.
     * zh_CN: 作用于自绘 iconfont 字形的等比缩放（1.0 为不缩放）。应用可以对该属性播放按下反馈动画；
     * Button 自身不会启动缩放动画。
     */
    Q_PROPERTY(qreal iconScale READ iconScale WRITE setIconScale)
    /**
     * @brief Painter-level opacity for the whole button (1.0 = fully opaque).
     * Lightweight fade that avoids QGraphicsOpacityEffect's offscreen compositing overhead.
     * zh_CN: 作用于整个按钮的画笔级不透明度（1.0 为完全不透明），
     * 可实现轻量淡入淡出并避免 QGraphicsOpacityEffect 的离屏合成开销。
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
     * @brief Text/icon composition mode, including centered icons above or below the label.
     * zh_CN: 文本与图标的组合模式，支持将图标居中放在文字上方或下方。
     */
    enum ButtonLayout { TextOnly, IconBefore, IconOnly, IconAfter, IconAbove, IconBelow };
    Q_ENUM(ButtonLayout)

    /** @brief Text and icon color emphasis level. zh_CN: 文字与图标的颜色强调级别。 */
    enum class TextColorRole { Primary, Secondary, Accent };
    Q_ENUM(TextColorRole)

    /**
     * @brief Paint-state override; Rest uses the actual hover or press state.
     * zh_CN: 绘制状态覆盖值；为 Rest 时按真实悬停或按压状态绘制。
     */
    enum InteractionState { Rest, Hover, Pressed, Disabled };
    Q_ENUM(InteractionState)

    explicit Button(const QString& text, QWidget* parent = nullptr);
    explicit Button(QWidget* parent = nullptr);

    ButtonStyle fluentStyle() const { return m_style; }
    void setFluentStyle(ButtonStyle style);

    ButtonSize fluentSize() const { return m_size; }
    void setFluentSize(ButtonSize size);

    Typography::FontRole fontRole() const { return m_fontRole; }
    void setFontRole(Typography::FontRole role);
    TextColorRole textColorRole() const { return m_textColorRole; }
    void setTextColorRole(TextColorRole role);

    /**
     * @brief Sets an explicit font that remains unchanged by later theme refreshes.
     * zh_CN: 设置显式字体；后续主题刷新不会覆盖该字体。
     */
    void setFont(const QFont& font);

    ButtonLayout fluentLayout() const { return m_layout; }
    /** @brief Selects an explicit layout, including TextOnly before an icon is assigned.
     * zh_CN: 显式指定布局；在设置图标前指定 TextOnly 也会保持仅文字显示。 */
    void setFluentLayout(ButtonLayout layout);

    /** @brief Sets a Qt icon; the first icon uses IconBefore unless a layout was explicitly selected.
     * zh_CN: 设置 Qt 图标；未显式指定布局时，首个图标使用 IconBefore。 */
    void setIcon(const QIcon& icon);
    QIcon icon() const { return QPushButton::icon(); }

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

    void onThemeUpdated() override;

    /** @brief Computes preferred size including Fluent padding, text, and iconfont content.
     * zh_CN: 计算包含 Fluent 内边距、文本与 iconfont 图标的建议尺寸。 */
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    /**
     * @brief Sets an iconfont glyph drawn directly in paintEvent().
     * The first non-empty glyph switches layout to IconBefore unless setFluentLayout() was called.
     * zh_CN: 设置在 paintEvent() 中直接绘制的 iconfont 字符；未显式指定布局时，首个非空字形会自动切换为 IconBefore。
     *
     * @param glyph Icon glyph, usually from Typography::Icons.
     * @param pixelSize Icon slot and optical size; defaults to 16 px.
     * @param family Icon font family; defaults to FluentQt Icons.
     */
    void setIconGlyph(const QString& glyph,
        int pixelSize = Typography::IconSize::Standard,
        const QString& family = Typography::FontFamily::FluentIcons);

    void setIconGlyph(QChar glyph,
        int pixelSize = Typography::IconSize::Standard,
        const QString& family = Typography::FontFamily::FluentIcons)
    {
        setIconGlyph(QString(glyph), pixelSize, family);
    }

signals:
    void fluentStyleChanged();
    void fluentSizeChanged();
    void fontRoleChanged();
    void textColorRoleChanged();
    void fluentLayoutChanged();
    void iconChanged();
    void focusVisualChanged();
    void interactionStateChanged();
    void criticalOnHoverChanged();
    void cornerRadiiChanged();

protected:
    // Custom-paint the button instead of delegating to QStyle.
    // zh_CN: 通过自绘完成外观，不走 QStyle 默认绘制。
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;

    /**
     * @brief Paints a visual state without changing the interaction-state property.
     * zh_CN: 按指定视觉状态绘制，不修改 interactionState 属性。
     */
    void paintButton(InteractionState state);

    /**
     * @brief Paints an explicit focus state without moving focus; focusVisual and disabled state still apply.
     * zh_CN: 不移动实际焦点，按指定焦点状态绘制；仍遵循 focusVisual 和禁用状态。
     */
    void paintButtonState(InteractionState state, bool focused);

    /**
     * @brief Returns the rectangle used to lay out painted icon and text content.
     * zh_CN: 返回用于布局自绘图标与文本内容的区域。
     */
    virtual QRectF contentPaintRect(const QRectF& surfaceRect) const;

    /** @brief Measures the stacked icon and label without outer padding.
     * zh_CN: 测量上下排列的图标和文字，不包含外部内边距。 */
    virtual QSize stackedContentSize() const;
    /** @brief Paints centered stacked content in the supplied bounds and returns the label rectangle.
     * zh_CN: 在指定区域居中绘制上下排列的内容，并返回文字矩形。 */
    QRectF paintStackedContent(QPainter& painter, const QRectF& bounds) const;

    // Exposes iconfont details to button subclasses.
    // zh_CN: 向子类暴露 iconfont 绘制参数。
    QString iconGlyph() const { return m_iconGlyph; }
    QString iconFontFamily() const { return m_iconFontFamily; }
    int iconPixelSize() const { return m_iconPixelSize; }

    void focusInEvent(QFocusEvent* event) override
    {
        QPushButton::focusInEvent(event);
        update();
    }
    void focusOutEvent(QFocusEvent* event) override
    {
        QPushButton::focusOutEvent(event);
        update();
    }

    void enterEvent(FluentEnterEvent* event) override
    {
        QPushButton::enterEvent(event);
        update();
    }
    void leaveEvent(QEvent* event) override
    {
        QPushButton::leaveEvent(event);
        update();
    }
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void applyFontRole();
    void updateIconPresentation(bool nativeIconChanged);

    ButtonStyle m_style = Standard;
    ButtonSize m_size = StandardSize;
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    TextColorRole m_textColorRole = TextColorRole::Primary;
    ButtonLayout m_layout = TextOnly;
    bool m_hasExplicitLayout = false;
    InteractionState m_interactionState = Rest;
    bool m_focusVisual = false;
    bool m_criticalOnHover = false;
    bool m_hasExplicitFont = false;
    bool m_applyingFontRole = false;
    bool m_hasCustomCornerRadii = false;
    QMargins m_cornerRadii;
    QPoint m_iconOffset{0, 0}; // Fine-tunes iconfont centering for glyphs with uneven metrics.
    qreal m_iconRotation = 0.0;
    qreal m_iconScale = 1.0;      // Press-feedback scale for the painted glyph; 1.0 = no scaling.
    qreal m_contentOpacity = 1.0; // Painter-level fade for the whole control; 1.0 = opaque.

    // Direct iconfont glyph state (avoids pixmap rasterization blur).
    QString m_iconGlyph;
    QString m_iconFontFamily;
    int m_iconPixelSize = 0;
};

} // namespace fluent::basicinput

#endif // BUTTON_H
