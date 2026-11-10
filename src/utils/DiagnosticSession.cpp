#include "DiagnosticSession.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QElapsedTimer>
#include <QEvent>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QScreen>
#include <QSysInfo>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QWidget>
#include <deque>

#include "components/foundation/FluentElement.h"
#include "utils/Inspector.h"

namespace fluent::diagnostics {
namespace {
constexpr int kMaximumEvents = 512;
constexpr int kMaximumWidgets = 256;
constexpr int kMaximumMessageCharacters = 512;
constexpr int kMaximumSessionMs = 5 * 60 * 1000;

QVariantMap rectangle(const QRect& rect)
{
    return {{"x", rect.x()}, {"y", rect.y()}, {"width", rect.width()}, {"height", rect.height()}};
}

QString boundedText(const QString& text, int limit)
{
    QString result = text.left(limit);
    if (!result.isEmpty() && result.at(result.size() - 1).isHighSurrogate())
        result.chop(1);
    return result;
}

QString severity(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:
        return QStringLiteral("debug");
    case QtInfoMsg:
        return QStringLiteral("info");
    case QtWarningMsg:
        return QStringLiteral("warning");
    case QtCriticalMsg:
        return QStringLiteral("critical");
    case QtFatalMsg:
        return QStringLiteral("fatal");
    }
    return QStringLiteral("unknown");
}
} // namespace

struct DiagnosticSession::State {
    mutable QMutex mutex;
    bool recording = false;
    bool includeMessages = false;
    QElapsedTimer clock;
    qint64 duration = 0;
    int maximumDuration = kMaximumSessionMs;
    quint64 dropped = 0;
    std::deque<QVariantMap> events;
    QVariantMap environment;
    QVariantMap initial;
    QVariantMap final;

    void append(QVariantMap event)
    {
        if (!recording)
            return;
        event.insert(QStringLiteral("elapsed_ms"), clock.elapsed());
        if (events.size() == kMaximumEvents) {
            events.pop_front();
            ++dropped;
        }
        events.push_back(std::move(event));
    }
};

struct DiagnosticSession::UiState {
    QPointer<QWidget> root;
    QTimer* timeout = nullptr;
    QMetaObject::Connection destroyed;
};

DiagnosticSession::DiagnosticSession(QObject* parent)
    : QObject(parent), m_state(std::make_shared<State>()), m_ui(std::make_unique<UiState>())
{
    m_ui->timeout = new QTimer(this);
    m_ui->timeout->setSingleShot(true);
    connect(m_ui->timeout, &QTimer::timeout, this, &DiagnosticSession::stop);
}

DiagnosticSession::~DiagnosticSession()
{
    if (qApp)
        qApp->removeEventFilter(this);
    QMutexLocker lock(&m_state->mutex);
    m_state->recording = false;
}

bool DiagnosticSession::start(QWidget* root, bool includeLogMessages, int maximumDurationMs)
{
    if (!qApp || QThread::currentThread() != qApp->thread() || thread() != qApp->thread() || !root
        || root->thread() != thread() || maximumDurationMs < 0)
        return false;
    if (isRecording())
        return false;
    clear();
    m_ui->root = root;
    const auto initial = snapshot();
    const auto runtime = environment();
    {
        QMutexLocker lock(&m_state->mutex);
        m_state->environment = runtime;
        m_state->initial = initial;
        m_state->includeMessages = includeLogMessages;
        m_state->maximumDuration = maximumDurationMs;
        m_state->clock.start();
        m_state->recording = true;
    }
    m_ui->destroyed = connect(root, &QObject::destroyed, this, [this] {
        m_ui->root.clear();
        stop();
    });
    qApp->installEventFilter(this);
    if (maximumDurationMs > 0)
        m_ui->timeout->start(maximumDurationMs);
    emit recordingChanged(true);
    return true;
}

bool DiagnosticSession::isRecording() const
{
    QMutexLocker lock(&m_state->mutex);
    return m_state->recording;
}

void DiagnosticSession::stop()
{
    if (QThread::currentThread() != thread() || !isRecording())
        return;
    qApp->removeEventFilter(this);
    m_ui->timeout->stop();
    disconnect(m_ui->destroyed);
    {
        QMutexLocker lock(&m_state->mutex);
        m_state->recording = false;
        m_state->duration = m_state->clock.elapsed();
    }
    const auto final = snapshot();
    {
        QMutexLocker lock(&m_state->mutex);
        m_state->final = final;
    }
    emit recordingChanged(false);
}

