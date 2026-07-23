#include "GalleryIconBrowser.h"

#include <algorithm>
#include <limits>
#include <utility>

#include <QAbstractScrollArea>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QGuiApplication>
#include <QHash>
#include <QHelpEvent>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPointer>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

#include "components/scrolling/PipsPager.h"
#include "components/status_info/ToolTip.h"
#include "components/textfields/Label.h"
#include "components/textfields/LineEdit.h"
#include "design/CornerRadius.h"
#include "design/IconCatalog.h"
#include "design/Typography.h"
#include "view/support/GalleryToast.h"

namespace fluent::gallery {
namespace {

constexpr int kMinimumTileWidth = 44;
constexpr int kTileHeight = 44;
constexpr int kTileGap = 6;
constexpr int kMaximumColumns = 24;
constexpr int kItemsPerPage = 216;
constexpr int kEmptyGridHeight = 96;
constexpr int kHoverTipDelayMs = 320;
constexpr int kNoMatchScore = std::numeric_limits<int>::max();

QString codepointText(quint32 codepoint)
{
    return QStringLiteral("U+%1").arg(codepoint, codepoint > 0xFFFF ? 6 : 4, 16,
                                      QLatin1Char('0')).toUpper();
}

struct SearchQuery {
    QStringList exactIconNames;
    QStringList textTerms;
    QSet<int> designSizes;
    QSet<quint32> codepoints;
};

struct SearchRecord {
    QString name;
    QString semanticName;
    QString compactSemanticName;
    QStringList semanticTokens;
    quint32 codepoint = 0;
    int designSize = 0;
    int catalogRow = 0;
};

struct RankedRow {
    int catalogRow = 0;
    int score = 0;
};

QString normalizedWords(QString value)
{
    value.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_+]+")),
                  QStringLiteral(" "));
    return value.toLower().simplified();
}

