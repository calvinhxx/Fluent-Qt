#include "GalleryPreviewActions.h"

#include <QApplication>
#include <QEvent>
#include <QEventLoop>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QTimer>
#include <QVariant>
#include <QVector>
#include <QWidget>

#include "compatibility/QtCompat.h"

namespace fluent::gallery {
namespace {

constexpr int kActionScriptSchemaVersion = 1;

struct TargetResolution {
  QWidget *widget = nullptr;
  QString error;
};

QJsonObject rectObject(const QRect &rect) {
  return {{QStringLiteral("x"), rect.x()},
          {QStringLiteral("y"), rect.y()},
          {QStringLiteral("width"), rect.width()},
          {QStringLiteral("height"), rect.height()}};
}

QString widgetPath(QWidget *widget, QWidget *root) {
  QStringList segments;
  for (QWidget *current = widget; current;
       current = current->parentWidget()) {
    QString segment = current->objectName();
    if (segment.isEmpty())
      segment = QString::fromLatin1(current->metaObject()->className());
    segments.prepend(segment);
    if (current == root)
      break;
  }
  return segments.join(QLatin1Char('/'));
}

TargetResolution resolveTarget(QWidget *root, const QString &selector,
                               bool allowFocusedWidget) {
  if (!root)
    return {nullptr, QStringLiteral("Preview root is missing.")};
  if (selector == QStringLiteral("@root"))
    return {root, QString()};
  if (selector.isEmpty() || selector == QStringLiteral("@focus")) {
    if (!allowFocusedWidget)
      return {nullptr, QStringLiteral("This action requires a target.")};
    QWidget *focused = QApplication::focusWidget();
    if (!focused || (focused != root && !root->isAncestorOf(focused))) {
      return {nullptr,
              QStringLiteral("No focused widget belongs to the preview.")};
    }
    // Some compound Qt widgets keep QApplication::focusWidget() on the
    // container while a focus-proxy editor also reports focus. Prefer the
    // deepest focused descendant so keyboard evidence reaches the painted
    // editor rather than only the compound control shell.
    const auto descendants = focused->findChildren<QWidget *>();
    for (QWidget *candidate : descendants) {
      if (candidate->hasFocus() &&
          (focused == candidate || focused->isAncestorOf(candidate))) {
        focused = candidate;
      }
    }
    return {focused, QString()};
  }

  QVector<QWidget *> matches;
  if (root->objectName() == selector)
    matches.append(root);
  const auto descendants = root->findChildren<QWidget *>(selector);
  for (QWidget *candidate : descendants)
    matches.append(candidate);
  if (matches.isEmpty()) {
    return {nullptr,
            QStringLiteral("No widget has objectName '%1'.").arg(selector)};
  }
  if (matches.size() > 1) {
    return {nullptr,
            QStringLiteral("objectName '%1' is ambiguous (%2 matches).")
                .arg(selector)
                .arg(matches.size())};
  }
  return {matches.first(), QString()};
}

Qt::KeyboardModifiers modifiersFromJson(const QJsonValue &value,
                                        QString &error) {
  Qt::KeyboardModifiers modifiers = Qt::NoModifier;
  for (const QJsonValue &entry : value.toArray()) {
    const QString name = entry.toString().trimmed().toLower();
    if (name == QStringLiteral("shift"))
      modifiers |= Qt::ShiftModifier;
    else if (name == QStringLiteral("control") ||
             name == QStringLiteral("ctrl"))
      modifiers |= Qt::ControlModifier;
    else if (name == QStringLiteral("alt"))
      modifiers |= Qt::AltModifier;
    else if (name == QStringLiteral("meta"))
      modifiers |= Qt::MetaModifier;
    else if (name == QStringLiteral("shortcut")) {
#if defined(Q_OS_MACOS) || defined(Q_OS_MAC)
      modifiers |= Qt::MetaModifier;
#else
      modifiers |= Qt::ControlModifier;
#endif
    }
    else {
      error = QStringLiteral("Unknown keyboard modifier '%1'.").arg(name);
      return Qt::NoModifier;
    }
  }
  return modifiers;
}

Qt::MouseButton mouseButtonFromJson(const QJsonValue &value, QString &error) {
  const QString name = value.toString(QStringLiteral("left")).trimmed().toLower();
  if (name == QStringLiteral("left"))
    return Qt::LeftButton;
  if (name == QStringLiteral("right"))
    return Qt::RightButton;
  if (name == QStringLiteral("middle"))
    return Qt::MiddleButton;
  error = QStringLiteral("Unknown mouse button '%1'.").arg(name);
  return Qt::NoButton;
}

int keyFromName(const QString &raw, QString &text, QString &error) {
  const QString name = raw.trimmed().toLower();
  if (name == QStringLiteral("enter") || name == QStringLiteral("return"))
    return Qt::Key_Return;
  if (name == QStringLiteral("space")) {
    text = QStringLiteral(" ");
    return Qt::Key_Space;
  }
  if (name == QStringLiteral("escape") || name == QStringLiteral("esc"))
    return Qt::Key_Escape;
  if (name == QStringLiteral("tab"))
    return Qt::Key_Tab;
  if (name == QStringLiteral("backtab"))
    return Qt::Key_Backtab;
  if (name == QStringLiteral("up"))
    return Qt::Key_Up;
  if (name == QStringLiteral("down"))
    return Qt::Key_Down;
  if (name == QStringLiteral("left"))
    return Qt::Key_Left;
  if (name == QStringLiteral("right"))
    return Qt::Key_Right;
  if (name == QStringLiteral("home"))
    return Qt::Key_Home;
  if (name == QStringLiteral("end"))
    return Qt::Key_End;
  if (name == QStringLiteral("pageup"))
    return Qt::Key_PageUp;
  if (name == QStringLiteral("pagedown"))
    return Qt::Key_PageDown;
  if (name == QStringLiteral("backspace"))
    return Qt::Key_Backspace;
  if (name == QStringLiteral("delete"))
    return Qt::Key_Delete;
  if (raw.size() == 1) {
    text = raw;
    return raw.at(0).toUpper().unicode();
  }
  error = QStringLiteral("Unknown key '%1'.").arg(raw);
  return 0;
}

void processUi(int milliseconds = 0) {
  QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
  QApplication::processEvents(QEventLoop::AllEvents, 50);
  if (milliseconds <= 0)
    return;
  QEventLoop loop;
  QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
  loop.exec();
  QApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
  QApplication::processEvents(QEventLoop::AllEvents, 50);
}

QPoint localPosition(QWidget *target, const QJsonObject &step, QString &error) {
  const QJsonValue positionValue = step.value(QStringLiteral("position"));
  if (positionValue.isUndefined())
    return target->rect().center();
  if (!positionValue.isObject()) {
    error = QStringLiteral("position must be an object with x and y.");
    return {};
  }
  const QJsonObject position = positionValue.toObject();
  if (!position.value(QStringLiteral("x")).isDouble() ||
      !position.value(QStringLiteral("y")).isDouble()) {
    error = QStringLiteral("position.x and position.y must be numbers.");
    return {};
  }
  const QPoint point(position.value(QStringLiteral("x")).toInt(),
                     position.value(QStringLiteral("y")).toInt());
  if (!target->rect().contains(point)) {
    error = QStringLiteral("position is outside the target widget.");
    return {};
  }
  return point;
}

void sendMouse(QWidget *target, QEvent::Type type, const QPoint &position,
               Qt::MouseButton button, Qt::MouseButtons buttons,
               Qt::KeyboardModifiers modifiers) {
  FLUENT_MAKE_MOUSE_EVENT(event, type, target, position, button, buttons,
                          modifiers);
  QApplication::sendEvent(target, &event);
}

QJsonValue observedProperty(QWidget *target, const QString &name,
                            bool &found) {
  found = true;
  if (name == QStringLiteral("visible"))
    return target->isVisible();
  if (name == QStringLiteral("enabled"))
    return target->isEnabled();
  if (name == QStringLiteral("has_focus"))
    return target->hasFocus();
  if (name == QStringLiteral("x"))
    return target->x();
  if (name == QStringLiteral("y"))
    return target->y();
  if (name == QStringLiteral("width"))
    return target->width();
  if (name == QStringLiteral("height"))
    return target->height();
  if (name == QStringLiteral("object_name"))
    return target->objectName();
  if (name == QStringLiteral("accessible_name"))
    return target->accessibleName();

  const QByteArray key = name.toUtf8();
  const QVariant value = target->property(key.constData());
  if (!value.isValid()) {
    found = false;
    return {};
  }
  return QJsonValue::fromVariant(value);
}

QJsonObject observation(QWidget *target, QWidget *root,
                        const QJsonObject &expectations,
                        const QJsonArray &requestedProperties,
                        QStringList &expectationErrors) {
  QJsonObject observed;
  QStringList properties;
  for (auto it = expectations.constBegin(); it != expectations.constEnd(); ++it)
    properties.append(it.key());
  for (const QJsonValue &value : requestedProperties) {
    const QString property = value.toString();
    if (!property.isEmpty() && !properties.contains(property))
      properties.append(property);
  }
  for (const QString &property : properties) {
    bool found = false;
    const QJsonValue actual = observedProperty(target, property, found);
    if (!found) {
      expectationErrors.append(
          QStringLiteral("Property '%1' is not observable.").arg(property));
      continue;
    }
    observed.insert(property, actual);
    if (expectations.contains(property) &&
        expectations.value(property) != actual) {
      expectationErrors.append(
          QStringLiteral("Property '%1' did not match the expected value.")
              .arg(property));
    }
  }

  return {{QStringLiteral("path"), widgetPath(target, root)},
          {QStringLiteral("class"),
           QString::fromLatin1(target->metaObject()->className())},
          {QStringLiteral("object_name"), target->objectName()},
          {QStringLiteral("rect"), rectObject(target->geometry())},
          {QStringLiteral("visible"), target->isVisible()},
          {QStringLiteral("enabled"), target->isEnabled()},
          {QStringLiteral("has_focus"), target->hasFocus()},
          {QStringLiteral("properties"), observed}};
}

bool performAction(QWidget *target, const QJsonObject &step, QString &mechanism,
                   QString &error) {
  const QString action =
      step.value(QStringLiteral("action")).toString().trimmed().toLower();
  QString modifierError;
  const Qt::KeyboardModifiers modifiers =
      modifiersFromJson(step.value(QStringLiteral("modifiers")), modifierError);
  if (!modifierError.isEmpty()) {
    error = modifierError;
    return false;
  }

  if (action == QStringLiteral("wait")) {
    const QJsonValue millisecondsValue =
        step.value(QStringLiteral("milliseconds"));
    const int milliseconds =
        millisecondsValue.toInt(-1);
    if (!millisecondsValue.isDouble() ||
        millisecondsValue.toDouble(-1.0) !=
            static_cast<double>(milliseconds) ||
        milliseconds < 0 || milliseconds > 10000) {
      error = QStringLiteral("wait milliseconds must be from 0 to 10000.");
      return false;
    }
    mechanism = QStringLiteral("event-loop");
    processUi(milliseconds);
    return true;
  }
  if (!target) {
    error = QStringLiteral("The action target is missing.");
    return false;
  }
  if (action == QStringLiteral("focus")) {
    mechanism = QStringLiteral("focus-api");
    target->setFocus(Qt::OtherFocusReason);
    return true;
  }
  if (action == QStringLiteral("set_property")) {
    const QString property =
        step.value(QStringLiteral("property")).toString().trimmed();
    if (property.isEmpty() || !step.contains(QStringLiteral("value"))) {
      error = QStringLiteral("set_property requires property and value.");
      return false;
    }
    const QByteArray key = property.toUtf8();
    if (!target->setProperty(key.constData(),
                             step.value(QStringLiteral("value")).toVariant())) {
      error = QStringLiteral("Property '%1' is not writable.").arg(property);
      return false;
    }
    mechanism = QStringLiteral("state-staging");
    return true;
  }
  if (action == QStringLiteral("key")) {
    QString text;
    QString keyError;
    const int key = keyFromName(
        step.value(QStringLiteral("key")).toString(), text, keyError);
    if (!keyError.isEmpty()) {
      error = keyError;
      return false;
    }
    mechanism = QStringLiteral("input-event");
    QKeyEvent press(QEvent::KeyPress, key, modifiers, text);
    QApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, modifiers, text);
    QApplication::sendEvent(target, &release);
    return true;
  }
  if (action == QStringLiteral("type_text")) {
    const QString text = step.value(QStringLiteral("text")).toString();
    if (text.isEmpty()) {
      error = QStringLiteral("type_text requires non-empty text.");
      return false;
    }
    mechanism = QStringLiteral("input-event");
    for (const QChar character : text) {
      QKeyEvent press(QEvent::KeyPress, 0, modifiers, QString(character));
      QApplication::sendEvent(target, &press);
      QKeyEvent release(QEvent::KeyRelease, 0, modifiers, QString(character));
      QApplication::sendEvent(target, &release);
    }
    return true;
  }
  if (action == QStringLiteral("mouse_leave")) {
    mechanism = QStringLiteral("input-event");
    QEvent leave(QEvent::Leave);
    QApplication::sendEvent(target, &leave);
    return true;
  }

