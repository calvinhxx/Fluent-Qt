#include "components/layout/Field.h"

#include <QHBoxLayout>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

#include "components/foundation/FontIcon.h"
#include "components/textfields/Label.h"
#include "design/Typography.h"

namespace fluent::layout {
namespace {

QString colorCss(const QColor& color)
{
    return QStringLiteral("color: rgba(%1, %2, %3, %4);")
        .arg(color.red())
        .arg(color.green())
        .arg(color.blue())
        .arg(color.alpha());
}

} // namespace

Field::Field(QWidget* parent)
    : QWidget(parent)
{
    qRegisterMetaType<ValidationState>("fluent::layout::Field::ValidationState");

    setObjectName(QStringLiteral("fluentField"));
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // Field is presentation chrome, not an input target. The hosted editor
    // remains the only Tab stop and receives explicit focus through the proxy.
    // zh_CN: Field 只是展示外壳，不是输入目标；Tab 焦点由 editor 自己拥有，
    // 显式聚焦 Field 时再通过 focusProxy 转交。
    setFocusPolicy(Qt::NoFocus);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(themeSpacing().gap.tight);

    m_captionRow = new QWidget(this);
    m_captionRow->setObjectName(QStringLiteral("fluentFieldCaptionRow"));
    auto* captionLayout = new QHBoxLayout(m_captionRow);
    captionLayout->setContentsMargins(0, 0, 0, 0);
    captionLayout->setSpacing(themeSpacing().gap.tight);

    m_caption = new textfields::Label(m_captionRow);
    m_caption->setObjectName(QStringLiteral("fluentFieldCaption"));
    m_caption->setFluentTypography(Typography::FontRole::Body);
    m_caption->setTextColorRole(textfields::Label::TextColorRole::Primary);
    m_caption->setWordWrap(true);
    m_caption->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    m_requiredMark = new textfields::Label(QStringLiteral("*"), m_captionRow);
    m_requiredMark->setObjectName(QStringLiteral("fluentFieldRequired"));
    m_requiredMark->setFluentTypography(Typography::FontRole::Body);
    m_requiredMark->hide();

    captionLayout->addWidget(m_caption, 0, Qt::AlignBaseline);
    captionLayout->addWidget(m_requiredMark, 0, Qt::AlignBaseline);
    captionLayout->addStretch(1);

    m_editorHost = new QWidget(this);
    m_editorHost->setObjectName(QStringLiteral("fluentFieldEditorHost"));
    m_editorHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* hostLayout = new QVBoxLayout(m_editorHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    hostLayout->setSpacing(0);

    m_helper = new textfields::Label(this);
    m_helper->setObjectName(QStringLiteral("fluentFieldHelper"));
    m_helper->setFluentTypography(Typography::FontRole::Caption);
    m_helper->setTextColorRole(textfields::Label::TextColorRole::Secondary);
    m_helper->setWordWrap(true);
    m_helper->hide();

    m_statusRow = new QWidget(this);
    m_statusRow->setObjectName(QStringLiteral("fluentFieldStatusRow"));
    auto* statusLayout = new QHBoxLayout(m_statusRow);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(themeSpacing().gap.tight);

    m_statusIcon = new FontIcon(m_statusRow);
    m_statusIcon->setObjectName(QStringLiteral("fluentFieldStatusIcon"));
    m_statusIcon->setIconSize(Typography::IconSize::Compact);
    m_statusIcon->setFixedSize(
        Typography::LineHeight::Caption,
        Typography::LineHeight::Caption);
    m_statusIcon->hide();

    m_status = new textfields::Label(m_statusRow);
    m_status->setObjectName(QStringLiteral("fluentFieldStatus"));
    m_status->setFluentTypography(Typography::FontRole::Caption);
    m_status->setWordWrap(true);
    statusLayout->addWidget(m_statusIcon, 0, Qt::AlignTop);
    statusLayout->addWidget(m_status, 1, Qt::AlignTop);
    m_statusRow->hide();

    root->addWidget(m_captionRow);
    root->addWidget(m_editorHost);
    root->addWidget(m_helper);
    root->addWidget(m_statusRow);

    updateCaption();
    updateHelperAndStatus();
    updateAccessible();
}

Field::~Field()
{
    releaseEditorInternal(false, true);
}

void Field::setLabelText(const QString& text)
{
    if (m_labelText == text)
        return;

    m_labelText = text;
    updateCaption();
    updateAccessible();
    emit labelTextChanged(m_labelText);
}

void Field::setRequired(bool required)
{
    if (m_required == required)
        return;

    m_required = required;
    updateCaption();
    updateAccessible();
    emit requiredChanged(m_required);
}

void Field::setHelperText(const QString& text)
{
    if (m_helperText == text)
        return;

    m_helperText = text;
    updateHelperAndStatus();
    updateAccessible();
    emit helperTextChanged(m_helperText);
}

void Field::setValidationMessage(const QString& text)
{
    if (m_validationMessage == text)
        return;

    m_validationMessage = text;
    updateHelperAndStatus();
    updateAccessible();
    emit validationMessageChanged(m_validationMessage);
}

void Field::setValidationState(ValidationState state)
{
    if (m_validationState == state)
        return;

    m_validationState = state;
    applyStatusColor();
    emit validationStateChanged(m_validationState);
}

void Field::setEditor(QWidget* widget)
{
    setEditor(widget, WidgetOwnership::Borrowed);
}

bool Field::setEditor(QWidget* widget, WidgetOwnership ownership)
{
    if (widget == this || (widget && widget->isAncestorOf(this)))
        return false;

    if (m_editor == widget) {
        // Changing lifetime policy in place is ambiguous for language
        // bindings. Require an explicit transfer boundary first.
        // zh_CN: 原地切换生命周期策略会让语言绑定的所有权不明确；
        // 必须先 takeEditor()，再按新策略装回。
        return !widget || m_editorOwnership == ownership;
    }

    releaseEditorInternal(true, true);
    m_editor = widget;
    m_editorOriginalParent = widget ? widget->parentWidget() : nullptr;
    m_editorOriginalAccessibleName = widget ? widget->accessibleName() : QString();
    m_editorManagedAccessibleName = m_editorOriginalAccessibleName;
    m_editorAccessibleNameManaged =
        widget && m_editorOriginalAccessibleName.isEmpty();
    const WidgetOwnership previousOwnership = m_editorOwnership;
    m_editorOwnership = widget ? ownership : WidgetOwnership::Borrowed;

    if (widget) {
        widget->setParent(m_editorHost);
        if (auto* hostLayout = m_editorHost->layout())
            hostLayout->addWidget(widget);
        widget->show();
        m_editorDestroyedConnection = connect(
            widget, &QObject::destroyed, this, [this]() {
                handleEditorDestroyed();
            });
        setFocusProxy(widget);
        m_caption->setBuddy(widget);
    } else {
        setFocusProxy(nullptr);
        m_caption->setBuddy(nullptr);
    }

    updateAccessible();
    emit editorChanged(m_editor.data());
    if (previousOwnership != m_editorOwnership)
        emit editorOwnershipChanged(m_editorOwnership);
    return true;
}

QWidget* Field::takeEditor()
{
    QWidget* editor = m_editor.data();
    if (!editor)
        return nullptr;

    QObject::disconnect(m_editorDestroyedConnection);
    m_editorDestroyedConnection = QMetaObject::Connection();
    restoreEditorAccessibleName(editor);
    m_editor = nullptr;
    m_editorOriginalParent = nullptr;
    const WidgetOwnership previousOwnership = m_editorOwnership;
    m_editorOwnership = WidgetOwnership::Borrowed;
    setFocusProxy(nullptr);
    m_caption->setBuddy(nullptr);
    editor->setParent(nullptr);
    updateAccessible();
    emit editorChanged(nullptr);
    if (previousOwnership != m_editorOwnership)
        emit editorOwnershipChanged(m_editorOwnership);
    return editor;
}

void Field::releaseEditor()
{
    if (!m_editor)
        return;

    const WidgetOwnership previousOwnership = m_editorOwnership;
    releaseEditorInternal(true, true);
    updateAccessible();
    emit editorChanged(nullptr);
    if (previousOwnership != m_editorOwnership)
        emit editorOwnershipChanged(m_editorOwnership);
}

void Field::onThemeUpdated()
{
    if (auto* root = layout())
        root->setSpacing(themeSpacing().gap.tight);
    if (m_captionRow) {
        if (auto* captionLayout = m_captionRow->layout())
            captionLayout->setSpacing(themeSpacing().gap.tight);
    }
    if (m_statusRow) {
        if (auto* statusLayout = m_statusRow->layout())
            statusLayout->setSpacing(themeSpacing().gap.tight);
    }
    applyStatusColor();
    update();
}

void Field::releaseEditorInternal(bool deleteOwned, bool restoreParent)
{
    QWidget* editor = m_editor.data();
    if (!editor)
        return;

    QObject::disconnect(m_editorDestroyedConnection);
    m_editorDestroyedConnection = QMetaObject::Connection();
    restoreEditorAccessibleName(editor);
    m_editor = nullptr;
    QWidget* originalParent = m_editorOriginalParent.data();
    m_editorOriginalParent = nullptr;
    const WidgetOwnership ownership = m_editorOwnership;
    m_editorOwnership = WidgetOwnership::Borrowed;
    setFocusProxy(nullptr);
    m_caption->setBuddy(nullptr);

    if (ownership == WidgetOwnership::Owned) {
        if (deleteOwned)
            delete editor;
        return;
    }

    if (!restoreParent)
        return;

    if (ownership == WidgetOwnership::Reparented)
        editor->setParent(originalParent);
    else
        editor->setParent(nullptr);
}

void Field::handleEditorDestroyed()
{
    m_editorDestroyedConnection = QMetaObject::Connection();
    m_editor = nullptr;
    m_editorOriginalParent = nullptr;
    m_editorOriginalAccessibleName.clear();
    m_editorManagedAccessibleName.clear();
    m_editorAccessibleNameManaged = false;
    const WidgetOwnership previousOwnership = m_editorOwnership;
    m_editorOwnership = WidgetOwnership::Borrowed;
    setFocusProxy(nullptr);
    m_caption->setBuddy(nullptr);
    updateAccessible();
    emit editorChanged(nullptr);
    if (previousOwnership != m_editorOwnership)
        emit editorOwnershipChanged(m_editorOwnership);
}

void Field::updateCaption()
{
    m_caption->setText(m_labelText);
    m_caption->setVisible(!m_labelText.isEmpty());
    m_requiredMark->setVisible(m_required && !m_labelText.isEmpty());
    m_captionRow->setVisible(!m_labelText.isEmpty());
    applyStatusColor();
}

void Field::updateHelperAndStatus()
{
    m_helper->setText(m_helperText);
    m_helper->setVisible(!m_helperText.isEmpty());
    m_status->setText(m_validationMessage);
    const bool hasStatus = !m_validationMessage.isEmpty();
    m_status->setVisible(hasStatus);
    m_statusRow->setVisible(hasStatus);
    applyStatusColor();
}

void Field::applyStatusColor()
{
    m_requiredMark->setStyleSheet(colorCss(themeColors().systemCritical));
    const bool showIcon = !m_validationMessage.isEmpty()
        && m_validationState != ValidationState::None;
    m_statusIcon->setGlyph(showIcon ? statusGlyph() : QString());
    m_statusIcon->setColor(statusColor());
    m_statusIcon->setVisible(showIcon);
    m_status->setStyleSheet(colorCss(statusColor()));
}

void Field::updateAccessible()
{
    setAccessibleName(m_labelText);
    QStringList parts;
    if (!m_helperText.isEmpty())
        parts.append(m_helperText);
    if (!m_validationMessage.isEmpty())
        parts.append(m_validationMessage);
    setAccessibleDescription(parts.join(QLatin1Char('\n')));

    if (!m_editor || !m_editorAccessibleNameManaged)
        return;

    if (m_editor->accessibleName() != m_editorManagedAccessibleName) {
        // The caller replaced Field's generated name while the editor was
        // hosted. Stop managing it so later label changes and release do not
        // overwrite the caller's accessibility contract.
        m_editorAccessibleNameManaged = false;
        m_editorOriginalAccessibleName.clear();
        m_editorManagedAccessibleName.clear();
        return;
    }

    m_editor->setAccessibleName(m_labelText);
    m_editorManagedAccessibleName = m_labelText;
}

void Field::restoreEditorAccessibleName(QWidget* editor)
{
    if (editor && m_editorAccessibleNameManaged
        && editor->accessibleName() == m_editorManagedAccessibleName) {
        editor->setAccessibleName(m_editorOriginalAccessibleName);
    }
    m_editorOriginalAccessibleName.clear();
    m_editorManagedAccessibleName.clear();
    m_editorAccessibleNameManaged = false;
}

QColor Field::statusColor() const
{
    const Colors& colors = themeColorsRef();
    switch (m_validationState) {
    case ValidationState::Error:
        return colors.systemCritical;
    case ValidationState::Warning:
        return colors.systemCaution;
    case ValidationState::Success:
        return colors.systemSuccess;
    case ValidationState::None:
        break;
    }
    return colors.textSecondary;
}

QString Field::statusGlyph() const
{
    switch (m_validationState) {
    case ValidationState::Error:
        return Typography::Icons::ErrorBadge12;
    case ValidationState::Warning:
        return Typography::Icons::ImportantBadge12;
    case ValidationState::Success:
        return Typography::Icons::CheckmarkBadge12;
    case ValidationState::None:
        break;
    }
    return QString();
}

} // namespace fluent::layout
