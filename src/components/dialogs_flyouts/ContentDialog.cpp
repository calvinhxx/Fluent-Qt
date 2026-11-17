#include "ContentDialog.h"
#include "components/basicinput/Button.h"
#include "components/foundation/QMLPlus.h"
#include "components/textfields/Label.h"
#include "design/CornerRadius.h"
#include "design/Spacing.h"
#include <QHBoxLayout>
#include <QLayout>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

namespace fluent::dialogs_flyouts {

using Edge = AnchorLayout::Edge;
using Anchors = AnchorLayout::Anchors;

static constexpr int kButtonMinWidth = 96;
static constexpr int kDialogPadding = Spacing::Padding::Dialog; // 24
static constexpr int kContentGap = Spacing::Medium;             // 12
static constexpr int kButtonGap = Spacing::Gap::Normal;         // 8
static constexpr int kMinDialogWidth = 320;

ContentDialog::ContentDialog(QWidget* parent) : Dialog(parent)
{
    setSmokeEnabled(true);
    setDragEnabled(false);
    setupInternalLayout();
    setMinimumWidth(kMinDialogWidth + 2 * shadowSize());
    onThemeUpdated();
}

void ContentDialog::setupInternalLayout()
{
    auto* anchorLayout = new AnchorLayout(this);

    m_titleLabel = new fluent::textfields::Label(this);
    m_titleLabel->setFluentTypography(Typography::FontRole::Subtitle);
    m_titleLabel->setTextColorRole(fluent::textfields::Label::TextColorRole::Primary);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setVisible(false);
    m_titleLabel->anchors()->top = {this, Edge::Top, kDialogPadding};
    m_titleLabel->anchors()->left = {this, Edge::Left, kDialogPadding};
    m_titleLabel->anchors()->right = {this, Edge::Right, -kDialogPadding};
    anchorLayout->addWidget(m_titleLabel);

    m_buttonBar = new QWidget(this);
    m_buttonBar->setFixedHeight(m_buttonBarHeight);

    Anchors barAnchors;
    barAnchors.left = {this, Edge::Left, 0};
    barAnchors.right = {this, Edge::Right, 0};
    barAnchors.bottom = {this, Edge::Bottom, 0};
    anchorLayout->addAnchoredWidget(m_buttonBar, barAnchors);

    // contentsMargins already covers the shadow offset, so barAnchors align with contentsRect.
    const int hPad = kDialogPadding;
    const int vPad = (m_buttonBarHeight - ::Spacing::ControlHeight::Standard) / 2;
    auto* btnLayout = new QHBoxLayout(m_buttonBar);
    btnLayout->setContentsMargins(hPad, vPad, hPad, vPad);
    btnLayout->setSpacing(kButtonGap);

    m_primaryBtn = new fluent::basicinput::Button(m_buttonBar);
    m_secondaryBtn = new fluent::basicinput::Button(m_buttonBar);
    m_closeBtn = new fluent::basicinput::Button(m_buttonBar);

    m_primaryBtn->setMinimumWidth(kButtonMinWidth);
    m_secondaryBtn->setMinimumWidth(kButtonMinWidth);
    m_closeBtn->setMinimumWidth(kButtonMinWidth);

    // Equal stretch factors let QBoxLayout balance visible buttons automatically.
    btnLayout->addWidget(m_primaryBtn, 1);
    btnLayout->addWidget(m_secondaryBtn, 1);
    btnLayout->addWidget(m_closeBtn, 1);

    connect(m_primaryBtn, &fluent::basicinput::Button::clicked, this, [this]() {
        QPointer<ContentDialog> guard(this);
        emit primaryButtonClicked();
        if (guard)
            guard->done(ResultPrimary);
    });
    connect(m_secondaryBtn, &fluent::basicinput::Button::clicked, this, [this]() {
        QPointer<ContentDialog> guard(this);
        emit secondaryButtonClicked();
        if (guard)
            guard->done(ResultSecondary);
    });
    connect(m_closeBtn, &fluent::basicinput::Button::clicked, this, [this]() {
        QPointer<ContentDialog> guard(this);
        emit closeButtonClicked();
        if (guard)
            guard->done(ResultNone);
    });

    updateButtonBar();
}

QString ContentDialog::title() const
{
    return m_titleLabel->text();
}
void ContentDialog::setTitle(const QString& text)
{
    if (m_titleLabel->text() == text)
        return;
    m_titleLabel->setText(text);
    m_titleLabel->setVisible(!text.isEmpty());
    updateContentAnchors();
    emit titleChanged(text);
}

QString ContentDialog::primaryButtonText() const
{
    return m_primaryBtn->text();
}
void ContentDialog::setPrimaryButtonText(const QString& text)
{
    if (m_primaryBtn->text() == text)
        return;
    m_primaryBtn->setText(text);
    updateButtonBar();
    emit primaryButtonTextChanged(text);
}

QString ContentDialog::secondaryButtonText() const
{
    return m_secondaryBtn->text();
}
void ContentDialog::setSecondaryButtonText(const QString& text)
{
    if (m_secondaryBtn->text() == text)
        return;
    m_secondaryBtn->setText(text);
    updateButtonBar();
    emit secondaryButtonTextChanged(text);
}

QString ContentDialog::closeButtonText() const
{
    return m_closeBtn->text();
}
void ContentDialog::setCloseButtonText(const QString& text)
{
    if (m_closeBtn->text() == text)
        return;
    m_closeBtn->setText(text);
    updateButtonBar();
    emit closeButtonTextChanged(text);
}

int ContentDialog::defaultButton() const
{
    return m_defaultButton;
}
void ContentDialog::setDefaultButton(int btn)
{
    if (m_defaultButton == btn)
        return;
    m_defaultButton = btn;
    updateButtonBar();
    emit defaultButtonChanged(btn);
}

QWidget* ContentDialog::content() const
{
    return m_contentWidget.data();
}
void ContentDialog::setContent(QWidget* widget)
{
    if (widget == m_contentWidget)
        return;

    if (m_contentWidget) {
        if (auto* al = qobject_cast<AnchorLayout*>(layout()))
            al->removeWidget(m_contentWidget);
        m_contentWidget->setParent(nullptr);
    }
    m_contentWidget = widget;
    if (m_contentWidget) {
        m_contentWidget->setParent(this);
        updateContentAnchors();
        m_contentWidget->show();
    }
}

void ContentDialog::setButtonBarHeight(int px)
{
    if (px <= 0 || px == m_buttonBarHeight)
        return;
    m_buttonBarHeight = px;
    if (m_buttonBar) {
        m_buttonBar->setFixedHeight(px);
        const int vPad = qMax(0, (px - ::Spacing::ControlHeight::Standard) / 2);
        if (auto* bl = qobject_cast<QHBoxLayout*>(m_buttonBar->layout()))
            bl->setContentsMargins(kDialogPadding, vPad, kDialogPadding, vPad);
    }
    updateContentAnchors();
}

void ContentDialog::updateButtonBar()
{
    m_primaryBtn->setVisible(!m_primaryBtn->text().isEmpty());
    m_secondaryBtn->setVisible(!m_secondaryBtn->text().isEmpty());
    m_closeBtn->setVisible(!m_closeBtn->text().isEmpty());

    m_primaryBtn->setFluentStyle(fluent::basicinput::Button::Standard);
    m_secondaryBtn->setFluentStyle(fluent::basicinput::Button::Standard);
    m_closeBtn->setFluentStyle(fluent::basicinput::Button::Standard);
    switch (m_defaultButton) {
    case Primary:
        m_primaryBtn->setFluentStyle(fluent::basicinput::Button::Accent);
        break;
    case Secondary:
        m_secondaryBtn->setFluentStyle(fluent::basicinput::Button::Accent);
        break;
    case Close:
        m_closeBtn->setFluentStyle(fluent::basicinput::Button::Accent);
        break;
    default:
        break;
    }

    // Check button text rather than isVisible(), which returns false before the dialog is shown.
    const bool anyVisible = !m_primaryBtn->text().isEmpty() || !m_secondaryBtn->text().isEmpty()
        || !m_closeBtn->text().isEmpty();
    m_buttonBar->setVisible(anyVisible);
    if (layout())
        layout()->invalidate();
}

void ContentDialog::updateContentAnchors()
{
    if (!m_contentWidget)
        return;

    Anchors a;
    // Check title text rather than isVisible(), which is always false before show().
    const bool titleShown = !m_titleLabel->text().isEmpty();
    if (titleShown)
        a.top = {m_titleLabel, Edge::Bottom, kContentGap};
    else
        a.top = {this, Edge::Top, kDialogPadding};
    a.left = {this, Edge::Left, kDialogPadding};
    a.right = {this, Edge::Right, -kDialogPadding};
    a.bottom = {m_buttonBar, Edge::Top, -kDialogPadding};

    if (auto* qp = dynamic_cast<QMLPlus*>(m_contentWidget.data()))
        *(qp->anchors()) = a;

    if (auto* al = qobject_cast<AnchorLayout*>(layout()))
        al->addAnchoredWidget(m_contentWidget, a);
}

void ContentDialog::ensureContentFits()
{
    if (!m_contentWidget)
        return;

    ensurePolished();
    m_contentWidget->ensurePolished();
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }

