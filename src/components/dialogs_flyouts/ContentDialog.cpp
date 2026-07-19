#include "ContentDialog.h"
#include "components/basicinput/Button.h"
#include "components/textfields/Label.h"
#include "components/foundation/QMLPlus.h"
#include "design/Spacing.h"
#include "design/CornerRadius.h"
#include <QPainter>
#include <QPainterPath>
#include <QHBoxLayout>
#include <QLayout>

namespace fluent::dialogs_flyouts {

using Edge    = AnchorLayout::Edge;
using Anchors = AnchorLayout::Anchors;

static constexpr int kButtonBarHeight = 68;
static constexpr int kButtonMinWidth  = 96;
static constexpr int kDialogPadding   = Spacing::Padding::Dialog; // 24
static constexpr int kContentGap      = Spacing::Medium;          // 12
static constexpr int kButtonGap       = Spacing::Gap::Normal;     // 8
static constexpr int kMinDialogWidth  = 320;

// ── Construction. zh_CN: 构造 ────────────────────────────────────────────────

ContentDialog::ContentDialog(QWidget* parent) : Dialog(parent) {
    setSmokeEnabled(true);   // Modal dialogs dim the background by default. zh_CN: 模态蒙层对话框默认开启蒙层。
    setDragEnabled(false);   // Modal dialogs are not draggable by default. zh_CN: 模态蒙层对话框默认不可拖拽。
    setupInternalLayout();
    setMinimumWidth(kMinDialogWidth + 2 * shadowSize());
    onThemeUpdated();
}

// ── Internal layout (AnchorLayout driven). zh_CN: 内部布局（AnchorLayout 驱动）──

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

    // --- Button bar (bottom area inside contentRect). zh_CN: 底部按钮栏，置于 contentRect 内。---
    m_buttonBar = new QWidget(this);
    m_buttonBar->setFixedHeight(m_buttonBarHeight);
    m_buttonBar->setAttribute(Qt::WA_TranslucentBackground);

    Anchors barAnchors;
    barAnchors.left   = {this, Edge::Left,   0};
    barAnchors.right  = {this, Edge::Right,  0};
    barAnchors.bottom = {this, Edge::Bottom, 0};
    anchorLayout->addAnchoredWidget(m_buttonBar, barAnchors);

    // The bar's inner QHBoxLayout distributes buttons evenly; barAnchors align
    // with contentsRect (contentsMargins already covers the shadow offset).
    // zh_CN: 按钮栏内部 QHBoxLayout 等宽分配；barAnchors 与 contentsRect 对齐
    // （contentsMargins 已处理 shadow 偏移）。
    const int hPad = kDialogPadding;
    const int vPad = (m_buttonBarHeight - ::Spacing::ControlHeight::Standard) / 2;
    auto* btnLayout = new QHBoxLayout(m_buttonBar);
    btnLayout->setContentsMargins(hPad, vPad, hPad, vPad);
    btnLayout->setSpacing(kButtonGap);

    m_primaryBtn   = new fluent::basicinput::Button(m_buttonBar);
    m_secondaryBtn = new fluent::basicinput::Button(m_buttonBar);
    m_closeBtn     = new fluent::basicinput::Button(m_buttonBar);

    m_primaryBtn  ->setMinimumWidth(kButtonMinWidth);
    m_secondaryBtn->setMinimumWidth(kButtonMinWidth);
    m_closeBtn    ->setMinimumWidth(kButtonMinWidth);