SearchQuery parseSearchQuery(const QString& query, const QSet<int>& knownDesignSizes)
{
    static const QRegularExpression completeIconNamePattern(
        QStringLiteral(R"((ic_fluent_[A-Za-z0-9_]+_[0-9]+_regular)\b)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression codepointPattern(
        QStringLiteral(R"(^u\+([0-9a-f]{1,6})$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression integerPattern(QStringLiteral(R"(^[0-9]+$)"));

    SearchQuery result;
    auto iconMatches = completeIconNamePattern.globalMatch(query);
    while (iconMatches.hasNext()) {
        const QString name = iconMatches.next().captured(1).toLower();
        if (!result.exactIconNames.contains(name))
            result.exactIconNames.append(name);
    }
    // A copied lookup is a stable identifier. Ignore surrounding C++ syntax
    // and never reinterpret a misspelled identifier as a fuzzy query.
    if (!result.exactIconNames.isEmpty())
        return result;

    const QStringList terms = normalizedWords(query).split(
        QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& term : terms) {
        const QRegularExpressionMatch codepointMatch = codepointPattern.match(term);
        if (codepointMatch.hasMatch()) {
            bool ok = false;
            const quint32 codepoint = codepointMatch.captured(1).toUInt(&ok, 16);
            if (ok)
                result.codepoints.insert(codepoint);
            continue;
        }

        if (integerPattern.match(term).hasMatch()) {
            bool ok = false;
            const int number = term.toInt(&ok);
            if (ok && knownDesignSizes.contains(number)) {
                result.designSizes.insert(number);
                continue;
            }
        }
        result.textTerms.append(term);
    }
    return result;
}

const QHash<QString, QStringList>& searchAliases()
{
    static const QHash<QString, QStringList> aliases = [] {
        QHash<QString, QStringList> value;
        value.insert(QStringLiteral("trash"), {QStringLiteral("delete")});
        value.insert(QStringLiteral("gear"), {QStringLiteral("settings")});
        value.insert(QStringLiteral("cog"), {QStringLiteral("settings")});
        value.insert(QStringLiteral("preferences"), {QStringLiteral("settings")});
        value.insert(QStringLiteral("close"), {QStringLiteral("dismiss")});
        value.insert(QStringLiteral("x"), {QStringLiteral("dismiss")});
        value.insert(QStringLiteral("duplicate"), {QStringLiteral("copy")});
        value.insert(QStringLiteral("bell"), {QStringLiteral("alert")});
        value.insert(QStringLiteral("notification"), {QStringLiteral("alert")});
        value.insert(QStringLiteral("email"), {QStringLiteral("mail")});
        value.insert(QStringLiteral("envelope"), {QStringLiteral("mail")});
        value.insert(QStringLiteral("photo"), {QStringLiteral("image")});
        value.insert(QStringLiteral("picture"), {QStringLiteral("image")});
        value.insert(QStringLiteral("user"), {QStringLiteral("person")});
        value.insert(QStringLiteral("profile"), {QStringLiteral("person")});
        value.insert(QStringLiteral("account"), {QStringLiteral("person")});
        return value;
    }();
    return aliases;
}

int directTermScore(const SearchRecord& record, const QString& term)
{
    if (record.semanticName == term)
        return 0;
    if (record.compactSemanticName == term)
        return 1;
    for (const QString& token : record.semanticTokens) {
        if (token == term)
            return 2;
    }
    if (record.semanticName.startsWith(term)
        || record.compactSemanticName.startsWith(term)) {
        return 4;
    }
    for (const QString& token : record.semanticTokens) {
        if (token.startsWith(term))
            return 6;
    }
    if (term.size() >= 3 && record.semanticName.contains(term))
        return 10;
    if ((term.size() >= 3 || term.startsWith(QStringLiteral("ic_fluent_")))
        && record.name.contains(term)) {
        return 12;
    }
    return kNoMatchScore;
}

bool matchesStructuredFilters(const SearchRecord& record, const SearchQuery& query)
{
    return (query.designSizes.isEmpty() || query.designSizes.contains(record.designSize))
        && (query.codepoints.isEmpty() || query.codepoints.contains(record.codepoint));
}

int strictMatchScore(const SearchRecord& record, const SearchQuery& query)
{
    if (!matchesStructuredFilters(record, query))
        return kNoMatchScore;

    int score = 0;
    for (const QString& term : query.textTerms) {
        const int termScore = directTermScore(record, term);
        if (termScore == kNoMatchScore)
            return kNoMatchScore;
        score += termScore;
    }
    return score;
}

bool isFuzzyTextTerm(const QString& term)
{
    if (term.size() < 3)
        return false;
    for (const QChar character : term) {
        if (!character.isLetter())
            return false;
    }
    return true;
}

int maximumEditDistance(int length)
{
    if (length <= 4)
        return 1;
    if (length <= 8)
        return 2;
    return qMin(3, qMax(2, length / 4));
}

int boundedDamerauLevenshtein(const QString& left, const QString& right,
                              int maximumDistance)
{
    if (qAbs(left.size() - right.size()) > maximumDistance)
        return maximumDistance + 1;
    if (left == right)
        return 0;

    QVector<int> previousPrevious(right.size() + 1);
    QVector<int> previous(right.size() + 1);
    QVector<int> current(right.size() + 1);
    for (int column = 0; column <= right.size(); ++column)
        previous[column] = column;

    for (int row = 1; row <= left.size(); ++row) {
        current[0] = row;
        for (int column = 1; column <= right.size(); ++column) {
            const int substitutionCost = left.at(row - 1) == right.at(column - 1) ? 0 : 1;
            int distance = qMin(previous[column] + 1,
                                qMin(current[column - 1] + 1,
                                     previous[column - 1] + substitutionCost));
            if (row > 1 && column > 1
                && left.at(row - 1) == right.at(column - 2)
                && left.at(row - 2) == right.at(column - 1)) {
                distance = qMin(distance, previousPrevious[column - 2] + 1);
            }
            current[column] = distance;
        }
        previousPrevious.swap(previous);
        previous.swap(current);
    }
    return previous.at(right.size());
}

int fuzzyTermScore(const SearchRecord& record, const QString& term)
{
    if (!isFuzzyTextTerm(term))
        return kNoMatchScore;

    const int threshold = maximumEditDistance(term.size());
    int bestDistance = threshold + 1;
    int bestLengthDifference = threshold + 1;
    for (const QString& token : record.semanticTokens) {
        if (!isFuzzyTextTerm(token))
            continue;
        const int lengthDifference = qAbs(term.size() - token.size());
        if (lengthDifference > threshold)
            continue;
        const int distance = boundedDamerauLevenshtein(term, token, threshold);
        if (distance < bestDistance
            || (distance == bestDistance && lengthDifference < bestLengthDifference)) {
            bestDistance = distance;
            bestLengthDifference = lengthDifference;
        }
    }

    if (term.size() >= 5) {
        const int lengthDifference = qAbs(term.size() - record.compactSemanticName.size());
        if (lengthDifference <= threshold) {
            const int distance = boundedDamerauLevenshtein(
                term, record.compactSemanticName, threshold);
            if (distance < bestDistance
                || (distance == bestDistance && lengthDifference < bestLengthDifference)) {
                bestDistance = distance;
                bestLengthDifference = lengthDifference;
            }
        }
    }

    return bestDistance <= threshold
        ? 100 + bestDistance * 20 + bestLengthDifference
        : kNoMatchScore;
}

int closestMatchScore(const SearchRecord& record, const SearchQuery& query,
                      bool allowFuzzy)
{
    if (!matchesStructuredFilters(record, query))
        return kNoMatchScore;

    int score = 0;
    for (const QString& term : query.textTerms) {
        const int directScore = directTermScore(record, term);
        if (directScore != kNoMatchScore) {
            score += directScore;
            continue;
        }

        int aliasScore = kNoMatchScore;
        const auto alias = searchAliases().constFind(term);
        if (alias != searchAliases().constEnd()) {
            for (const QString& alternative : alias.value()) {
                const int alternativeScore = directTermScore(record, alternative);
                if (alternativeScore != kNoMatchScore)
                    aliasScore = qMin(aliasScore, 60 + alternativeScore);
            }
        }
        if (aliasScore != kNoMatchScore) {
            score += aliasScore;
            continue;
        }

        if (!allowFuzzy)
            return kNoMatchScore;
        const int fuzzyScore = fuzzyTermScore(record, term);
        if (fuzzyScore == kNoMatchScore)
            return kNoMatchScore;
        score += fuzzyScore;
    }
    return score;
}

} // namespace

/**
 * Dense paint-on-demand grid for one bounded catalog page. Pagination keeps
 * the outer Gallery scroll range useful; this widget never creates a nested
 * scrolling surface or thousands of child controls.
 */
class GalleryIconBrowser::Grid final : public QWidget {
public:
    explicit Grid(GalleryIconBrowser* browser)
        : QWidget(browser)
        , m_browser(browser)
        , m_catalog(Typography::Icons::catalog())
    {
        setObjectName(QStringLiteral("galleryIconGrid"));
        setAccessibleName(QStringLiteral("Fluent icon catalog page"));
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

        m_searchRecords.reserve(m_catalog.size());
        m_rows.reserve(m_catalog.size());
        for (int row = 0; row < m_catalog.size(); ++row) {
            const auto& icon = m_catalog.at(row);
            SearchRecord record;
            record.name = icon.name.toLower();
            record.semanticName = normalizedWords(icon.displayName);
            record.compactSemanticName = record.semanticName;
            record.compactSemanticName.remove(QLatin1Char(' '));
            record.semanticTokens = record.semanticName.split(
                QLatin1Char(' '), Qt::SkipEmptyParts);
            record.codepoint = icon.codepoint;
            record.designSize = icon.designSize;
            record.catalogRow = row;
            m_searchRecords.append(std::move(record));
            m_knownDesignSizes.insert(icon.designSize);
            m_rows.append(row);
        }

        m_toolTipAnchor = new QWidget(this);
        m_toolTipAnchor->setObjectName(QStringLiteral("galleryIconHoverAnchor"));
        m_toolTipAnchor->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_toolTipAnchor->setAttribute(Qt::WA_NoSystemBackground);
        m_toolTipAnchor->hide();

        m_hoverTip = fluent::status_info::ToolTip::attach(
            m_toolTipAnchor, QString(), fluent::status_info::ToolTip::Above);
        m_hoverTip->setObjectName(QStringLiteral("galleryIconHoverTip"));
        m_hoverTip->setThemeSource(browser);
        m_hoverTip->setMargins(QMargins(12, 8, 12, 10));

        m_hoverTimer = new QTimer(this);
        m_hoverTimer->setSingleShot(true);
        m_hoverTimer->setInterval(kHoverTipDelayMs);
        connect(m_hoverTimer, &QTimer::timeout, this, [this]() { showHoverTip(); });
    }

    int totalCount() const { return m_catalog.size(); }
    int filteredCount() const { return m_rows.size(); }
    int currentPage() const { return m_pageIndex; }
    bool showingClosestMatches() const { return m_showingClosestMatches; }

    int pageCount() const
    {
        return qMax(1, (m_rows.size() + kItemsPerPage - 1) / kItemsPerPage);
    }

    int pageItemCount() const
    {
        return qMax(0, qMin(kItemsPerPage,
                            m_rows.size() - m_pageIndex * kItemsPerPage));
    }

    int firstItemOffset() const
    {
        return m_rows.isEmpty() ? 0 : m_pageIndex * kItemsPerPage;
    }

    const Typography::Icons::IconInfo* iconAt(int pageItemIndex) const
    {
        const int filteredIndex = firstItemOffset() + pageItemIndex;
        if (pageItemIndex < 0 || filteredIndex < 0 || filteredIndex >= m_rows.size())
            return nullptr;
        return &m_catalog.at(m_rows.at(filteredIndex));
    }

    void setFilter(const QString& filter)
    {
        const SearchQuery query = parseSearchQuery(filter, m_knownDesignSizes);
        QVector<RankedRow> rankedRows;
        rankedRows.reserve(m_catalog.size());

        if (!query.exactIconNames.isEmpty()) {
            for (const SearchRecord& record : m_searchRecords) {
                if (query.exactIconNames.contains(record.name))
                    rankedRows.append({record.catalogRow, 0});
            }
        } else {
            for (const SearchRecord& record : m_searchRecords) {
                const int score = strictMatchScore(record, query);
                if (score != kNoMatchScore)
                    rankedRows.append({record.catalogRow, score});
            }
        }

        m_showingClosestMatches = false;
        if (rankedRows.isEmpty() && query.exactIconNames.isEmpty()
            && !query.textTerms.isEmpty()) {
            for (const SearchRecord& record : m_searchRecords) {
                const int score = closestMatchScore(record, query, false);
                if (score != kNoMatchScore)
                    rankedRows.append({record.catalogRow, score});
            }
            if (rankedRows.isEmpty()) {
                for (const SearchRecord& record : m_searchRecords) {
                    const int score = closestMatchScore(record, query, true);
                    if (score != kNoMatchScore)
                        rankedRows.append({record.catalogRow, score});
                }
            }
            m_showingClosestMatches = !rankedRows.isEmpty();
        }

        std::stable_sort(rankedRows.begin(), rankedRows.end(),
                         [](const RankedRow& left, const RankedRow& right) {
            return left.score == right.score
                ? left.catalogRow < right.catalogRow
                : left.score < right.score;
        });

        m_rows.clear();
        m_rows.reserve(rankedRows.size());
        for (const RankedRow& row : rankedRows)
            m_rows.append(row.catalogRow);

        m_pageIndex = 0;
        resetInteraction();
        refreshGeometry();
    }

    void setPage(int pageIndex)
    {
        const int bounded = qBound(0, pageIndex, pageCount() - 1);
        if (bounded == m_pageIndex)
            return;
        m_pageIndex = bounded;
        resetInteraction();
        refreshGeometry();
    }

    void refreshTheme()
    {
        if (m_hoverTip)
            m_hoverTip->onThemeUpdated();
        update();
    }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int availableWidth) const override
    {
        const int itemCount = pageItemCount();
        if (itemCount == 0)
            return kEmptyGridHeight;
        const int columns = columnCountForWidth(availableWidth);
        const int rowCount = (itemCount + columns - 1) / columns;
        return rowCount * kTileHeight + qMax(0, rowCount - 1) * kTileGap;
    }

    QSize sizeHint() const override
    {
        const int preferredWidth = width() >= kMinimumTileWidth ? width() : 920;
        return QSize(preferredWidth, heightForWidth(preferredWidth));
    }

    QSize minimumSizeHint() const override
    {
        return QSize(kMinimumTileWidth, kTileHeight);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.setClipRect(event->rect());
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        const auto colors = m_browser->themeColors();
        const int itemCount = pageItemCount();
        if (itemCount == 0) {
            painter.setFont(m_browser->themeFont(Typography::FontRole::Body).toQFont());
            painter.setPen(colors.textSecondary);
            painter.drawText(rect(), Qt::AlignCenter,
                             QStringLiteral("No icons match this search."));
            return;
        }

        const int columns = columnCountForWidth(width());
        const int rowStep = kTileHeight + kTileGap;
        const int firstRow = qMax(0, event->rect().top() / rowStep);
        const int lastRow = qMin((itemCount - 1) / columns,
                                 event->rect().bottom() / rowStep);

        for (int row = firstRow; row <= lastRow; ++row) {
            for (int column = 0; column < columns; ++column) {
                const int index = row * columns + column;
                const auto* icon = iconAt(index);
                if (!icon)
                    break;

                const QRect tile = tileRect(index);
                const bool hovered = index == m_hoveredIndex;
                const bool pressed = index == m_pressedIndex;
                const QRectF card = QRectF(tile).adjusted(0.5, 0.5, -0.5, -0.5);
                painter.setPen(QPen(hovered ? colors.accentDefault : colors.strokeCard, 1.0));
                painter.setBrush(pressed ? colors.subtleTertiary
                                         : hovered ? colors.subtleSecondary : colors.bgLayer);
                painter.drawRoundedRect(card, ::CornerRadius::Control,
                                        ::CornerRadius::Control);

                painter.setFont(Typography::Icons::font(Typography::IconSize::Large));
                painter.setPen(hovered ? colors.textAccentPrimary : colors.textPrimary);
                painter.drawText(tile.adjusted(5, 5, -5, -5), Qt::AlignCenter,
                                 Typography::Icons::glyphForSize(
                                     icon->glyph(), Typography::IconSize::Large));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int hovered = indexAt(event->pos());
        if (hovered == m_hoveredIndex)
            return;
        updateTile(m_hoveredIndex);
        hideHoverTip();
        m_hoveredIndex = hovered;
        updateTile(m_hoveredIndex);
        setCursor(m_hoveredIndex >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
        if (m_hoveredIndex >= 0)
            m_hoverTimer->start();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        hideHoverTip();
        if (event->button() != Qt::LeftButton)
            return;
        m_pressedIndex = indexAt(event->pos());
        updateTile(m_pressedIndex);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const int released = event->button() == Qt::LeftButton
            ? indexAt(event->pos()) : -1;
        const int pressed = m_pressedIndex;
        m_pressedIndex = -1;
        updateTile(pressed);
        if (pressed >= 0 && released == pressed)
            m_browser->copyIcon(pressed);
    }

    void leaveEvent(QEvent*) override
    {
        updateTile(m_hoveredIndex);
        updateTile(m_pressedIndex);
        m_hoveredIndex = -1;
        m_pressedIndex = -1;
        unsetCursor();
        hideHoverTip();
    }

    void wheelEvent(QWheelEvent* event) override
    {
        hideHoverTip();
        event->ignore();
    }

    void showEvent(QShowEvent* event) override
    {
        QWidget::showEvent(event);
        if (m_scrollBar)
            return;
        for (QWidget* ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            if (auto* area = qobject_cast<QAbstractScrollArea*>(ancestor)) {
                m_scrollBar = area->verticalScrollBar();
                connect(m_scrollBar, &QScrollBar::valueChanged,
                        this, [this](int) { hideHoverTip(); });
                break;
            }
        }
    }

    void hideEvent(QHideEvent* event) override
    {
        hideHoverTip();
        QWidget::hideEvent(event);
    }

private:
    int columnCountForWidth(int availableWidth) const
    {
        return qBound(1,
                      (qMax(1, availableWidth) + kTileGap)
                          / (kMinimumTileWidth + kTileGap),
                      kMaximumColumns);
    }

    QRect tileRect(int index) const
    {
        const int columns = columnCountForWidth(width());
        const int column = index % columns;
        const int row = index / columns;
        const int contentWidth = qMax(1, width()) - (columns - 1) * kTileGap;
        const int baseWidth = contentWidth / columns;
        const int remainder = contentWidth % columns;
        const int left = column * (baseWidth + kTileGap) + qMin(column, remainder);
        const int tileWidth = baseWidth + (column < remainder ? 1 : 0);
        return QRect(left, row * (kTileHeight + kTileGap), tileWidth, kTileHeight);
    }

    int indexAt(const QPoint& point) const
    {
        if (!rect().contains(point) || pageItemCount() == 0)
            return -1;
        const int rowStep = kTileHeight + kTileGap;
        const int row = point.y() / rowStep;
        if (point.y() % rowStep >= kTileHeight)
            return -1;

        const int columns = columnCountForWidth(width());
        for (int column = 0; column < columns; ++column) {
            const int index = row * columns + column;
            if (index >= pageItemCount())
                return -1;
            if (tileRect(index).contains(point))
                return index;
        }
        return -1;
    }

    void updateTile(int index)
    {
        if (index >= 0 && index < pageItemCount())
            update(tileRect(index).adjusted(-1, -1, 1, 1));
    }

    void showHoverTip()
    {
        const auto* icon = iconAt(m_hoveredIndex);
        if (!icon || !m_toolTipAnchor || !m_hoverTip)
            return;

        m_toolTipAnchor->setGeometry(tileRect(m_hoveredIndex));
        m_toolTipAnchor->show();
        m_toolTipAnchor->raise();
        m_hoverTip->setText(
            QStringLiteral("%1\n%2\n%3  ·  %4 px\nClick to copy C++ lookup")
                .arg(icon->displayName,
                     icon->name,
                     codepointText(icon->codepoint))
                .arg(icon->designSize));

        const QPoint local = m_toolTipAnchor->rect().center();
        QHelpEvent help(QEvent::ToolTip,
                        local,
                        m_toolTipAnchor->mapToGlobal(local));
        QCoreApplication::sendEvent(m_toolTipAnchor, &help);
    }

    void hideHoverTip()
    {
        if (m_hoverTimer)
            m_hoverTimer->stop();
        if (m_hoverTip)
            m_hoverTip->hide();
        if (m_toolTipAnchor)
            m_toolTipAnchor->hide();
    }

    void resetInteraction()
    {
        hideHoverTip();
        m_hoveredIndex = -1;
        m_pressedIndex = -1;
        unsetCursor();
    }

    void refreshGeometry()
    {
        updateGeometry();
        if (m_browser) {
            if (m_browser->layout())
                m_browser->layout()->invalidate();
            m_browser->updateGeometry();
        }
        update();
    }

    GalleryIconBrowser* m_browser = nullptr;
    const QVector<Typography::Icons::IconInfo>& m_catalog;
    QVector<SearchRecord> m_searchRecords;
    QSet<int> m_knownDesignSizes;
    QVector<int> m_rows;
    bool m_showingClosestMatches = false;
    int m_pageIndex = 0;
    int m_hoveredIndex = -1;
    int m_pressedIndex = -1;
    QWidget* m_toolTipAnchor = nullptr;
    fluent::status_info::ToolTip* m_hoverTip = nullptr;
    QTimer* m_hoverTimer = nullptr;
    QPointer<QScrollBar> m_scrollBar;
};

GalleryIconBrowser::GalleryIconBrowser(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("galleryIconBrowser"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* guidance = new fluent::textfields::Label(
        QStringLiteral("Search the complete Regular catalog by name, size, or U+ codepoint. Exact matches stay deterministic; typos fall back to the closest names. Hover for metadata and click to copy a C++ lookup."),
        this);
    guidance->setFluentTypography(Typography::FontRole::Caption);
    guidance->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    guidance->setWordWrap(true);
    layout->addWidget(guidance);

    auto* searchRow = new QHBoxLayout;
    searchRow->setContentsMargins(0, 0, 0, 0);
    searchRow->setSpacing(12);
    auto* search = new fluent::textfields::LineEdit(this);
    search->setObjectName(QStringLiteral("galleryIconSearch"));
    search->setPlaceholderText(
        QStringLiteral("Search name, 20, U+F109, or paste a C++ lookup..."));
    search->setClearButtonEnabled(true);
    search->setMinimumWidth(240);
    m_countLabel = new fluent::textfields::Label(this);
    m_countLabel->setObjectName(QStringLiteral("galleryIconCount"));
    m_countLabel->setFluentTypography(Typography::FontRole::Caption);
    m_countLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);
    searchRow->addWidget(search, 1);
    searchRow->addWidget(m_countLabel, 0, Qt::AlignVCenter);
    layout->addLayout(searchRow);

    m_grid = new Grid(this);
    layout->addWidget(m_grid);

    m_pagination = new QWidget(this);
    m_pagination->setObjectName(QStringLiteral("galleryIconPagination"));
    auto* paginationLayout = new QHBoxLayout(m_pagination);
    paginationLayout->setContentsMargins(0, 0, 0, 0);
    paginationLayout->setSpacing(12);

    m_pageLabel = new fluent::textfields::Label(m_pagination);
    m_pageLabel->setObjectName(QStringLiteral("galleryIconPageLabel"));
    m_pageLabel->setFluentTypography(Typography::FontRole::Caption);
    m_pageLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Secondary);

    m_pager = new fluent::scrolling::PipsPager(m_pagination);
    m_pager->setObjectName(QStringLiteral("galleryIconPager"));
    m_pager->setMaxVisiblePips(7);
    m_pager->setPreviousButtonVisibility(
        fluent::scrolling::PipsPager::PipsPagerButtonVisibility::Visible);
    m_pager->setNextButtonVisibility(
        fluent::scrolling::PipsPager::PipsPagerButtonVisibility::Visible);

    paginationLayout->addWidget(m_pageLabel, 0, Qt::AlignVCenter);
    paginationLayout->addStretch(1);
    paginationLayout->addWidget(m_pager, 0, Qt::AlignVCenter);
    layout->addWidget(m_pagination);

    connect(search, &QLineEdit::textChanged, this, [this](const QString& value) {
        m_grid->setFilter(value);
        updateCountLabel();
        updatePagination();
    });
    connect(m_pager, &fluent::scrolling::PipsPager::selectedPageIndexChanged,
            this, [this](int pageIndex) { selectPage(pageIndex, true); });

    updateCountLabel();
    updatePagination();
    onThemeUpdated();
}

void GalleryIconBrowser::onThemeUpdated()
{
    if (m_grid)
        m_grid->refreshTheme();
    if (m_countLabel)
        m_countLabel->onThemeUpdated();
    if (m_pageLabel)
        m_pageLabel->onThemeUpdated();
    if (m_pager)
        m_pager->onThemeUpdated();
}

int GalleryIconBrowser::iconCount() const
{
    return m_grid ? m_grid->totalCount() : 0;
}

int GalleryIconBrowser::visibleIconCount() const
{
    return m_grid ? m_grid->filteredCount() : 0;
}

bool GalleryIconBrowser::showingClosestMatches() const
{
    return m_grid && m_grid->showingClosestMatches();
}

void GalleryIconBrowser::updateCountLabel()
{
    if (!m_countLabel || !m_grid)
        return;
    const QLocale locale;
    if (m_grid->showingClosestMatches()) {
        m_countLabel->setText(
            QStringLiteral("Closest matches: %1 of %2")
                .arg(locale.toString(m_grid->filteredCount()),
                     locale.toString(m_grid->totalCount())));
    } else {
        m_countLabel->setText(
            m_grid->filteredCount() == m_grid->totalCount()
                ? QStringLiteral("%1 icons").arg(locale.toString(m_grid->totalCount()))
                : QStringLiteral("%1 of %2")
                      .arg(locale.toString(m_grid->filteredCount()),
                           locale.toString(m_grid->totalCount())));
    }
}

void GalleryIconBrowser::updatePagination()
{
    if (!m_grid || !m_pagination || !m_pageLabel || !m_pager)
        return;

    const int pageCount = m_grid->pageCount();
    {
        const QSignalBlocker blocker(m_pager);
        m_pager->setNumberOfPages(pageCount);
        m_pager->setSelectedPageIndex(m_grid->currentPage());
    }

    m_pagination->setVisible(pageCount > 1);
    const int first = m_grid->filteredCount() == 0 ? 0 : m_grid->firstItemOffset() + 1;
    const int last = m_grid->firstItemOffset() + m_grid->pageItemCount();
    const QLocale locale;
    m_pageLabel->setText(
        QStringLiteral("%1-%2 of %3  ·  Page %4 of %5")
            .arg(locale.toString(first),
                 locale.toString(last),
                 locale.toString(m_grid->filteredCount()),
                 locale.toString(m_grid->currentPage() + 1),
                 locale.toString(pageCount)));
}

void GalleryIconBrowser::selectPage(int pageIndex, bool scrollToGrid)
{
    if (!m_grid)
        return;
    const int previousPage = m_grid->currentPage();
    m_grid->setPage(pageIndex);
    updatePagination();
    if (!scrollToGrid || previousPage == m_grid->currentPage())
        return;

    QTimer::singleShot(0, this, [this]() {
        for (QWidget* ancestor = parentWidget(); ancestor; ancestor = ancestor->parentWidget()) {
            auto* scrollArea = qobject_cast<QScrollArea*>(ancestor);
            if (!scrollArea || !scrollArea->widget() || !scrollArea->verticalScrollBar())
                continue;
            const int gridTop = m_grid->mapTo(scrollArea->widget(), QPoint(0, 0)).y();
            QScrollBar* bar = scrollArea->verticalScrollBar();
            bar->setValue(qBound(bar->minimum(), gridTop - 12, bar->maximum()));
            break;
        }
    });
}

void GalleryIconBrowser::copyIcon(int pageItemIndex)
{
    const auto* icon = m_grid ? m_grid->iconAt(pageItemIndex) : nullptr;
    if (!icon)
        return;
    const QString snippet = QStringLiteral(
                                "Typography::Icons::glyph(QStringLiteral(\"%1\"))")
                                .arg(icon->name);
    if (QClipboard* clipboard = QGuiApplication::clipboard())
        clipboard->setText(snippet);
    showGalleryToast(this, QStringLiteral("Copied %1").arg(icon->name));
}

} // namespace fluent::gallery
