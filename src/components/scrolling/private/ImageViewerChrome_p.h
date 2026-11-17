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

// Object names shared by ImageViewer, its accessibility adapter, and tests.
inline constexpr const char* kImageViewerZoomBarName = "fluentImageViewerZoomBar";
inline constexpr const char* kImageViewerZoomIndicatorName = "fluentImageViewerZoomIndicator";

// Floating zoom toolbar; buttons stay unfocusable so keyboard focus remains on ImageViewer.
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

    // Fades to the requested visibility and hides once opacity reaches zero.
    void setShown(bool shown);
    bool isShown() const { return m_shown; }

    // Surface size excluding shadow margins.
    QSize surfaceSize() const;
    // Surface width of the full or compact layout regardless of current mode.
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

// Transient zoom percentage badge shown at the viewport center.
class ImageViewerZoomIndicator : public QWidget, public FluentElement {
public:
    explicit ImageViewerZoomIndicator(QWidget* parent = nullptr);

    // Shows the text briefly, then fades out after the hold timer expires.
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