    // WinUI action rows divide the available width between the visible
    // actions. Hidden buttons are automatically excluded by QBoxLayout, so a
    // two-action confirmation remains balanced without special-case code.
    // zh_CN: WinUI 操作栏会在可见操作之间等分可用宽度；QBoxLayout 会自动排除隐藏
    // 按钮，因此双按钮确认框无需特判也能保持均衡。
    btnLayout->addWidget(m_primaryBtn,   1);
    btnLayout->addWidget(m_secondaryBtn, 1);
    btnLayout->addWidget(m_closeBtn,     1);

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

// ── Properties. zh_CN: 属性 ──────────────────────────────────────────────────

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

void ContentDialog::setButtonBarHeight(int px) {
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

// ── Internal refresh. zh_CN: 内部刷新 ────────────────────────────────────────

void ContentDialog::updateButtonBar() {
    m_primaryBtn  ->setVisible(!m_primaryBtn->text().isEmpty());
    m_secondaryBtn->setVisible(!m_secondaryBtn->text().isEmpty());
    m_closeBtn    ->setVisible(!m_closeBtn->text().isEmpty());

    // Reset all styles, then give the default button the accent style.
    // zh_CN: 重置风格，再给 defaultButton 应用 Accent。
    m_primaryBtn  ->setFluentStyle(fluent::basicinput::Button::Standard);
    m_secondaryBtn->setFluentStyle(fluent::basicinput::Button::Standard);
    m_closeBtn    ->setFluentStyle(fluent::basicinput::Button::Standard);
    switch (m_defaultButton) {
        case Primary:   m_primaryBtn  ->setFluentStyle(fluent::basicinput::Button::Accent); break;
        case Secondary: m_secondaryBtn->setFluentStyle(fluent::basicinput::Button::Accent); break;
        case Close:     m_closeBtn    ->setFluentStyle(fluent::basicinput::Button::Accent); break;
        default: break;
    }

    // isVisible() is unusable here: before the dialog is shown every child
    // reports false. Test the text instead, which matches the setVisible call.
    // zh_CN: 不能用 isVisible() 判断——dialog 未 show() 前子控件恒为 false；
    // 改用文本空判断，与 setVisible 参数等价。
    const bool anyVisible = !m_primaryBtn->text().isEmpty()
                         || !m_secondaryBtn->text().isEmpty()
                         || !m_closeBtn->text().isEmpty();
    m_buttonBar->setVisible(anyVisible);
    if (layout()) layout()->invalidate();
}

void ContentDialog::updateContentAnchors() {
    if (!m_contentWidget) return;

    Anchors a;
    // m_titleLabel->isVisible() is unusable before show(): it reports false and
    // would anchor the content to the dialog top instead of below the title.
    // zh_CN: 不能用 m_titleLabel->isVisible()——show() 前恒为 false，会让 content
    // 错误地锚到 dialog 顶部而非 title 下方。
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

// ── Theme. zh_CN: 主题 ───────────────────────────────────────────────────────

void ContentDialog::ensureContentFits() {
    if (!m_contentWidget)
        return;

    ensurePolished();
    m_contentWidget->ensurePolished();
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }

    const int contentWidth = m_contentWidget->width();
    int requiredHeight = qMax(m_contentWidget->sizeHint().height(),
                              m_contentWidget->minimumSizeHint().height());
    if (contentWidth > 0 && m_contentWidget->hasHeightForWidth())
        requiredHeight = qMax(requiredHeight,
                              m_contentWidget->heightForWidth(contentWidth));

    const int missingHeight = requiredHeight - m_contentWidget->height();
    if (missingHeight <= 0)
        return;

    // Anchored content stretches between the title and action row. Grow the
    // surface before Dialog centers it when wrapped text or custom content
    // needs more room than the caller reserved. This absorbs platform-font
    // and fractional-DPI rounding without duplicating sizing in every app.
    // zh_CN: 锚定内容会在标题与操作栏之间拉伸；若换行文本或自定义内容所需高度
    // 超过调用方预留值，则在 Dialog 居中前扩展表面，避免平台字体或小数 DPI
    // 取整造成文字裁剪，也无需每个应用重复计算高度。
    resize(width(), height() + missingHeight);
    if (layout()) {
        layout()->invalidate();
        layout()->activate();
    }
}

void ContentDialog::showEvent(QShowEvent* event) {
    ensureContentFits();
    Dialog::showEvent(event);
}

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

// ── Painting: two regions, bgLayer + bgCanvas. zh_CN: 绘制（两区域）──────────

void ContentDialog::paintEvent(QPaintEvent*) {

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    if (!usesSameWindowSurfaceBackend()) {
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect(), Qt::transparent);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    }

    const int s = shadowSize();
    const QRect contentRect = rect().adjusted(s, s, -s, -s);
    drawShadow(painter, contentRect);

    const auto& colors = themeColors();
    const int r = themeRadius().overlay;
    const DesignLanguage lang = themeDesignLanguage();

    // Clip to the rounded card. zh_CN: 用圆角矩形做裁剪区。
    QPainterPath clipPath;
    clipPath.addRoundedRect(contentRect, r, r);
    painter.setClipPath(clipPath);

    if (lang == DesignMaterial) {
        // Material 3 dialog: a single tonal "surface-container-high" surface — NO separate button
        // region, NO divider. Elevation is conveyed by drawShadow alone (no outer stroke, applied
        // below). zh_CN: Material 3 对话框:单一色调 "surface-container-high" 表面——无独立按钮区、无分割线。
        // 高度仅由 drawShadow 表达(无外描边,见下方)。
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(contentRect);
    } else if (m_buttonBar && m_buttonBar->isVisible()) {
        const int dividerY = m_buttonBar->geometry().top();

        // Content area (bgLayer). zh_CN: 内容区。
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(QRect(contentRect.left(), contentRect.top(),
                               contentRect.width(), dividerY - contentRect.top()));

        // Button area (bgCanvas). zh_CN: 按钮区。
        painter.setBrush(colors.bgCanvas);
        painter.drawRect(QRect(contentRect.left(), dividerY,
                               contentRect.width(), contentRect.bottom() - dividerY + 1));

        // 1px divider. zh_CN: 1px 分割线。
        painter.setPen(QPen(colors.strokeDivider, 1));
        painter.drawLine(contentRect.left(), dividerY, contentRect.right(), dividerY);
    } else {
        // Without a button bar the whole card is bgLayer. zh_CN: 无按钮区域时整块 bgLayer。
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.bgLayer);
        painter.drawRect(contentRect);
    }

    painter.setClipping(false);

    // Outer border. zh_CN: 外边框。
    painter.setBrush(Qt::NoBrush);
    if (lang == DesignMaterial) {
        // M3 conveys elevation with the shadow, not a stroke. zh_CN: M3 以阴影而非描边表达高度。
        painter.setPen(Qt::NoPen);
    } else if (lang == DesignCupertino) {
        // macOS alert/sheet: a crisp 1px hairline edge using the stronger neutral stroke (the
        // two-region + divider body layout is kept). zh_CN: macOS 警告/sheet:用更强的中性描边绘制清晰
        // 的 1px 发丝边缘(保留两区域 + 分割线布局)。
        painter.setPen(QPen(colors.strokeStrong, 1));
    } else {
        // DesignFluent (default): unchanged WinUI overlay stroke. zh_CN: 默认 Fluent,WinUI 浮层描边不变。
        painter.setPen(colors.strokeDefault);
    }
    painter.drawRoundedRect(contentRect, r, r);
}

} // namespace fluent::dialogs_flyouts
