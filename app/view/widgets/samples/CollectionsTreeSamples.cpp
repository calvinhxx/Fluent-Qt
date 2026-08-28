#include "CollectionsSampleSupport.h"

#include <QColor>
#include <QLayout>
#include <QModelIndex>
#include <QObject>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QtGlobal>

#include "components/basicinput/Button.h"
#include "components/collections/TreeView.h"
#include "design/Spacing.h"
#include "design/Typography.h"
#include "CollectionSampleDelegates.h"
#include "SampleBuilders.h"

namespace fluent::gallery::detail {
namespace {

using fluent::basicinput::Button;
using fluent::collections::TreeView;
using samples::horizontalGroup;
using samples::makeSample;
using samples::verticalGroup;

// Build a QStandardItem carrying a FluentQt Icons glyph (drawn crisply by the
// TreeRowDelegate via TreeIconGlyphRole) plus an optional accent color, instead of a
// rasterized icon pixmap — matching the role-based model the TreeView UT exercises.
// zh_CN: 构造携带 FluentQt Icons 字形（由 TreeRowDelegate 经 TreeIconGlyphRole 清晰绘制）
// 及可选强调色的 QStandardItem，而非位图图标——与 TreeView UT 使用的 role 化模型一致。
QStandardItem* makeTreeItem(const QString& text, const QString& glyph, const QColor& color)
{
    auto* item = new QStandardItem(text);
    item->setEditable(false);
    item->setData(glyph, TreeIconGlyphRole);
    item->setData(color, TreeIconColorRole);
    return item;
}

/**
 * @brief Nested folder/file model shared by the basic and drag-reorder tree samples.
 * zh_CN: 基础树与拖拽换位树示例共用的嵌套文件夹/文件模型。
 */
QStandardItemModel* makeFolderTreeModel(QObject* owner, const QColor& folderColor,
                                        const QColor& fileColor)
{
    auto* model = new QStandardItemModel(owner);
    auto file = [&](const QString& text) {
        return makeTreeItem(text, Typography::Icons::Document, fileColor);
    };
    auto folder = [&](const QString& text) {
        return makeTreeItem(text, Typography::Icons::Folder, folderColor);
    };

    auto* work = folder(QStringLiteral("Work documents"));
    work->appendRow(file(QStringLiteral("Proposal.docx")));
    work->appendRow(file(QStringLiteral("Budget.xlsx")));
    auto* archive = folder(QStringLiteral("Archive"));
    archive->appendRow(file(QStringLiteral("Q1-review.pdf")));
    archive->appendRow(file(QStringLiteral("Q2-review.pdf")));
    work->appendRow(archive);
    model->appendRow(work);

    auto* photos = folder(QStringLiteral("Photos"));
    photos->appendRow(file(QStringLiteral("Trip.png")));
    photos->appendRow(file(QStringLiteral("Family.png")));
    model->appendRow(photos);

    auto* music = folder(QStringLiteral("Music"));
    music->appendRow(file(QStringLiteral("Playlist.m3u")));
    model->appendRow(music);
    return model;
}

} // namespace

QVector<GallerySample> treeViewSamples()
{
    const QColor folderColor(0xCA, 0x8A, 0x1A);
    const QColor fileColor(0x52, 0x8B, 0xC4);
    const int rowHeight = Spacing::ControlHeight::Standard + Spacing::Gap::Tight;

    return {
        makeSample(QStringLiteral("tree-view-basic"),
                   QStringLiteral("Folder hierarchy"),
                   QStringLiteral("The essential tree: click a chevron to expand or collapse a folder, and selecting a row shows the accent indicator. A custom delegate paints the rotating chevron and per-row icon glyph."),
                   QStringLiteral("auto* tree = new TreeView(this);\n"
                                  "tree->setItemDelegate(new TreeRowDelegate(\n"
                                  "    themeHost, rowHeight, tree, tree));\n"
                                  "tree->setModel(model);\n"
                                  "tree->expandAll();"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       auto* tree = flatPreviewSurface(new TreeView(parent));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(252);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       tree->setSelectedItem(model->index(0, 0));
                       return tree;
                   }),
        makeSample(QStringLiteral("tree-view-checkboxes"),
                   QStringLiteral("Checkable items"),
                   QStringLiteral("The delegate's checkbox mode shows a tri-state box on every row: ticking a parent cascades to its children, and editing a child rolls the parent up to checked / unchecked / partial."),
                   QStringLiteral("auto* tree = new TreeView(this);\n"
                                  "auto* d = new TreeRowDelegate(\n"
                                  "    themeHost, rowHeight, tree, tree);\n"
                                  "d->setCheckBoxVisible(true);\n"
                                  "tree->setItemDelegate(d);\n"
                                  "// clicks cascade down + roll the tri-state up"),
                   [folderColor, rowHeight](QWidget* parent) {
                       auto* tree = flatPreviewSurface(new TreeView(parent));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(258);
                       auto* delegate = new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree);
                       delegate->setCheckBoxVisible(true);
                       tree->setItemDelegate(delegate);

                       auto* model = new QStandardItemModel(tree);
                       auto leaf = [](const QString& text, Qt::CheckState state,
                                      const QString& glyph, const QColor& color) {
                           auto* item = makeTreeItem(text, glyph, color);
                           item->setCheckable(true);
                           item->setCheckState(state);
                           return item;
                       };
                       auto group = [&](const QString& text, Qt::CheckState state) {
                           auto* node = makeTreeItem(text, Typography::Icons::Folder, folderColor);
                           node->setCheckable(true);
                           node->setCheckState(state);
                           return node;
                       };

                       auto* sync = group(QStringLiteral("Sync these settings"), Qt::PartiallyChecked);
                       sync->appendRow(leaf(QStringLiteral("Passwords"), Qt::Checked,
                                            Typography::Icons::Pin, QColor(0x49, 0x82, 0x05)));
                       sync->appendRow(leaf(QStringLiteral("Bookmarks"), Qt::Checked,
                                            Typography::Icons::FavoriteStar, QColor(0xCA, 0x50, 0x10)));
                       sync->appendRow(leaf(QStringLiteral("History"), Qt::Unchecked,
                                            Typography::Icons::Calendar, QColor(0x87, 0x64, 0xB8)));
                       model->appendRow(sync);

                       auto* notify = group(QStringLiteral("Notifications"), Qt::Checked);
                       notify->appendRow(leaf(QStringLiteral("Email"), Qt::Checked,
                                              Typography::Icons::Mail, QColor(0x00, 0x78, 0xD4)));
                       notify->appendRow(leaf(QStringLiteral("Messages"), Qt::Checked,
                                              Typography::Icons::Message, QColor(0x03, 0x83, 0x87)));
                       model->appendRow(notify);

                       auto* privacy = group(QStringLiteral("Privacy"), Qt::Unchecked);
                       privacy->appendRow(leaf(QStringLiteral("Location"), Qt::Unchecked,
                                               Typography::Icons::MapPin, QColor(0xD8, 0x3B, 0x01)));
                       privacy->appendRow(leaf(QStringLiteral("Camera"), Qt::Unchecked,
                                               Typography::Icons::Camera, QColor(0x2D, 0x7D, 0x9A)));
                       privacy->appendRow(leaf(QStringLiteral("Microphone"), Qt::Unchecked,
                                               Typography::Icons::Microphone, QColor(0x5C, 0x2D, 0x91)));
                       model->appendRow(privacy);

                       tree->setModel(model);
                       tree->expandAll();
                       return tree;
                   }),
        makeSample(QStringLiteral("tree-view-reorder"),
                   QStringLiteral("Drag to reorder"),
                   QStringLiteral("With reordering enabled, drag a row to a new spot among its siblings; the model updates and itemReordered fires with the source and destination."),
                   QStringLiteral("tree->setCanReorderItems(true);\n"
                                  "QObject::connect(tree, &TreeView::itemReordered,\n"
                                  "    [](const QModelIndex& srcParent, int srcRow,\n"
                                  "       const QModelIndex& dstParent, int dstRow) { /* ... */ });"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       auto* tree = flatPreviewSurface(new TreeView(parent));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(252);
                       tree->setCanReorderItems(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       return tree;
                   }),
        makeSample(QStringLiteral("tree-view-indicator-motion"),
                   QStringLiteral("Selection indicator motion"),
                   QStringLiteral("Selecting rows at different depths updates the indicator direction and hierarchy transition."),
                   QStringLiteral("tree->setSelectionIndicatorVisible(true);\n"
                                  "tree->setIndicatorMotionAnimationEnabled(true);\n"
                                  "\n"
                                  "const QModelIndex parentIndex = model->index(0, 0);\n"
                                  "const QModelIndex childIndex = model->index(0, 0, parentIndex);\n"
                                  "const QModelIndex siblingIndex = model->index(1, 0, parentIndex);\n"
                                  "tree->setSelectedItem(parentIndex);\n"
                                  "\n"
                                  "auto bindTarget = [tree](Button* button, const QModelIndex& index) {\n"
                                  "    QObject::connect(button, &Button::clicked, tree,\n"
                                  "                     [tree, index] { tree->setSelectedItem(index); });\n"
                                  "};\n"
                                  "bindTarget(parentButton, parentIndex);\n"
                                  "bindTarget(childButton, childIndex);\n"
                                  "bindTarget(siblingButton, siblingIndex);\n"
                                  "\n"
                                  "auto updateStatus = [tree, status] { /* refresh transition label */ };\n"
                                  "QObject::connect(tree, &TreeView::indicatorHierarchyTransitionChanged,\n"
                                  "                 status, updateStatus);"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       QWidget* group = verticalGroup(parent, 10);
                       auto* tree = flatPreviewSurface(new TreeView(group));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(238);
                       tree->setSelectionIndicatorVisible(true);
                       tree->setIndicatorMotionAnimationEnabled(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = makeFolderTreeModel(tree, folderColor, fileColor);
                       tree->setModel(model);
                       tree->expandAll();
                       const QModelIndex parentIndex = model->index(0, 0);
                       const QModelIndex childIndex = model->index(0, 0, parentIndex);
                       // Keep all three animation targets in the initial viewport.  The old
                       // sibling target was the second root item (Photos), so QTreeView quite
                       // correctly auto-scrolled to reveal it and made the indicator demo look
                       // as if the whole control jumped.  Budget is a true sibling of Proposal
                       // and still exercises the same-level transition without moving the view.
                       // zh_CN: 三个动画目标均保持在初始视口内。旧 sibling 指向第二个根
                       // 节点 Photos，QTreeView 会为显示它而自动滚动，看起来像整个控件跳动；
                       // Budget 是 Proposal 的真实同级节点，可在不滚动的情况下演示同级过渡。
                       const QModelIndex siblingIndex = model->index(1, 0, parentIndex);
                       tree->setSelectedItem(parentIndex);

                       QWidget* controls = horizontalGroup(group, 8);
                       auto* parentButton = new Button(QStringLiteral("Parent"), controls);
                       auto* childButton = new Button(QStringLiteral("Child"), controls);
                       auto* siblingButton = new Button(QStringLiteral("Sibling"), controls);
                       auto* status = makeStatusLabel(controls, QStringLiteral("Transition: none"));
                       // Reserve room for the LONGEST transition text up front. This label has no width
                       // floor, so its width tracks the text, which changes length per selection
                       // ("none" → "same level"); the label — and the left-aligned group it shares with the
                       // tree — would then grow/shrink, visibly jumping the TreeView's width and its
                       // translucent backdrop on every selection. zh_CN: 预留「最长过渡文案」的宽度。该标签无宽度下限,
                       // 宽度随文本变化,而文本随选择变化("none"→"same level"),于是标签——以及它与 tree 共处的左对齐 group——
                       // 会伸缩,使每次选择时 TreeView 宽度及其半透明背景跳动。
                       status->setText(QStringLiteral("Transition: same level"));
                       status->setMinimumWidth(qMax(status->minimumWidth(), status->sizeHint().width()));
                       status->setText(QStringLiteral("Transition: none"));
                       controls->layout()->addWidget(parentButton);
                       controls->layout()->addWidget(childButton);
                       controls->layout()->addWidget(siblingButton);
                       controls->layout()->addWidget(status);

                       const auto transitionText = [tree]() {
                           switch (tree->indicatorHierarchyTransition()) {
                           case TreeView::IndicatorHierarchyTransition::Inward:
                               return QStringLiteral("inward");
                           case TreeView::IndicatorHierarchyTransition::Outward:
                               return QStringLiteral("outward");
                           case TreeView::IndicatorHierarchyTransition::SameLevel:
                               return QStringLiteral("same level");
                           case TreeView::IndicatorHierarchyTransition::None:
                               return QStringLiteral("none");
                           }
                           return QStringLiteral("none");
                       };
                       const auto updateStatus = [status, transitionText]() {
                           status->setText(QStringLiteral("Transition: %1").arg(transitionText()));
                       };
                       QObject::connect(parentButton, &Button::clicked, tree, [tree, parentIndex]() {
                           tree->setSelectedItem(parentIndex);
                       });
                       QObject::connect(childButton, &Button::clicked, tree, [tree, childIndex]() {
                           tree->setSelectedItem(childIndex);
                       });
                       QObject::connect(siblingButton, &Button::clicked, tree, [tree, siblingIndex]() {
                           tree->setSelectedItem(siblingIndex);
                       });
                       QObject::connect(tree, &TreeView::indicatorHierarchyTransitionChanged,
                                        status, updateStatus);
                       updateStatus();

                       group->layout()->addWidget(tree);
                       group->layout()->addWidget(controls);
                       return group;
                   }),
        makeSample(QStringLiteral("tree-view-scroll-bounce"),
                   QStringLiteral("Contained tree scrolling"),
                   QStringLiteral("TreeView exposes the same scrollChaining and overscroll controls as the wrapped collection views."),
                   QStringLiteral("auto* tree = new TreeView(this);\n"
                                  "tree->setScrollChainingEnabled(false);\n"
                                  "tree->setOverscrollEnabled(true);\n"
                                  "tree->setModel(model);\n"
                                  "tree->expandAll();"),
                   [folderColor, fileColor, rowHeight](QWidget* parent) {
                       auto* tree = flatPreviewSurface(new TreeView(parent));
                       tree->setHeaderHidden(true);
                       tree->setFixedHeight(258);
                       tree->setScrollChainingEnabled(false);
                       tree->setOverscrollEnabled(true);
                       tree->setItemDelegate(new TreeRowDelegate(
                           static_cast<fluent::FluentElement*>(tree), rowHeight, tree, tree));

                       auto* model = new QStandardItemModel(tree);
                       for (int folderIndex = 0; folderIndex < 9; ++folderIndex) {
                           auto* folder = makeTreeItem(QStringLiteral("Folder %1").arg(folderIndex + 1),
                                                       Typography::Icons::Folder,
                                                       folderColor);
                           for (int fileIndex = 0; fileIndex < 3; ++fileIndex) {
                               folder->appendRow(makeTreeItem(
                                   QStringLiteral("Document %1.%2").arg(folderIndex + 1).arg(fileIndex + 1),
                                   Typography::Icons::Document,
                                   fileColor));
                           }
                           model->appendRow(folder);
                       }
                       tree->setModel(model);
                       tree->expandAll();
                       tree->setSelectedItem(model->index(0, 0));
                       return tree;
                   })
    };
}

} // namespace fluent::gallery::detail
