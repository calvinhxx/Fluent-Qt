#include <QAbstractListModel>
#include <QAccessible>
#include <QApplication>
#include <QImage>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QScrollBar>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTest>
#include <QTextLayout>
#include <QtMath>
#include <QtTest/QSignalSpy>

#include <gtest/gtest.h>

#include "components/collections/FileListView.h"
#include "components/foundation/MotionPolicy.h"

using fluent::collections::FileListView;

namespace {

class InspectableFileListView : public FileListView {
public:
    using FileListView::FileListView;
    using FileListView::visualRect;

    void doItemsLayout() override
    {
        ++layoutCount;
        FileListView::doItemsLayout();
    }

    int layoutCount = 0;
    int paintCount = 0;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        ++paintCount;
        FileListView::paintEvent(event);
    }
};

void showView(FileListView& view, const QSize& size = QSize(640, 320))
{
    view.setAttribute(Qt::WA_DontShowOnScreen);
    view.resize(size);
    view.show();
    QApplication::processEvents();
    view.viewport()->grab();
    QTest::qWait(30);
    view.viewport()->grab();
    QApplication::processEvents();
}

QStandardItem* addFile(QStandardItemModel& model, const QString& name,
                       FileListView::Status status = FileListView::Status::Ready,
                       const QString& metadata = QStringLiteral("2.4 MB · Ready to use"))
{
    auto* item = new QStandardItem(name);
    item->setData(int(status), FileListView::StatusRole);
    item->setData(metadata, FileListView::MetadataRole);
    item->setData(0.62, FileListView::ProgressRole);
    model.appendRow(item);
    return item;
}

QPoint removePoint(const InspectableFileListView& view, const QModelIndex& index)
{
    const QRect rect = view.visualRect(index);
    return QPoint(rect.right() - 31, rect.top() + 32);
}

class CountingFileModel : public QAbstractListModel {
public:
    explicit CountingFileModel(int count) : m_count(count) {}
    int rowCount(const QModelIndex& parent = {}) const override
    {
        return parent.isValid() ? 0 : m_count;
    }
    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_count)
            return {};
        queriedRows.insert(index.row());
        ++dataCalls;
        switch (role) {
        case Qt::DisplayRole:
            return QStringLiteral("file-%1.pdf").arg(index.row());
        case FileListView::MetadataRole:
            return QStringLiteral("%1 / 5.8 MB · Uploading")
                .arg(m_metadataProgress * 5.8, 0, 'f', 1);
        case FileListView::StatusRole:
            return int(FileListView::Status::Uploading);
        case FileListView::ProgressRole:
            return m_progress;
        default:
            return {};
        }
    }
    void changeProgress(qreal progress, bool allRows = false, bool withMetadata = false)
    {
        m_progress = progress;
        QVector<int> roles{FileListView::ProgressRole};
        if (withMetadata) {
            m_metadataProgress = progress;
            roles.append(FileListView::MetadataRole);
        }
        emit dataChanged(index(0, 0), index(allRows ? m_count - 1 : 0, 0), roles);
    }
    mutable int dataCalls = 0;
    mutable QSet<int> queriedRows;

private:
    int m_count;
    qreal m_progress = 0.2;
    qreal m_metadataProgress = 0.2;
};

} // namespace

TEST(FileListViewTest, Contract_ModelRemainsCallerOwnedAndRequestsDoNotMutate)
{
    QStandardItemModel model;
    addFile(model, QStringLiteral("brand-guidelines.pdf"));
    QPointer<QAbstractItemModel> guarded(&model);
    {
        InspectableFileListView view;
        view.setModel(&model);
        EXPECT_EQ(view.model(), &model);
        EXPECT_EQ(model.parent(), nullptr);
        showView(view);
        QSignalSpy remove(&view, &FileListView::removeRequested);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                          removePoint(view, model.index(0, 0)));
        ASSERT_EQ(remove.count(), 1);
        EXPECT_EQ(remove.at(0).at(0).value<QModelIndex>(), model.index(0, 0));
        EXPECT_EQ(model.rowCount(), 1);
        view.setModel(nullptr);
    }
    EXPECT_FALSE(guarded.isNull());
    EXPECT_EQ(model.rowCount(), 1);
}

