#include "ChartReadout_p.h"
#include "ChartView.h"

#include <QEvent>
#include <QPainter>
#include <algorithm>
#include "components/foundation/overlay/OverlayGeometry.h"
#include "components/textfields/Label.h"

namespace fluent::charts::detail {
namespace {
textfields::Label* makeLabel(QWidget* parent)
{
    auto* label = new textfields::Label(parent);
    label->setTextFormat(Qt::PlainText);
    label->setTextElideMode(Qt::ElideNone);
    label->setWordWrap(true);
    label->setTextColorRole(textfields::Label::TextColorRole::Primary);
    label->setAlignment(Qt::AlignLeading | Qt::AlignVCenter);
    return label;
}

int naturalWidth(const textfields::Label* label)
{
    int width = 0;
    for (const auto& line : label->text().split(QLatin1Char('\n')))
        width = std::max(width, label->fontMetrics().horizontalAdvance(line));
    return width + 2;
}

int wrappedHeight(const textfields::Label* label, int width, int minimum = 20)
{
    return std::max(minimum, label->heightForWidth(width));
}
} // namespace

ChartReadout::ChartReadout(QWidget* chart)
    : Popup(chart), m_chart(chart), m_heading(makeLabel(this)), m_more(makeLabel(this))
{
    setObjectName(QStringLiteral("FluentChartReadout"));
    setFocusPolicy(Qt::NoFocus);
    setFocusOnOpenEnabled(false);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setClosePolicy(NoAutoClose);
    setModal(false);
    setDim(false);
    setAnimationEnabled(false);
    setThemeSource(chart);
    m_heading->setFluentTypography(Typography::FontRole::Caption);
    m_heading->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    m_more->setFluentTypography(Typography::FontRole::Caption);
    m_more->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    m_more->hide();
}

void ChartReadout::present(const QString& heading, const QVector<ReadoutRow>& rows,
                           const QPointF& anchor)
{
    if (!m_chart || rows.isEmpty()) {
        close();
        return;
    }
    const int count = std::min(int(rows.size()), std::max(1, (m_chart->height() - 100) / 28));
    const int margin = overlay::defaultShadowMargin();
    m_heading->setText(heading);
    int nameWidth = 0, valueWidth = 0;
    for (int i = 0; i < count; ++i) {
        if (i == m_rows.size())
            m_rows.append({makeLabel(this), makeLabel(this), makeLabel(this), {}, 0});
        auto& row = m_rows[i];
        const auto& data = rows[i];
        const qreal dpr = devicePixelRatioF();
        if (row.color != data.color || row.dpr != dpr) {
            QPixmap marker(QSize(qRound(8 * dpr), qRound(8 * dpr)));
            marker.setDevicePixelRatio(dpr);
            marker.fill(Qt::transparent);
            QPainter painter(&marker);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(data.color);
            painter.drawRoundedRect(QRectF(0, 0, 8, 8), 2, 2);
            painter.end();
            row.marker->setPixmap(marker);
            row.color = data.color;
            row.dpr = dpr;
        }
        const auto role =
            data.selected ? Typography::FontRole::BodyStrong : Typography::FontRole::Caption;
        row.name->setFluentTypography(role);
        row.value->setFluentTypography(role);
        row.name->setTextColorRole(data.selected ? textfields::Label::TextColorRole::Primary
                                                 : textfields::Label::TextColorRole::Secondary);
        row.name->setText(data.name);
        row.value->setText(data.text);
        nameWidth = std::max(nameWidth, naturalWidth(row.name));
        valueWidth = std::max(valueWidth, naturalWidth(row.value));
    }

    // Detail text is never elided. Grow to the content, then wrap names;
    // use a stacked value when two readable columns no longer fit.
    const int availableWidth = std::max(96, m_chart->width() - 32);
    const int preferredWidth =
        std::max({224, naturalWidth(m_heading) + 32, nameWidth + valueWidth + 60});
    const int width = std::min(preferredWidth, std::min(360, availableWidth));
    const bool stacked = width - 60 - valueWidth < std::min(nameWidth, 96);
    const int headingHeight = wrappedHeight(m_heading, width - 32, 24);
    m_heading->setGeometry(margin + 16, margin + 12, width - 32, headingHeight);
    m_more->setText(ChartView::tr("%1 more series").arg(rows.size()));
    const int moreHeight = wrappedHeight(m_more, width - 32);
    int y = margin + 12 + headingHeight + 8;
    int visibleCount = 0;
    for (int i = 0; i < count; ++i) {
        auto& row = m_rows[i];
        const int textWidth = stacked ? width - 48 : width - 60 - valueWidth;
        const int nameHeight = wrappedHeight(row.name, textWidth);
        const int valueHeight = wrappedHeight(row.value, stacked ? width - 48 : valueWidth);
        const int rowHeight =
            stacked ? nameHeight + 4 + valueHeight : std::max(nameHeight, valueHeight);
        const int remainingHeight = i + 1 < rows.size() ? moreHeight + 8 : 0;
        if (i > 0 && y + rowHeight + remainingHeight + 16 > margin + m_chart->height() - 32)
            break;
        row.marker->setGeometry(margin + 16, y + (row.name->fontMetrics().height() - 8) / 2, 8, 8);
        row.name->setAlignment(Qt::AlignLeading | Qt::AlignTop);
        row.name->setGeometry(margin + 32, y, textWidth, nameHeight);
        if (stacked) {
            row.value->setAlignment(Qt::AlignLeading | Qt::AlignTop);
            row.value->setGeometry(margin + 32, y + nameHeight + 4, width - 48, valueHeight);
        } else {
            row.value->setAlignment(Qt::AlignRight | Qt::AlignTop);
            row.value->setGeometry(margin + width - 16 - valueWidth, y, valueWidth, valueHeight);
        }
        y += rowHeight + 8;
        ++visibleCount;
        row.marker->show();
        row.name->show();
        row.value->show();
    }
    for (int i = visibleCount; i < m_rows.size(); ++i) {
        m_rows[i].marker->hide();
        m_rows[i].name->hide();
        m_rows[i].value->hide();
    }
    if (visibleCount < rows.size()) {
        m_more->setText(ChartView::tr("%1 more series").arg(rows.size() - visibleCount));
        m_more->setGeometry(margin + 16, y, width - 32, moreHeight);
        m_more->show();
        y += moreHeight + 8;
    } else {
        m_more->hide();
    }
    const int height = y - margin + 8;
    setFixedSize(overlay::outerSizeForVisibleCard(QSize(width, height)));
    QPoint origin(qRound(anchor.x()) + 16, qRound(anchor.y()) - height / 2);
    if (origin.x() + width > m_chart->width() - 16)
        origin.setX(qRound(anchor.x()) - width - 16);
    origin.setX(std::clamp(origin.x(), 16, std::max(16, m_chart->width() - width - 16)));
    origin.setY(std::clamp(origin.y(), 16, std::max(16, m_chart->height() - height - 16)));
    setPosition(m_chart, origin);
    if (isOpen())
        move(overlay::outerTopLeftForVisibleCard(m_chart->mapTo(parentWidget(), origin)));
    else
        open();
}

bool ChartReadout::eventFilter(QObject* watched, QEvent* event)
{
    if (m_chart && watched == m_chart->window() && event->type() == QEvent::WindowDeactivate)
        close();
    return Popup::eventFilter(watched, event);
}

} // namespace fluent::charts::detail
