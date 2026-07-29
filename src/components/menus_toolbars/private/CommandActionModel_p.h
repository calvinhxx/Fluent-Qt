#ifndef FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDACTIONMODEL_P_H
#define FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDACTIONMODEL_P_H

#include <QHash>
#include <QList>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

class QAction;
class QActionEvent;
class QWidget;

namespace fluent::menus_toolbars::detail {

// Shared semantic action registry for CommandBar surfaces. Actions are
// borrowed: this model never reparents or deletes them.
class CommandActionModel final : public QObject {
    Q_OBJECT

public:
    enum class Section {
        Primary,
        Secondary,
    };

    explicit CommandActionModel(QWidget* owner);
    ~CommandActionModel() override;

    bool add(Section section, QAction* action);
    bool insert(Section section, QAction* before, QAction* action);
    bool remove(QAction* action);
    void clear(Section section);

    QList<QAction*> actions(Section section) const;
    QList<QAction*> presentableActions(Section section) const;
    bool contains(Section section, const QAction* action) const;
    bool isPresentable(const QAction* action) const;

    void handleActionEvent(QActionEvent* event);
    bool isChangingAssociation() const
    {
        return m_associationChangeDepth > 0;
    }

signals:
    void structureChanged();
    void presentationChanged();

private:
    struct Connections {
        QMetaObject::Connection changed;
        QMetaObject::Connection destroyed;
    };
    struct PendingRemoval {
        QPointer<QAction> actionGuard;
        Section section = Section::Primary;
        int index = -1;
        bool presentable = true;
        bool suppressionWarningIssued = false;
        quint64 revision = 0;
    };

    QList<QAction*>& sectionActions(Section section);
    const QList<QAction*>& sectionActions(Section section) const;
    QList<QAction*>& otherSectionActions(Section section);

    bool validateForRegistration(const QAction* action,
                                 QString* reason = nullptr) const;
    bool validateForPresentation(const QAction* action,
                                 QString* reason = nullptr) const;
    void ensureAssociated(QAction* action);
    void removeAssociation(QAction* action);
    void connectAction(QAction* action);
    void disconnectAction(QAction* action);
    void handleActionChanged(QAction* action);
    void handleActionDestroyed(QAction* action);
    void warnRejected(const QAction* action, const QString& reason) const;
    void warnSuppressed(const QAction* action, const QString& reason);

    QPointer<QWidget> m_owner;
    QList<QAction*> m_primaryActions;
    QList<QAction*> m_secondaryActions;
    QHash<QAction*, Connections> m_connections;
    QHash<QAction*, bool> m_presentationState;
    QHash<QAction*, PendingRemoval> m_pendingRemovals;
    QList<QAction*> m_warnedSuppressedActions;
    quint64 m_pendingRemovalRevision = 0;
    int m_associationChangeDepth = 0;
};

} // namespace fluent::menus_toolbars::detail

#endif // FLUENTQT_COMPONENTS_MENUS_TOOLBARS_PRIVATE_COMMANDACTIONMODEL_P_H