TEST(FileListViewTest, Contract_KeyboardCommandsRespectRolesAndEnabledState)
{
    QStandardItemModel model;
    auto* file = addFile(model, QStringLiteral("archive.zip"), FileListView::Status::Rejected,
                         QStringLiteral("28 MB · Exceeds the 20 MB limit"));
    file->setData(true, FileListView::RetryableRole);
    InspectableFileListView view;
    view.setModel(&model);
    showView(view);
    view.setCurrentIndex(model.index(0, 0));
    QSignalSpy remove(&view, &FileListView::removeRequested);
    QSignalSpy retry(&view, &FileListView::retryRequested);
    QTest::keyClick(&view, Qt::Key_Delete);
    QTest::keyClick(&view, Qt::Key_R, Qt::ControlModifier);
    EXPECT_EQ(remove.count(), 1);
    EXPECT_EQ(retry.count(), 1);
    file->setData(false, FileListView::RemovableRole);
    file->setData(false, FileListView::RetryableRole);
    QTest::keyClick(&view, Qt::Key_Backspace);
    QTest::keyClick(&view, Qt::Key_R, Qt::ControlModifier);
    EXPECT_EQ(remove.count(), 1);
    EXPECT_EQ(retry.count(), 1);
    file->setData(true, FileListView::RemovableRole);
    file->setData(true, FileListView::RetryableRole);
    file->setEnabled(false);
    QTest::keyClick(&view, Qt::Key_Delete);
    QTest::keyClick(&view, Qt::Key_R, Qt::ControlModifier);
    EXPECT_EQ(remove.count(), 1);
    EXPECT_EQ(retry.count(), 1);
}

TEST(FileListViewTest, Contract_RowBodyRetainsPointerSignalsAndCommandsAreSeparate)
{
    QStandardItemModel model;
    addFile(model, QStringLiteral("document.pdf"));
    InspectableFileListView view;
    view.setModel(&model);
    showView(view);
    QSignalSpy pressed(&view, &QAbstractItemView::pressed);
    QSignalSpy clicked(&view, &QAbstractItemView::clicked);
    QSignalSpy itemClicked(&view, &fluent::collections::ListView::itemClicked);
    QSignalSpy remove(&view, &FileListView::removeRequested);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(120, 32));
    EXPECT_EQ(pressed.count(), 1);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(itemClicked.count(), 1);
    EXPECT_EQ(remove.count(), 0);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                      removePoint(view, model.index(0, 0)));
    EXPECT_EQ(pressed.count(), 1);
    EXPECT_EQ(clicked.count(), 1);
    EXPECT_EQ(itemClicked.count(), 1);
    EXPECT_EQ(remove.count(), 1);
}

