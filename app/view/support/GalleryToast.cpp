#include "GalleryToast.h"

#include <QAbstractAnimation>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QParallelAnimationGroup>
#include <QPen>
#include <QPoint>
#include <QPointer>
#include <QPropertyAnimation>
#include <QRectF>
#include <QSize>
#include <QTimer>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/overlay/OverlayGeometry.h"
#include "design/CornerRadius.h"
#include "design/Elevation.h"
#include "design/Typography.h"
#include "view/support/GalleryStyleSupport.h"

namespace fluent::gallery {
namespace {

constexpr char kToastObjectName[] = "galleryToast";
constexpr int kTitleBarHeight = 36;   // custom window chrome height
constexpr int kToastTopGap = 14;      // gap between the title bar and the toast
constexpr int kShadowMargin = ::fluent::overlay::defaultShadowMargin();
constexpr int kToastVisibleMs = 1700;
constexpr int kSlideDistance = 12;    // slide-down distance while entering
constexpr int kCornerRadius = 20;     // full capsule for the ~40px-tall pill
constexpr int kBadgeSize    = 26;     // success badge diameter
constexpr int kBadgeGlyphPx = 13;     // glyph size inside the success badge
constexpr int kToastShadowLayers = 8;
constexpr double kToastShadowOpacityScale = 0.10;

/**
 * @brief A light, translucent success toast (a rounded pill) themed with Fluent tokens.
 * zh_CN: 轻盈、半透明的成功提示 toast（圆角胶囊），使用 Fluent 设计 token 着色。
 *
 * The outer widget is fully transparent and only exists to give the drop shadow room and to
 * carry the fade/slide animation; the inner card paints the translucent surface so window
 * content shows through behind it.
 * zh_CN: 外层 widget 完全透明，仅用于给阴影留白并承载淡入/滑入动画；内层 card 绘制半透明表面，
 * 让后面的窗口内容透出来。
 */
class GalleryToastWidget : public QWidget, public fluent::FluentElement {
public:
    GalleryToastWidget(const QString& message, QWidget* parent)
        : QWidget(parent)
    {
        setObjectName(QString::fromLatin1(kToastObjectName));
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);

        auto* outer = new QHBoxLayout(this);
        outer->setContentsMargins(::fluent::overlay::uniformShadowMargins(kShadowMargin));
        outer->setSpacing(0);

        m_card = new QFrame(this);
        m_card->setObjectName(QStringLiteral("galleryToastCard"));
        m_card->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_card->setAttribute(Qt::WA_NoSystemBackground);
        m_card->setFrameShape(QFrame::NoFrame);
        auto* row = new QHBoxLayout(m_card);
        row->setContentsMargins(7, 7, 18, 7);   // tight left; the badge sets the inset
        row->setSpacing(10);

        m_icon = new QLabel(m_card);
        m_icon->setObjectName(QStringLiteral("galleryToastIcon"));
        m_icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_icon->setFixedSize(kBadgeSize, kBadgeSize);
        m_icon->setAlignment(Qt::AlignCenter);
        QFont glyph(Typography::FontFamily::SegoeFluentIcons);
        glyph.setPixelSize(kBadgeGlyphPx);
        m_icon->setFont(glyph);
        m_icon->setText(Typography::Icons::Success);

        m_text = new QLabel(message, m_card);
        m_text->setObjectName(QStringLiteral("galleryToastText"));
        m_text->setAttribute(Qt::WA_TransparentForMouseEvents);
        QFont textFont = m_text->font();
        textFont.setPixelSize(Typography::FontSize::Body);
        m_text->setFont(textFont);

        row->addWidget(m_icon, 0, Qt::AlignVCenter);
        row->addWidget(m_text, 0, Qt::AlignVCenter);

        outer->addWidget(m_card);
        applyPalette();
    }

    QSize cardHint() const { return m_card->sizeHint(); }

    void onThemeUpdated() override { applyPalette(); }

protected:
    // Paint the soft shadow ourselves rather than via a QGraphicsDropShadowEffect: the outer
    // widget already carries the opacity effect for the fade, and nesting two graphics effects
    // (opacity over a child's drop shadow) makes Qt fail to render the toast entirely.
    // zh_CN: 自己绘制柔和投影，而不用 QGraphicsDropShadowEffect：外层已经挂了淡入用的透明度效果，
    // 两个图形效果嵌套（透明度套子部件的投影）会让 Qt 直接渲染不出 toast。
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        const QRectF cardRect = QRectF(::fluent::overlay::visibleCardRect(rect(), kShadowMargin));

        // Toasts are much smaller than dialogs/flyouts, so use a lighter, mostly downward
        // shadow. Expanding equally in every direction makes a dark halo around the capsule.
        // zh_CN: toast 比 dialog/flyout 小很多，使用更轻、主要向下的阴影。四周等量扩散会在胶囊外形成深色光环。
        for (int i = 0; i < kToastShadowLayers; ++i) {
            const double ratio = 1.0 - static_cast<double>(i) / kToastShadowLayers;
            QColor shadow = m_shadowColor;
            shadow.setAlphaF(m_shadowOpacity * ratio * kToastShadowOpacityScale);
            p.setBrush(shadow);
            const qreal spread = i + 1;
            const QRectF shadowRect = cardRect
                                          .adjusted(-spread, -spread * 0.25,
                                                    spread, spread * 0.85)
                                          .translated(0, 3);
            p.drawRoundedRect(shadowRect,
                              kCornerRadius + spread,
                              kCornerRadius + spread);
        }

