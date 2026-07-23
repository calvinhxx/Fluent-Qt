#include "GalleryIntroTour.h"

#include <algorithm>

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QMargins>
#include <QPropertyAnimation>
#include <QRect>
#include <QSize>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/foundation/overlay/OverlayScrim.h"
#include "components/textfields/Label.h"
#include "components/windowing/Window.h"
#include "compatibility/QtCompat.h"
#include "design/Typography.h"

namespace fluent::gallery {

using fluent::basicinput::Button;
using fluent::dialogs_flyouts::CoachMark;
using fluent::overlay::OverlayScrim;
using fluent::textfields::Label;

namespace {
constexpr int kCardWidth = 330;
constexpr int kCardHeight = 168;
constexpr double kDimStrength = 0.40;
constexpr int kSpotlightPadding = 1;  // tight breathing room around the highlighted target
constexpr int kSpotlightRadius = 8;   // rounded corners of the cut-out
}  // namespace

GalleryIntroTour::GalleryIntroTour(QWidget* host, QObject* parent)
    : QObject(parent)
    , m_host(host)
{
}

void GalleryIntroTour::build()
{
    if (m_card)
        return;

    QWidget* win = m_host->window();

    // Dim = an OverlayScrim child of the app window. The CoachMark is another same-window child raised
    // above it, so the startup tour scales with the app instead of as a separate tool window.
    // zh_CN: 压暗 = app 窗口的 OverlayScrim 子级。CoachMark 是另一个同窗口子级并置于其上方，
    // 因此启动引导会随 app 一起缩放，而不是作为独立工具窗口分开合成。
    m_scrim = new OverlayScrim(win, QStringLiteral("GalleryIntroTour.Scrim"));
    QColor dim(0, 0, 0);
    dim.setAlphaF(kDimStrength);
    m_scrim->setColor(dim);
    m_scrim->setModalAndDim(true, true);
    m_scrim->setProgress(0.0);

    m_card = new CoachMark(win);
    m_card->setCardSize(QSize(kCardWidth, kCardHeight));

    auto* host = m_card->contentHost();
    auto* root = new QVBoxLayout(host);
    root->setContentsMargins(18, 14, 14, 14);
    root->setSpacing(8);

    auto* header = new QHBoxLayout();
    header->setSpacing(10);
    m_glyph = new Label(QString(), host);
    QFont glyphFont(Typography::FontFamily::FluentIcons);
    glyphFont.setPixelSize(22);
    m_glyph->setFont(glyphFont);
    m_glyph->setStyleSheet(QStringLiteral("color: %1;").arg(m_card->themeColors().textAccentPrimary.name()));
    m_glyph->setFixedWidth(26);
    m_glyph->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    header->addWidget(m_glyph, 0, Qt::AlignTop);

    m_title = new Label(QString(), host);
    m_title->setFluentTypography(Typography::FontRole::BodyStrong);
    m_title->setWordWrap(true);
    header->addWidget(m_title, 1);

    m_close = new Button(host);
    m_close->setObjectName(QStringLiteral("GalleryIntroTour.CloseButton"));
    m_close->setFluentStyle(Button::Subtle);
    m_close->setFluentLayout(Button::IconOnly);
    m_close->setFluentSize(Button::Small);
    m_close->setIconGlyph(Typography::Icons::Cancel, Typography::IconSize::Standard);
    m_close->setFixedSize(28, 28);
    m_close->setFocusPolicy(Qt::NoFocus);
    m_close->setToolTip(QStringLiteral("Skip tour"));
    header->addWidget(m_close, 0, Qt::AlignTop);
    root->addLayout(header);

    m_body = new Label(QString(), host);
    m_body->setFluentTypography(Typography::FontRole::Body);
    m_body->setWordWrap(true);
    root->addWidget(m_body, 1);

    auto* footer = new QHBoxLayout();
    footer->setSpacing(8);
    m_counter = new Label(QString(), host);
    m_counter->setFluentTypography(Typography::FontRole::Caption);
    footer->addWidget(m_counter, 0);
    footer->addStretch(1);
    m_prev = new Button(QStringLiteral("Previous"), host);
    m_prev->setFluentStyle(Button::Standard);
    m_prev->setFocusPolicy(Qt::NoFocus);
    m_next = new Button(QStringLiteral("Next"), host);
    m_next->setFluentStyle(Button::Accent);
    m_next->setFocusPolicy(Qt::NoFocus);
    footer->addWidget(m_prev, 0);
    footer->addWidget(m_next, 0);
    root->addLayout(footer);

    connect(m_close, &Button::clicked, this, &GalleryIntroTour::finishTour);
    connect(m_prev, &Button::clicked, this, [this]() {
        if (m_index > 0)
            goToStep(m_index - 1);
    });
    connect(m_next, &Button::clicked, this, [this]() {
        if (m_index + 1 >= m_steps.size())
            finishTour();
        else
            goToStep(m_index + 1);
    });

    win->installEventFilter(this);

    m_dimAnim = new QPropertyAnimation(m_scrim, "progress", this);
    m_dimAnim->setDuration(m_card->themeAnimation().normal);
    m_dimAnim->setEasingCurve(m_card->themeAnimation().decelerate);

    m_scrim->setSpotlightRadius(kSpotlightRadius);
    // The spotlight cut-out glides between targets in lock-step with the CoachMark (same normal
    // duration + decelerate curve). zh_CN: 聚光挖空与 CoachMark 同步在目标间滑动(同样的 normal 时长 + decelerate 曲线)。
    m_spotAnim = new QPropertyAnimation(m_scrim, "spotlightRect", this);
    m_spotAnim->setDuration(m_card->themeAnimation().normal);
    m_spotAnim->setEasingCurve(m_card->themeAnimation().decelerate);
}

QRect GalleryIntroTour::spotlightRectFor(QWidget* target) const
{
    QWidget* win = m_host ? m_host->window() : nullptr;
    if (!win || !target)
        return QRect();
    const QRect surface = scrimGeometryForWindow();
    if (surface.isEmpty())
        return QRect();

    const QRect inWindow(target->mapTo(win, QPoint(0, 0)), target->size());
    const QRect inScrim = inWindow.translated(-surface.topLeft());
    return inScrim.marginsAdded(QMargins(kSpotlightPadding, kSpotlightPadding,
                                         kSpotlightPadding, kSpotlightPadding))
        .intersected(QRect(QPoint(0, 0), surface.size()));
}

void GalleryIntroTour::applyStepSpotlight(int index, bool animate)
{
    if (!m_scrim || !m_spotAnim)
        return;
    m_spotAnim->stop();

    const Step& step = m_steps.at(index);
    if (step.centered || step.target.isNull()) {
        // Centered / target-less step → uniform dim, no cut-out. zh_CN: 居中/无目标步 → 均匀压暗,无挖空。
        m_scrim->setSpotlightEnabled(false);
        m_haveSpot = false;
        return;
    }

    const QRect target = spotlightRectFor(step.target.data());
    m_scrim->setSpotlightEnabled(true);
    if (animate && m_haveSpot) {
        m_spotAnim->setStartValue(m_scrim->spotlightRect());
        m_spotAnim->setEndValue(target);
        m_spotAnim->start();
    } else {
        // First spotlight (coming from full dim) or a resize-follow: snap, don't glide from a stale rect.
        // zh_CN: 首个聚光(从全压暗而来)或跟随缩放:直接定位,不从过时矩形滑入。
        m_scrim->setSpotlightRect(target);
    }
    m_haveSpot = true;
}

void GalleryIntroTour::syncScrimGeometry()
{
    if (m_scrim && m_host && m_host->window()) {
        m_scrim->setGeometry(scrimGeometryForWindow());
        syncScrimSurfaceRadius();
        // The target moved with the window; re-anchor the cut-out without a glide. zh_CN: 目标随窗口移动,
        // 不滑动地重新对齐挖空。
        if (m_haveSpot && m_index >= 0 && m_index < m_steps.size()) {
            const Step& step = m_steps.at(m_index);
            if (!step.centered && !step.target.isNull())
                m_scrim->setSpotlightRect(spotlightRectFor(step.target.data()));
        }
    }
}

QRect GalleryIntroTour::scrimGeometryForWindow() const
{
    QWidget* win = m_host ? m_host->window() : nullptr;
    if (!win)
        return QRect();

    return ::fluent::overlay::overlaySurfaceRect(win);
}

void GalleryIntroTour::syncScrimSurfaceRadius()
{
    QWidget* win = m_host ? m_host->window() : nullptr;
    if (!m_scrim || !win)
        return;

    m_scrim->setSurfaceRadius(qRound(::fluent::overlay::overlaySurfaceRadius(win)));
}

void GalleryIntroTour::start()
{
    m_steps.erase(std::remove_if(m_steps.begin(), m_steps.end(),
                                 [](const Step& step) {
                                     return !step.centered && step.target.isNull();
                                 }),
                  m_steps.end());
    if (m_steps.isEmpty()) {
        emit finished();
        return;
    }

    build();
    // Modal: the scrim blocks clicks; lock the window chrome so it can't be moved or resized.
    // zh_CN: 模态:遮罩拦截点击;锁定窗口 chrome,使其不可移动或缩放。
    if (auto* w = qobject_cast<fluent::windowing::Window*>(m_host->window()))
        w->setChromeInteractive(false);
    syncScrimGeometry();
    m_scrim->show();
    m_scrim->raise();
    m_dimAnim->stop();
    m_dimAnim->setStartValue(0.0);
    m_dimAnim->setEndValue(1.0);
    m_dimAnim->start();

    m_index = 0;
    applyStep(0, /*animateSpotlight*/ false);
    m_card->open();  // CoachMark positions + fades itself in
    m_card->raise();
}

void GalleryIntroTour::applyStep(int index, bool animateSpotlight)
{
    const Step& step = m_steps.at(index);
    m_glyph->setText(step.glyph);
    m_glyph->setVisible(!step.glyph.isEmpty());
    m_title->setText(step.title);
    m_body->setText(step.body);
    m_counter->setText(QStringLiteral("%1 / %2").arg(index + 1).arg(m_steps.size()));
    m_prev->setVisible(index > 0);
    const bool last = (index + 1 == m_steps.size());
    m_next->setText(last ? QStringLiteral("Finish") : QStringLiteral("Next"));

    // CoachMark glides to the new target itself when retargeted while open.
    // zh_CN: 打开状态下重定向时,CoachMark 自己滑动到新目标。
    m_card->setPlacement(step.placement);
    m_card->setTarget(step.centered ? nullptr : step.target.data());

    applyStepSpotlight(index, animateSpotlight);
}

void GalleryIntroTour::goToStep(int index)
{
    if (index < 0 || index >= m_steps.size())
        return;
    m_index = index;
    applyStep(index, /*animateSpotlight*/ true);
    m_card->raise();
}

void GalleryIntroTour::finishTour()
{
    if (m_finished)
        return;
    m_finished = true;
    if (m_host && m_host->window()) {
        m_host->window()->removeEventFilter(this);
        if (auto* w = qobject_cast<fluent::windowing::Window*>(m_host->window()))
            w->setChromeInteractive(true);
    }

    if (m_card) {
        m_card->close();  // fades out + hides
        QPointer<CoachMark> card = m_card;
        fluentConnectSingleShot(m_card, &CoachMark::closed, card.data(), [card]() {
            if (card)
                card->deleteLater();
        });
    }

    if (m_spotAnim)
        m_spotAnim->stop();  // freeze the cut-out; let the whole dim fade out uniformly

    if (m_scrim && m_dimAnim) {
        m_dimAnim->stop();
        m_dimAnim->setStartValue(m_scrim->property("progress"));
        m_dimAnim->setEndValue(0.0);
        QPointer<QWidget> scrim = m_scrim;
        fluentConnectSingleShot(m_dimAnim, &QPropertyAnimation::finished, scrim.data(), [scrim]() {
            if (scrim)
                scrim->deleteLater();
        });
        m_dimAnim->start();
    }

    emit finished();
}

bool GalleryIntroTour::eventFilter(QObject* watched, QEvent* event)
{
    // The window is locked during the tour, but keep the scrim glued to it and on top as a safety net.
    // zh_CN: 引导期间窗口已锁定,但仍把遮罩贴住窗口并置顶,作为保险。
    if (m_scrim && m_host && watched == m_host->window()
        && (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
        syncScrimGeometry();
        m_scrim->raise();
        if (m_card && m_card->isOpen())
            m_card->raise();
    }
    return QObject::eventFilter(watched, event);
}

} // namespace fluent::gallery
