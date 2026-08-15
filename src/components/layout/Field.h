#ifndef FLUENTQT_COMPONENTS_LAYOUT_FIELD_H
#define FLUENTQT_COMPONENTS_LAYOUT_FIELD_H

#include <QColor>
#include <QMetaObject>
#include <QMetaType>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"

namespace fluent::textfields {
class Label;
}

namespace fluent {
class FontIcon;
}

namespace fluent::layout {

/**
 * @brief Composition shell that labels an existing editor and presents helper
 *        or validation text without owning the editor value.
 * zh_CN: 为已有编辑器提供标签、帮助/校验展示的组合外壳，不接管编辑器的值。
 *
 * Field hosts a caller-supplied editor with `WidgetOwnership`. It does not
 * replace LineEdit, PasswordBox, NumberBox, ComboBox, or TextEdit, and it
 * never writes the editor's text or value as part of validation presentation.
 * zh_CN: Field 用 `WidgetOwnership` 承载调用方提供的编辑器。它不替代各类输入控件，
 * 也不会在展示校验状态时改写编辑器的文本或值。
 */
class Field : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText
                   NOTIFY labelTextChanged)
    Q_PROPERTY(bool required READ isRequired WRITE setRequired
                   NOTIFY requiredChanged)
    Q_PROPERTY(QString helperText READ helperText WRITE setHelperText
                   NOTIFY helperTextChanged)
    Q_PROPERTY(QString validationMessage READ validationMessage
                   WRITE setValidationMessage NOTIFY validationMessageChanged)
    Q_PROPERTY(ValidationState validationState READ validationState
                   WRITE setValidationState NOTIFY validationStateChanged)
    Q_PROPERTY(QWidget* editor READ editor WRITE setEditor
                   NOTIFY editorChanged)

public:
    /**
     * @brief Presentation of the current validation message.
     * zh_CN: 当前校验文案的展示状态。
     *
     * Field only paints this state. Callers own actual validation and editor
     * values.
     * zh_CN: Field 只负责展示该状态，真正的校验与编辑器值仍由调用方拥有。
     */
    enum class ValidationState {
        None,
        Error,
        Warning,
        Success
    };
    Q_ENUM(ValidationState)

    explicit Field(QWidget* parent = nullptr);
    ~Field() override;

    QString labelText() const { return m_labelText; }
    void setLabelText(const QString& text);

    bool isRequired() const { return m_required; }
    void setRequired(bool required);

    QString helperText() const { return m_helperText; }
    void setHelperText(const QString& text);

    QString validationMessage() const { return m_validationMessage; }
    void setValidationMessage(const QString& text);

    ValidationState validationState() const { return m_validationState; }
    void setValidationState(ValidationState state);

    QWidget* editor() const { return m_editor.data(); }
    void setEditor(QWidget* widget);
    bool setEditor(QWidget* widget, WidgetOwnership ownership);
    QWidget* takeEditor();
    void releaseEditor();
    WidgetOwnership editorOwnership() const { return m_editorOwnership; }

    void onThemeUpdated() override;

signals:
    void labelTextChanged(const QString& text);
    void requiredChanged(bool required);
    void helperTextChanged(const QString& text);
    void validationMessageChanged(const QString& text);
    void validationStateChanged(ValidationState state);
    void editorChanged(QWidget* widget);
    void editorOwnershipChanged(WidgetOwnership ownership);

private:
    void releaseEditorInternal(bool deleteOwned, bool restoreParent);
    void handleEditorDestroyed();
    void updateCaption();
    void updateHelperAndStatus();
    void applyStatusColor();
    void updateAccessible();
    void restoreEditorAccessibleName(QWidget* editor);
    QColor statusColor() const;
    QString statusGlyph() const;

    QString m_labelText;
    QString m_helperText;
    QString m_validationMessage;
    bool m_required = false;
    ValidationState m_validationState = ValidationState::None;

    QWidget* m_captionRow = nullptr;
    textfields::Label* m_caption = nullptr;
    textfields::Label* m_requiredMark = nullptr;
    QWidget* m_editorHost = nullptr;
    textfields::Label* m_helper = nullptr;
    QWidget* m_statusRow = nullptr;
    FontIcon* m_statusIcon = nullptr;
    textfields::Label* m_status = nullptr;

    QPointer<QWidget> m_editor;
    QPointer<QWidget> m_editorOriginalParent;
    WidgetOwnership m_editorOwnership = WidgetOwnership::Borrowed;
    QMetaObject::Connection m_editorDestroyedConnection;
    QString m_editorOriginalAccessibleName;
    QString m_editorManagedAccessibleName;
    bool m_editorAccessibleNameManaged = false;
};

} // namespace fluent::layout

Q_DECLARE_METATYPE(fluent::layout::Field::ValidationState)

#endif // FLUENTQT_COMPONENTS_LAYOUT_FIELD_H