TEST(FileListViewTest, Contract_RowBackgroundMatchesListViewAndDisabledRowsClearAcrossThemes)
{
    struct RestoreTheme {
        fluent::FluentElement::Theme previous = fluent::FluentElement::currentTheme();
        ~RestoreTheme() { fluent::FluentElement::setTheme(previous); }
    } restoreTheme;

    auto backgroundPixel = [](QAbstractItemView& view, const QModelIndex& index,
                              QStyle::State state) {
        QImage image(QSize(640, 100), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        QStyleOptionViewItem option;
        option.rect = QRect(0, 0, 640, 100);
        option.state = state;
        option.font = view.font();
        view.itemDelegate()->paint(&painter, option, index);
        painter.end();
        // Empty top padding, away from text, icons, focus, and rounded edges.
        // zh_CN: 采样空白顶内边距，避开文字、图标、焦点和圆角边缘。
        return image.pixelColor(300, 8);
    };

    const fluent::FluentElement::Theme themes[]{fluent::FluentElement::Light,
                                                fluent::FluentElement::Dark,
                                                fluent::FluentElement::HighContrast};
    for (const auto theme : themes) {
        SCOPED_TRACE(int(theme));
        fluent::FluentElement::setTheme(theme);
        QStandardItemModel model;
        auto* item = addFile(model, QStringLiteral("document.pdf"));
        const QModelIndex index = model.index(0, 0);
        fluent::collections::ListView reference;
        reference.setModel(&model);
        FileListView files;
        files.setModel(&model);
        files.setAnimationEnabled(false);

        const QStyle::State selected = QStyle::State_Enabled | QStyle::State_Selected;
        const QStyle::State pressed =
            QStyle::State_Enabled | QStyle::State_MouseOver | QStyle::State_Sunken;
        const QVector<QStyle::State> states{selected, selected | QStyle::State_MouseOver, pressed,
                                            pressed | QStyle::State_Selected};
        // Compare two actual delegates, rather than reproducing the shared
        // helper's token mapping inside the test.
        // zh_CN: 比较两种实际 delegate 的结果，不在测试中复制 helper 的配色映射。
        for (const auto state : states)
            EXPECT_EQ(backgroundPixel(files, index, state),
                      backgroundPixel(reference, index, state));
        EXPECT_NE(backgroundPixel(reference, index, selected),
                  backgroundPixel(reference, index, pressed));

        showView(files);
        QTest::mouseMove(files.viewport(), QPoint(120, 32));
        const QStyle::State disabledHovered = QStyle::State_MouseOver;
        const QStyle::State disabledSelected = disabledHovered | QStyle::State_Selected;
        files.setEnabled(false);
        EXPECT_EQ(backgroundPixel(files, index, disabledHovered).alpha(), 0);
        EXPECT_EQ(backgroundPixel(files, index, disabledSelected).alpha(), 0);
        EXPECT_EQ(backgroundPixel(files, index, disabledSelected),
                  backgroundPixel(reference, index, disabledSelected));

        files.setEnabled(true);
        item->setEnabled(false);
        // Model flags also override stale enabled/selected option bits.
        // zh_CN: 模型的禁用标记同样优先于残留的启用和选中状态位。
        EXPECT_EQ(backgroundPixel(files, index, selected | QStyle::State_MouseOver).alpha(), 0);
    }
}

TEST(FileListViewTest, Contract_PressedRemovalCannotTargetReplacementRow)
{
    QStandardItemModel model;
    addFile(model, QStringLiteral("first.pdf"));
    addFile(model, QStringLiteral("second.pdf"));
    InspectableFileListView view;
    view.setModel(&model);
    showView(view);
    QSignalSpy remove(&view, &FileListView::removeRequested);
    const QPoint point = removePoint(view, model.index(0, 0));
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    model.removeRow(0);
    QApplication::processEvents();
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(remove.count(), 0);
    EXPECT_EQ(model.index(0, 0).data().toString(), QStringLiteral("second.pdf"));
}

TEST(FileListViewTest, Contract_ProxyRequestsUseThePresentedModelIndex)
{
    QStandardItemModel source;
    addFile(source, QStringLiteral("z-last.pdf"));
    addFile(source, QStringLiteral("a-first.pdf"));
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.sort(0);
    InspectableFileListView view;
    view.setModel(&proxy);
    showView(view);
    QSignalSpy remove(&view, &FileListView::removeRequested);
    const QModelIndex index = proxy.index(0, 0);
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, removePoint(view, index));
    ASSERT_EQ(remove.count(), 1);
    EXPECT_EQ(remove.at(0).at(0).value<QModelIndex>(), index);
    EXPECT_EQ(proxy.mapToSource(index).row(), 1);
}

TEST(FileListViewTest, Contract_ModelDestructionAndReplacementClearPendingInteraction)
{
    InspectableFileListView view;
    auto* first = new QStandardItemModel;
    addFile(*first, QStringLiteral("first.pdf"));
    view.setModel(first);
    showView(view);
    QSignalSpy remove(&view, &FileListView::removeRequested);
    const QPoint point = removePoint(view, first->index(0, 0));
    QTest::mousePress(view.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    delete first;
    QStandardItemModel second;
    addFile(second, QStringLiteral("replacement.pdf"));
    view.setModel(&second);
    QApplication::processEvents();
    QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(remove.count(), 0);
    EXPECT_EQ(view.model(), &second);
}

TEST(FileListViewTest, Contract_SelectionCallbackCanReplaceModelDuringActionPress)
{
    QStandardItemModel first;
    addFile(first, QStringLiteral("first.pdf"));
    addFile(first, QStringLiteral("second.pdf"));
    QStandardItemModel replacement;
    addFile(replacement, QStringLiteral("replacement.pdf"));
    InspectableFileListView view;
    view.setModel(&first);
    showView(view);
    view.setCurrentIndex(first.index(0, 0));
    QSignalSpy remove(&view, &FileListView::removeRequested);
    QObject::connect(view.selectionModel(), &QItemSelectionModel::currentChanged, &view,
                     [&] { view.setModel(&replacement); });
    const QPoint point = removePoint(view, first.index(1, 0));
    QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, point);
    EXPECT_EQ(view.model(), &replacement);
    EXPECT_EQ(remove.count(), 0);
}

