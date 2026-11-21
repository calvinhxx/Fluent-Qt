#pragma once

#include <FluentQt/BasicInput.h>
#include <QPointer>
#include <QStringList>

class QAction;
class QTimer;

namespace fluentqt_designer {

enum class Surface {
    Popup,
    Flyout,
    TeachingTip,
    CoachMark,
    DrawerView,
    SplashScreen,
    Toast,
    ToolTip,
    Menu,
    CommandBarFlyout,
    CommandPalette
};

// The button is the designable anchor. The component is created for its current window.
class SurfaceButton : public fluent::basicinput::Button {
    Q_OBJECT
public:
    ~SurfaceButton() override;
    QWidget* surface();

public slots:
    void openSurface();

protected:
    SurfaceButton(Surface kind, QWidget* parent);
    virtual void configure(QWidget* surface) = 0;
    Surface m_kind;
    QPointer<QTimer> m_dismissTimer;

private:
    QPointer<QWidget> m_surface;
    QPointer<QWidget> m_host;
};

class MessageButton : public SurfaceButton {
    Q_OBJECT
    Q_PROPERTY(QString surfaceText READ surfaceText WRITE setSurfaceText NOTIFY surfaceTextChanged)
public:
    MessageButton(Surface kind, QWidget* parent);
    QString surfaceText() const { return m_text; }
    void setSurfaceText(const QString& text);
signals:
    void surfaceTextChanged(const QString& text);

protected:
    void configure(QWidget* surface) override;

private:
    QString m_text;
};

class TimedMessageButton : public MessageButton {
    Q_OBJECT
    Q_PROPERTY(int duration READ duration WRITE setDuration NOTIFY durationChanged)
public:
    TimedMessageButton(Surface kind, QWidget* parent) : MessageButton(kind, parent) {}
    int duration() const { return m_duration; }
    void setDuration(int milliseconds);
signals:
    void durationChanged(int milliseconds);

private:
    void configure(QWidget* surface) override;
    int m_duration = 2200;
};

class CommandsButton : public SurfaceButton {
    Q_OBJECT
    Q_PROPERTY(QStringList actionTexts READ actionTexts WRITE setActionTexts)
public:
    CommandsButton(Surface kind, QWidget* parent);
    QStringList actionTexts() const { return m_texts; }
    void setActionTexts(const QStringList& texts);
signals:
    void actionTriggered(const QString& text);

private:
    void configure(QWidget* surface) override;
    QStringList m_texts;
    QList<QPointer<QAction>> m_actions;
    QPointer<QWidget> m_menu;
};

} // namespace fluentqt_designer
