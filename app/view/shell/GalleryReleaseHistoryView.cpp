#include "GalleryReleaseHistoryView.h"

#include <QAbstractListModel>
#include <QCache>
#include <QCoreApplication>
#include <QDate>
#include <QFile>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QPainter>
#include <QResizeEvent>
#include <QStaticText>
#include <QStyledItemDelegate>
#include <QVersionNumber>
#include <QtMath>
#include <algorithm>
#include <utility>

namespace fluent::gallery {
namespace {
using Role = Typography::FontRole;

struct Release {
    QString date;
    QString version;
    QString headline;
    QStringList paragraphs;
};

QString english(const QJsonValue& field)
{
    QString text = field.toObject().value(QStringLiteral("en")).toString();
    return text.remove(QLatin1Char('`'));
}

QVector<Release> readHistory()
{
    QVector<Release> entries;
    QFile file(QStringLiteral(":/app/assets/release-history.json"));
    QVector<QJsonValue> versions;
    if (file.open(QIODevice::ReadOnly))
        for (const auto& release :
            QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("versions")).toArray())
            versions.append(release);
    const auto versionOf = [](const QJsonValue& release) {
        return QVersionNumber::fromString(release.toObject().value(QStringLiteral("version")).toString());
    };
    std::sort(versions.begin(), versions.end(), [&](const QJsonValue& a, const QJsonValue& b) {
        return versionOf(a) > versionOf(b);
    });
    const auto installed = QVersionNumber::fromString(QCoreApplication::applicationVersion());
    for (const auto& value : versions) {
        const auto release = value.toObject();
        const auto version = release.value(QStringLiteral("version")).toString();
        const auto status = release.value(QStringLiteral("status")).toString();
        // Tagged packages are built while the current release still lacks its publication date.
        const bool bundledCandidate = status == QStringLiteral("candidate") && versionOf(value) == installed;
        if ((!bundledCandidate && status != QStringLiteral("published"))
            || (!installed.isNull() && versionOf(value) > installed))
            continue;
        for (const auto& item : release.value(QStringLiteral("groups")).toArray()) {
            const auto group = item.toObject();
            if (group.value(QStringLiteral("scope")).toString() != QStringLiteral("gallery"))
                continue;
            const auto date = QDate::fromString(release.value(QStringLiteral("published_at")).toString().left(10),
                Qt::ISODate);
            const QString dateText = date.isValid()
                ? QLocale(QLocale::English).toString(date, QStringLiteral("MMMM d, yyyy"))
                : QStringLiteral("This version");
            const auto headline = english(group.value(QStringLiteral("headline")));
            const auto summary = english(group.value(QStringLiteral("summary")));
            QStringList paragraphs;
            for (const auto& change : group.value(QStringLiteral("changes")).toArray())
                paragraphs.append(english(change));
            if (paragraphs.isEmpty() && !headline.isEmpty())
                paragraphs.append(summary);
            entries.append(
                Release{dateText, QStringLiteral("v") + version, headline.isEmpty() ? summary : headline, paragraphs});
        }
    }
    return entries;
}

class HistoryModel final : public QAbstractListModel {
public:
    enum { Date = Qt::UserRole + 1, Version, Paragraphs };
    explicit HistoryModel(QObject* parent) : QAbstractListModel(parent), m_entries(readHistory()) {}

    int rowCount(const QModelIndex& parent = {}) const override { return parent.isValid() ? 0 : m_entries.size(); }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() >= m_entries.size())
            return {};
        const auto& entry = m_entries.at(index.row());
        switch (role) {
        case Qt::DisplayRole:
            return entry.headline;
        case Qt::AccessibleTextRole:
            return QStringList{entry.date, entry.version, entry.headline}.join(QStringLiteral(", "));
        case Qt::AccessibleDescriptionRole:
            return entry.paragraphs.join(QLatin1Char('\n'));
        case Date:
            return entry.date;
        case Version:
            return entry.version;
        case Paragraphs:
            return entry.paragraphs;
        default:
            return {};
        }
    }

private:
    const QVector<Release> m_entries;
};

struct TextBlock {
    int top;
    QStaticText text;
};

struct RowLayout {
    int inset;
    int metaTop;
    int metaHeight;
    QVector<TextBlock> blocks;
    int height;
};

class HistoryDelegate final : public QStyledItemDelegate {
public:
    explicit HistoryDelegate(GalleryReleaseHistoryView* view) : QStyledItemDelegate(view), m_view(view)
    {
        refreshFonts();
    }

