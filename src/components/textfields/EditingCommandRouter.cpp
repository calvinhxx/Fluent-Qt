#include "EditingCommandRouter.h"

#include <array>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPointer>
#include <QScopedPointer>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QVariant>
#include <QWidget>

#include "LineEdit.h"
#include "PasswordBox.h"
#include "TextEdit.h"
#include "utils/private/FluentQtLogging_p.h"

namespace fluent::textfields {
namespace {

using Command = EditingCommandRouter::Command;

constexpr std::array<Command, 7> kCommands = {
    Command::Undo,
    Command::Redo,
    Command::Cut,
    Command::Copy,
    Command::Paste,
    Command::Delete,
    Command::SelectAll,
};

constexpr const char* kScopeRouterProperty =
    "_fluentqt_editingCommandRouter";
constexpr const char* kCommandScopeProperty =
    "_fluentqt_editingCommandScope";
constexpr const char* kCommandPresentationProperty =
    "_fluentqt_commandActionPresentation";

int commandIndex(Command command)
{
    switch (command) {
    case Command::Undo:
        return 0;
    case Command::Redo:
        return 1;
    case Command::Cut:
        return 2;
    case Command::Copy:
        return 3;
    case Command::Paste:
        return 4;
    case Command::Delete:
        return 5;
    case Command::SelectAll:
        return 6;
    }
    return -1;
}

QKeySequence::StandardKey standardKey(Command command)
{
    switch (command) {
    case Command::Undo:
        return QKeySequence::Undo;
    case Command::Redo:
        return QKeySequence::Redo;
    case Command::Cut:
        return QKeySequence::Cut;
    case Command::Copy:
        return QKeySequence::Copy;
    case Command::Paste:
        return QKeySequence::Paste;
    case Command::Delete:
        return QKeySequence::Delete;
    case Command::SelectAll:
        return QKeySequence::SelectAll;
    }
    return QKeySequence::UnknownKey;
}

QString fallbackText(Command command)
{
    const char* source = "";
    switch (command) {
    case Command::Undo:
        source = "&Undo";
        break;
    case Command::Redo:
        source = "&Redo";
        break;
    case Command::Cut:
        source = "Cu&t";
        break;
    case Command::Copy:
        source = "&Copy";
        break;
    case Command::Paste:
        source = "&Paste";
        break;
    case Command::Delete:
        source = "&Delete";
        break;
    case Command::SelectAll:
        source = "Select &All";
        break;
    }
    return QCoreApplication::translate(
        "fluent::textfields::EditingCommandRouter", source);
}

QString actionObjectName(Command command)
{
    switch (command) {
    case Command::Undo:
        return QStringLiteral("FluentEditing.Undo");
    case Command::Redo:
        return QStringLiteral("FluentEditing.Redo");
    case Command::Cut:
        return QStringLiteral("FluentEditing.Cut");
    case Command::Copy:
        return QStringLiteral("FluentEditing.Copy");
    case Command::Paste:
        return QStringLiteral("FluentEditing.Paste");
    case Command::Delete:
        return QStringLiteral("FluentEditing.Delete");
    case Command::SelectAll:
        return QStringLiteral("FluentEditing.SelectAll");
    }
    return QStringLiteral("FluentEditing.Unknown");
}

bool matchesStandardShortcut(const QAction* action,
                             QKeySequence::StandardKey key)
{
    if (!action)
        return false;

    QList<QKeySequence> shortcuts = action->shortcuts();
    if (shortcuts.isEmpty()) {
        const int tabIndex = action->text().indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0) {
            const QKeySequence embedded(
                action->text().mid(tabIndex + 1).trimmed(),
                QKeySequence::NativeText);
            if (!embedded.isEmpty())
                shortcuts.append(embedded);
        }
    }

