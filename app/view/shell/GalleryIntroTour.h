#ifndef GALLERYINTROTOUR_H
#define GALLERYINTROTOUR_H

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/dialogs_flyouts/CoachMark.h"

class QEvent;
class QPropertyAnimation;

namespace fluent::basicinput {
class Button;
}
namespace fluent::dialogs_flyouts {
class SmokeOverlay;
}
namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

/**
 * @brief First-launch coach-mark tour: dims the app and walks a CoachMark card across a list of
 * targets. zh_CN: 首次启动的操作引导：压暗 app,让 CoachMark 卡片依次走过一串目标。
 *
 * Dim = a SmokeOverlay child of the app window (alone → composites stably, like Dialog) which also
 * blocks clicks (modal); title-bar dragging is disabled via Window::setTitleBarDragEnabled. The card
 * is a CoachMark (separate top-level) raised above, with a tail pointing at each target. Previous /
 * Next / Finish drive it; close skips. The owner persists "seen" and only calls start() on first launch.
 * zh_CN: 压暗 = app 窗口的 SmokeOverlay 子级(单独存在→稳定合成,与 Dialog 同理),并拦截点击(模态);标题栏拖动经
 * Window::setTitleBarDragEnabled 禁用。卡片是 CoachMark(独立顶层)浮于其上,尾巴指向每个目标。Previous/Next/Finish
 * 推进,关闭跳过。是否「已看过」由调用方持久化,仅首启调用 start()。
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
    void applyStep(int index);  // content + card target/placement (CoachMark glides itself)
    void goToStep(int index);
    void syncScrimGeometry();
    void finishTour();

    QWidget* m_host = nullptr;
    fluent::dialogs_flyouts::SmokeOverlay* m_scrim = nullptr;  // dim: child of the app window, alone
    fluent::dialogs_flyouts::CoachMark* m_card = nullptr;      // separate top-level card (owns fade + glide)
    QPropertyAnimation* m_dimAnim = nullptr;
    fluent::textfields::Label* m_glyph = nullptr;
    fluent::textfields::Label* m_title = nullptr;
    fluent::textfields::Label* m_body = nullptr;
    fluent::textfields::Label* m_counter = nullptr;
    fluent::basicinput::Button* m_prev = nullptr;
    fluent::basicinput::Button* m_next = nullptr;
    fluent::basicinput::Button* m_close = nullptr;

    QVector<Step> m_steps;
    int m_index = -1;
    bool m_finished = false;
};

} // namespace fluent::gallery

#endif // GALLERYINTROTOUR_H
