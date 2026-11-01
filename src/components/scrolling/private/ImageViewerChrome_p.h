#ifndef FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERCHROME_P_H
#define FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERCHROME_P_H

#include <functional>
#include <QWidget>

#include "components/foundation/FluentElement.h"

class QGraphicsOpacityEffect;
class QTimer;
class QVariantAnimation;

namespace fluent::basicinput {
class Button;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::scrolling {

// Object names shared by ImageViewer, its accessibility adapter and tests.
// zh_CN: ImageViewer、读屏适配器和测试共用的对象名。
inline constexpr const char* kImageViewerZoomBarName = "fluentImageViewerZoomBar";
inline constexpr const char* kImageViewerZoomIndicatorName = "fluentImageViewerZoomIndicator";

// Floating zoom toolbar composed of Fluent subtle buttons and a percentage label.
// Buttons do not take focus, so keyboard input stays with the viewer.
// zh_CN: 由 Fluent subtle 按钮和比例标签组成的浮动缩放工具条。按钮不获取焦点，
// 键盘输入始终留在查看器上。
class ImageViewerZoomBar : public QWidget, public FluentElement {
public:
    enum class Command { ZoomOut, ZoomIn, Fit, ActualSize, Reset };

    struct State {
        QString percent;
        bool canZoomOut = false;
        bool canZoomIn = false;
        bool canFit = false;
        bool canActualSize = false;
        bool canReset = false;
    };

    explicit ImageViewerZoomBar(std::function<void(Command)> handler, QWidget* parent = nullptr);

    void setState(const State& state);
    void setCompact(bool compact);
    bool isCompact() const { return m_compact; }

    // Fades to the requested visibility through MotionPolicy; hides at zero opacity.
    // zh_CN: 通过 MotionPolicy 淡入淡出到目标可见性；不透明度为零时隐藏。
    void setShown(bool shown);
    bool isShown() const { return m_shown; }

    // Surface size without the shadow margin.
    // zh_CN: 不含阴影边距的表面尺寸。
    QSize surfaceSize() const;
    // Surface width of the full or compact layout, independent of the current mode.
    // zh_CN: 完整或紧凑布局的表面宽度，与当前模式无关。
    int surfaceWidth(bool compact) const;
    static int shadowMargin();

    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    basicinput::Button*
    makeButton(const QString& iconName, const QString& toolTip, const QString& accessibleName, Command command);
    void applyOpacity(qreal opacity);

    std::function<void(Command)> m_handler;
    basicinput::Button* m_zoomOut = nullptr;
    basicinput::Button* m_zoomIn = nullptr;
    basicinput::Button* m_fit = nullptr;
    basicinput::Button* m_actualSize = nullptr;
    basicinput::Button* m_reset = nullptr;
    textfields::Label* m_percent = nullptr;
    QWidget* m_firstSeparator = nullptr;
    QWidget* m_secondSeparator = nullptr;
    QGraphicsOpacityEffect* m_effect = nullptr;
    QVariantAnimation* m_fade = nullptr;
    qreal m_opacity = 0.0;
    bool m_shown = false;
    bool m_compact = false;
};

// Transient zoom percentage shown at the viewport center.
// zh_CN: 显示在视口中心的临时缩放比例提示。
class ImageViewerZoomIndicator : public QWidget, public FluentElement {
public:
    explicit ImageViewerZoomIndicator(QWidget* parent = nullptr);

    // Shows the text, then fades out after the hold interval.
    // zh_CN: 显示文本，并在停留时间后淡出。
    void flash(const QString& text);
    void dismiss();
    QString text() const { return m_text; }

    void onThemeUpdated() override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void fadeTo(qreal opacity);
    void applyOpacity(qreal opacity);
    void resizeToText();

    QString m_text;
    QVariantAnimation* m_fade = nullptr;
    QTimer* m_holdTimer = nullptr;
    qreal m_opacity = 0.0;
    qreal m_targetOpacity = 0.0;
};

} // namespace fluent::scrolling

#endif // FLUENTQT_COMPONENTS_SCROLLING_PRIVATE_IMAGEVIEWERCHROME_P_H
