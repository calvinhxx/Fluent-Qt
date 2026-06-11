#include "MenusToolbarsSamples.h"

#include <QKeySequence>

#include "components/basicinput/DropDownButton.h"
#include "components/menus_toolbars/Menu.h"
#include "components/menus_toolbars/MenuBar.h"
#include "SampleBuilders.h"

namespace fluent::gallery {
namespace {

using fluent::basicinput::DropDownButton;
using fluent::menus_toolbars::FluentMenu;
using fluent::menus_toolbars::FluentMenuBar;
using samples::makeSample;

QVector<GallerySample> menuSamples()
{
    return {
        makeSample(QStringLiteral("menu-basic"),
                   QStringLiteral("Menu with shortcuts and a submenu"),
                   QStringLiteral("FluentMenu renders commands, separators, shortcuts, and nested menus."),
                   QStringLiteral("auto* menu = new FluentMenu(QString(), this);\n"
                                  "menu->addAction(\"New\")->setShortcut(QKeySequence::New);\n"
                                  "menu->addSeparator();\n"
                                  "auto* share = new FluentMenu(\"Share\", menu);\n"
                                  "menu->addMenu(share);"),
                   [](QWidget* parent) {
                       auto* button = new DropDownButton(QStringLiteral("File"), parent);
                       auto* menu = new FluentMenu(QString(), button);
                       menu->addAction(QStringLiteral("New"))->setShortcut(QKeySequence::New);
                       menu->addAction(QStringLiteral("Open..."))->setShortcut(QKeySequence::Open);
                       menu->addAction(QStringLiteral("Save"))->setShortcut(QKeySequence::Save);
                       menu->addSeparator();
                       auto* share = new FluentMenu(QStringLiteral("Share"), menu);
                       share->addAction(QStringLiteral("Email"));
                       share->addAction(QStringLiteral("Copy link"));
                       menu->addMenu(share);
                       menu->addSeparator();
                       menu->addAction(QStringLiteral("Exit"));
                       button->setMenu(menu);
                       return button;
                   })
    };
}

QVector<GallerySample> menuBarSamples()
{
    return {
        makeSample(QStringLiteral("menu-bar-basic"),
                   QStringLiteral("MenuBar with top-level menus"),
                   QStringLiteral("Each top-level item opens its own Fluent menu."),
                   QStringLiteral("auto* menuBar = new FluentMenuBar(this);\n"
                                  "auto* fileMenu = new FluentMenu(\"File\", menuBar);\n"
                                  "fileMenu->addAction(\"New\");\n"
                                  "menuBar->addMenu(fileMenu);"),
                   [](QWidget* parent) {
                       auto* menuBar = new FluentMenuBar(parent);

                       auto* fileMenu = new FluentMenu(QStringLiteral("File"), menuBar);
                       fileMenu->addAction(QStringLiteral("New"));
                       fileMenu->addAction(QStringLiteral("Open..."));
                       fileMenu->addAction(QStringLiteral("Save"));
                       menuBar->addMenu(fileMenu);

                       auto* editMenu = new FluentMenu(QStringLiteral("Edit"), menuBar);
                       editMenu->addAction(QStringLiteral("Undo"));
                       editMenu->addAction(QStringLiteral("Cut"));
                       editMenu->addAction(QStringLiteral("Copy"));
                       editMenu->addAction(QStringLiteral("Paste"));
                       menuBar->addMenu(editMenu);

                       auto* helpMenu = new FluentMenu(QStringLiteral("Help"), menuBar);
                       helpMenu->addAction(QStringLiteral("About"));
                       menuBar->addMenu(helpMenu);

                       menuBar->setMinimumWidth(280);
                       return menuBar;
                   })
    };
}

} // namespace

QVector<GallerySample> menusToolbarsSamples(const QString& routeId)
{
    if (routeId == QStringLiteral("menu"))
        return menuSamples();
    if (routeId == QStringLiteral("menu-bar"))
        return menuBarSamples();
    return {};
}

} // namespace fluent::gallery