    const int contentWidth = m_contentWidget->width();
    int requiredHeight = qMax(m_contentWidget->sizeHint().height(), m_contentWidget->minimumSizeHint().height());
    if (contentWidth > 0 && m_contentWidget->hasHeightForWidth())
        requiredHeight = qMax(requiredHeight, m_contentWidget->heightForWidth(contentWidth));

    const int missingHeight = requiredHeight - m_contentWidget->height();
    if (missingHeight <= 0)
        return;

    // Grow the dialog before centering if wrapped text or custom content exceeds reserved space.
    resize(width(), height() + missingHeight);
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
}

void ContentDialog::showEvent(QShowEvent* event)
{
    ensureContentFits();
    Dialog::showEvent(event);
}

void ContentDialog::onThemeUpdated()
{
    Dialog::onThemeUpdated();
    if (m_titleLabel)
        m_titleLabel->onThemeUpdated();
    if (m_primaryBtn)
        m_primaryBtn->onThemeUpdated();
    if (m_secondaryBtn)
        m_secondaryBtn->onThemeUpdated();
    if (m_closeBtn)
        m_closeBtn->onThemeUpdated();
    if (m_contentWidget) {
        if (auto* fe = dynamic_cast<FluentElement*>(m_contentWidget.data()))
            fe->onThemeUpdated();
    }
}

void ContentDialog::paintEvent(QPaintEvent*)
{

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const int s = shadowSize();
    const QRect contentRect = rect().adjusted(s, s, -s, -s);
    drawShadow(painter, contentRect);

    const auto& colors = themeColorsRef();
    const int r = themeRadius().overlay;

    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    painter.setClipPath(clipPath);
    if (m_buttonBar && m_buttonBar->isVisible()) {
        const int dividerY = m_buttonBar->geometry().top();

        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(
            QRect(contentRect.left(), contentRect.top(), contentRect.width(), dividerY - contentRect.top()));

        painter.setBrush(colors.bgCanvas);
        painter.drawRect(QRect(contentRect.left(), dividerY, contentRect.width(), contentRect.bottom() - dividerY + 1));

        painter.setPen(QPen(colors.strokeDivider, 1));
        painter.drawLine(contentRect.left(), dividerY, contentRect.right(), dividerY);
    } else {
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(contentRect);
    }

    painter.setClipping(false);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(colors.strokeDefault);
    painter.drawRoundedRect(contentRect, r, r);
}

} // namespace fluent::dialogs_flyouts