void DiagnosticSession::clear()
{
    if (QThread::currentThread() != thread())
        return;
    stop();
    m_ui->root.clear();
    QMutexLocker lock(&m_state->mutex);
    m_state->events.clear();
    m_state->initial.clear();
    m_state->final.clear();
    m_state->environment.clear();
    m_state->dropped = 0;
    m_state->duration = 0;
    m_state->includeMessages = false;
}

QVariantMap DiagnosticSession::environment()
{
    if (!qApp || QThread::currentThread() != qApp->thread())
        return {};
    QVariantMap result{{"fluentqt_version", QStringLiteral(FLUENTQT_DIAGNOSTIC_VERSION)},
        {"application_version", QCoreApplication::applicationVersion()},
        {"qt_version", QString::fromLatin1(qVersion())}, {"os", QSysInfo::prettyProductName()},
        {"architecture", QSysInfo::currentCpuArchitecture()}, {"platform", QGuiApplication::platformName()},
        {"locale", QLocale().name()}, {"font_family", QApplication::font().family()},
        {"font_point_size", QApplication::font().pointSizeF()},
        {"theme", static_cast<int>(FluentElement::currentTheme())},
        {"layout_direction", qApp->layoutDirection() == Qt::RightToLeft ? "rtl" : "ltr"}};
    QVariantList screens;
    for (const auto* screen : QGuiApplication::screens()) {
        screens.append(QVariantMap{{"width", screen->size().width()}, {"height", screen->size().height()},
            {"device_pixel_ratio", screen->devicePixelRatio()}, {"logical_dpi", screen->logicalDotsPerInch()}});
        if (screens.size() == 16)
            break;
    }
    result.insert(QStringLiteral("screens"), screens);
    return result;
}

QVariantMap DiagnosticSession::snapshot()
{
    if (!m_ui->root)
        return {{"root_destroyed", true}};
    QList<QWidget*> pending{m_ui->root};
    QVariantList widgets;
    bool truncated = false;
    for (int i = 0; i < pending.size(); ++i) {
        QWidget* widget = pending.at(i);
        QVariantMap data{{"class", QString::fromLatin1(widget->metaObject()->className())},
            {"rect", rectangle(QRect(widget->mapTo(m_ui->root, QPoint()), widget->size()))},
            {"visible", widget->isVisible()}, {"enabled", widget->isEnabled()}, {"focused", widget->hasFocus()},
            {"focus_policy", static_cast<int>(widget->focusPolicy())}, {"minimum_width", widget->minimumWidth()},
            {"minimum_height", widget->minimumHeight()}};
        if (const auto* editor = qobject_cast<QLineEdit*>(widget)) {
            data.insert(QStringLiteral("text_margins"),
                QVariantList{editor->textMargins().left(), editor->textMargins().top(), editor->textMargins().right(),
                    editor->textMargins().bottom()});
            data.insert(QStringLiteral("read_only"), editor->isReadOnly());
        }
        if (const auto* view = qobject_cast<QAbstractItemView*>(widget); view && view->model()) {
            data.insert(QStringLiteral("rows"), view->model()->rowCount(view->rootIndex()));
            data.insert(QStringLiteral("columns"), view->model()->columnCount(view->rootIndex()));
            data.insert(QStringLiteral("current_row"), view->currentIndex().row());
        }
        widgets.append(data);
        for (QObject* child : widget->children()) {
            if (auto* childWidget = qobject_cast<QWidget*>(child)) {
                if (pending.size() == kMaximumWidgets) {
                    truncated = true;
                    break;
                }
                pending.append(childWidget);
            }
        }
    }
    // Inspector may contain label text and authored object names. Copy only
    // rule identifiers and geometry, and skip its full traversal for large roots.
    // zh_CN: Inspector 可能含文本与对象名称，仅保留规则标识和几何；大树不作全量检查。
    QVariantList findings;
    if (!truncated) {
        const auto report = Inspector::report(m_ui->root).toVariantMap();
        for (const auto& item : report.value(QStringLiteral("findings")).toList()) {
            const auto finding = item.toMap();
            findings.append(QVariantMap{{"code", finding.value(QStringLiteral("code"))},
                {"severity", finding.value(QStringLiteral("severity"))},
                {"rect", finding.value(QStringLiteral("rect"))}});
            if (findings.size() == 64)
                break;
        }
    }
    return {{"widgets", widgets}, {"truncated", truncated}, {"findings", findings}};
}

