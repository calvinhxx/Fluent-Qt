#include "GallerySplashScreen.h"

#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QResizeEvent>

#include "components/status_info/ProgressRing.h"
#include "design/Typography.h"
#include "view/shell/AppIcon.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {
namespace {

// From the Windows UI kit "Splash screen" Win32 spec: 96px app icon dead-centered,
// 32px progress ring centered horizontally 144px below the center.
// zh_CN: 取自 Windows UI kit「Splash screen」Win32 规范：96px 图标居中，32px 进度环水平居中、位于中心下方 144px。
constexpr int kLogoSize = 96;
constexpr int kSpinnerSize = 32;
constexpr int kSpinnerCenterOffsetBelow = 144;
constexpr int kCaptionGap = 12;
constexpr int kCaptionHeight = 20;

} // namespace

GallerySplashScreen::GallerySplashScreen(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("gallerySplashScreen"));
    // We paint a fully opaque background in paintEvent, so the surface still swallows
    // clicks and hides content underneath. We deliberately do NOT set
    // WA_OpaquePaintEvent: it makes the QGraphicsOpacityEffect used by dismiss() render
    // the widget black during the fade. zh_CN: paintEvent 里画满不透明背景，依然吞点击、遮内容；
    // 刻意不设 WA_OpaquePaintEvent——它会让 dismiss() 的 QGraphicsOpacityEffect 在淡出时把控件渲染成黑色。
    m_logo = appicon::pixmap(kLogoSize, devicePixelRatioF());

    m_spinner = new fluent::status_info::ProgressRing(this);
    m_spinner->setObjectName(QStringLiteral("gallerySplashSpinner"));
    m_spinner->setFixedSize(kSpinnerSize, kSpinnerSize);
    m_spinner->setIsIndeterminate(true);
    m_spinner->setIsActive(true);

    if (parent) {
        parent->installEventFilter(this);
        setGeometry(parent->rect());
    }
    raise();
    layoutContent();
}

void GallerySplashScreen::setProgress(int done, int total)
{
    const QString next = total > 0
        ? QStringLiteral("%1%").arg(qBound(0, done * 100 / total, 100))
        : QString();
    if (next == m_progressText)
        return;
    m_progressText = next;
    update();
}

void GallerySplashScreen::dismiss()
{
    if (m_dismissing)
        return;
    m_dismissing = true;

    if (m_spinner)
        m_spinner->setIsActive(false);

    auto* effect = new QGraphicsOpacityEffect(this);
    setGraphicsEffect(effect);
    auto* fade = new QPropertyAnimation(effect, "opacity", this);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    fade->setDuration(themeAnimation().normal);
    fade->setEasingCurve(themeAnimation().decelerate);
    connect(fade, &QPropertyAnimation::finished, this, &QObject::deleteLater);
    fade->start(QAbstractAnimation::DeleteWhenStopped);
}

void GallerySplashScreen::onThemeUpdated()
{
    update();
}

bool GallerySplashScreen::eventFilter(QObject* watched, QEvent* event)
{
    // Track the parent (content host) so the splash always fills the content area.
    // zh_CN: 跟随父级（content host），让 splash 始终铺满内容区。
    if (watched == parentWidget() && event->type() == QEvent::Resize) {
        setGeometry(parentWidget()->rect());
        raise();
    }
    return QWidget::eventFilter(watched, event);
}

void GallerySplashScreen::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutContent();
}

void GallerySplashScreen::layoutContent()
{
    if (!m_spinner)
        return;
    const int centerX = width() / 2;
    const int centerY = height() / 2;
    m_spinner->move(centerX - kSpinnerSize / 2,
                    centerY + kSpinnerCenterOffsetBelow - kSpinnerSize / 2);
}

void GallerySplashScreen::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // bgCanvas (#F3F3F3 light) matches the Windows UI kit "Solid Background Base" and
    // the home page beneath, so the dismiss reveals content seamlessly.
    // zh_CN: bgCanvas（浅色 #F3F3F3）匹配 Windows UI kit 的「Solid Background Base」与下方 home 页，淡出时无缝衔接。
    painter.fillRect(rect(), themeColors().bgCanvas);

    if (!m_logo.isNull()) {
        const QRect logoRect(width() / 2 - kLogoSize / 2,
                             height() / 2 - kLogoSize / 2,
                             kLogoSize, kLogoSize);
        painter.drawPixmap(logoRect, m_logo);
    }

    // Explicit progress percentage, centered just below the spinner.
    // zh_CN: 明确的进度百分比，居中显示在转圈正下方。
    if (!m_progressText.isEmpty()) {
        const int captionTop = height() / 2 + kSpinnerCenterOffsetBelow
            + kSpinnerSize / 2 + kCaptionGap;
        painter.setFont(themeFont(Typography::FontRole::Caption).toQFont());
        painter.setPen(themeColors().textSecondary);
        painter.drawText(QRect(0, captionTop, width(), kCaptionHeight),
                         Qt::AlignHCenter | Qt::AlignTop, m_progressText);
    }
}

} // namespace fluent::gallery
