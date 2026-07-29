#include "TextEditingMenu_p.h"

#include <QAction>
#include <QIcon>
#include <QKeySequence>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QScopedPointer>
#include <QtMath>

#include "components/menus_toolbars/Menu.h"
#include "design/Spacing.h"
#include "design/Typography.h"

namespace fluent::menus_toolbars::detail {
namespace {

bool matchesStandardShortcut(const QAction* action,
                             QKeySequence::StandardKey standardKey)
{
    if (!action)
        return false;

    QKeySequence shortcut = action->shortcut();
    if (shortcut.isEmpty()) {
        const QString text = action->text();
        const int tabIndex = text.indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            shortcut = QKeySequence(
                text.mid(tabIndex + 1).trimmed(),
                QKeySequence::NativeText);
        }
    }

    if (shortcut.isEmpty())
        return false;

    const QList<QKeySequence> bindings =
        QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& binding : bindings) {
        if (shortcut.matches(binding) == QKeySequence::ExactMatch)
            return true;
    }
    return false;
}

QKeySequence::StandardKey standardEditingActionKey(
    const QAction* action)
{
    if (matchesStandardShortcut(action, QKeySequence::Undo))
        return QKeySequence::Undo;
    if (matchesStandardShortcut(action, QKeySequence::Redo))
        return QKeySequence::Redo;
    if (matchesStandardShortcut(action, QKeySequence::Cut))
        return QKeySequence::Cut;
    if (matchesStandardShortcut(action, QKeySequence::Copy))
        return QKeySequence::Copy;
    if (matchesStandardShortcut(action, QKeySequence::Paste))
        return QKeySequence::Paste;
    if (matchesStandardShortcut(action, QKeySequence::Delete))
        return QKeySequence::Delete;
    if (matchesStandardShortcut(action, QKeySequence::SelectAll))
        return QKeySequence::SelectAll;
    return QKeySequence::UnknownKey;
}

QKeySequence::StandardKey positionalEditingActionKey(
    int section,
    int indexInSection)
{
    if (section == 0) {
        if (indexInSection == 0)
            return QKeySequence::Undo;
        if (indexInSection == 1)
            return QKeySequence::Redo;
    } else if (section == 1) {
        switch (indexInSection) {
        case 0:
            return QKeySequence::Cut;
        case 1:
            return QKeySequence::Copy;
        case 2:
            return QKeySequence::Paste;
        case 3:
            return QKeySequence::Delete;
        default:
            break;
        }
    } else if (section == 2 && indexInSection == 0) {
        return QKeySequence::SelectAll;
    }
    return QKeySequence::UnknownKey;
}

QString standardEditingShortcutText(
    QKeySequence::StandardKey standardKey)
{
    if (standardKey == QKeySequence::UnknownKey)
        return QString();

    const QList<QKeySequence> bindings =
        QKeySequence::keyBindings(standardKey);
    const QKeySequence shortcut =
        bindings.isEmpty()
        ? QKeySequence(standardKey)
        : bindings.constFirst();
    return shortcut.toString(QKeySequence::NativeText);
}

QString standardEditingActionGlyph(const QAction* action)
{
    if (matchesStandardShortcut(action, QKeySequence::Undo))
        return Typography::Icons::Undo;
    if (matchesStandardShortcut(action, QKeySequence::Redo))
        return Typography::Icons::Redo;
    if (matchesStandardShortcut(action, QKeySequence::Cut))
        return Typography::Icons::Cut;
    if (matchesStandardShortcut(action, QKeySequence::Copy))
        return Typography::Icons::Copy;
    if (matchesStandardShortcut(action, QKeySequence::Paste))
        return Typography::Icons::Paste;
    if (matchesStandardShortcut(action, QKeySequence::Delete))
        return Typography::Icons::Delete;
    if (matchesStandardShortcut(action, QKeySequence::SelectAll))
        return Typography::Icons::SelectAll;

    const QString iconName =
        action ? action->icon().name() : QString();
    const struct {
        const char* keyword;
        const QString* glyph;
    } iconMappings[] = {
        {"undo", &Typography::Icons::Undo},
        {"redo", &Typography::Icons::Redo},
        {"cut", &Typography::Icons::Cut},
        {"copy", &Typography::Icons::Copy},
        {"paste", &Typography::Icons::Paste},
        {"delete", &Typography::Icons::Delete},
        {"select-all", &Typography::Icons::SelectAll},
    };
    for (const auto& mapping : iconMappings) {
        if (iconName.contains(
                QString::fromLatin1(mapping.keyword),
                Qt::CaseInsensitive)) {
            return *mapping.glyph;
        }
    }
    return QString();
}

QString positionalEditingActionGlyph(
    int section,
    int indexInSection)
{
    // QLineEdit and QTextEdit expose the same stable standard-menu groups:
    // Undo/Redo, Cut/Copy/Paste/Delete, then Select All. Some Linux platform
    // styles omit QAction shortcuts and themed icons for part of that menu.
    // Position is therefore the final locale-independent fallback.
    if (section == 0) {
        if (indexInSection == 0)
            return Typography::Icons::Undo;
        if (indexInSection == 1)
            return Typography::Icons::Redo;
    } else if (section == 1) {
        switch (indexInSection) {
        case 0:
            return Typography::Icons::Cut;
        case 1:
            return Typography::Icons::Copy;
        case 2:
            return Typography::Icons::Paste;
        case 3:
            return Typography::Icons::Delete;
        default:
            break;
        }
    } else if (section == 2 && indexInSection == 0) {
        return Typography::Icons::SelectAll;
    }
    return QString();
}

