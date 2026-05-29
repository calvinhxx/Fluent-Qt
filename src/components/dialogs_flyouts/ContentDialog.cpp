#include "ContentDialog.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/CornerRadius.h"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>

namespace fluent::dialogs_flyouts {

using Edge    = AnchorLayout::Edge;
using Anchors = AnchorLayout::Anchors;

static constexpr int kButtonBarHeight = 68;
static constexpr int kButtonMinWidth  = 96;
static constexpr int kDialogPadding   = Spacing::Padding::Dialog; // 24
static constexpr int kContentGap      = Spacing::Medium;          // 12
static constexpr int kButtonGap       = Spacing::Gap::Normal;     // 8
static constexpr int kMinDialogWidth  = 320;

// ── 构造 ─────────────────────────────────────────────────────────────────────

ContentDialog::ContentDialog(QWidget* parent) : Dialog(parent) {
    setSmokeEnabled(true);   // 模态蒙层对话框默认开启蒙层
    setDragEnabled(false);   // 模态蒙层对话框默认不可拖拽
    setupInternalLayout();
    setMinimumWidth(kMinDialogWidth + 2 * shadowSize());
    onThemeUpdated();
}

// ── 内部布局（AnchorLayout 驱动） ────────────────────────────────────────────

void ContentDialog::setupInternalLayout() {
    auto* anchorLayout = new AnchorLayout(this);

    // --- Title ---
    m_titleLabel = new fluent::textfields::Label(this);
    m_titleLabel->setFluentTypography("Subtitle");
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setVisible(false);
    m_titleLabel->anchors()->top   = {this, Edge::Top,   kDialogPadding};
    m_titleLabel->anchors()->left  = {this, Edge::Left,  kDialogPadding};
    m_titleLabel->anchors()->right = {this, Edge::Right, -kDialogPadding};
    anchorLayout->addWidget(m_titleLabel);

    // --- Button bar (底部区域，置于 contentRect 内) ---
    m_buttonBar = new QWidget(this);
    m_buttonBar->setFixedHeight(kButtonBarHeight);
    m_buttonBar->setAttribute(Qt::WA_TranslucentBackground);

    Anchors barAnchors;
    barAnchors.left   = {this, Edge::Left,   0};
    barAnchors.right  = {this, Edge::Right,  0};
    barAnchors.bottom = {this, Edge::Bottom, 0};
    anchorLayout->addAnchoredWidget(m_buttonBar, barAnchors);

    // 按钮栏内部 QHBoxLayout（等宽分配）
    // barAnchors 与 contentsRect 对齐（contentsMargins 已处理 shadow 偏移）
    const int hPad = kDialogPadding;
    const int vPad = (kButtonBarHeight - ::Spacing::ControlHeight::Standard) / 2;
    auto* btnLayout = new QHBoxLayout(m_buttonBar);
    btnLayout->setContentsMargins(hPad, vPad, hPad, vPad);
    btnLayout->setSpacing(kButtonGap);

    m_primaryBtn   = new fluent::basicinput::Button(m_buttonBar);
    m_secondaryBtn = new fluent::basicinput::Button(m_buttonBar);
    m_closeBtn     = new fluent::basicinput::Button(m_buttonBar);

    m_primaryBtn  ->setMinimumWidth(kButtonMinWidth);
    m_secondaryBtn->setMinimumWidth(kButtonMinWidth);
    m_closeBtn    ->setMinimumWidth(kButtonMinWidth);

    btnLayout->addWidget(m_primaryBtn,   0);
    btnLayout->addWidget(m_secondaryBtn, 0);
    btnLayout->addWidget(m_closeBtn,     0);
    btnLayout->addStretch(1);  // 左对齐：右侧留白

    connect(m_primaryBtn, &fluent::basicinput::Button::clicked, this, [this]() {
        emit primaryButtonClicked();
        done(ResultPrimary);
    });
    connect(m_secondaryBtn, &fluent::basicinput::Button::clicked, this, [this]() {
        emit secondaryButtonClicked();
        done(ResultSecondary);
    });
    connect(m_closeBtn, &fluent::basicinput::Button::clicked, this, [this]() {
        emit closeButtonClicked();
        done(ResultNone);
    });

    updateButtonBar();
}

// ── 属性 ─────────────────────────────────────────────────────────────────────

QString ContentDialog::title() const { return m_titleLabel->text(); }
void ContentDialog::setTitle(const QString& text) {
    m_titleLabel->setText(text);
    m_titleLabel->setVisible(!text.isEmpty());
    updateContentAnchors();
}

QString ContentDialog::primaryButtonText() const { return m_primaryBtn->text(); }
void ContentDialog::setPrimaryButtonText(const QString& text) {
    m_primaryBtn->setText(text);
    updateButtonBar();
}

QString ContentDialog::secondaryButtonText() const { return m_secondaryBtn->text(); }
void ContentDialog::setSecondaryButtonText(const QString& text) {
    m_secondaryBtn->setText(text);
    updateButtonBar();
}

QString ContentDialog::closeButtonText() const { return m_closeBtn->text(); }
void ContentDialog::setCloseButtonText(const QString& text) {
    m_closeBtn->setText(text);
    updateButtonBar();
}

int  ContentDialog::defaultButton() const { return m_defaultButton; }
void ContentDialog::setDefaultButton(int btn) {
    m_defaultButton = btn;
    updateButtonBar();
}

QWidget* ContentDialog::content() const { return m_contentWidget; }
void ContentDialog::setContent(QWidget* widget) {
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

// ── 内部刷新 ─────────────────────────────────────────────────────────────────

void ContentDialog::updateButtonBar() {
    m_primaryBtn  ->setVisible(!m_primaryBtn->text().isEmpty());
    m_secondaryBtn->setVisible(!m_secondaryBtn->text().isEmpty());
    m_closeBtn    ->setVisible(!m_closeBtn->text().isEmpty());

    // 重置风格，再给 defaultButton 应用 Accent
    m_primaryBtn  ->setFluentStyle(fluent::basicinput::Button::Standard);
    m_secondaryBtn->setFluentStyle(fluent::basicinput::Button::Standard);
    m_closeBtn    ->setFluentStyle(fluent::basicinput::Button::Standard);
    switch (m_defaultButton) {
        case Primary:   m_primaryBtn  ->setFluentStyle(fluent::basicinput::Button::Accent); break;
        case Secondary: m_secondaryBtn->setFluentStyle(fluent::basicinput::Button::Accent); break;
        case Close:     m_closeBtn    ->setFluentStyle(fluent::basicinput::Button::Accent); break;
        default: break;
    }

    // 注意：不能用 isVisible() 判断，因为 dialog 未 show() 前子控件 isVisible() 永返 false。
    // 改用 text 空判断，与 setVisible 参数等价。
    const bool anyVisible = !m_primaryBtn->text().isEmpty()
                         || !m_secondaryBtn->text().isEmpty()
                         || !m_closeBtn->text().isEmpty();
    m_buttonBar->setVisible(anyVisible);
    if (layout()) layout()->invalidate();
}

void ContentDialog::updateContentAnchors() {
    if (!m_contentWidget) return;

    Anchors a;
    // 注意：不能用 m_titleLabel->isVisible()——dialog 未 show() 前子控件
    // isVisible() 永返 false，会导致 content 错误地锚到 dialog 顶部而非 title 下方。
    const bool titleShown = !m_titleLabel->text().isEmpty();
    if (titleShown)
        a.top = {m_titleLabel, Edge::Bottom, kContentGap};
    else
        a.top = {this, Edge::Top, kDialogPadding};
    a.left   = {this, Edge::Left,  kDialogPadding};
    a.right  = {this, Edge::Right, -kDialogPadding};
    a.bottom = {m_buttonBar, Edge::Top, -kDialogPadding};

    if (auto* qp = dynamic_cast<QMLPlus*>(m_contentWidget))
        *(qp->anchors()) = a;

    if (auto* al = qobject_cast<AnchorLayout*>(layout()))
        al->addAnchoredWidget(m_contentWidget, a);
}

// ── 主题 ─────────────────────────────────────────────────────────────────────

void ContentDialog::onThemeUpdated() {
    Dialog::onThemeUpdated();
    if (m_titleLabel)  m_titleLabel->onThemeUpdated();
    if (m_primaryBtn)  m_primaryBtn->onThemeUpdated();
    if (m_secondaryBtn) m_secondaryBtn->onThemeUpdated();
    if (m_closeBtn)    m_closeBtn->onThemeUpdated();
    if (m_contentWidget) {
        if (auto* fe = dynamic_cast<FluentElement*>(m_contentWidget))
            fe->onThemeUpdated();
    }
}

// ── 绘制（两区域：bgLayer + bgCanvas） ──────────────────────────────────────

void ContentDialog::paintEvent(QPaintEvent*) {

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect(), Qt::transparent);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    const int s = shadowSize();
    const QRect contentRect = rect().adjusted(s, s, -s, -s);
    drawShadow(painter, contentRect);

    const auto& colors = themeColors();
    const int r = themeRadius().overlay;

    // 用圆角矩形做裁剪区
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    painter.setClipPath(clipPath);

    if (m_buttonBar && m_buttonBar->isVisible()) {
        const int dividerY = m_buttonBar->geometry().top();

        // 内容区（bgLayer）
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(QRect(contentRect.left(), contentRect.top(),
                               contentRect.width(), dividerY - contentRect.top()));

        // 按钮区（bgCanvas）
        painter.setBrush(colors.bgCanvas);
        painter.drawRect(QRect(contentRect.left(), dividerY,
                               contentRect.width(), contentRect.bottom() - dividerY + 1));

        // 1px 分割线
        painter.setPen(QPen(colors.strokeDivider, 1));
        painter.drawLine(contentRect.left(), dividerY, contentRect.right(), dividerY);
    } else {
        // 无按钮区域时，整块 bgLayer
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(contentRect);
    }

    painter.setClipping(false);

    // 外边框
    painter.setBrush(Qt::NoBrush);
    painter.setPen(colors.strokeDefault);
    painter.drawRoundedRect(contentRect, r, r);
}

} // namespace fluent::dialogs_flyouts