TEST(FileListViewTest, Contract_SelectionCallbackCanDestroyViewDuringActionPress)
{
    QStandardItemModel model;
    addFile(model, QStringLiteral("first.pdf"));
    addFile(model, QStringLiteral("second.pdf"));
    auto* view = new InspectableFileListView;
    QPointer<FileListView> guarded(view);
    view->setModel(&model);
    showView(*view);
    view->setCurrentIndex(model.index(0, 0));
    QPointer<QItemSelectionModel> ownedSelection(view->selectionModel());
    QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, view,
                     [view] { delete view; });
    const QPoint point = removePoint(*view, model.index(1, 0));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(point),
                      QPointF(view->viewport()->mapToGlobal(point)), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    EXPECT_TRUE(guarded.isNull());
    // Qt must finish emitting its current-index notifications before its
    // view-owned selection model is released, then no orphan may remain.
    // zh_CN: Qt 发完当前项通知后才能释放原视图持有的 selection model，之后不遗留对象。
    EXPECT_FALSE(ownedSelection.isNull());
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_TRUE(ownedSelection.isNull());
}

TEST(FileListViewTest, Contract_ActionSelectionPreservesBorrowedSelectionModelLifetime)
{
    QStandardItemModel model;
    addFile(model, QStringLiteral("first.pdf"));
    addFile(model, QStringLiteral("second.pdf"));
    QObject controller;
    QItemSelectionModel borrowedSelection(&model, &controller);
    QPointer<QItemSelectionModel> selectionGuard(&borrowedSelection);
    auto* view = new InspectableFileListView;
    QPointer<FileListView> viewGuard(view);
    view->setModel(&model);
    view->setSelectionModel(&borrowedSelection);
    showView(*view);
    view->setCurrentIndex(model.index(0, 0));
    QSignalSpy rowChanged(&borrowedSelection, &QItemSelectionModel::currentRowChanged);
    QObject::connect(&borrowedSelection, &QItemSelectionModel::currentChanged, view,
                     [view] { delete view; });
    const QPoint point = removePoint(*view, model.index(1, 0));
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(point),
                      QPointF(view->viewport()->mapToGlobal(point)), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(view->viewport(), &press);
    EXPECT_TRUE(viewGuard.isNull());
    EXPECT_FALSE(selectionGuard.isNull());
    EXPECT_EQ(borrowedSelection.parent(), &controller);
    EXPECT_EQ(borrowedSelection.currentIndex(), model.index(1, 0));
    EXPECT_EQ(rowChanged.count(), 1);
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    EXPECT_FALSE(selectionGuard.isNull());
}

TEST(FileListViewTest, Contract_FullFilenameAndMetadataWrapAtNarrowWidth)
{
    QStandardItemModel model;
    addFile(
        model,
        QStringLiteral("international-brand-guidelines-2026-accessibility-and-localization.pdf"),
        FileListView::Status::Rejected,
        QStringLiteral("此文件超过所选工作区的大小限制。Choose a smaller file and try again."));
    InspectableFileListView view;
    view.setModel(&model);
    showView(view, QSize(720, 420));
    const int wideHeight = view.visualRect(model.index(0, 0)).height();
    view.resize(320, 620);
    QApplication::processEvents();
    view.viewport()->grab();
    QTest::qWait(30);
    view.viewport()->grab();
    QApplication::processEvents();
    const int narrowHeight = view.visualRect(model.index(0, 0)).height();
    EXPECT_GE(wideHeight, 64);
    // Wrapping must add a full text line; the previous trailing status column
    // no longer determines how many lines a narrow row needs.
    // zh_CN: 换行应增加完整文字行，不再依赖旧版尾部状态列挤占宽度后的固定高度。
    EXPECT_GE(narrowHeight,
              wideHeight + qCeil(view.themeFont(Typography::FontRole::Body).lineHeight));
    EXPECT_LE(view.visualRect(model.index(0, 0)).width(), view.viewport()->width());
}