  QString buttonError;
  const Qt::MouseButton button =
      mouseButtonFromJson(step.value(QStringLiteral("button")), buttonError);
  if (!buttonError.isEmpty()) {
    error = buttonError;
    return false;
  }
  const QPoint position = localPosition(target, step, error);
  if (!error.isEmpty())
    return false;
  mechanism = QStringLiteral("input-event");
  if (action == QStringLiteral("mouse_move")) {
    QEvent enter(QEvent::Enter);
    QApplication::sendEvent(target, &enter);
    sendMouse(target, QEvent::MouseMove, position, Qt::NoButton,
              Qt::NoButton, modifiers);
    return true;
  }
  if (action == QStringLiteral("mouse_press")) {
    sendMouse(target, QEvent::MouseButtonPress, position, button, button,
              modifiers);
    return true;
  }
  if (action == QStringLiteral("mouse_release")) {
    sendMouse(target, QEvent::MouseButtonRelease, position, button,
              Qt::NoButton, modifiers);
    return true;
  }
  if (action == QStringLiteral("click")) {
    sendMouse(target, QEvent::MouseButtonPress, position, button, button,
              modifiers);
    processUi();
    sendMouse(target, QEvent::MouseButtonRelease, position, button,
              Qt::NoButton, modifiers);
    return true;
  }