    const QList<QKeySequence> bindings = QKeySequence::keyBindings(key);
    for (const QKeySequence& shortcut : shortcuts) {
        for (const QKeySequence& binding : bindings) {
            if (shortcut.matches(binding) == QKeySequence::ExactMatch)
                return true;
        }
    }
    return false;
}

bool commandForAction(const QAction* action, Command* command)
{
    if (!action || !command)
        return false;

    for (Command candidate : kCommands) {
        if (matchesStandardShortcut(action, standardKey(candidate))) {
            *command = candidate;
            return true;
        }
    }
    return false;
}

QString displayText(const QAction* action)
{
    if (!action)
        return QString();
    const QString text = action->text();
    const int tabIndex = text.indexOf(QLatin1Char('\t'));
    return tabIndex >= 0 ? text.left(tabIndex) : text;
}

QWidget* resolveScopeWindow(QWidget* scopeWidget)
{
    return scopeWidget ? scopeWidget->window() : nullptr;
}

TextEdit* owningTextEdit(QWidget* widget)
{
    for (QWidget* current = widget; current;
         current = current->parentWidget()) {
        if (auto* owner = qobject_cast<TextEdit*>(current))
            return owner;
    }
    return nullptr;
}

bool fullySelected(const QLineEdit* edit)
{
    return edit && !edit->text().isEmpty()
        && edit->selectionStart() == 0
        && edit->selectedText().size() == edit->text().size();
}

bool fullySelected(const QTextEdit* edit)
{
    if (!edit || edit->document()->isEmpty())
        return false;
    const QTextCursor cursor = edit->textCursor();
    return cursor.hasSelection()
        && cursor.selectionStart() == 0
        && cursor.selectionEnd() >= edit->document()->characterCount() - 1;
}

} // namespace

class EditingCommandRouterPrivate final : public QObject {
public:
    enum class TargetKind {
        None,
        LineEdit,
        TextEdit
    };

    EditingCommandRouterPrivate(
        EditingCommandRouter* owner,
        QWidget* resolvedScope)
        : QObject(owner),
          q(owner),
          scope(resolvedScope)
    {
        createActions();
        scopeRegistered = registerScope();
        if (scopeRegistered)
            activateActions();
        connectScopeLifetime();
        refreshStandardMetadata();
        if (!scopeRegistered)
            return;
        connectApplicationState();
        handleFocusChanged(
            nullptr,
            QApplication::instance()
                ? QApplication::focusWidget()
                : nullptr);
    }

