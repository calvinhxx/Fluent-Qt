#ifndef GALLERYCONTENTPAGE_H
#define GALLERYCONTENTPAGE_H

#include <QPointer>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

class QVBoxLayout;

namespace fluent::scrolling {
class ScrollView;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::gallery {

/**
 * @brief Scrollable right-side content page shell with title, subtitle, and sections.
 * zh_CN: 可滚动的右侧内容页外壳，含标题、副标题与分区。
 *
 * GalleryContentPage owns the common page chrome (scroll area, heading, section
 * stack, theme surface) so concrete pages only contribute section content.
 * zh_CN: GalleryContentPage 负责通用页面外观（滚动区、标题、分区堆叠、主题表面），
 * 具体页面只需贡献分区内容。
 */
class GalleryContentPage : public QWidget, public fluent::FluentElement, public fluent::QMLPlus {
    Q_OBJECT

public:
    explicit GalleryContentPage(const QString& routeId,
                                const QString& title,
                                const QString& subtitle = QString(),
                                QWidget* parent = nullptr);

    QString routeId() const { return m_routeId; }
    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    fluent::textfields::Label* titleLabel() const { return m_titleLabel; }

    void onThemeUpdated() override;

signals:
    /**
     * @brief Emitted when an in-page card or link requests navigation to a route.
     * zh_CN: 当页面内卡片或链接请求跳转到某路由时发出。
     */
    void routeActivated(const QString& routeId);

protected:
    /** @brief Adds a section heading label to the content column. */
    fluent::textfields::Label* addSectionHeader(const QString& text);
    /** @brief Adds a wrapped body paragraph to the content column. */
    fluent::textfields::Label* addBodyText(const QString& text);
    /** @brief Adds an arbitrary widget to the content column. */
    void addContentWidget(QWidget* widget);
    /** @brief Adds fixed vertical spacing to the content column. */
    void addContentSpacing(int pixels);
    /**
     * @brief Shows or hides the default title/subtitle header block.
     * zh_CN: 显示或隐藏默认的标题/副标题头部。
     *
     * Pages with a custom hero (e.g. Home) hide the stock header and render
     * their own heading inside the content column.
     * zh_CN: 自带 hero 的页面（如 Home）隐藏默认头部，在内容列内自绘标题。
     */
    void setPageHeaderVisible(bool visible);
    /** @brief Overrides the scroll viewport content margins (default 48/34/48/48). */
    void setViewportMargins(const QMargins& margins);
    /** @brief Overrides the vertical spacing between content sections. */
    void setContentSpacing(int spacing);

    enum class TextRole { Primary, Secondary };

    /**
     * @brief Creates a label whose color follows theme refreshes; caller places it.
     * zh_CN: 创建颜色随主题刷新的标签；由调用方负责布局。
     */
    fluent::textfields::Label* createTrackedLabel(const QString& text,
                                                  const QString& fontRole,
                                                  TextRole textRole);

private:
    void applyPalette();

    fluent::scrolling::ScrollView* m_scrollArea = nullptr;
    QWidget* m_viewport = nullptr;
    QVBoxLayout* m_contentLayout = nullptr;
    fluent::textfields::Label* m_titleLabel = nullptr;
    fluent::textfields::Label* m_subtitleLabel = nullptr;

    struct TrackedLabel {
        QPointer<fluent::textfields::Label> label;
        TextRole role;
    };
    QVector<TrackedLabel> m_trackedLabels;

    QString m_routeId;
    QString m_title;
    QString m_subtitle;
};

} // namespace fluent::gallery

#endif // GALLERYCONTENTPAGE_H
