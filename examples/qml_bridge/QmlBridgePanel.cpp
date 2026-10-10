#include "QmlBridgePanel.h"
#include "QmlBridgeModel.h"

#include <FluentQt/FluentQt.h>
#include <FluentQt/QmlBridge.h>
#include <QCoreApplication>
#include <QEvent>
#include <QHBoxLayout>
#include <QMetaObject>
#include <QPointer>
#include <QStackedLayout>
#include <QVBoxLayout>

namespace fluentqt::qml_bridge {
namespace {
using fluent::basicinput::Button;
using fluent::basicinput::Slider;
using fluent::layout::Card;
using fluent::qmlbridge::QmlSession;
using fluent::qmlbridge::QmlView;
using fluent::textfields::Label;
using fluent::textfields::LineEdit;

const QUrl kPreviewSource(QStringLiteral("qrc:/fluentqt/qmlbridge/Preview.qml"));
} // namespace

struct QmlBridgePanel::Private {
    PreviewModel* model = nullptr;
    QmlSession* session = nullptr;
    QmlView* view = nullptr;
    LineEdit* title = nullptr;
    Slider* progress = nullptr;
    Label* progressText = nullptr;
    Label* placeholder = nullptr;
    Label* status = nullptr;
    Button* deactivate = nullptr;
    QStackedLayout* preview = nullptr;
    QString error;
};

QmlBridgePanel::QmlBridgePanel(QWidget* parent) : QWidget(parent), d(std::make_unique<Private>())
{
    setObjectName(QStringLiteral("qmlBridgePanel"));
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(24, 24, 24, 24);
    column->setSpacing(12);

    auto* heading = new QWidget(this);
    auto* headingLayout = new QVBoxLayout(heading);
    headingLayout->setContentsMargins(0, 0, 0, 0);
    headingLayout->setSpacing(4);
    auto* title = new Label(QStringLiteral("One model, two views"), heading);
    title->setFluentTypography(Typography::FontRole::Title);
    title->setWordWrap(true);
    auto* description =
        new Label(QStringLiteral("Edit the Widgets form or advance the QML preview."), heading);
    description->setTextColorRole(Label::TextColorRole::Secondary);
    description->setWordWrap(true);
    headingLayout->addWidget(title);
    headingLayout->addWidget(description);
    column->addWidget(heading);

    d->model = new PreviewModel(this);
    d->model->setObjectName(QStringLiteral("qmlBridgeModel"));
    d->session = new QmlSession(this, this);
    d->session->setObjectName(QStringLiteral("qmlBridgeSession"));

    auto* form = new Card(this);
    form->setObjectName(QStringLiteral("qmlBridgeForm"));
    auto* fields = new QVBoxLayout(form);
    fields->setContentsMargins(16, 16, 16, 16);
    fields->setSpacing(8);
    auto* formTitle = new Label(QStringLiteral("Widgets form"), form);
    formTitle->setFluentTypography(Typography::FontRole::BodyStrong);
    fields->addWidget(formTitle);
    auto* titleLabel = new Label(QStringLiteral("Title"), form);
    fields->addWidget(titleLabel);
    d->title = new LineEdit(form);
    d->title->setObjectName(QStringLiteral("qmlBridgeTitleEdit"));
    d->title->setAccessibleName(QStringLiteral("Preview title"));
    d->title->setText(d->model->title());
    titleLabel->setBuddy(d->title);
    fields->addWidget(d->title);
    auto* progressRow = new QHBoxLayout;
    progressRow->setSpacing(8);
    progressRow->addWidget(new Label(QStringLiteral("Progress"), form));
    progressRow->addStretch();
    d->progressText = new Label(form);
    d->progressText->setObjectName(QStringLiteral("qmlBridgeProgressText"));
    d->progressText->setTextColorRole(Label::TextColorRole::Secondary);
    progressRow->addWidget(d->progressText);
    fields->addLayout(progressRow);
    d->progress = new Slider(Qt::Horizontal, form);
    d->progress->setObjectName(QStringLiteral("qmlBridgeProgressSlider"));
    d->progress->setAccessibleName(QStringLiteral("Preview progress"));
    d->progress->setRange(0, 100);
    d->progress->setValue(d->model->progress());
    fields->addWidget(d->progress);
    column->addWidget(form);

    auto* preview = new Card(this);
    preview->setObjectName(QStringLiteral("qmlBridgePreviewCard"));
    preview->setMinimumHeight(220);
    d->preview = new QStackedLayout(preview);
    d->preview->setContentsMargins(0, 0, 0, 0);
    d->placeholder = new Label(preview);
    d->placeholder->setObjectName(QStringLiteral("qmlBridgePlaceholder"));
    d->placeholder->setTextColorRole(Label::TextColorRole::Secondary);
    d->placeholder->setAlignment(Qt::AlignCenter);
    d->placeholder->setWordWrap(true);
    d->placeholder->setMargin(20);
    d->preview->addWidget(d->placeholder);
    column->addWidget(preview, 1);

    auto* commands = new QHBoxLayout;
    commands->setSpacing(12);
    d->deactivate = new Button(QStringLiteral("Deactivate"), this);
    d->deactivate->setObjectName(QStringLiteral("qmlBridgeDeactivate"));
    auto* reload = new Button(QStringLiteral("Reload QML"), this);
    reload->setObjectName(QStringLiteral("qmlBridgeReload"));
    reload->setFluentStyle(Button::Accent);
    commands->addWidget(d->deactivate);
    commands->addWidget(reload);
    commands->addStretch();
    column->addLayout(commands);
    d->status = new Label(this);
    d->status->setObjectName(QStringLiteral("qmlBridgeStatus"));
    d->status->setWordWrap(true);
    d->status->setTextColorRole(Label::TextColorRole::Secondary);
    column->addWidget(d->status);

    connect(d->title, &LineEdit::textChanged, d->model, &PreviewModel::setTitle);
    connect(d->progress, &Slider::valueChanged, d->model, &PreviewModel::setProgress);
    connect(d->model, &PreviewModel::titleChanged, this, [this] {
        d->title->setText(d->model->title());
        updateState();
    });
    connect(d->model, &PreviewModel::progressChanged, this, [this] {
        d->progress->setValue(d->model->progress());
        updateState();
    });
    connect(d->session, &QmlSession::activeChanged, this, [this] { updateState(); });
    connect(d->deactivate, &Button::clicked, this, [this] { deactivateQml(); });
    connect(reload, &Button::clicked, this, [this] { reloadQml(); });
    reloadQml();
}

QmlBridgePanel::~QmlBridgePanel()
{
    disconnect(d->session, nullptr, this, nullptr);
    if (d->view)
        disconnect(d->view, nullptr, this, nullptr);
    d->session->deactivate();
}

bool QmlBridgePanel::reloadQml()
{
    d->error.clear();
    if (!d->session->activate(&d->error)) {
        updateState();
        return false;
    }
    if (!d->view) {
        d->view = d->session->createView(d->placeholder->parentWidget());
        if (d->view) {
            d->view->setObjectName(QStringLiteral("qmlBridgeView"));
            d->view->setAccessibleName(QStringLiteral("QML model preview"));
            d->preview->addWidget(d->view);
            connect(d->view, &QmlView::statusChanged, this, [this] { updateState(); });
            connect(d->view, &QmlView::errorOccurred, this, [this](const QString& error) {
                d->error = error;
                updateState();
            });
        }
    }
    if (!d->view) {
        d->error = QStringLiteral("The QML preview is unavailable. The Widgets form stays usable.");
        updateState();
        return false;
    }
    d->view->unload();
    if (!d->view->setContextObject(QStringLiteral("previewModel"), d->model)) {
        d->error = QStringLiteral("The preview model could not be attached.");
        updateState();
        return false;
    }
    const bool loaded = d->view->load(kPreviewSource, &d->error);
    updateState();
    return loaded;
}

bool QmlBridgePanel::deactivateQml()
{
    const bool stopped = d->session->deactivate(&d->error);
    updateState();
    return stopped;
}

void QmlBridgePanel::updateState()
{
    if (!d->status)
        return;
    const bool active = d->session->isActive();
    const bool ready = d->view && d->view->status() == QmlView::Ready;
    const bool loading = d->view && d->view->status() == QmlView::Loading;
    d->preview->setCurrentWidget(ready ? static_cast<QWidget*>(d->view) : d->placeholder);
    d->progressText->setText(QStringLiteral("%1%").arg(d->model->progress()));
    d->deactivate->setEnabled(active);
    d->placeholder->setText(
        !d->error.isEmpty() ? d->error
        : loading           ? QStringLiteral("Loading QML…")
                            : QStringLiteral("QML is unloaded. Your model is still available."));
    d->status->setText(!d->error.isEmpty() ? d->error
                       : ready   ? QStringLiteral("QML ready · model shared with Widgets")
                       : loading ? QStringLiteral("Loading QML…")
                                 : QStringLiteral("QML unloaded · Widgets remain editable"));
    emit stateChanged();
}

QVariantMap QmlBridgePanel::bridgeState() const
{
    return {{QStringLiteral("active"), d->session->isActive()},
            {QStringLiteral("ready"), d->view && d->view->status() == QmlView::Ready},
            {QStringLiteral("title"), d->model->title()},
            {QStringLiteral("progress"), d->model->progress()},
            {QStringLiteral("widgetTitle"), d->title->text()},
            {QStringLiteral("widgetProgress"), d->progress->value()},
            {QStringLiteral("error"), d->error}};
}

bool QmlBridgePanel::runSmokeTest()
{
    if (!d->view || d->view->status() != QmlView::Ready)
        return false;
    d->title->setText(QStringLiteral("Edited in Widgets"));
    d->progress->setValue(65);
    auto* root = d->view->rootObject();
    if (!root || root->property("previewTitle").toString() != d->model->title() ||
        root->property("previewProgress").toInt() != 65)
        return false;
    if (!QMetaObject::invokeMethod(root, "advance", Qt::DirectConnection) ||
        d->title->text() != QStringLiteral("Updated from QML") || d->progress->value() != 75)
        return false;
    const auto previousTheme = fluent::FluentElement::currentTheme();
    const auto nextTheme = previousTheme == fluent::FluentElement::Dark
                               ? fluent::FluentElement::Light
                               : fluent::FluentElement::Dark;
    fluent::FluentElement::setTheme(nextTheme);
    const bool themeSynchronized = d->view->themeObject()->property("dark").toBool() ==
                                       (nextTheme == fluent::FluentElement::Dark) &&
                                   root->property("color").value<QColor>() ==
                                       d->view->themeObject()->property("surface").value<QColor>();
    fluent::FluentElement::setTheme(previousTheme);
    if (!themeSynchronized)
        return false;
    const QPointer<PreviewModel> model(d->model);
    const QPointer<QObject> oldRoot(root);
    if (!deactivateQml() || !model || !d->view || d->view->rootObject() ||
        d->view->status() != QmlView::Unloaded || !d->view->source().isEmpty())
        return false;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    if (oldRoot)
        return false;
    d->title->setText(QStringLiteral("Edited while unloaded"));
    d->progress->setValue(25);
    if (!reloadQml() || !model || d->view->status() != QmlView::Ready)
        return false;
    root = d->view->rootObject();
    return root && root->property("previewTitle").toString() == d->title->text() &&
           root->property("previewProgress").toInt() == 25 &&
           QMetaObject::invokeMethod(root, "advance", Qt::DirectConnection) &&
           d->title->text() == QStringLiteral("Updated from QML") && d->progress->value() == 35;
}

} // namespace fluentqt::qml_bridge