bool hasStandardEditingActionShape(
    const QList<QAction*>& actions)
{
    int sectionSizes[3] = {0, 0, 0};
    int section = 0;
    for (QAction* action : actions) {
        if (action->isSeparator()) {
            ++section;
            continue;
        }
        if (section >= 0 && section < 3)
            ++sectionSizes[section];
    }
    return section >= 2
        && sectionSizes[0] == 2
        && sectionSizes[1] >= 4
        && sectionSizes[2] >= 1;
}

class TextEditingContextMenu final : public FluentMenu {
public:
    TextEditingContextMenu(QWidget* parent, const QString& objectName)
        : FluentMenu(QString(), parent)
    {
        setObjectName(objectName);
        setFontStyle(Typography::FontRole::Caption);
    }

    void onThemeUpdated() override
    {
        FluentMenu::onThemeUpdated();

        // Keep editing menus compact without shrinking their labels below the
        // Caption token. WinUI menu glyphs use a 16 px slot; pairing that slot
        // with 12 px Caption text avoids the oversized-icon ratio caused by
        // the former 18 px / 10 px combination.
        setFont(themeFont(Typography::FontRole::Caption).toQFont());
        const auto spacing = themeSpacing();
        const int shadow = ::Spacing::Standard;
        const int verticalInset =
            qMax(1, spacing.gap.tight / 2);
        setContentsMargins(
            shadow,
            shadow + verticalInset,
            shadow,
            shadow + verticalInset);
        setStyleSheet(QStringLiteral(
            "QMenu { background-color: transparent; border: 0px; padding: 0px; }"
            "QMenu::item { background-color: transparent; padding: %1px 0px; margin: 0px; }"
            "QMenu::separator { height: %2px; }")
            .arg(qMax(1, spacing.padding.listItemV / 2))
            .arg(spacing.gap.normal));
        setMinimumWidth(sizeHint().width());
        updateGeometry();
        update();
    }

    QIcon editingIcon(const QString& glyph) const
    {
        if (glyph.isEmpty())
            return QIcon();

        constexpr int iconSize = Typography::IconSize::Standard;
        const Colors& colors = themeColorsRef();
        const QColor activeColor =
            themeDesignLanguage() == DesignCupertino
            ? colors.textOnAccent
            : colors.textPrimary;

        auto pixmapForColor =
            [this, &glyph, iconSize](const QColor& color) {
                const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
                const int physicalSize =
                    qMax(1, qCeil(iconSize * dpr));
                QPixmap pixmap(physicalSize, physicalSize);
                pixmap.setDevicePixelRatio(dpr);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setPen(color);
                Typography::Icons::paintGlyph(
                    painter,
                    QRectF(0, 0, iconSize, iconSize),
                    glyph,
                    iconSize,
                    Qt::AlignCenter);
                return pixmap;
            };

        QIcon icon;
        icon.addPixmap(
            pixmapForColor(colors.textPrimary), QIcon::Normal);
        icon.addPixmap(
            pixmapForColor(activeColor), QIcon::Active);
        icon.addPixmap(
            pixmapForColor(colors.textDisabled), QIcon::Disabled);
        return icon;
    }
};

} // namespace

bool execTextEditingContextMenu(QWidget* parent,
                                QMenu* standardMenu,
                                const QPoint& globalPosition,
                                const QString& objectName)
{
    if (!standardMenu)
        return false;

    QScopedPointer<QMenu> standardMenuGuard(standardMenu);
    TextEditingContextMenu menu(parent, objectName);
    const QList<QAction*> standardActions = standardMenu->actions();
    const bool usePositionalFallback =
        hasStandardEditingActionShape(standardActions);
    int section = 0;
    int indexInSection = 0;

    for (QAction* sourceAction : standardActions) {
        if (sourceAction->isSeparator()) {
            menu.addSeparator();
            ++section;
            indexInSection = 0;
            continue;
        }

        // Some Qt versions dispatch Undo/Redo through the standard menu's
        // action chain. Proxy the action instead of changing its owner.
        auto* action = new QAction(
            sourceAction->icon(), sourceAction->text(), &menu);
        action->setEnabled(sourceAction->isEnabled());
        action->setCheckable(sourceAction->isCheckable());
        action->setChecked(sourceAction->isChecked());
        action->setShortcuts(sourceAction->shortcuts());
        action->setShortcutContext(sourceAction->shortcutContext());
        action->setData(sourceAction->data());
        action->setStatusTip(sourceAction->statusTip());
        action->setToolTip(sourceAction->toolTip());

        QKeySequence::StandardKey editingKey =
            standardEditingActionKey(sourceAction);
        if (editingKey == QKeySequence::UnknownKey
            && usePositionalFallback) {
            editingKey = positionalEditingActionKey(
                section, indexInSection);
        }
        if (menu.shortcutTextForAction(action).isEmpty()) {
            const QString shortcutText =
                standardEditingShortcutText(editingKey);
            if (!shortcutText.isEmpty()) {
                action->setProperty(
                    "shortcutText", shortcutText);
            }
        }

        QObject::connect(
            action,
            &QAction::triggered,
            sourceAction,
            [sourceAction]() { sourceAction->trigger(); });

        QString iconGlyph = standardEditingActionGlyph(sourceAction);
        if (iconGlyph.isEmpty() && usePositionalFallback)
            iconGlyph = positionalEditingActionGlyph(
                section, indexInSection);
        if (!iconGlyph.isEmpty())
            action->setIcon(menu.editingIcon(iconGlyph));
        menu.addAction(action);
        ++indexInSection;
    }

    menu.exec(globalPosition);
    return true;
}

} // namespace fluent::menus_toolbars::detail