    bool refreshFonts()
    {
        const QFont caption = m_view->themeFont(Role::Caption).toQFont();
        const QFont title = m_view->themeFont(Role::BodyLargeStrong).toQFont();
        const QFont body = m_view->themeFont(Role::Body).toQFont();
        if (caption == m_caption && title == m_title && body == m_body)
            return false;
        m_caption = caption;
        m_title = title;
        m_body = body;
        m_width = -1;
        return true;
    }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override
    {
        syncViewport(index.model()->rowCount());
        int height = m_heights.at(index.row());
        if (height < 0)
            height = layout(index).height;
        return QSize(m_width, height);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        syncViewport(index.model()->rowCount());
        const auto& row = layout(index);
        const auto& colors = m_view->themeColorsRef();
        const int width = option.rect.width() - 2 * row.inset;
        painter->save();
        painter->setClipRect(option.rect, Qt::IntersectClip);
        painter->translate(option.rect.topLeft() + QPoint(row.inset, 0));
        if (index.row() > 0)
            painter->fillRect(0, 0, width, 1, colors.strokeDivider);
        painter->setFont(m_caption);
        const QString version = index.data(HistoryModel::Version).toString();
        const int badgeWidth = QFontMetrics(m_caption).horizontalAdvance(version) + 18;
        const QRect badge(width - badgeWidth, row.metaTop, badgeWidth, row.metaHeight);
        painter->setPen(colors.textSecondary);
        painter->drawText(QRect(0, row.metaTop, badge.left(), row.metaHeight), Qt::AlignLeft | Qt::AlignVCenter,
            index.data(HistoryModel::Date).toString());
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.bgCanvas);
        const int radius = m_view->themeRadius().control;
        painter->drawRoundedRect(badge, radius, radius);
        painter->setPen(colors.textSecondary);
        painter->drawText(badge, Qt::AlignCenter, version);
        for (int i = 0; i < row.blocks.size(); ++i) {
            painter->setFont(i == 0 ? m_title : m_body);
            painter->setPen(i == 0 ? colors.textPrimary : colors.textSecondary);
            painter->drawStaticText(QPointF(0, row.blocks[i].top), row.blocks[i].text);
        }
        painter->restore();
    }

private:
    void syncViewport(int count) const
    {
        const int width = m_view->viewport()->width();
        const qreal scale = m_view->devicePixelRatioF();
        if (width != m_width || scale != m_scale || count != m_heights.size()) {
            m_width = width;
            m_scale = scale;
            m_rows.clear();
            m_heights.fill(-1, count);
        }
    }

    const RowLayout& layout(const QModelIndex& index) const
    {
        if (auto* cached = m_rows.object(index.row()))
            return *cached;

        const bool compact = m_width < 558;
        RowLayout row{};
        row.inset = compact ? 22 : 28;
        row.metaTop = index.row() == 0 ? (compact ? 16 : 20) : 14;
        row.metaHeight = QFontMetrics(m_caption).height() + 6;
        int bottom = row.metaTop + row.metaHeight;
        auto append = [&](const QString& text, Qt::TextFormat format, const QFont& font) {
            QStaticText block(text);
            block.setTextFormat(format);
            block.setTextWidth(qMax(1, m_width - 2 * row.inset));
            block.prepare(QTransform(), font);
            const int top = bottom + 9;
            row.blocks.append({top, block});
            bottom = top + qCeil(block.size().height());
        };
        append(index.data().toString(), Qt::PlainText, m_title);
        for (const QString& paragraph : index.data(HistoryModel::Paragraphs).toStringList()) {
            append(QStringLiteral("<p style='margin:0; line-height:125%'>%1</p>").arg(paragraph.toHtmlEscaped()),
                Qt::RichText, m_body);
        }
        row.height = bottom + (index.row() + 1 == index.model()->rowCount() ? 20 : 21);
        m_heights[index.row()] = row.height;
        auto* result = new RowLayout(std::move(row));
        m_rows.insert(index.row(), result);
        return *result;
    }

    GalleryReleaseHistoryView* m_view;
    QFont m_caption;
    QFont m_title;
    QFont m_body;
    mutable int m_width = -1;
    mutable qreal m_scale = 0;
    mutable QVector<int> m_heights;
    // Retain heights for scrolling, but only a working set of shaped text.
    mutable QCache<int, RowLayout> m_rows{32};
};
} // namespace

GalleryReleaseHistoryView::GalleryReleaseHistoryView(QWidget* parent) : ListView(parent)
{
    setObjectName(QStringLiteral("GalleryWhatsNew.History"));
    setAccessibleName(QStringLiteral("Gallery release history"));
    setSpacing(0);
    setBorderVisible(false);
    setSelectionMode(SelectionMode::None);
    setSelectionIndicatorVisible(false);
    setResizeMode(QListView::Adjust);
    setItemDelegate(new HistoryDelegate(this));
    setModel(new HistoryModel(this));
    if (!model()->rowCount())
        setAccessibleDescription(QStringLiteral("No Gallery updates for this version."));
}

void GalleryReleaseHistoryView::onThemeUpdated()
{
    ListView::onThemeUpdated();
    auto* delegate = dynamic_cast<HistoryDelegate*>(itemDelegate());
    if (delegate && delegate->refreshFonts())
        scheduleDelayedItemsLayout();
}

void GalleryReleaseHistoryView::resizeEvent(QResizeEvent* event)
{
    ListView::resizeEvent(event);
    if (event->size().width() != event->oldSize().width())
        scheduleDelayedItemsLayout();
}

void GalleryReleaseHistoryView::paintEvent(QPaintEvent* event)
{
    ListView::paintEvent(event);
    if (model() && !model()->rowCount()) {
        const bool compact = viewport()->width() < 558;
        QPainter painter(viewport());
        painter.setFont(themeFont(Role::Body).toQFont());
        painter.setPen(themeColorsRef().textSecondary);
        painter.drawText(viewport()->rect().adjusted(compact ? 22 : 28, compact ? 16 : 20, 0, 0),
            Qt::AlignLeft | Qt::AlignTop, accessibleDescription());
    }
}

} // namespace fluent::gallery