TEST(FileListViewTest, Contract_RtlLayoutPreservesEnglishNumbersAndArabicTextDirection)
{
    struct TextCase {
        QString text;
        Qt::LayoutDirection paragraphDirection;
    };
    const QVector<TextCase> cases = {
        {QStringLiteral("3.6 / 5.8 MB · Uploading"), Qt::LeftToRight},
        {QStringLiteral("2.4 MB · Ready to use"), Qt::LeftToRight},
        {QStringLiteral("جارٍ رفع الملف · 3.6 / 5.8 MB"), Qt::RightToLeft},
    };
    for (Qt::LayoutDirection widgetDirection : {Qt::LeftToRight, Qt::RightToLeft}) {
        for (const auto& test : cases) {
            SCOPED_TRACE(test.text.toStdString());
            SCOPED_TRACE(widgetDirection == Qt::RightToLeft ? "RTL layout" : "LTR layout");
            QStandardItemModel model;
            addFile(model, QStringLiteral("example.pdf"), FileListView::Ready, test.text);
            FileListView view;
            view.setLayoutDirection(widgetDirection);
            view.setModel(&model);
            view.setAnimationEnabled(false);

            // Verify the actual delegate's pixels against the language's known
            // paragraph direction. Alignment follows the widget; glyph order
            // and Arabic shaping must follow the content in both layouts.
            // zh_CN: 用已知段落方向验证实际 delegate 的像素。对齐跟随控件，
            // 字符顺序及阿拉伯文连接形态跟随内容，而不是跟随布局镜像。
            const auto caption = view.themeFont(Typography::FontRole::Caption);
            QTextLayout reference(test.text, caption.toQFont());
            QTextOption textOption;
            textOption.setTextDirection(test.paragraphDirection);
            textOption.setAlignment(widgetDirection == Qt::RightToLeft ? Qt::AlignRight
                                                                       : Qt::AlignLeft);
            reference.setTextOption(textOption);
            reference.beginLayout();
            QTextLine line = reference.createLine();
            ASSERT_TRUE(line.isValid());
            line.setLineWidth(528.0);
            const qreal lineHeight = qMax<qreal>(caption.lineHeight, line.height());
            line.setPosition(QPointF(0.0, (lineHeight - line.height()) / 2.0));
            reference.endLayout();

            QImage expected(QSize(528, qCeil(lineHeight)), QImage::Format_ARGB32_Premultiplied);
            expected.fill(Qt::transparent);
            QPainter referencePainter(&expected);
            referencePainter.setRenderHint(QPainter::Antialiasing);
            referencePainter.setPen(view.themeColorsRef().textSecondary);
            reference.draw(&referencePainter, QPointF());
            referencePainter.end();

            QImage painted(QSize(640, 100), QImage::Format_ARGB32_Premultiplied);
            painted.fill(Qt::transparent);
            QPainter painter(&painted);
            QStyleOptionViewItem option;
            option.rect = QRect(0, 0, 640, 100);
            option.state = QStyle::State_Enabled;
            option.direction = widgetDirection;
            view.itemDelegate()->paint(&painter, option, model.index(0, 0));
            painter.end();

            // Compact rows keep the 32px remove target, with a 20px document
            // icon and a 12px status beside the supporting text. Only geometry
            // and alignment mirror; the paragraph's glyph order stays intact.
            // zh_CN: 紧凑行保留 32px 移除热区，20px 文件图标和说明旁的 12px
            // 状态图标。仅镜像几何与对齐，段落字形顺序仍跟随语言。
            const int textLeft = widgetDirection == Qt::RightToLeft ? 52 : 60;
            const QImage actual = painted.copy(QRect(textLeft, 34, 528, qCeil(lineHeight)));
            EXPECT_EQ(actual, expected);
        }
    }
}