  error = QStringLiteral("Unknown action '%1'.").arg(action);
  return false;
}

QJsonObject failedReport(const QString &sourcePath, const QString &message) {
  return {{QStringLiteral("schema_version"), 1},
          {QStringLiteral("requested"), true},
          {QStringLiteral("source"), sourcePath},
          {QStringLiteral("status"), QStringLiteral("fail")},
          {QStringLiteral("error"), message},
          {QStringLiteral("summary"),
           QJsonObject{{QStringLiteral("total"), 0},
                       {QStringLiteral("executed"), 0},
                       {QStringLiteral("passed"), 0},
                       {QStringLiteral("failed"), 1}}},
          {QStringLiteral("steps"), QJsonArray{}}};
}

} // namespace

QJsonObject galleryPreviewActionsNotRequested() {
  return {{QStringLiteral("schema_version"), 1},
          {QStringLiteral("requested"), false},
          {QStringLiteral("status"), QStringLiteral("not-requested")},
          {QStringLiteral("summary"),
           QJsonObject{{QStringLiteral("total"), 0},
                       {QStringLiteral("executed"), 0},
                       {QStringLiteral("passed"), 0},
                       {QStringLiteral("failed"), 0}}},
          {QStringLiteral("steps"), QJsonArray{}}};
}