        const QRectF fillRect = cardRect.adjusted(0.5, 0.5, -0.5, -0.5);
        p.setBrush(m_surfaceColor);
        p.setPen(QPen(m_borderColor, 1.0));
        p.drawRoundedRect(fillRect, kCornerRadius, kCornerRadius);

        QPainterPath clip = ::fluent::overlay::roundedRectPath(fillRect, kCornerRadius);
        p.save();
        p.setClipPath(clip);
        p.setPen(QPen(m_highlightColor, 1.0));
        p.drawLine(QPointF(fillRect.left() + kCornerRadius, fillRect.top() + 0.5),
                   QPointF(fillRect.right() - kCornerRadius, fillRect.top() + 0.5));
        p.restore();
    }

private:
    void applyPalette()
    {
        const Colors colors = themeColors();
        // Light, see-through surface: take the layer fill and lower its alpha so the toast
        // reads as a translucent pill rather than a solid banner.
        // zh_CN: 轻盈、可透视的表面：取 layer 填充色并降低 alpha，让 toast 像半透明胶囊而非实心横幅。
        m_surfaceColor = colors.bgLayer;
        m_surfaceColor.setAlpha(currentTheme() == Dark ? 209 : 224);
        m_borderColor = colors.strokeCard;
        m_highlightColor = currentTheme() == Dark
                               ? QColor(255, 255, 255, 20)
                               : QColor(255, 255, 255, 191);
        m_card->setStyleSheet(QStringLiteral("#galleryToastCard { background: transparent; border: none; }"));
        // Success glyph sits in a soft success-tinted circular badge.
        // zh_CN: 成功勾选放进一个柔和的 success-tint 圆形徽标里。
        m_icon->setStyleSheet(QStringLiteral(
                                  "#galleryToastIcon { color: %1; background: %2;"
                                  " border-radius: %3px; }")
                                  .arg(cssColor(colors.systemSuccess),
                                       cssColor(colors.systemSuccessBg))
                                  .arg(kBadgeSize / 2));
        m_text->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                  .arg(cssColor(colors.textPrimary)));

        const Elevation::ShadowParams sp = themeShadow(Elevation::Medium);
        m_shadowColor = sp.color;
        m_shadowOpacity = sp.opacity;
        update();
    }

    QFrame* m_card = nullptr;
    QLabel* m_icon = nullptr;
    QLabel* m_text = nullptr;
    QColor m_shadowColor = QColor(0, 0, 0, 90);
    double m_shadowOpacity = 0.15;
    QColor m_surfaceColor = Qt::white;
    QColor m_borderColor = QColor(0, 0, 0, 14);
    QColor m_highlightColor = QColor(255, 255, 255, 191);
};

} // namespace

void showGalleryToast(QWidget* anchor, const QString& message)
{
    QWidget* host = anchor ? anchor->window() : nullptr;
    if (!host)
        return;

    // Replace any in-flight toast so repeated copies don't stack.
    // zh_CN: 替换正在显示的 toast，避免连续复制时堆叠。
    if (auto* existing = host->findChild<QWidget*>(QString::fromLatin1(kToastObjectName)))
        delete existing;

    auto* toast = new GalleryToastWidget(message, host);
    const QSize card = toast->cardHint();
    const QSize rootSize = ::fluent::overlay::outerSizeForVisibleCard(card, kShadowMargin);
    toast->resize(rootSize);

    const QPoint cardTopLeft((host->width() - card.width()) / 2,
                             kTitleBarHeight + kToastTopGap);
    const QPoint endPos = ::fluent::overlay::outerTopLeftForVisibleCard(cardTopLeft,
                                                                        kShadowMargin);
    const QPoint startPos = endPos - QPoint(0, kSlideDistance);
    toast->move(startPos);

    auto* opacity = new QGraphicsOpacityEffect(toast);
    opacity->setOpacity(0.0);
    toast->setGraphicsEffect(opacity);
    toast->show();
    toast->raise();

    const auto motion = toast->themeAnimation();
    auto* fadeIn = new QPropertyAnimation(opacity, "opacity", toast);
    fadeIn->setDuration(motion.normal);
    fadeIn->setStartValue(0.0);
    fadeIn->setEndValue(1.0);
    fadeIn->setEasingCurve(motion.decelerate);

    auto* slideIn = new QPropertyAnimation(toast, "pos", toast);
    slideIn->setDuration(motion.normal);
    slideIn->setStartValue(startPos);
    slideIn->setEndValue(endPos);
    slideIn->setEasingCurve(motion.decelerate);

    auto* enter = new QParallelAnimationGroup(toast);
    enter->addAnimation(fadeIn);
    enter->addAnimation(slideIn);
    enter->start(QAbstractAnimation::DeleteWhenStopped);

    QPointer<GalleryToastWidget> guard = toast;
    QTimer::singleShot(kToastVisibleMs, toast, [guard, opacity]() {
        if (!guard)
            return;
        const auto motion = guard->themeAnimation();
        auto* fadeOut = new QPropertyAnimation(opacity, "opacity", guard);
        fadeOut->setDuration(motion.fast);
        fadeOut->setStartValue(1.0);
        fadeOut->setEndValue(0.0);
        fadeOut->setEasingCurve(motion.exit);
        QObject::connect(fadeOut, &QPropertyAnimation::finished, guard, [guard]() {
            if (guard)
                guard->deleteLater();
        });
        fadeOut->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

} // namespace fluent::gallery