TEST(FileListViewTest, Performance_UnchangedHoverDoesNotRepaint)
{
    QStandardItemModel model;
    auto* first = addFile(model, QStringLiteral("first.pdf"), FileListView::Rejected);
    first->setData(true, FileListView::RetryableRole);
    addFile(model, QStringLiteral("second.pdf"));
    InspectableFileListView view;
    view.setAnimationEnabled(false);
    view.setModel(&model);
    showView(view);

    auto moveTo = [&view](const QPoint& point) {
        QMouseEvent move(QEvent::MouseMove, QPointF(point),
                         QPointF(view.viewport()->mapToGlobal(point)), Qt::NoButton, Qt::NoButton,
                         Qt::NoModifier);
        QApplication::sendEvent(view.viewport(), &move);
        QApplication::sendPostedEvents(nullptr, QEvent::UpdateRequest);
        QApplication::processEvents();
    };

    const QModelIndex firstIndex = model.index(0, 0);
    const QRect firstRect = view.visualRect(firstIndex);
    const QPoint body(firstRect.left() + 120, firstRect.top() + 32);
    moveTo(body);
    const int bodyPaints = view.paintCount;
    for (int dx = 1; dx <= 20; ++dx)
        moveTo(body + QPoint(dx, 0));
    EXPECT_EQ(view.paintCount, bodyPaints);

    // Changed action/row states still repaint; a stable target does not.
    // zh_CN: 操作热区或行变化仍需重绘，同一目标内移动不重复绘制。
    const QPoint remove = removePoint(view, firstIndex);
    moveTo(remove);
    ASSERT_GT(view.paintCount, bodyPaints);
    const int removePaints = view.paintCount;
    moveTo(remove + QPoint(1, 0));
    EXPECT_EQ(view.paintCount, removePaints);

    const QPoint retry = remove - QPoint(40, 0);
    moveTo(retry);
    ASSERT_GT(view.paintCount, removePaints);
    const int retryPaints = view.paintCount;
    moveTo(retry + QPoint(1, 0));
    EXPECT_EQ(view.paintCount, retryPaints);

    moveTo(QPoint(body.x(), view.visualRect(model.index(1, 0)).top() + 32));
    EXPECT_GT(view.paintCount, retryPaints);
}

TEST(FileListViewTest, Performance_HundredThousandRowsReadOnlyVisibleDataAndCreateNoRowWidgets)
{
    CountingFileModel model(100000);
    InspectableFileListView view;
    view.setAnimationEnabled(false);
    const int widgetsBefore = view.findChildren<QWidget*>().size();
    view.setModel(&model);
    showView(view, QSize(640, 320));
    EXPECT_LT(model.queriedRows.size(), 100);
    EXPECT_EQ(view.findChildren<QWidget*>().size(), widgetsBefore);
    EXPECT_EQ(view.indexWidget(model.index(0, 0)), nullptr);

    model.queriedRows.clear();
    const int layoutsBefore = view.layoutCount;
    model.changeProgress(0.85, true, true);
    QApplication::processEvents();
    view.viewport()->grab();
    QTest::qWait(30);
    EXPECT_LT(model.queriedRows.size(), 100);
    EXPECT_EQ(view.layoutCount, layoutsBefore);
    EXPECT_EQ(view.findChildren<QWidget*>().size(), widgetsBefore);
}

TEST(FileListViewTest, Performance_ProgressUpdatesPreserveGeometryAndScrollPosition)
{
    CountingFileModel model(100);
    InspectableFileListView view;
    view.setAnimationEnabled(false);
    view.setModel(&model);
    showView(view);
    const QModelIndex first = model.index(0, 0);
    const QRect rect = view.visualRect(first);
    const int layouts = view.layoutCount;
    const int scroll = view.verticalScrollBar()->value();
    for (int i = 0; i < 20; ++i)
        model.changeProgress(qreal(i) / 20.0, false, true);
    QTest::qWait(30);
    EXPECT_EQ(view.layoutCount, layouts);
    EXPECT_EQ(view.visualRect(first), rect);
    EXPECT_EQ(view.verticalScrollBar()->value(), scroll);
}

