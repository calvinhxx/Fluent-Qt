#include "platform/GalleryPlatform.h"

#include <QCoreApplication>
#include <QClipboard>
#include <QGuiApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QPointer>
#include <QRect>
#include <QSettings>
#include <QStandardPaths>
#include <QWidget>

#ifndef FLUENT_QT_GALLERY_DISPLAY_NAME
#define FLUENT_QT_GALLERY_DISPLAY_NAME "Fluent-Qt Gallery"
#endif

namespace fluent::gallery::platform {

QString graphicsRendererOverride()
{
    return {};
}

void copyText(QObject* context, const QString& text, std::function<void(bool)> completed)
{
    if (!context || !completed)
        return;
    const QPointer<QObject> guard(context);
    QClipboard* clipboard = QGuiApplication::clipboard();
    if (clipboard)
        clipboard->setText(text);
    // Clipboard signals may synchronously destroy the requesting code block.
    // zh_CN: 剪贴板信号可能同步销毁发起复制的代码块。
    if (guard)
        completed(clipboard != nullptr);
}

void chooseFiles(QWidget* context, const QString& filter,
                 std::function<void(const QString&, qint64)> selected)
{
    if (!context || !selected)
        return;
    auto* dialog = new QFileDialog(context, QObject::tr("Choose files"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setNameFilter(filter);
    const QPointer<QWidget> guard(context);
    QObject::connect(dialog, &QFileDialog::filesSelected, context,
                     [guard, selected](const QStringList& paths) {
                         for (const QString& path : paths) {
                             if (!guard)
                                 return;
                             const QFileInfo file(path);
                             selected(file.fileName(), file.size());
                         }
                     });
    dialog->open();
}

const Capabilities& capabilities()
{
    static const Capabilities value = [] {
        Capabilities result;
        result.applicationName = QStringLiteral(FLUENT_QT_GALLERY_DISPLAY_NAME);
        result.windowTitle = QStringLiteral("Fluent-Qt Gallery");
        result.distributionSectionTitle = QStringLiteral("Updates");
        result.distributionTitle = QStringLiteral("Gallery updates");
        result.distributionDescription =
            QStringLiteral("Check GitHub Releases and open the latest package for this platform");
        return result;
    }();
    return value;
}

bool persistenceAvailable()
{
    return QCoreApplication::organizationName() == QStringLiteral("Fluent-Qt") &&
           QCoreApplication::applicationName() == capabilities().applicationName;
}

QSettings createSettings()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
                         QStringLiteral("/config.ini");
    return QSettings(path, QSettings::IniFormat);
}

HostTheme hostTheme()
{
    return HostTheme::System;
}

void setHostThemeChangedHandler(QObject* context, HostThemeChangedHandler handler)
{
    Q_UNUSED(context);
    Q_UNUSED(handler);
}

void showTopLevelWindow(QWidget* window, const QRect& normalGeometry, bool maximized)
{
    if (!window)
        return;
    window->setGeometry(normalGeometry);
    if (maximized)
        window->showMaximized();
    else
        window->show();
}

} // namespace fluent::gallery::platform