QVariantMap DiagnosticSession::report() const
{
    QMutexLocker lock(&m_state->mutex);
    QVariantList events;
    for (const auto& event : m_state->events)
        events.append(event);
    return {{"schema_version", 1}, {"tool", "FluentQt Diagnostics"}, {"recording", m_state->recording},
        {"log_messages_included", m_state->includeMessages},
        {"duration_ms", m_state->recording ? m_state->clock.elapsed() : m_state->duration},
        {"dropped_events", QVariant::fromValue(m_state->dropped)},
        {"limits",
            QVariantMap{{"events", kMaximumEvents}, {"widgets_per_snapshot", kMaximumWidgets},
                {"message_characters", kMaximumMessageCharacters}, {"session_ms", m_state->maximumDuration}}},
        {"environment", m_state->environment}, {"initial", m_state->initial}, {"final", m_state->final},
        {"events", events}};
}

QVariantMap DiagnosticSession::captureReport()
{
    if (QThread::currentThread() != thread())
        return {};
    auto result = report();
    result.insert(QStringLiteral("final"), snapshot());
    result.insert(QStringLiteral("environment"), environment());
    return result;
}

DiagnosticSession::MessageSink DiagnosticSession::messageSink() const
{
    const std::weak_ptr<State> weak = m_state;
    return [weak](QtMsgType type, const QString& category, const QString& message) {
        if (const auto state = weak.lock()) {
            QMutexLocker lock(&state->mutex);
            if (!state->recording)
                return;
            QVariantMap event{{"kind", "log"}, {"severity", severity(type)}, {"category", boundedText(category, 80)}};
            if (state->includeMessages) {
                event.insert(QStringLiteral("message"), boundedText(message, kMaximumMessageCharacters));
                event.insert(QStringLiteral("truncated"), message.size() > kMaximumMessageCharacters);
            }
            state->append(std::move(event));
        }
    };
}

void DiagnosticSession::recordMessage(QtMsgType type, const QString& category, const QString& message)
{
    messageSink()(type, category, message);
}

bool DiagnosticSession::eventFilter(QObject* watched, QEvent* event)
{
    QString kind;
    int navigationKey = 0;
    switch (event->type()) {
    case QEvent::FocusIn:
        kind = QStringLiteral("focus_in");
        break;
    case QEvent::FocusOut:
        kind = QStringLiteral("focus_out");
        break;
    case QEvent::Show:
        kind = QStringLiteral("show");
        break;
    case QEvent::Hide:
        kind = QStringLiteral("hide");
        break;
    case QEvent::Resize:
        kind = QStringLiteral("resize");
        break;
    case QEvent::EnabledChange:
        kind = QStringLiteral("enabled_change");
        break;
    case QEvent::MouseButtonRelease:
        kind = QStringLiteral("pointer_release");
        break;
    case QEvent::KeyPress: {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key != Qt::Key_Up && key != Qt::Key_Down && key != Qt::Key_Left && key != Qt::Key_Right
            && key != Qt::Key_Tab && key != Qt::Key_Backtab && key != Qt::Key_Enter && key != Qt::Key_Return
            && key != Qt::Key_Escape)
            return false;
        kind = QStringLiteral("navigation_key");
        navigationKey = key;
        break;
    }
    default:
        return false;
    }
    auto* widget = qobject_cast<QWidget*>(watched);
    if (!widget || !m_ui->root || (widget != m_ui->root && !m_ui->root->isAncestorOf(widget)))
        return false;
    QVariantMap data{{"kind", kind}, {"class", QString::fromLatin1(widget->metaObject()->className())},
        {"rect", rectangle(QRect(widget->mapTo(m_ui->root, QPoint()), widget->size()))},
        {"focused", widget->hasFocus()}, {"visible", widget->isVisible()}, {"enabled", widget->isEnabled()}};
    if (navigationKey)
        data.insert(QStringLiteral("key"), navigationKey);
    QMutexLocker lock(&m_state->mutex);
    m_state->append(std::move(data));
    return false;
}

} // namespace fluent::diagnostics
