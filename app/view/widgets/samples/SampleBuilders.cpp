#include "SampleBuilders.h"

#include <QFont>
#include <QGuiApplication>
#include <QBoxLayout>
#include <QLayoutItem>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QSizePolicy>
#include <QStringList>
#include <QtGlobal>
#include <QVariant>
#include <QWidget>

#include "components/basicinput/Button.h"
#include "design/Typography.h"

namespace fluent::gallery::samples {
namespace {

class ExplicitSpacerBoxLayout final : public QBoxLayout {
public:
    ExplicitSpacerBoxLayout(Direction direction, int spacing, QWidget* parent)
        : QBoxLayout(direction, parent)
        , m_direction(direction)
        , m_spacing(qMax(0, spacing))
    {
        setContentsMargins(0, 0, 0, 0);
        setSpacing(0);
    }

    void addItem(QLayoutItem* item) override
    {
        QBoxLayout::addItem(item);
        ensureSpacingWidgets();
        QBoxLayout::invalidate();
    }

    void invalidate() override
    {
        ensureSpacingWidgets();
        QBoxLayout::invalidate();
    }

    QSize sizeHint() const override
    {
        const_cast<ExplicitSpacerBoxLayout*>(this)->ensureSpacingWidgets();
        return QBoxLayout::sizeHint();
    }

    QSize minimumSize() const override
    {
        const_cast<ExplicitSpacerBoxLayout*>(this)->ensureSpacingWidgets();
        return QBoxLayout::minimumSize();
    }

    void setGeometry(const QRect& rect) override
    {
        ensureSpacingWidgets();
        QBoxLayout::setGeometry(rect);
    }

private:
    static constexpr const char* kSpacingWidgetProperty = "_fluent_sample_spacing_widget";

    bool isHorizontal() const
    {
        return m_direction == QBoxLayout::LeftToRight || m_direction == QBoxLayout::RightToLeft;
    }

    static bool isSpacingWidget(QWidget* widget)
    {
        return widget && widget->property(kSpacingWidgetProperty).toBool();
    }

    static bool isContentItem(QLayoutItem* item)
    {
        return item && !item->spacerItem() && !isSpacingWidget(item->widget()) && !item->isEmpty();
    }

    static bool isButtonLikeItem(QLayoutItem* item)
    {
        return item && qobject_cast<fluent::basicinput::Button*>(item->widget());
    }

    int spacingBetween(QLayoutItem* previous, QLayoutItem* current) const
    {
        int spacing = m_spacing;
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0) && (defined(Q_OS_MACOS) || defined(Q_OS_MAC))
        if (isButtonLikeItem(previous) && isButtonLikeItem(current))
            spacing += 10;
#endif
        return spacing;
    }

    QWidget* createSpacingWidget(int spacing)
    {
        auto* spacer = new QWidget(parentWidget());
        spacer->setProperty(kSpacingWidgetProperty, true);
        spacer->setAttribute(Qt::WA_TransparentForMouseEvents);
        spacer->setFocusPolicy(Qt::NoFocus);
        if (isHorizontal()) {
            spacer->setFixedWidth(spacing);
            spacer->setMinimumHeight(0);
            spacer->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        } else {
            spacer->setMinimumWidth(0);
            spacer->setFixedHeight(spacing);
            spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        }
        return spacer;
    }

    void ensureSpacingWidgets()
    {
        if (m_insertingSpacer || m_spacing <= 0)
            return;

        m_insertingSpacer = true;
        for (int i = 1; i < count(); ++i) {
            if (isContentItem(itemAt(i - 1)) && isContentItem(itemAt(i))) {
                QBoxLayout::insertWidget(i, createSpacingWidget(spacingBetween(itemAt(i - 1), itemAt(i))));
                ++i;
            }
        }
        m_insertingSpacer = false;
    }

    Direction m_direction;
    int m_spacing = 0;
    bool m_insertingSpacer = false;
};

/**
 * @brief Retina-aware pixmap canvas shared by the decorative painters.
 * zh_CN: 各装饰绘制器共用的高分屏感知画布。
 */
qreal samplePixmapDevicePixelRatio()
{
    if (QScreen* screen = QGuiApplication::primaryScreen())
        return qMax<qreal>(1.0, screen->devicePixelRatio());
    return 1.0;
}

QPixmap makeCanvas(const QSize& size)
{
    const qreal dpr = samplePixmapDevicePixelRatio();
    QPixmap pixmap(QSize(qMax(1, qRound(size.width() * dpr)),
                         qMax(1, qRound(size.height() * dpr))));
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

} // namespace

GallerySample makeSample(const QString& id,
                         const QString& title,
                         const QString& description,
                         const QString& codeSnippet,
                         std::function<QWidget*(QWidget*)> createPreview)
{
    GallerySample sample;
    sample.id = id;
    sample.title = title;
    sample.description = description;
    sample.codeSnippet = codeSnippet;
    sample.createPreview = std::move(createPreview);
    return sample;
}

QWidget* verticalGroup(QWidget* parent, int spacing)
{
    auto* group = new QWidget(parent);
    auto* layout = new ExplicitSpacerBoxLayout(QBoxLayout::TopToBottom, spacing, group);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    return group;
}

QWidget* horizontalGroup(QWidget* parent, int spacing)
{
    auto* group = new QWidget(parent);
    auto* layout = new ExplicitSpacerBoxLayout(QBoxLayout::LeftToRight, spacing, group);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return group;
}

QPixmap glyphPixmap(const QString& glyph, const QColor& background, int size)
{
    QPixmap pixmap = makeCanvas(QSize(size, size));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF tile(0, 0, size, size);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawRoundedRect(tile, size / 4.0, size / 4.0);

    QFont iconFont(Typography::FontFamily::SegoeFluentIcons);
    iconFont.setPixelSize(qRound(size * 0.55));
    painter.setFont(iconFont);
    painter.setPen(Qt::white);
    painter.drawText(tile, Qt::AlignCenter, glyph);
    return pixmap;
}

QPixmap initialsAvatar(const QString& name, const QColor& background, int size)
{
    QString initials;
    const QStringList words = name.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& word : words.mid(0, 2))
        initials.append(word.front().toUpper());

    QPixmap pixmap = makeCanvas(QSize(size, size));
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF circle(0, 0, size, size);
    painter.setPen(Qt::NoPen);
    painter.setBrush(background);
    painter.drawEllipse(circle);

    QFont font;
    font.setPixelSize(qRound(size * 0.42));
    font.setWeight(QFont::DemiBold);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(circle, Qt::AlignCenter, initials);
    return pixmap;
}

QPixmap gradientPixmap(const QSize& size,
                       const QColor& from,
                       const QColor& to,
                       const QString& caption)
{
    QPixmap pixmap = makeCanvas(size);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);

    const QRectF surface(0, 0, size.width(), size.height());
    QLinearGradient gradient(surface.topLeft(), surface.bottomRight());
    gradient.setColorAt(0.0, from);
    gradient.setColorAt(1.0, to);

    QPainterPath path;
    path.addRoundedRect(surface, 8, 8);
    painter.fillPath(path, gradient);

    if (!caption.isEmpty()) {
        QFont font;
        font.setPixelSize(15);
        font.setWeight(QFont::DemiBold);
        painter.setFont(font);
        painter.setPen(QColor(255, 255, 255, 230));
        painter.drawText(surface.adjusted(16, 12, -16, -12),
                         Qt::AlignLeft | Qt::AlignBottom, caption);
    }
    return pixmap;
}

} // namespace fluent::gallery::samples
