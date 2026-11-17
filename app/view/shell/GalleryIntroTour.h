#ifndef GALLERYINTROTOUR_H
#define GALLERYINTROTOUR_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/dialogs_flyouts/CoachMark.h"
#include "components/foundation/overlay/OverlayScrim.h"

class QEvent;
class QPropertyAnimation;

namespace fluent::basicinput {
class Button;
}
namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

/**
 * @brief First-launch coach-mark tour that dims the app and guides users through target controls.
 *
 * Uses a modal same-window OverlayScrim with a spotlight cutout and a CoachMark card above it,
 * disabling window resize/move chrome while active.
 * zh_CN: 首次启动的操作引导：在同窗口内通过 OverlayScrim 遮罩挖空高亮目标，并在上方以 CoachMark 卡片分步引导。
 */
class GalleryIntroTour : public QObject {
    Q_OBJECT
public:
    struct Step {
        QPointer<QWidget> target;
        QString glyph;
        QString title;
        QString body;
        fluent::dialogs_flyouts::CoachMark::Placement placement = fluent::dialogs_flyouts::CoachMark::Auto;
        bool centered = false;
    };

    explicit GalleryIntroTour(QWidget* host, QObject* parent = nullptr);

    void setSteps(const QVector<Step>& steps) { m_steps = steps; }
    void start();

signals:
    void finished();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void build();
    void applyStep(int index,
        bool animateSpotlight); // content + card target/placement + dim spotlight
    void goToStep(int index);
    QRect scrimGeometryForWindow() const;
    void syncScrimGeometry();
    void syncScrimSurfaceRadius();
    QRect presentedTargetRect(QWidget* target) const;
    void syncCardAnchor(bool retarget = false);
    void applyStepSpotlight(int index, bool animate); // glide / pop the dim cut-out onto the target
    QRect spotlightRectFor(QWidget* target) const;    // target geometry in scrim-local coords + padding
    void focusPrimaryActionIfActive();
    void settleFocusForHostState();
    void finishTour();

    QWidget* m_host = nullptr;
    fluent::overlay::OverlayScrim* m_scrim = nullptr;     // dim: child of the app window
    fluent::dialogs_flyouts::CoachMark* m_card = nullptr; // same-window card (owns fade + glide)
    QWidget* m_presentedAnchor = nullptr;                 // geometry-only child of the scrim
    QPropertyAnimation* m_dimAnim = nullptr;
    QPropertyAnimation* m_spotAnim = nullptr; // glides the spotlight cut-out between step targets
    bool m_haveSpot = false;                  // a spotlight is currently shown (for glide-vs-pop)
    fluent::textfields::Label* m_glyph = nullptr;
    fluent::textfields::Label* m_title = nullptr;
    fluent::textfields::Label* m_body = nullptr;
    fluent::textfields::Label* m_counter = nullptr;
    fluent::basicinput::Button* m_prev = nullptr;
    fluent::basicinput::Button* m_next = nullptr;
    fluent::basicinput::Button* m_close = nullptr;
    QPointer<QWidget> m_focusBeforeStart;

    QVector<Step> m_steps;
    int m_index = -1;
    int m_animationDurationMs = 0;
    bool m_finished = false;
};

} // namespace fluent::gallery

#endif // GALLERYINTROTOUR_H
