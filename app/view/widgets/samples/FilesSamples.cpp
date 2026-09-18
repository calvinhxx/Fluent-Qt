#include "FilesSamples.h"
#include "CollectionsSampleSupport.h"
#include "SampleBuilders.h"
#include <FluentQt/FluentQt.h>
#include "platform/GalleryPlatform.h"
#include <QFileInfo>
#include <QStandardItemModel>
#include <QVBoxLayout>
namespace fluent::gallery {
using basicinput::Button;
using basicinput::FileDropZone;
using collections::FileListView;
using textfields::Label;
using samples::makeSample;
QVector<GallerySample> fileDropZoneSamples()
{
    return {
        makeSample(
            QStringLiteral("file-drop-zone-workspace"), QStringLiteral("Add to your workspace"),
            QStringLiteral(
                "Choose files from your computer, or drop them below. Example files and limits."),
            QStringLiteral(
                "auto* panel = new QWidget(this);\n"
                "auto* layout = new QVBoxLayout(panel);\n"
                "layout->setContentsMargins(0, 0, 0, 0);\n"
                "layout->setSpacing(24);\n"
                "auto* drop = new FileDropZone(panel);\n"
                "drop->setObjectName(\"fileDropZone\");\n"
                "drop->setDescription(\"Add documents, images or a ZIP archive.\");\n"
                "drop->setHintText(\"PDF, PNG, JPG, ZIP · Up to 20 MB per file\");\n"
                "layout->addWidget(drop);\n"
                "auto* heading = new Label(\"Files\", panel);\n"
                "heading->setFluentTypography(Typography::FontRole::BodyStrong);\n"
                "layout->addWidget(heading);\n"
                "auto* model = new QStandardItemModel(panel);\n"
                "auto addFile = [model](const QString& name, const QString& detail,\n"
                "                       FileListView::Status state, qreal progress) {\n"
                "    auto* item = new QStandardItem(name);\n"
                "    item->setData(detail, FileListView::MetadataRole);\n"
                "    item->setData(state, FileListView::StatusRole);\n"
                "    item->setData(progress, FileListView::ProgressRole);\n"
                "    model->appendRow(item);\n"
                "};\n"
                "addFile(\"brand-guidelines.pdf\", \"2.4 MB · Ready to use\", FileListView::Ready, "
                "1.0);\n"
                "addFile(\"product-shot.png\", \"3.6 / 5.8 MB · Uploading\", "
                "FileListView::Uploading, 0.62);\n"
                "addFile(\"archive.zip\", \"28 MB · Exceeds the 20 MB limit\", "
                "FileListView::Rejected, 0.0);\n"
                "auto* files = new FileListView(panel);\n"
                "files->setObjectName(\"selectedFiles\");\n"
                "files->setAccessibleName(\"Selected files\");\n"
                "files->setBackgroundVisible(false);\n"
                "files->setBorderVisible(false);\n"
                "files->setProperty(\"fluentPreserveParentSurface\", true);\n"
                "files->viewport()->setProperty(\"fluentPreserveParentSurface\", true);\n"
                "files->setMinimumHeight(260);\n"
                "files->setModel(model);\n"
                "layout->addWidget(files);\n"
                "QObject::connect(files, &FileListView::removeRequested, panel,\n"
                "                 [model](const QModelIndex& index) { "
                "model->removeRow(index.row()); });\n"
                "auto acceptFile = [drop, addFile](const QString& name, qint64 bytes) {\n"
                "    const QString suffix = QFileInfo(name).suffix().toLower();\n"
                "    const bool typeAllowed = QStringList{\"pdf\", \"png\", \"jpg\", \"jpeg\", "
                "\"zip\"}.contains(suffix);\n"
                "    const bool sizeAllowed = bytes <= 20 * 1024 * 1024;\n"
                "    if (!typeAllowed || !sizeAllowed) {\n"
                "        const QString reason = !typeAllowed ? \"This file type is not "
                "supported.\"\n"
                "                                             : \"Choose a file under 20 MB.\";\n"
                "        drop->setErrorMessage(name + \" · \" + reason);\n"
                "        addFile(name, reason, FileListView::Rejected, 0.0);\n"
                "        return;\n"
                "    }\n"
                "    drop->clearError();\n"
                "    addFile(name, QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + \" MB · "
                "Ready to use\",\n"
                "            FileListView::Ready, 1.0);\n"
                "};\n"
                "QObject::connect(drop, &FileDropZone::filesDropped, panel,\n"
                "                 [acceptFile](const QList<QUrl>& urls) {\n"
                "    for (const QUrl& url : urls) {\n"
                "        const QFileInfo file(url.toLocalFile());\n"
                "        if (file.isFile())\n"
                "            acceptFile(file.fileName(), file.size());\n"
                "    }\n"
                "});\n"
                "QObject::connect(drop, &FileDropZone::browseRequested, panel, [panel, "
                "acceptFile]() {\n"
                "    platform::chooseFiles(panel, \"Documents (*.pdf *.png *.jpg *.jpeg *.zip)\", "
                "acceptFile);\n"
                "});"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* layout = new QVBoxLayout(panel);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(24);
                auto* drop = new FileDropZone(panel);
                drop->setObjectName("fileDropZone");
                drop->setDescription("Add documents, images or a ZIP archive.");
                drop->setHintText("PDF, PNG, JPG, ZIP · Up to 20 MB per file");
                layout->addWidget(drop);
                auto* heading = new Label("Files", panel);
                heading->setFluentTypography(Typography::FontRole::BodyStrong);
                layout->addWidget(heading);
                auto* model = new QStandardItemModel(panel);
                auto addFile = [model](const QString& name, const QString& detail,
                                       FileListView::Status state, qreal progress) {
                    auto* item = new QStandardItem(name);
                    item->setData(detail, FileListView::MetadataRole);
                    item->setData(state, FileListView::StatusRole);
                    item->setData(progress, FileListView::ProgressRole);
                    model->appendRow(item);
                };
                addFile("brand-guidelines.pdf", "2.4 MB · Ready to use", FileListView::Ready, 1.0);
                addFile("product-shot.png", "3.6 / 5.8 MB · Uploading", FileListView::Uploading,
                        0.62);
                addFile("archive.zip", "28 MB · Exceeds the 20 MB limit", FileListView::Rejected,
                        0.0);
                auto* files = detail::flatPreviewSurface(new FileListView(panel));
                files->setObjectName("selectedFiles");
                files->setAccessibleName("Selected files");
                files->setMinimumHeight(260);
                files->setModel(model);
                layout->addWidget(files);
                QObject::connect(
                    files, &FileListView::removeRequested, panel,
                    [model](const QModelIndex& index) { model->removeRow(index.row()); });
                auto acceptFile = [drop, addFile](const QString& name, qint64 bytes) {
                    const QString suffix = QFileInfo(name).suffix().toLower();
                    const bool typeAllowed =
                        QStringList{"pdf", "png", "jpg", "jpeg", "zip"}.contains(suffix);
                    const bool sizeAllowed = bytes <= 20 * 1024 * 1024;
                    if (!typeAllowed || !sizeAllowed) {
                        const QString reason = !typeAllowed ? "This file type is not supported."
                                                            : "Choose a file under 20 MB.";
                        drop->setErrorMessage(name + " · " + reason);
                        addFile(name, reason, FileListView::Rejected, 0.0);
                        return;
                    }
                    drop->clearError();
                    addFile(name,
                            QString::number(bytes / (1024.0 * 1024.0), 'f', 1) +
                                " MB · Ready to use",
                            FileListView::Ready, 1.0);
                };
                QObject::connect(drop, &FileDropZone::filesDropped, panel,
                                 [acceptFile](const QList<QUrl>& urls) {
                                     for (const QUrl& url : urls) {
                                         const QFileInfo file(url.toLocalFile());
                                         if (file.isFile())
                                             acceptFile(file.fileName(), file.size());
                                     }
                                 });
                QObject::connect(
                    drop, &FileDropZone::browseRequested, panel, [panel, acceptFile]() {
                        platform::chooseFiles(panel, "Documents (*.pdf *.png *.jpg *.jpeg *.zip)",
                                              acceptFile);
                    });
                return panel;
            },
            true),
        makeSample(
            QStringLiteral("file-drop-zone-states"), QStringLiteral("Clear next steps"),
            QStringLiteral("Errors explain how to continue. File selection can be disabled without "
                           "changing its layout."),
            QStringLiteral(
                "auto* panel = new QWidget(this);\n"
                "auto* layout = new QVBoxLayout(panel);\n"
                "layout->setContentsMargins(0, 0, 0, 0);\n"
                "layout->setSpacing(24);\n"
                "auto* rejected = new FileDropZone(panel);\n"
                "rejected->setObjectName(\"rejectedDropZone\");\n"
                "rejected->setTitle(\"This file is too large\");\n"
                "rejected->setBrowseText(\"Reset example\");\n"
                "rejected->setErrorMessage(\"archive.zip is 28 MB. Choose a file under 20 MB.\");\n"
                "QObject::connect(rejected, &FileDropZone::browseRequested, rejected, [rejected]() "
                "{\n"
                "    rejected->clearError();\n"
                "    rejected->setTitle(\"Drop files here\");\n"
                "});\n"
                "layout->addWidget(rejected);\n"
                "auto* disabled = new FileDropZone(panel);\n"
                "disabled->setObjectName(\"disabledDropZone\");\n"
                "disabled->setTitle(\"File selection is unavailable\");\n"
                "disabled->setDescription(\"You need permission to add files to this "
                "workspace.\");\n"
                "disabled->setEnabled(false);\n"
                "layout->addWidget(disabled);"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                auto* layout = new QVBoxLayout(panel);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(24);
                auto* rejected = new FileDropZone(panel);
                rejected->setObjectName("rejectedDropZone");
                rejected->setTitle("This file is too large");
                rejected->setBrowseText("Reset example");
                rejected->setErrorMessage("archive.zip is 28 MB. Choose a file under 20 MB.");
                QObject::connect(rejected, &FileDropZone::browseRequested, rejected, [rejected]() {
                    rejected->clearError();
                    rejected->setTitle("Drop files here");
                });
                layout->addWidget(rejected);
                auto* disabled = new FileDropZone(panel);
                disabled->setObjectName("disabledDropZone");
                disabled->setTitle("File selection is unavailable");
                disabled->setDescription("You need permission to add files to this workspace.");
                disabled->setEnabled(false);
                layout->addWidget(disabled);
                return panel;
            },
            true),
    };
}
QVector<GallerySample> fileListViewSamples()
{
    return {
        makeSample(
            QStringLiteral("file-list-view-basic"), QStringLiteral("Files and progress"),
            QStringLiteral("Example transfer states. Remove a file, retry a failed transfer, or "
                           "advance the progress."),
            QStringLiteral(
                "auto* panel = new QWidget(this);\n"
                "panel->setMaximumWidth(720);\n"
                "auto* layout = new QVBoxLayout(panel);\n"
                "layout->setContentsMargins(0, 0, 0, 0);\n"
                "layout->setSpacing(16);\n"
                "auto* model = new QStandardItemModel(panel);\n"
                "auto addFile = [model](const QString& name, const QString& detail,\n"
                "                       FileListView::Status state, qreal progress) {\n"
                "    auto* item = new QStandardItem(name);\n"
                "    item->setData(detail, FileListView::MetadataRole);\n"
                "    item->setData(state, FileListView::StatusRole);\n"
                "    item->setData(progress, FileListView::ProgressRole);\n"
                "    item->setData(state == FileListView::Rejected, FileListView::RetryableRole);\n"
                "    model->appendRow(item);\n"
                "};\n"
                "addFile(\"brand-guidelines.pdf\", \"2.4 MB · Ready to use\", FileListView::Ready, "
                "1.0);\n"
                "addFile(\"product-shot.png\", \"3.6 / 5.8 MB · Uploading\", "
                "FileListView::Uploading, 0.62);\n"
                "addFile(\"research-notes-for-the-autumn-product-release-and-accessibility-review."
                "pdf\",\n"
                "        \"Connection lost · Retry when you are ready\", FileListView::Rejected, "
                "0.0);\n"
                "auto* files = new FileListView(panel);\n"
                "files->setObjectName(\"fileList\");\n"
                "files->setAccessibleName(\"Transfer examples\");\n"
                "files->setMinimumHeight(240);\n"
                "files->setBackgroundVisible(false);\n"
                "files->setBorderVisible(false);\n"
                "files->setProperty(\"fluentPreserveParentSurface\", true);\n"
                "files->viewport()->setProperty(\"fluentPreserveParentSurface\", true);\n"
                "files->setModel(model);\n"
                "layout->addWidget(files);\n"
                "QObject::connect(files, &FileListView::removeRequested, panel,\n"
                "                 [model](const QModelIndex& index) { "
                "model->removeRow(index.row()); });\n"
                "QObject::connect(files, &FileListView::retryRequested, panel,\n"
                "                 [model](const QModelIndex& index) {\n"
                "    model->setData(index, FileListView::Uploading, FileListView::StatusRole);\n"
                "    model->setData(index, false, FileListView::RetryableRole);\n"
                "    model->setData(index, \"Reconnected · Uploading\", "
                "FileListView::MetadataRole);\n"
                "    model->setData(index, 0.0, FileListView::ProgressRole);\n"
                "});\n"
                "auto* advance = new Button(\"Advance example transfers\", panel);\n"
                "advance->setObjectName(\"advanceTransfer\");\n"
                "advance->setFluentStyle(Button::Subtle);\n"
                "QObject::connect(advance, &Button::clicked, panel, [model]() {\n"
                "    for (int row = 0; row < model->rowCount(); ++row) {\n"
                "        const QModelIndex index = model->index(row, 0);\n"
                "        if (index.data(FileListView::StatusRole).toInt() != "
                "FileListView::Uploading)\n"
                "            continue;\n"
                "        const qreal progress = qMin(1.0, "
                "index.data(FileListView::ProgressRole).toReal() + 0.19);\n"
                "        model->setData(index, progress, FileListView::ProgressRole);\n"
                "        model->setData(index, progress >= 1.0 ? \"Ready to use\" : "
                "\"Uploading\",\n"
                "                       FileListView::MetadataRole);\n"
                "        if (progress >= 1.0)\n"
                "            model->setData(index, FileListView::Ready, "
                "FileListView::StatusRole);\n"
                "    }\n"
                "});\n"
                "layout->addWidget(advance, 0, Qt::AlignLeft);"),
            [](QWidget* parent) {
                auto* panel = new QWidget(parent);
                panel->setMaximumWidth(720);
                auto* layout = new QVBoxLayout(panel);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(16);
                auto* model = new QStandardItemModel(panel);
                auto addFile = [model](const QString& name, const QString& detail,
                                       FileListView::Status state, qreal progress) {
                    auto* item = new QStandardItem(name);
                    item->setData(detail, FileListView::MetadataRole);
                    item->setData(state, FileListView::StatusRole);
                    item->setData(progress, FileListView::ProgressRole);
                    item->setData(state == FileListView::Rejected, FileListView::RetryableRole);
                    model->appendRow(item);
                };
                addFile("brand-guidelines.pdf", "2.4 MB · Ready to use", FileListView::Ready, 1.0);
                addFile("product-shot.png", "3.6 / 5.8 MB · Uploading", FileListView::Uploading,
                        0.62);
                addFile(
                    "research-notes-for-the-autumn-product-release-and-accessibility-review.pdf",
                    "Connection lost · Retry when you are ready", FileListView::Rejected, 0.0);
                auto* files = detail::flatPreviewSurface(new FileListView(panel));
                files->setObjectName("fileList");
                files->setAccessibleName("Transfer examples");
                files->setMinimumHeight(240);
                files->setModel(model);
                layout->addWidget(files);
                QObject::connect(
                    files, &FileListView::removeRequested, panel,
                    [model](const QModelIndex& index) { model->removeRow(index.row()); });
                QObject::connect(
                    files, &FileListView::retryRequested, panel, [model](const QModelIndex& index) {
                        model->setData(index, FileListView::Uploading, FileListView::StatusRole);
                        model->setData(index, false, FileListView::RetryableRole);
                        model->setData(index, "Reconnected · Uploading",
                                       FileListView::MetadataRole);
                        model->setData(index, 0.0, FileListView::ProgressRole);
                    });
                auto* advance = new Button("Advance example transfers", panel);
                advance->setObjectName("advanceTransfer");
                advance->setFluentStyle(Button::Subtle);
                QObject::connect(advance, &Button::clicked, panel, [model]() {
                    for (int row = 0; row < model->rowCount(); ++row) {
                        const QModelIndex index = model->index(row, 0);
                        if (index.data(FileListView::StatusRole).toInt() != FileListView::Uploading)
                            continue;
                        const qreal progress =
                            qMin(1.0, index.data(FileListView::ProgressRole).toReal() + 0.19);
                        model->setData(index, progress, FileListView::ProgressRole);
                        model->setData(index, progress >= 1.0 ? "Ready to use" : "Uploading",
                                       FileListView::MetadataRole);
                        if (progress >= 1.0)
                            model->setData(index, FileListView::Ready, FileListView::StatusRole);
                    }
                });
                layout->addWidget(advance, 0, Qt::AlignLeft);
                return panel;
            },
            true),
    };
}
} // namespace fluent::gallery
