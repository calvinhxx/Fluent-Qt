#include "GalleryIntroTour.h"

#include <algorithm>

#include <QColor>
#include <QEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QSize>
#include <QVBoxLayout>

#include "components/basicinput/Button.h"
#include "components/dialogs_flyouts/Dialog.h"  // SmokeOverlay
#include "components/textfields/Label.h"
#include "components/windowing/Window.h"
#include "design/Typography.h"

namespace fluent::gallery {

using fluent::basicinput::Button;
using fluent::dialogs_flyouts::CoachMark;
using fluent::dialogs_flyouts::SmokeOverlay;
using fluent::textfields::Label;

namespace {
constexpr int kCardWidth = 330;
constexpr int kCardHeight = 168;
constexpr double kDimStrength = 0.40;
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

    // Dim = a SmokeOverlay child of the app window. Alone in the window it composites stably (a sibling
    // card would make it flicker), and it blocks clicks to the app. zh_CN: 压暗 = app 窗口的 SmokeOverlay
    // 子级。单独存在时稳定合成(若有同级卡片会闪),并拦截 app 点击。
    m_scrim = new SmokeOverlay(win);
    m_scrim->setObjectName(QStringLiteral("GalleryIntroTour.Scrim"));
    QColor dim(0, 0, 0);
    dim.setAlphaF(kDimStrength);
    m_scrim->setColor(dim);
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
    QFont glyphFont(Typography::FontFamily::SegoeFluentIcons);
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
    m_close->setIconGlyph(Typography::Icons::Cancel, 12);
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
}

void GalleryIntroTour::syncScrimGeometry()
{
    if (m_scrim && m_host && m_host->window())
        m_scrim->setGeometry(m_host->window()->rect());
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
    applyStep(0);
    m_card->open();  // CoachMark positions + fades itself in
    m_card->raise();
}

void GalleryIntroTour::applyStep(int index)
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
}

void GalleryIntroTour::goToStep(int index)
{
    if (index < 0 || index >= m_steps.size())
        return;
    m_index = index;
    applyStep(index);
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
        connect(m_card, &CoachMark::closed, card, [card]() {
            if (card)
                card->deleteLater();
        }, Qt::SingleShotConnection);
    }

    if (m_scrim && m_dimAnim) {
        m_dimAnim->stop();
        m_dimAnim->setStartValue(m_scrim->property("progress"));
        m_dimAnim->setEndValue(0.0);
        QPointer<QWidget> scrim = m_scrim;
        connect(m_dimAnim, &QPropertyAnimation::finished, scrim, [scrim]() {
            if (scrim)
                scrim->deleteLater();
        }, Qt::SingleShotConnection);
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