GalleryPreviewActionResult
executeGalleryPreviewActions(QWidget *root, const QJsonObject &script,
                             const QString &sourcePath) {
  if (!root)
    return {failedReport(sourcePath, QStringLiteral("Preview root is missing.")),
            false};
  if (script.value(QStringLiteral("schema_version")).toInt(-1) !=
      kActionScriptSchemaVersion) {
    return {failedReport(sourcePath,
                         QStringLiteral("Action script schema_version must be 1.")),
            false};
  }
  if (!script.value(QStringLiteral("steps")).isArray()) {
    return {failedReport(sourcePath,
                         QStringLiteral("Action script steps must be an array.")),
            false};
  }

  const QJsonArray steps = script.value(QStringLiteral("steps")).toArray();
  if (steps.isEmpty()) {
    return {failedReport(
                sourcePath,
                QStringLiteral("Action script steps must not be empty.")),
            false};
  }
  const bool stopOnFailure =
      script.value(QStringLiteral("stop_on_failure")).toBool(true);
  QJsonArray results;
  int passed = 0;
  int failed = 0;
  for (int index = 0; index < steps.size(); ++index) {
    const QJsonValue stepValue = steps.at(index);
    QJsonObject result{{QStringLiteral("index"), index}};
    if (!stepValue.isObject()) {
      result.insert(QStringLiteral("status"), QStringLiteral("fail"));
      result.insert(QStringLiteral("message"),
                    QStringLiteral("Step must be a JSON object."));
      results.append(result);
      ++failed;
      if (stopOnFailure)
        break;
      continue;
    }

    const QJsonObject step = stepValue.toObject();
    result.insert(QStringLiteral("request"), step);
    const QString id = step.value(QStringLiteral("id"))
                           .toString(QStringLiteral("step-%1").arg(index + 1));
    const QString action =
        step.value(QStringLiteral("action")).toString().trimmed().toLower();
    const QString selector =
        step.value(QStringLiteral("target")).toString().trimmed();
    result.insert(QStringLiteral("id"), id);
    result.insert(QStringLiteral("action"), action);
    result.insert(QStringLiteral("target"), selector);

    if (action.isEmpty()) {
      result.insert(QStringLiteral("status"), QStringLiteral("fail"));
      result.insert(QStringLiteral("message"),
                    QStringLiteral("Step action is required."));
      results.append(result);
      ++failed;
      if (stopOnFailure)
        break;
      continue;
    }

    const QJsonValue afterValue = step.value(QStringLiteral("after_ms"));
    const int afterMilliseconds = afterValue.toInt(-1);
    if (!afterValue.isUndefined() &&
        (!afterValue.isDouble() ||
         afterValue.toDouble(-1.0) !=
             static_cast<double>(afterMilliseconds) ||
         afterMilliseconds < 0 || afterMilliseconds > 10000)) {
      result.insert(QStringLiteral("status"), QStringLiteral("fail"));
      result.insert(QStringLiteral("message"),
                    QStringLiteral("after_ms must be from 0 to 10000."));
      results.append(result);
      ++failed;
      if (stopOnFailure)
        break;
      continue;
    }

    const bool allowFocused =
        action == QStringLiteral("key") ||
        action == QStringLiteral("type_text");
    TargetResolution target;
    if (action == QStringLiteral("wait") && selector.isEmpty())
      target = {root, QString()};
    else
      target = resolveTarget(root, selector, allowFocused);
    const QString descendantClass =
        step.value(QStringLiteral("descendant_class")).toString().trimmed();
    if (target.error.isEmpty() && !descendantClass.isEmpty()) {
      QVector<QWidget *> descendants;
      const auto candidates = target.widget->findChildren<QWidget *>();
      for (QWidget *candidate : candidates) {
        if (QString::fromLatin1(candidate->metaObject()->className()) ==
            descendantClass) {
          descendants.append(candidate);
        }
      }
      if (descendants.size() == 1) {
        target.widget = descendants.first();
      } else {
        target.error =
            QStringLiteral("Target '%1' has %2 descendants of class '%3'.")
                .arg(selector)
                .arg(descendants.size())
                .arg(descendantClass);
      }
    }
    if (!target.error.isEmpty()) {
      result.insert(QStringLiteral("status"), QStringLiteral("fail"));
      result.insert(QStringLiteral("message"), target.error);
      results.append(result);
      ++failed;
      if (stopOnFailure)
        break;
      continue;
    }

    QString mechanism;
    QString actionError;
    bool actionPassed =
        performAction(target.widget, step, mechanism, actionError);
    processUi(afterValue.isUndefined() ? 0 : afterMilliseconds);

    const QJsonObject expectations =
        step.value(QStringLiteral("expect")).toObject();
    QStringList expectationErrors;
    const QJsonObject observed =
        observation(target.widget, root, expectations,
                    step.value(QStringLiteral("observe")).toArray(),
                    expectationErrors);
    if (!expectationErrors.isEmpty())
      actionPassed = false;

    result.insert(QStringLiteral("mechanism"), mechanism);
    result.insert(QStringLiteral("expect"), expectations);
    result.insert(QStringLiteral("observation"), observed);
    result.insert(QStringLiteral("status"),
                  actionPassed ? QStringLiteral("pass")
                               : QStringLiteral("fail"));
    QStringList messages;
    if (!actionError.isEmpty())
      messages.append(actionError);
    messages.append(expectationErrors);
    result.insert(QStringLiteral("message"), messages.join(QStringLiteral(" ")));
    results.append(result);
    if (actionPassed)
      ++passed;
    else
      ++failed;
    if (!actionPassed && stopOnFailure)
      break;
  }

  const bool allPassed = failed == 0 && results.size() == steps.size();
  const QJsonObject report{
      {QStringLiteral("schema_version"), 1},
      {QStringLiteral("requested"), true},
      {QStringLiteral("source"), sourcePath},
      {QStringLiteral("status"),
       allPassed ? QStringLiteral("pass") : QStringLiteral("fail")},
      {QStringLiteral("summary"),
       QJsonObject{{QStringLiteral("total"), steps.size()},
                   {QStringLiteral("executed"), results.size()},
                   {QStringLiteral("passed"), passed},
                   {QStringLiteral("failed"), failed}}},
      {QStringLiteral("steps"), results}};
  return {report, allPassed};
}

GalleryPreviewActionResult runGalleryPreviewActions(QWidget *root,
                                                     const QString &path) {
  if (path.isEmpty())
    return {galleryPreviewActionsNotRequested(), true};
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return {failedReport(path,
                         QStringLiteral("Could not open action script: %1")
                             .arg(file.errorString())),
            false};
  }
  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    return {failedReport(path,
                         QStringLiteral("Could not parse action script: %1")
                             .arg(parseError.errorString())),
            false};
  }
  return executeGalleryPreviewActions(root, document.object(), path);
}

} // namespace fluent::gallery
