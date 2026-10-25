#ifndef GALLERY_CONNECTIONFORM_H
#define GALLERY_CONNECTIONFORM_H

#include <FluentQt/FluentQt.h>

#include <QBoxLayout>
#include <QMap>
#include <QResizeEvent>
#include <QTimer>
#include <QUrl>

namespace fluent::gallery {

// A deterministic connection demo: the application owns values and async work.
// zh_CN: 可重复的连接演示；应用负责字段值与异步操作。
class ConnectionForm final : public layout::Card {
public:
    explicit ConnectionForm(QWidget* parent = nullptr) : Card(parent)
    {
        setObjectName(QStringLiteral("connectionForm"));
        setMaximumWidth(560);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(32, 32, 32, 32);
        m_layout->setSpacing(24);
        m_layout->setAlignment(Qt::AlignTop);

        auto* heading = new QWidget(this);
        auto* headerLayout = new QVBoxLayout(heading);
        headerLayout->setContentsMargins(0, 0, 0, 0);
        headerLayout->setSpacing(8);
        headerLayout->addWidget(label(QStringLiteral("Connection settings"), Typography::FontRole::Subtitle));
        headerLayout->addWidget(
            label(QStringLiteral("Configure the endpoint used by this workspace."), Typography::FontRole::Body, true));
        m_layout->addWidget(heading);

        m_form = new FormController(this);
        m_form->setObjectName(QStringLiteral("connectionFormController"));
        addField(QStringLiteral("name"), QStringLiteral("Connection name"),
            QStringLiteral("A name to identify this connection."), QStringLiteral("Production API"));
        addField(QStringLiteral("address"), QStringLiteral("Server address"),
            QStringLiteral("Use HTTPS. Try offline.example to see a connection error."),
            QStringLiteral("https://api.example.com/v1"));
        addField(QStringLiteral("token"), QStringLiteral("API token"),
            QStringLiteral("Use any 8 characters in this demo."), QString(), true);

        m_status = label(QStringLiteral("No unsaved changes"), Typography::FontRole::Caption, true);
        m_status->setObjectName(QStringLiteral("connectionStatus"));
        m_status->setMinimumHeight(Typography::LineHeight::Caption);
        m_layout->addWidget(m_status);

        auto* actions = new QWidget(this);
        m_actions = new QBoxLayout(QBoxLayout::LeftToRight, actions);
        m_actions->setContentsMargins(0, 0, 0, 0);
        m_actions->setSpacing(8);
        m_test = new basicinput::Button(QStringLiteral("Test connection"), actions);
        m_test->setObjectName(QStringLiteral("connectionTest"));
        m_save = new basicinput::Button(QStringLiteral("Save connection"), actions);
        m_save->setObjectName(QStringLiteral("connectionSave"));
        m_save->setFluentStyle(basicinput::Button::Accent);
        m_actions->addWidget(m_test);
        m_actions->addWidget(m_save);
        m_layout->addWidget(actions);
        m_layout->addWidget(label(QStringLiteral("Local demo · no credentials are sent or stored."),
            Typography::FontRole::Caption, true));

        connect(m_form, &FormController::validationRequested, this, [this](const QString& key, quint64 token) {
            validate(key, token);
        });
        connect(m_form, &FormController::submitRequested, this, [this](quint64 token) {
            QTimer::singleShot(450, m_form, [form = m_form, token]() { form->completeSubmit(token, true); });
        });
        connect(m_form, &FormController::stateChanged, this, [this]() { updateStatus(); });
        connect(m_form, &FormController::submissionFinished, this, [this](bool success) {
            if (success)
                status_info::Toast::showOrUpdateToast(this, QStringLiteral("gallery.action-feedback"),
                    QStringLiteral("Connection saved for this demo"), status_info::Toast::Success, 2200,
                    status_info::Toast::Top, QMargins(16, 50, 16, 16));
        });
        connect(m_test, &basicinput::Button::clicked, m_form, &FormController::validate);
        connect(m_save, &basicinput::Button::clicked, m_form, &FormController::submit);
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        Card::resizeEvent(event);
        const bool narrow = width() < 400;
        const int padding = narrow ? 16 : 32;
        m_layout->setContentsMargins(padding, padding, padding, padding);
        m_actions->setDirection(narrow ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
    }

private:
    textfields::Label* label(const QString& text, Typography::FontRole role, bool secondary = false)
    {
        auto* result = new textfields::Label(text, this);
        result->setFluentTypography(role);
        result->setTextColorRole(
            secondary ? textfields::Label::TextColorRole::Secondary : textfields::Label::TextColorRole::Primary);
        result->setWordWrap(true);
        return result;
    }

    void
    addField(const QString& key, const QString& title, const QString& helper, const QString& value, bool secret = false)
    {
        auto* field = new layout::Field(this);
        field->setObjectName(QStringLiteral("connectionField_") + key);
        field->setLabelText(title);
        field->setHelperText(helper);
        textfields::LineEdit* editor = secret ? new textfields::PasswordBox : new textfields::LineEdit;
        editor->setObjectName(QStringLiteral("connectionInput_") + key);
        editor->setText(value);
        editor->setMinimumHeight(::Spacing::ControlHeight::Standard);
        if (secret)
            editor->setPlaceholderText(QStringLiteral("Enter a demo token"));
        field->setEditor(editor, WidgetOwnership::Owned);
        m_layout->addWidget(field);
        m_editors.insert(key, editor);
        m_form->addField(key, field);
        connect(editor, &textfields::LineEdit::textChanged, m_form, [form = m_form, key]() {
            form->markFieldChanged(key);
        });
        connect(editor, &textfields::LineEdit::editingFinished, m_form, [form = m_form, key]() {
            form->setFieldTouched(key);
        });
    }

    void validate(const QString& key, quint64 token)
    {
        const QString value = m_editors.value(key)->text().trimmed();
        if (key == QStringLiteral("name")) {
            m_form->completeValidation(key, token, !value.isEmpty(),
                value.isEmpty() ? QStringLiteral("Enter a connection name.") : QString());
        } else if (key == QStringLiteral("token")) {
            m_form->completeValidation(key, token, value.size() >= 8,
                value.size() < 8 ? QStringLiteral("Enter at least 8 characters.") : QString());
        } else {
            const QUrl url(value, QUrl::StrictMode);
            if (!url.isValid() || url.scheme() != QStringLiteral("https") || url.host().isEmpty()) {
                m_form->completeValidation(key, token, false, QStringLiteral("Enter a valid HTTPS address."));
                return;
            }
            // Context-owned timer is cancelled on teardown; the token rejects stale results.
            // zh_CN: 上下文计时器随对象销毁取消，token 拒绝过期结果。
            QTimer::singleShot(650, m_form, [form = m_form, key, token, value]() {
                const bool reachable = !value.contains(QStringLiteral("offline"), Qt::CaseInsensitive);
                form->completeValidation(key, token, reachable,
                    reachable ? QStringLiteral("Connection verified.")
                              : QStringLiteral("Server unavailable. Try another address."));
            });
        }
    }

    void updateStatus()
    {
        const bool busy = m_form->isValidating() || m_form->isSubmitting();
        m_test->setEnabled(!busy);
        m_save->setEnabled(!busy);
        m_save->setText(m_form->isSubmitting() ? QStringLiteral("Saving…") : QStringLiteral("Save connection"));
        if (m_form->isValidating())
            m_status->setText(QStringLiteral("Checking connection…"));
        else if (m_form->isSubmitting())
            m_status->setText(QStringLiteral("Saving connection…"));
        else if (!m_form->errorMessages().isEmpty())
            m_status->setText(m_form->errorMessages().join(QLatin1Char('\n')));
        else if (!m_form->submissionError().isEmpty())
            m_status->setText(m_form->submissionError());
        else if (m_form->isValid())
            m_status->setText(m_form->isDirty()
                    ? QStringLiteral("Ready to save · unsaved changes")
                    : QStringLiteral("Connection verified."));
        else
            m_status->setText(
                m_form->isDirty() ? QStringLiteral("Unsaved changes") : QStringLiteral("No unsaved changes"));
    }

    FormController* m_form = nullptr;
    QMap<QString, textfields::LineEdit*> m_editors;
    QVBoxLayout* m_layout = nullptr;
    QBoxLayout* m_actions = nullptr;
    textfields::Label* m_status = nullptr;
    basicinput::Button* m_test = nullptr;
    basicinput::Button* m_save = nullptr;
};

} // namespace fluent::gallery

#endif // GALLERY_CONNECTIONFORM_H