TEST(FileListViewTest, Contract_AccessibilityExposesCompleteDetailsAndGuardedActions)
{
#if QT_CONFIG(accessibility)
    QStandardItemModel model;
    auto* item = addFile(model, QStringLiteral("full-untruncated-document-name.pdf"),
                         FileListView::Status::Rejected, QStringLiteral("28 MB · Too large"));
    item->setData(true, FileListView::RetryableRole);
    InspectableFileListView view;
    view.setModel(&model);
    showView(view);
    auto* accessible = QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(accessible, nullptr);
    EXPECT_EQ(accessible->role(), QAccessible::List);
    ASSERT_EQ(accessible->childCount(), 1);
    auto* row = accessible->child(0);
    ASSERT_NE(row, nullptr);
    EXPECT_EQ(row->text(QAccessible::Name), QStringLiteral("full-untruncated-document-name.pdf"));
    EXPECT_TRUE(row->text(QAccessible::Description).contains(QStringLiteral("28 MB · Too large")));
    EXPECT_TRUE(row->text(QAccessible::Description).contains(QStringLiteral("Rejected")));
    ASSERT_NE(row->actionInterface(), nullptr);
    EXPECT_TRUE(row->actionInterface()->actionNames().contains(QStringLiteral("retry")));
    QSignalSpy retry(&view, &FileListView::retryRequested);
    row->actionInterface()->doAction(QStringLiteral("retry"));
    EXPECT_EQ(retry.count(), 1);
    EXPECT_EQ(model.rowCount(), 1);
    item->setEnabled(false);
    EXPECT_TRUE(row->state().disabled);
    row->actionInterface()->doAction(QStringLiteral("retry"));
    EXPECT_EQ(retry.count(), 1);
#else
    GTEST_SKIP() << "Qt accessibility is not configured";
#endif
}

TEST(FileListViewTest, Contract_AccessibilityTracksReorderAndResetWithoutCopyingModel)
{
#if QT_CONFIG(accessibility)
    QStandardItemModel model;
    addFile(model, QStringLiteral("z.pdf"));
    addFile(model, QStringLiteral("a.pdf"));
    FileListView view;
    view.setModel(&model);
    showView(view);
    auto* accessible = QAccessible::queryAccessibleInterface(&view);
    ASSERT_NE(accessible, nullptr);
    auto* original = accessible->child(0);
    ASSERT_NE(original, nullptr);
    model.sort(0);
    EXPECT_EQ(accessible->child(1), original);
    EXPECT_EQ(accessible->indexOfChild(original), 1);
    model.clear();
    addFile(model, QStringLiteral("replacement.pdf"));
    ASSERT_EQ(accessible->childCount(), 1);
    EXPECT_EQ(accessible->child(0)->text(QAccessible::Name), QStringLiteral("replacement.pdf"));
#else
    GTEST_SKIP() << "Qt accessibility is not configured";
#endif
}

TEST(FileListViewTest, Contract_AccessibilityFocusCallbackCanDestroyView)
{
#if QT_CONFIG(accessibility)
    QStandardItemModel model;
    addFile(model, QStringLiteral("first.pdf"));
    addFile(model, QStringLiteral("second.pdf"));
    auto* view = new FileListView;
    QPointer<FileListView> guarded(view);
    view->setModel(&model);
    showView(*view);
    view->setCurrentIndex(model.index(0, 0));
    auto* accessible = QAccessible::queryAccessibleInterface(view);
    ASSERT_NE(accessible, nullptr);
    auto* target = accessible->child(1);
    ASSERT_NE(target, nullptr);
    ASSERT_NE(target->actionInterface(), nullptr);
    QObject::connect(view->selectionModel(), &QItemSelectionModel::currentChanged, view,
                     [view] { delete view; });
    target->actionInterface()->doAction(QAccessibleActionInterface::setFocusAction());
    EXPECT_TRUE(guarded.isNull());
#else
    GTEST_SKIP() << "Qt accessibility is not configured";
#endif
}

TEST(FileListViewTest, Contract_AnimationToggleIsIdempotentAndReducedMotionSafe)
{
    FileListView view;
    QSignalSpy changed(&view, &FileListView::animationEnabledChanged);
    EXPECT_TRUE(view.isAnimationEnabled());
    view.setAnimationEnabled(false);
    view.setAnimationEnabled(false);
    EXPECT_EQ(changed.count(), 1);
    EXPECT_FALSE(view.isAnimationEnabled());
    CountingFileModel model(3);
    view.setModel(&model);
    showView(view);
    const auto previous = fluent::MotionPolicy::instance().mode();
    fluent::MotionPolicy::instance().setMode(fluent::MotionPolicy::Mode::Disabled);
    model.changeProgress(1.0, true);
    view.hide();
    model.changeProgress(0.5, true);
    view.show();
    view.viewport()->grab();
    QApplication::processEvents();
    fluent::MotionPolicy::instance().setMode(previous);
    EXPECT_FALSE(view.isAnimationEnabled());
}