    ~EditingCommandRouterPrivate() override
    {
        disconnectTarget();
        if (scope) {
            QObject* registered =
                scope->property(kScopeRouterProperty).value<QObject*>();
            if (registered == q)
                scope->setProperty(kScopeRouterProperty, QVariant());
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (!event)
            return QObject::eventFilter(watched, event);

        if (watched == scope.data()) {
            if (event->type() == QEvent::LanguageChange)
                refreshStandardMetadata();
            return QObject::eventFilter(watched, event);
        }

        if (watched == target.data()) {
            if (passwordTarget
                && passwordTarget->passwordRevealMode()
                    != PasswordBox::PasswordRevealMode::Visible
                && event->type() == QEvent::KeyPress) {
                auto* keyEvent = static_cast<QKeyEvent*>(event);
                if (keyEvent->matches(QKeySequence::Cut)
                    || keyEvent->matches(QKeySequence::Copy)) {
                    event->accept();
                    return true;
                }
            }
            switch (event->type()) {
            case QEvent::EnabledChange:
            case QEvent::ReadOnlyChange:
                refreshCapabilities();
                break;
            default:
                break;
            }
        }
        return QObject::eventFilter(watched, event);
    }

    QAction* commandAction(Command command) const
    {
        const int index = commandIndex(command);
        return index >= 0 ? commandActions[static_cast<size_t>(index)] : nullptr;
    }

    QList<QAction*> allActions() const
    {
        QList<QAction*> result;
        result.reserve(static_cast<int>(commandActions.size()));
        for (QAction* action : commandActions)
            result.append(action);
        return result;
    }

    bool hasTarget() const
    {
        return scopeRegistered
            && kind != TargetKind::None
            && !target.isNull();
    }

    bool execute(Command command)
    {
        const bool restorePresentationFocus =
            isRouterPresentationActive();
        if (restorePresentationFocus)
            restoreEditingState();
        refreshCapabilities();
        QAction* routedAction = commandAction(command);
        if (!routedAction || !routedAction->isEnabled() || !hasTarget())
            return false;

        const QPointer<QWidget> executionTarget = target;
        if (kind == TargetKind::LineEdit) {
            auto* edit = qobject_cast<QLineEdit*>(target.data());
            if (!edit)
                return false;
            switch (command) {
            case Command::Undo:
                edit->undo();
                break;
            case Command::Redo:
                edit->redo();
                break;
            case Command::Cut:
                edit->cut();
                break;
            case Command::Copy:
                edit->copy();
                break;
            case Command::Paste:
                edit->paste();
                break;
            case Command::Delete:
                edit->del();
                break;
            case Command::SelectAll:
                edit->selectAll();
                break;
            }
        } else if (kind == TargetKind::TextEdit) {
            auto* edit = qobject_cast<QTextEdit*>(target.data());
            if (!edit)
                return false;
            switch (command) {
            case Command::Undo:
                edit->undo();
                break;
            case Command::Redo:
                edit->redo();
                break;
            case Command::Cut:
                edit->cut();
                break;
            case Command::Copy:
                edit->copy();
                break;
            case Command::Paste:
                edit->paste();
                break;
            case Command::Delete: {
                QTextCursor cursor = edit->textCursor();
                cursor.removeSelectedText();
                edit->setTextCursor(cursor);
                break;
            }
            case Command::SelectAll:
                edit->selectAll();
                break;
            }
        } else {
            return false;
        }

        qCDebug(logging::editingCategory)
            << "EditingCommandRouter execute"
            << "command=" << commandIndex(command)
            << "target=" << target->metaObject()->className();
        captureEditingState();
        refreshCapabilities();
        if (restorePresentationFocus) {
            QTimer::singleShot(
                0,
                this,
                [this, executionTarget]() {
                    if (!executionTarget
                        || !scope
                        || executionTarget->window() != scope.data()
                        || !executionTarget->isEnabled()) {
                        return;
                    }
                    QWidget* focus = QApplication::focusWidget();
                    if (!focus || isRouterPresentationActive(focus)) {
                        TargetKind restoredKind = TargetKind::None;
                        QWidget* restoredTarget = nullptr;
                        PasswordBox* restoredPassword = nullptr;
                        if (!resolveTarget(
                                executionTarget.data(),
                                &restoredKind,
                                &restoredTarget,
                                &restoredPassword)) {
                            return;
                        }
                        ++focusRevision;
                        executionTarget->setFocus(
                            Qt::ShortcutFocusReason);
                        setTarget(
                            restoredKind,
                            restoredTarget,
                            restoredPassword);
                    }
                });
        }
        return true;
    }

    void refreshCapabilities()
    {
        std::array<bool, 7> enabled = {};
        if (hasTarget() && target->isEnabled()) {
            if (kind == TargetKind::LineEdit) {
                const auto* edit =
                    qobject_cast<const QLineEdit*>(target.data());
                if (edit) {
                    const bool writable = !edit->isReadOnly();
                    const bool selected = edit->hasSelectedText();
                    const bool exportAllowed =
                        !passwordTarget
                        || passwordTarget->passwordRevealMode()
                            == PasswordBox::PasswordRevealMode::Visible;
                    const QMimeData* mimeData =
                        QApplication::clipboard()
                        ? QApplication::clipboard()->mimeData()
                        : nullptr;

                    enabled[0] = writable && edit->isUndoAvailable();
                    enabled[1] = writable && edit->isRedoAvailable();
                    enabled[2] =
                        writable && selected && exportAllowed;
                    enabled[3] = selected && exportAllowed;
                    enabled[4] =
                        writable && mimeData && mimeData->hasText();
                    enabled[5] = writable && selected;
                    enabled[6] =
                        !edit->text().isEmpty() && !fullySelected(edit);
                }
            } else if (kind == TargetKind::TextEdit) {
                const auto* edit =
                    qobject_cast<const QTextEdit*>(target.data());
                if (edit) {
                    const bool writable = !edit->isReadOnly();
                    const bool selected =
                        edit->textCursor().hasSelection();
                    enabled[0] =
                        writable && edit->document()->isUndoAvailable();
                    enabled[1] =
                        writable && edit->document()->isRedoAvailable();
                    enabled[2] = writable && selected;
                    enabled[3] = selected;
                    enabled[4] = writable && edit->canPaste();
                    enabled[5] = writable && selected;
                    enabled[6] =
                        !edit->document()->isEmpty()
                        && !fullySelected(edit);
                }
            }
        }

        for (size_t index = 0; index < commandActions.size(); ++index) {
            QAction* action = commandActions[index];
            if (action->isEnabled() == enabled[index])
                continue;
            action->setEnabled(enabled[index]);
            emit q->commandCapabilityChanged(
                kCommands[index], enabled[index]);
        }
    }

private:
    friend class EditingCommandRouter;

    void createActions()
    {
        for (size_t index = 0; index < kCommands.size(); ++index) {
            const Command command = kCommands[index];
            auto* action = new QAction(q);
            action->setObjectName(actionObjectName(command));
            action->setEnabled(false);
            action->setProperty(
                kCommandScopeProperty,
                QVariant::fromValue(
                    static_cast<QObject*>(scope.data())));
            QObject::connect(
                action,
                &QAction::triggered,
                q,
                [this, command]() { execute(command); });
            commandActions[index] = action;
        }
    }

    void activateActions()
    {
        if (!scope)
            return;
        for (size_t index = 0; index < kCommands.size(); ++index) {
            QAction* action = commandActions[index];
            action->setShortcutContext(Qt::WindowShortcut);
            action->setShortcuts(
                QKeySequence::keyBindings(
                    standardKey(kCommands[index])));
            scope->addAction(action);
        }
    }

    bool registerScope()
    {
        if (!scope) {
            qCWarning(logging::editingCategory)
                << "EditingCommandRouter requires a scope window";
            return false;
        }

        QObject* existing =
            scope->property(kScopeRouterProperty).value<QObject*>();
        if (existing && existing != q) {
            qCWarning(logging::editingCategory)
                << "Rejecting duplicate EditingCommandRouter for window"
                << scope.data();
            return false;
        }

        scope->setProperty(
            kScopeRouterProperty,
            QVariant::fromValue(static_cast<QObject*>(q)));
        scope->installEventFilter(this);
        return true;
    }

    void connectScopeLifetime()
    {
        if (!scope)
            return;
        QObject::connect(
            scope.data(),
            &QObject::destroyed,
            this,
            [this]() {
                for (QAction* action : commandActions) {
                    if (action)
                        action->setProperty(
                            kCommandScopeProperty, QVariant());
                }
                scopeRegistered = false;
                scope = nullptr;
                setTarget(TargetKind::None, nullptr, nullptr);
            });
    }

    void connectApplicationState()
    {
        auto* application =
            qobject_cast<QApplication*>(QCoreApplication::instance());
        if (application) {
            QObject::connect(
                application,
                &QApplication::focusChanged,
                this,
                [this](QWidget* oldFocus, QWidget* newFocus) {
                    handleFocusChanged(oldFocus, newFocus);
                });
        }
        if (QApplication::clipboard()) {
            QObject::connect(
                QApplication::clipboard(),
                &QClipboard::dataChanged,
                this,
                [this]() { refreshCapabilities(); });
        }
    }

    void refreshStandardMetadata()
    {
        std::array<QString, 7> resolvedTexts;
        for (size_t index = 0; index < kCommands.size(); ++index)
            resolvedTexts[index] = fallbackText(kCommands[index]);

        QLineEdit probe;
        QScopedPointer<QMenu> standardMenu(
            probe.createStandardContextMenu());
        bool pasteSeen = false;
        if (standardMenu) {
            for (QAction* sourceAction : standardMenu->actions()) {
                if (sourceAction->isSeparator()) {
                    pasteSeen = false;
                    continue;
                }

                Command command = Command::Undo;
                bool matched = commandForAction(sourceAction, &command);
                if (!matched && pasteSeen) {
                    command = Command::Delete;
                    matched = true;
                    pasteSeen = false;
                }
                if (!matched)
                    continue;

                const int index = commandIndex(command);
                if (index >= 0) {
                    const QString text = displayText(sourceAction);
                    if (!text.isEmpty())
                        resolvedTexts[static_cast<size_t>(index)] = text;
                }
                pasteSeen = command == Command::Paste;
            }
        }

        for (size_t index = 0; index < commandActions.size(); ++index) {
            QAction* action = commandActions[index];
            const bool usesDefault =
                action->text().isEmpty()
                || action->text() == defaultTexts[index];
            if (usesDefault)
                action->setText(resolvedTexts[index]);
            defaultTexts[index] = resolvedTexts[index];
        }
    }

    void handleFocusChanged(QWidget*, QWidget* newFocus)
    {
        TargetKind resolvedKind = TargetKind::None;
        QWidget* resolvedTarget = nullptr;
        PasswordBox* resolvedPassword = nullptr;
        if (resolveTarget(
                newFocus,
                &resolvedKind,
                &resolvedTarget,
                &resolvedPassword)) {
            ++focusRevision;
            setTarget(
                resolvedKind,
                resolvedTarget,
                resolvedPassword);
            return;
        }

        if (isRouterPresentation(newFocus))
            return;

        const int revision = ++focusRevision;
        if (!newFocus) {
            QTimer::singleShot(0, this, [this, revision]() {
                if (revision != focusRevision)
                    return;
                QWidget* current = QApplication::focusWidget();
                TargetKind delayedKind = TargetKind::None;
                QWidget* delayedTarget = nullptr;
                PasswordBox* delayedPassword = nullptr;
                if (resolveTarget(
                        current,
                        &delayedKind,
                        &delayedTarget,
                        &delayedPassword)) {
                    setTarget(
                        delayedKind,
                        delayedTarget,
                        delayedPassword);
                } else if (!isRouterPresentationActive(current)) {
                    setTarget(TargetKind::None, nullptr, nullptr);
                }
            });
            return;
        }

        setTarget(TargetKind::None, nullptr, nullptr);
    }

    bool resolveTarget(
        QWidget* focus,
        TargetKind* resolvedKind,
        QWidget** resolvedTarget,
        PasswordBox** resolvedPassword) const
    {
        if (!focus || !scope || focus->window() != scope)
            return false;

        if (auto* lineEdit = qobject_cast<LineEdit*>(focus)) {
            *resolvedKind = TargetKind::LineEdit;
            *resolvedTarget = lineEdit;
            *resolvedPassword = qobject_cast<PasswordBox*>(lineEdit);
            return true;
        }

        if (auto* textEdit = qobject_cast<QTextEdit*>(focus)) {
            if (owningTextEdit(textEdit)) {
                *resolvedKind = TargetKind::TextEdit;
                *resolvedTarget = textEdit;
                *resolvedPassword = nullptr;
                return true;
            }
        }
        return false;
    }

    bool isRouterPresentation(QWidget* focus) const
    {
        if (!focus)
            return false;

        const auto containsRouterAction =
            [this](const QWidget* container) {
                if (!container)
                    return false;
                const QList<QAction*> associated =
                    container->actions();
                for (QAction* action : commandActions) {
                    if (associated.contains(action))
                        return true;
                }
                return false;
            };

        for (QWidget* current = focus;
             current && current != scope.data();
             current = current->parentWidget()) {
            if (containsRouterAction(current))
                return true;
        }

        // Native QMenu presentations can put focus on a popup child or on the
        // popup window rather than on the action-owning menu itself.
        // zh_CN: 原生 QMenu 可能把焦点放到弹层子控件或弹层窗口，而非持有 action
        // 的菜单本身，因此保留顶层弹层兜底检查。
        return containsRouterAction(
            qobject_cast<QMenu*>(focus->window()));
    }

    bool isRouterPresentationActive(
        QWidget* focus = nullptr) const
    {
        QWidget* current =
            focus ? focus : QApplication::focusWidget();
        if (isRouterPresentation(current))
            return true;
        if (isRouterPresentation(
                QApplication::activePopupWidget())) {
            return true;
        }
        for (QWidget* topLevel : QApplication::topLevelWidgets()) {
            if (topLevel
                && topLevel->isVisible()
                && topLevel->windowType() == Qt::Popup
                && isRouterPresentation(topLevel)) {
                return true;
            }
        }
        if (scope) {
            const QList<QWidget*> containers =
                scope->findChildren<QWidget*>();
            for (QWidget* container : containers) {
                if (!container || !container->isVisible()
                    || !container->property(
                            kCommandPresentationProperty)
                            .toBool()) {
                    continue;
                }
                for (QAction* action : commandActions) {
                    if (container->actions().contains(action))
                        return true;
                }
            }
        }
        return false;
    }

    void setTarget(
        TargetKind newKind,
        QWidget* newTarget,
        PasswordBox* newPasswordTarget)
    {
        if (kind == newKind
            && target == newTarget
            && passwordTarget == newPasswordTarget) {
            if (!isRouterPresentationActive())
                captureEditingState();
            refreshCapabilities();
            return;
        }

        disconnectTarget();
        kind = newKind;
        target = newTarget;
        passwordTarget = newPasswordTarget;
        connectTarget();
        captureEditingState();

        qCDebug(logging::editingCategory)
            << "EditingCommandRouter targetChanged"
            << "active=" << hasTarget()
            << "target="
            << (target ? target->metaObject()->className() : "none");
        QPointer<EditingCommandRouter> guard(q);
        emit q->activeTargetChanged(hasTarget());
        if (!guard)
            return;
        refreshCapabilities();
    }

    void connectTarget()
    {
        if (!target)
            return;

        target->installEventFilter(this);
        targetConnections.append(QObject::connect(
            target.data(),
            &QObject::destroyed,
            this,
            [this]() {
                kind = TargetKind::None;
                target = nullptr;
                passwordTarget = nullptr;
                disconnectTarget();
                QPointer<EditingCommandRouter> guard(q);
                emit q->activeTargetChanged(false);
                if (!guard)
                    return;
                refreshCapabilities();
            }));

        if (kind == TargetKind::LineEdit) {
            auto* edit = qobject_cast<QLineEdit*>(target.data());
            targetConnections.append(QObject::connect(
                edit,
                &QLineEdit::textChanged,
                this,
                [this]() {
                    captureEditingStateUnlessPresented();
                    refreshCapabilities();
                }));
            targetConnections.append(QObject::connect(
                edit,
                &QLineEdit::selectionChanged,
                this,
                [this]() {
                    captureEditingStateUnlessPresented();
                    refreshCapabilities();
                }));
            targetConnections.append(QObject::connect(
                edit,
                &QLineEdit::cursorPositionChanged,
                this,
                [this](int, int) {
                    captureEditingStateUnlessPresented();
                }));
            if (passwordTarget) {
                targetConnections.append(QObject::connect(
                    passwordTarget.data(),
                    &PasswordBox::passwordRevealModeChanged,
                    this,
                    [this]() { refreshCapabilities(); }));
            }
        } else if (kind == TargetKind::TextEdit) {
            auto* edit = qobject_cast<QTextEdit*>(target.data());
            targetConnections.append(QObject::connect(
                edit,
                &QTextEdit::textChanged,
                this,
                [this]() {
                    captureEditingStateUnlessPresented();
                    refreshCapabilities();
                }));
            targetConnections.append(QObject::connect(
                edit,
                &QTextEdit::selectionChanged,
                this,
                [this]() {
                    captureEditingStateUnlessPresented();
                    refreshCapabilities();
                }));
            targetConnections.append(QObject::connect(
                edit,
                &QTextEdit::cursorPositionChanged,
                this,
                [this]() {
                    captureEditingStateUnlessPresented();
                }));
            targetConnections.append(QObject::connect(
                edit,
                &QTextEdit::undoAvailable,
                this,
                [this](bool) { refreshCapabilities(); }));
            targetConnections.append(QObject::connect(
                edit,
                &QTextEdit::redoAvailable,
                this,
                [this](bool) { refreshCapabilities(); }));
            targetConnections.append(QObject::connect(
                edit,
                &QTextEdit::copyAvailable,
                this,
                [this](bool) { refreshCapabilities(); }));
        }
    }

    void disconnectTarget()
    {
        if (target)
            target->removeEventFilter(this);
        for (const QMetaObject::Connection& connection : targetConnections)
            QObject::disconnect(connection);
        targetConnections.clear();
    }

    void captureEditingStateUnlessPresented()
    {
        if (!isRouterPresentationActive())
            captureEditingState();
    }

    void captureEditingState()
    {
        lineSelectionStart = -1;
        lineSelectionLength = 0;
        lineCursorPosition = 0;
        textCursorSnapshot = QTextCursor();
        if (kind == TargetKind::LineEdit) {
            auto* edit = qobject_cast<QLineEdit*>(target.data());
            if (!edit)
                return;
            lineSelectionStart = edit->selectionStart();
            lineSelectionLength =
                lineSelectionStart >= 0
                ? edit->selectedText().size()
                : 0;
            lineCursorPosition = edit->cursorPosition();
        } else if (kind == TargetKind::TextEdit) {
            auto* edit = qobject_cast<QTextEdit*>(target.data());
            if (edit)
                textCursorSnapshot = edit->textCursor();
        }
    }

    void restoreEditingState()
    {
        if (kind == TargetKind::LineEdit) {
            auto* edit = qobject_cast<QLineEdit*>(target.data());
            if (!edit)
                return;
            if (lineSelectionStart >= 0
                && lineSelectionLength > 0) {
                edit->setSelection(
                    lineSelectionStart, lineSelectionLength);
            } else {
                edit->setCursorPosition(
                    qBound(
                        0,
                        lineCursorPosition,
                        edit->text().size()));
            }
        } else if (kind == TargetKind::TextEdit) {
            auto* edit = qobject_cast<QTextEdit*>(target.data());
            if (edit && !textCursorSnapshot.isNull())
                edit->setTextCursor(textCursorSnapshot);
        }
    }

    EditingCommandRouter* q = nullptr;
    QPointer<QWidget> scope;
    TargetKind kind = TargetKind::None;
    QPointer<QWidget> target;
    QPointer<PasswordBox> passwordTarget;
    std::array<QAction*, 7> commandActions = {};
    std::array<QString, 7> defaultTexts;
    QList<QMetaObject::Connection> targetConnections;
    QTextCursor textCursorSnapshot;
    int lineSelectionStart = -1;
    int lineSelectionLength = 0;
    int lineCursorPosition = 0;
    int focusRevision = 0;
    bool scopeRegistered = false;
};

EditingCommandRouter::EditingCommandRouter(
    QWidget* scopeWindow,
    QObject* parent)
    : QObject(
          parent
              ? parent
              : static_cast<QObject*>(resolveScopeWindow(scopeWindow))),
      d(new EditingCommandRouterPrivate(
          this,
          resolveScopeWindow(scopeWindow)))
{
}

EditingCommandRouter::~EditingCommandRouter()
{
    delete d;
    d = nullptr;
}

QWidget* EditingCommandRouter::scopeWindow() const
{
    return d->scope.data();
}

bool EditingCommandRouter::hasActiveTarget() const
{
    return d->hasTarget();
}

QAction* EditingCommandRouter::action(Command command) const
{
    return d->commandAction(command);
}

QList<QAction*> EditingCommandRouter::actions() const
{
    return d->allActions();
}

bool EditingCommandRouter::canExecute(Command command) const
{
    QAction* commandAction = action(command);
    return commandAction && commandAction->isEnabled();
}

bool EditingCommandRouter::execute(Command command)
{
    return d->execute(command);
}

void EditingCommandRouter::refresh()
{
    d->refreshCapabilities();
}

} // namespace fluent::textfields
