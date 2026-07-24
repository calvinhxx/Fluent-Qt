#include "TextEdit.h"
#include <QTextEdit>
#include <QScrollBar>
#include <QTextOption>
#include <QTextCursor>
#include <QTextBlockFormat>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFrame>
#include <QTextFrameFormat>
#include <QAbstractTextDocumentLayout>
#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QFocusEvent>
#include <QEvent>
#include <QFontMetrics>
#include <QIcon>
#include <QPixmap>
#include <QResizeEvent>
#include <QTimer>
#include <QTextLayout>
#include "components/menus_toolbars/Menu.h"
#include "components/scrolling/ScrollBar.h"

namespace fluent::textfields {

// ── Helpers. zh_CN: 辅助函数 ───────────────────────────────────────────────────

static int calcTopPad(const QFont& font, int lineHeight) {
    const int fontLh = QFontMetrics(font).lineSpacing();
    return qMax(0, lineHeight - fontLh) / 2;
}

static int calcBotPad(const QFont& font, int lineHeight) {
    const int fontLh = QFontMetrics(font).lineSpacing();
    return qMax(0, lineHeight - fontLh);
}

static bool formatMetricEquals(qreal lhs, qreal rhs) {
    return qAbs(lhs - rhs) < 0.01;
}

static bool matchesStandardShortcut(const QAction* action, QKeySequence::StandardKey standardKey) {
    if (!action)
        return false;

    QKeySequence shortcut = action->shortcut();
    if (shortcut.isEmpty()) {
        const QString text = action->text();
        const int tabIndex = text.indexOf(QLatin1Char('\t'));
        if (tabIndex >= 0)
            shortcut = QKeySequence(text.mid(tabIndex + 1).trimmed(), QKeySequence::NativeText);
    }

    if (shortcut.isEmpty())
        return false;

    const QList<QKeySequence> bindings = QKeySequence::keyBindings(standardKey);
    for (const QKeySequence& binding : bindings) {
        if (shortcut.matches(binding) == QKeySequence::ExactMatch)
            return true;
    }
    return false;
}

static QString standardEditingActionGlyph(const QAction* action) {
    if (matchesStandardShortcut(action, QKeySequence::Undo))
        return Typography::Icons::Undo;
    if (matchesStandardShortcut(action, QKeySequence::Redo))
        return Typography::Icons::Redo;
    if (matchesStandardShortcut(action, QKeySequence::Cut))
        return Typography::Icons::Cut;
    if (matchesStandardShortcut(action, QKeySequence::Copy))
        return Typography::Icons::Copy;
    if (matchesStandardShortcut(action, QKeySequence::Paste))
        return Typography::Icons::Paste;
    if (matchesStandardShortcut(action, QKeySequence::Delete))
        return Typography::Icons::Delete;
    if (matchesStandardShortcut(action, QKeySequence::SelectAll))
        return Typography::Icons::SelectAll;
    if (action && action->icon().name().contains(QStringLiteral("delete"), Qt::CaseInsensitive))
        return Typography::Icons::Delete;
    return QString();
}

class TextEditingContextMenu final : public fluent::menus_toolbars::FluentMenu {
public:
    explicit TextEditingContextMenu(QWidget* parent)
        : FluentMenu(QString(), parent) {
        setObjectName(QStringLiteral("FluentTextEdit.ContextMenu"));
        setFontStyle(Typography::FontRole::Caption);
    }

    void onThemeUpdated() override {
        FluentMenu::onThemeUpdated();

        // Text editing commands intentionally use denser typography than
        // application menus. Keep this policy private to TextEdit.
        // zh_CN: 文本编辑命令刻意使用比应用菜单更紧凑的排版；该策略仅属于
        // TextEdit，不下沉到 FluentMenu 公共 API。
        QFont menuFont = themeFont(Typography::FontRole::Caption).toQFont();
        menuFont.setPixelSize(10);
        setFont(menuFont);

        const auto spacing = themeSpacing();
        const int shadow = ::Spacing::Standard;
        const int verticalInset = qMax(1, spacing.gap.tight / 2);
        setContentsMargins(shadow, shadow + verticalInset,
                           shadow, shadow + verticalInset);
        setStyleSheet(QStringLiteral(
            "QMenu { background-color: transparent; border: 0px; padding: 0px; }"
            "QMenu::item { background-color: transparent; padding: %1px 0px; margin: 0px; }"
            "QMenu::separator { height: %2px; }")
            .arg(qMax(1, spacing.padding.listItemV / 2))
            .arg(spacing.gap.normal));
        setMinimumWidth(sizeHint().width());
        updateGeometry();
        update();
    }

    QIcon editingIcon(const QString& glyph) const {
        if (glyph.isEmpty())
            return QIcon();

        constexpr int iconSize = 18;
        const Colors colors = themeColors();
        const QColor activeColor = themeDesignLanguage() == DesignCupertino
            ? colors.textOnAccent
            : colors.textPrimary;

        auto pixmapForColor = [this, &glyph](const QColor& color) {
            const qreal dpr = qMax<qreal>(1.0, devicePixelRatioF());
            const int physicalSize = qMax(1, qCeil(iconSize * dpr));
            QPixmap pixmap(physicalSize, physicalSize);
            pixmap.setDevicePixelRatio(dpr);
            pixmap.fill(Qt::transparent);
            QPainter painter(&pixmap);
            painter.setPen(color);
            Typography::Icons::paintGlyph(
                painter, QRectF(0, 0, iconSize, iconSize),
                glyph, iconSize, Qt::AlignCenter);
            return pixmap;
        };

        QIcon icon;
        icon.addPixmap(pixmapForColor(colors.textPrimary), QIcon::Normal);
        icon.addPixmap(pixmapForColor(activeColor), QIcon::Active);
        icon.addPixmap(pixmapForColor(colors.textDisabled), QIcon::Disabled);
        return icon;
    }
};

// ── Inner editor. zh_CN: 内部编辑器 ────────────────────────────────────────────
//
// QTextEdit is used (not QPlainTextEdit) because QTextDocumentLayout natively
// honors QTextBlockFormat top/bottom margins plus the rootFrame margin. Each
// text line and the caret center vertically inside the lineHeight slot via:
//   - rootFrame topMargin = (lineHeight - fontLh) / 2  … space above line one
//   - per-block bottomMargin = lineHeight - fontLh     … line spacing
// Qt then handles caret placement, selection, and hit testing without a
// custom paintEvent.
// zh_CN: 使用 QTextEdit（而非 QPlainTextEdit），因为 QTextDocumentLayout 原生
// 支持 QTextBlockFormat 的上下边距及 rootFrame 边距。借助上述两条公式，每行
// 文本与光标在 lineHeight 槽内垂直居中；光标定位、选区高亮、点击映射均由 Qt
// 自动处理，无需自定义 paintEvent。

class InnerTextEdit : public QTextEdit {
public:
    explicit InnerTextEdit(TextEdit* owner, QWidget* parent = nullptr)
        : QTextEdit(parent), m_owner(owner) {
        setAcceptRichText(false);
    }

    void setContentViewportMargins(int left, int top, int right, int bottom) {
        setViewportMargins(left, top, right, bottom);
    }

protected:
    void contextMenuEvent(QContextMenuEvent* event) override {
        if (!event)
            return;

        // Keep QTextEdit's standard editing actions and their enabled state,
        // but host them in FluentMenu so the popup does not inherit this
        // editor's transparent Base/Window palette. Native QMenu renders that
        // inherited palette differently across Windows 10 and Windows 11.
        // zh_CN: 保留 QTextEdit 标准编辑动作及其启用状态，但改由 FluentMenu
        // 承载，避免原生 QMenu 继承编辑器透明的 Base/Window 调色板；Win10
        // 与 Win11 对该透明调色板的回退绘制并不一致。
        QMenu* standardMenu = createStandardContextMenu(event->pos());
        if (!standardMenu) {
            event->ignore();
            return;
        }

        TextEditingContextMenu menu(this);
        const QList<QAction*> standardActions = standardMenu->actions();
        bool pasteActionSeen = false;
        for (QAction* sourceAction : standardActions) {
            if (sourceAction->isSeparator()) {
                menu.addSeparator();
                pasteActionSeen = false;
                continue;
            }

            // Keep Qt's standard action owned by its original menu. Some Qt
            // versions dispatch Undo/Redo through that menu's action chain;
            // moving the QAction to FluentMenu silently breaks the command.
            // The visible proxy preserves metadata and forwards activation
            // while the native menu remains alive for the duration of exec().
            // zh_CN: 保留 Qt 标准动作及其原菜单所有权。部分 Qt 版本通过原菜单
            // 的动作链分发 Undo/Redo；移动 QAction 会让命令静默失效。可见代理
            // 复制元数据并转发触发，原菜单在 exec() 期间持续存活。
            auto* action = new QAction(sourceAction->icon(), sourceAction->text(), &menu);
            action->setEnabled(sourceAction->isEnabled());
            action->setCheckable(sourceAction->isCheckable());
            action->setChecked(sourceAction->isChecked());
            action->setShortcuts(sourceAction->shortcuts());
            action->setShortcutContext(sourceAction->shortcutContext());
            action->setData(sourceAction->data());
            action->setStatusTip(sourceAction->statusTip());
            action->setToolTip(sourceAction->toolTip());
            QObject::connect(action, &QAction::triggered, sourceAction,
                             [sourceAction]() { sourceAction->trigger(); });

            QString iconGlyph = standardEditingActionGlyph(sourceAction);
            if (iconGlyph.isEmpty() && pasteActionSeen) {
                // Some Qt platform styles omit both the Delete shortcut and
                // the themed icon name. In QTextEdit's standard edit group,
                // Delete is the action immediately following Paste.
                // zh_CN: 部分 Qt 平台样式同时省略 Delete 快捷键和主题图标名；
                // QTextEdit 标准编辑动作组中，Delete 固定位于 Paste 之后。
                iconGlyph = Typography::Icons::Delete;
                pasteActionSeen = false;
            } else if (iconGlyph == Typography::Icons::Paste) {
                pasteActionSeen = true;
            }
            if (!iconGlyph.isEmpty())
                action->setIcon(menu.editingIcon(iconGlyph));
            menu.addAction(action);
        }

        menu.exec(event->globalPos());
        delete standardMenu;
        event->accept();
    }

    void wheelEvent(QWheelEvent* event) override {
        if (m_owner && handleBoundaryWheel(event))
            return;
        QTextEdit::wheelEvent(event);
        if (m_owner && !m_owner->isScrollChainingEnabled() && hasScrollableRange())
            event->accept();
    }

    void paintEvent(QPaintEvent* e) override {
        QTextEdit::paintEvent(e);
        // Qt's stock placeholder ignores the rootFrame topMargin, so paint it
        // ourselves to keep it vertically centered.
        // zh_CN: Qt 原生 placeholder 不受 rootFrame topMargin 影响，自绘实现垂直居中。
        if (!m_owner || !document()->isEmpty()) return;
        const QString ph = m_owner->placeholderText();
        if (ph.isEmpty()) return;

        QPainter painter(viewport());
        const int topPad = calcTopPad(font(), m_owner->lineHeight());
        const int fontLh = QFontMetrics(font()).lineSpacing();
        QRect textRect(0, topPad, viewport()->width(), fontLh);
        painter.setPen(palette().color(QPalette::PlaceholderText));
        painter.setFont(font());
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, ph);
    }

private:
    bool hasScrollableRange() const {
        const QScrollBar* bar = verticalScrollBar();
        return bar && bar->maximum() > bar->minimum();
    }

    bool handleBoundaryWheel(QWheelEvent* event) {
        if (!event)
            return false;
        QScrollBar* bar = verticalScrollBar();
        if (!bar || bar->maximum() <= bar->minimum()) {
            event->ignore();
            return true;
        }

        const qreal scrollPx = !event->pixelDelta().isNull()
            ? static_cast<qreal>(event->pixelDelta().y())
            : static_cast<qreal>(event->angleDelta().y());
        if (qFuzzyIsNull(scrollPx))
            return false;

        const bool atTop = bar->value() <= bar->minimum();
        const bool atBottom = bar->value() >= bar->maximum();
        const bool boundaryTail = (atTop && scrollPx > 0.0) ||
                                  (atBottom && scrollPx < 0.0);
        if (!boundaryTail)
            return false;

        if (m_owner->isScrollChainingEnabled()) {
            event->ignore();
            return true;
        }

        event->accept();
        return true;
    }

    TextEdit* m_owner = nullptr;
};

// ── Construction. zh_CN: 构造 ───────────────────────────────────────────────────

TextEdit::TextEdit(QWidget* parent)
    : QWidget(parent) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);

    m_editor = new InnerTextEdit(this, this);
    m_editor->setFrameStyle(QFrame::NoFrame);
    m_editor->setBackgroundRole(QPalette::NoRole);
    m_editor->setLineWrapMode(QTextEdit::WidgetWidth);
    m_editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    m_editor->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_editor->setAutoFillBackground(false);
    setFocusPolicy(m_editor->focusPolicy());
    setFocusProxy(m_editor);

    // Remove the document's default padding; rootFrame and block margins own it.
    // zh_CN: 移除文档默认四周留白（由 rootFrame margin + block margin 全权控制）。
    m_editor->document()->setDocumentMargin(0);

    // New blocks inherit the current QTextBlockFormat, so normal typing only
    // needs a height refresh. Reapplying the frame/block format here would add
    // an invisible formatting command after every edit and make the first
    // Undo appear to do nothing.
    // zh_CN: 新 block 会继承当前 QTextBlockFormat，普通输入只需更新高度。
    // 若每次编辑后都重写 frame/block 格式，会在 undo 栈末尾加入不可见的格式
    // 命令，导致第一次 Undo 看起来没有效果。
    connect(m_editor, &QTextEdit::textChanged, this, [this]() {
        if (!m_updatingFormat)
            updateHeightForContent();
        emit textChanged();
    });
    connect(m_editor, &QTextEdit::cursorPositionChanged,
            this, &TextEdit::cursorPositionChanged);
    connect(m_editor, &QTextEdit::selectionChanged,
            this, &TextEdit::selectionChanged);
    connect(m_editor->document()->documentLayout(),
            &QAbstractTextDocumentLayout::documentSizeChanged,
            this, [this](const QSizeF&) {
                scheduleHeightForContentUpdate();
            });
    m_editor->installEventFilter(this);

    // Custom fluent scroll bar. zh_CN: 自定义 Fluent 滚动条。
    m_vScrollBar = new ::fluent::scrolling::ScrollBar(Qt::Vertical, this);
    m_vScrollBar->hide();

    auto* innerVBar = m_editor->verticalScrollBar();
    m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
    m_vScrollBar->setPageStep(innerVBar->pageStep());
    m_vScrollBar->setValue(innerVBar->value());

    connect(innerVBar, &QScrollBar::rangeChanged,
            this, [this, innerVBar](int /*min*/, int /*max*/) {
                if (!m_vScrollBar) return;
                m_vScrollBar->setRange(innerVBar->minimum(), innerVBar->maximum());
                m_vScrollBar->setPageStep(innerVBar->pageStep());
                // Scroll bar visibility is owned by updateHeightForContent.
                // zh_CN: 滚动条可见性由 updateHeightForContent 统一管理。
            });
    connect(innerVBar, &QScrollBar::valueChanged,
            this, [this](int v) {
                if (m_vScrollBar && m_vScrollBar->value() != v)
                    m_vScrollBar->setValue(v);
            });
    connect(m_vScrollBar, &QScrollBar::valueChanged,
            this, [this, innerVBar](int v) {
                if (innerVBar->value() != v)
                    innerVBar->setValue(v);
            });

    // Initial theme, line format, then height (order matters). zh_CN: 初始主题 + 行格式 + 高度（顺序重要）。
    onThemeUpdated();
    updateHeightForContent();
}

// ── Text API. zh_CN: 文本 API ───────────────────────────────────────────────────

void TextEdit::setPlainText(const QString& text) {
    if (m_editor) {
        if (m_editor->toPlainText() == text)
            return;
        m_editor->setPlainText(text);
        applyBlockCenterFormat();
        updateHeightForContent();
    }
}

QString TextEdit::toPlainText() const {
    return m_editor ? m_editor->toPlainText() : QString();
}

void TextEdit::clear() {
    if (m_editor) {
        if (m_editor->toPlainText().isEmpty())
            return;
        m_editor->clear();
        applyBlockCenterFormat();
        updateHeightForContent();
    }
}

void TextEdit::setPlaceholderText(const QString& text) {
    m_placeholderText = text;
    if (m_editor && m_editor->viewport()) m_editor->viewport()->update();
}

QString TextEdit::placeholderText() const {
    return m_placeholderText;
}

void TextEdit::setReadOnly(bool readOnly) {
    if (m_editor) m_editor->setReadOnly(readOnly);
}

bool TextEdit::isReadOnly() const {
    return m_editor ? m_editor->isReadOnly() : false;
}

::fluent::scrolling::ScrollBar* TextEdit::verticalScrollBar() const {
    return m_vScrollBar;
}

void TextEdit::setFocus() {
    if (m_editor) m_editor->setFocus(Qt::OtherFocusReason);
}

void TextEdit::setFocus(Qt::FocusReason reason) {
    if (m_editor) m_editor->setFocus(reason);
}

// ── Events. zh_CN: 事件 ─────────────────────────────────────────────────────────

void TextEdit::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    paintFrame(p);
    QWidget::paintEvent(event);
}

void TextEdit::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_editor) {
        QRect r = rect();
        int sbw = (m_vScrollBar && m_vScrollBar->isVisible()) ? m_vScrollBar->thickness() : 0;
        m_editor->setGeometry(r.adjusted(0, 0, -sbw, 0));
        if (m_vScrollBar) {
            int x = r.right() - m_vScrollBar->thickness() + 1;
            int y = r.top() + 2;
            int h = r.height() - 4;
            m_vScrollBar->setGeometry(x, y, m_vScrollBar->thickness(), h);
        }
        scheduleHeightForContentUpdate();
    }
}

void TextEdit::paintFrame(QPainter& painter) {
    const auto& colors = themeColors();
    const auto& radius = themeRadius();

    // Branch the border/focus treatment per design language (palette is already swapped by
    // ThemeRegistry; this picks the SHAPE of the field). Fluent keeps its bottom accent underline;
    // Material is an outlined rect that thickens to a 2dp accent outline on focus; Cupertino is a
    // hairline rounded rect that gains an accent focus ring. All paths paint strictly inside rect()
    // so no halo gets clipped. Mirrors LineEdit::paintFrame. zh_CN: 按设计语言分支边框/焦点处理
    // (调色板已由 ThemeRegistry 替换,此处决定字段「形状」)。Fluent 保留底部强调下划线;Material 为描边
    // 矩形,聚焦时加粗为 2dp 强调描边;Cupertino 为发丝圆角矩形,聚焦时出现强调焦点环。所有路径都严格
    // 绘制在 rect() 内,焦点光环不会被裁切。与 LineEdit::paintFrame 一致。
    const auto lang = themeDesignLanguage();

    if (lang == DesignMaterial) {
        // Material 3 OUTLINED text field: transparent fill inside a 1 dp `outline` (strokeStrong)
        // rounded rect at the control radius, thickening to a 2 dp `accentDefault` outline on focus.
        // No bottom underline here — the full outline replaces the Fluent look. zh_CN: Material 3 描边
        // 文本框:控件圆角的 1dp outline(strokeStrong)圆角矩形包裹透明填充,聚焦时加粗为 2dp accentDefault
        // 描边。此分支无底部下划线——完整描边取代 Fluent 样式。
        QColor outlineColor;
        qreal outlineWidth = 1.0;
        if (!isEnabled()) {
            outlineColor = colors.strokeDivider;
        } else if (m_isFocused) {
            outlineColor = colors.accentDefault;
            outlineWidth = 2.0;
        } else if (m_isHovered) {
            outlineColor = colors.strokeStrong;
        } else {
            outlineColor = colors.strokeStrong;
        }

        // Inset by half the stroke so a thick (2dp) focus outline stays within the widget bounds.
        // zh_CN: 按描边一半内缩,使加粗(2dp)焦点描边仍处于控件范围内。
        const qreal inset = outlineWidth / 2.0;
        QRectF outlineRect = QRectF(rect()).adjusted(inset, inset, -inset, -inset);
        const qreal mr = radius.control;
        QPainterPath framePath;
        framePath.addRoundedRect(outlineRect, mr, mr);

        painter.setBrush(Qt::NoBrush); // Outlined variant has no opaque fill. zh_CN: 描边变体无不透明填充。
        painter.setPen(QPen(outlineColor, outlineWidth));
        painter.drawPath(framePath);
        return;
    }

    if (lang == DesignCupertino) {
        // macOS field: a small-radius (~6) rounded rect with a 1 px hairline `strokeStrong` border at
        // rest. On focus it gains an accent (blue) RING — a 2 px accentDefault border plus a faint
        // outer accent glow drawn INSET so it never paints beyond rect() (which would clip).
        // Non-focused = just the hairline. zh_CN: macOS 字段:小圆角(~6)圆角矩形,静息态 1px strokeStrong
        // 发丝边框。聚焦时出现强调(蓝)环——2px accentDefault 边框 + 内缩绘制的淡淡外层强调辉光(绝不超出
        // rect(),否则被裁切)。非聚焦=仅发丝。
        const qreal cr = 6.0; // macOS regular-control radius. zh_CN: macOS 常规控件圆角。

        // Bezel fill (docs §4): restrained near-white / white@10% surface — the same opaque bezel as
        // LineEdit/ComboBox so the multi-line field reads as a raised control on any background, not a
        // transparent hole. The editor viewport is transparent so this fill shows behind the text.
        // zh_CN: bezel 填充(文档 §4):与 LineEdit/ComboBox 同款不透明 bezel;编辑器视口透明,填充透出于文字之后。
        {
            const QColor fill = !isEnabled() ? colors.controlDisabled : colors.bgLayerAlt;
            QRectF fillRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
            QPainterPath fillPath;
            fillPath.addRoundedRect(fillRect, cr, cr);
            painter.setPen(Qt::NoPen);
            painter.setBrush(fill);
            painter.drawPath(fillPath);
        }

        if (isEnabled() && m_isFocused) {
            // Faint outer glow ring first, inset 0.5px so its 2px stroke stays inside the widget.
            // zh_CN: 先画淡淡外层辉光环,内缩 0.5px 使其 2px 描边留在控件内。
            QColor glow = colors.accentDefault;
            glow.setAlpha(0x40); // ~25% — a soft halo, not a hard line. zh_CN: 约 25%——柔和光晕而非硬线。
            QRectF glowRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
            QPainterPath glowPath;
            glowPath.addRoundedRect(glowRect, cr, cr);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(glow, 2.0));
            painter.drawPath(glowPath);

            // Crisp 2px accent ring inset further so it sits inside the glow. zh_CN: 再内缩画清晰 2px 强调环,位于辉光内侧。
            QRectF ringRect = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
            QPainterPath ringPath;
            ringPath.addRoundedRect(ringRect, qMax<qreal>(0.0, cr - 1.0), qMax<qreal>(0.0, cr - 1.0));
            painter.setPen(QPen(colors.accentDefault, 2.0));
            painter.drawPath(ringPath);
            return;
        }

        // Rest / hover / disabled: just the 1px hairline. zh_CN: 静息/悬停/禁用:仅 1px 发丝边框。
        QColor hairline = !isEnabled() ? colors.strokeDivider
                                       : (m_isHovered ? colors.strokeStrong : colors.strokeDefault);
        QRectF hairRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath hairPath;
        hairPath.addRoundedRect(hairRect, cr, cr);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(hairline, 1.0));
        painter.drawPath(hairPath);
        return;
    }

    // DesignFluent (default): unchanged WinUI treatment — fill + border + bottom accent underline
    // on focus. zh_CN: 默认 Fluent:WinUI 处理不变——填充 + 边框 + 聚焦时底部强调下划线。
    QRectF bgRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bgColor, borderColor, bottomBorderColor;
    int bottomBorderWidth = m_unfocusedBorderWidth;
    if (!isEnabled()) {
        bgColor = colors.controlDisabled;
        borderColor = colors.strokeDivider;
        bottomBorderColor = borderColor;
    } else if (isReadOnly()) {
        bgColor = colors.controlAltSecondary;
        borderColor = colors.strokeDefault;
        bottomBorderColor = colors.strokeDivider;
    } else if (m_isFocused) {
        bgColor = (effectiveTheme() == Dark) ? colors.bgSolid : colors.controlDefault;
        borderColor = colors.strokeSecondary;
        bottomBorderColor = colors.accentDefault;
        bottomBorderWidth = m_focusedBorderWidth;
    } else if (m_isHovered) {
        bgColor = colors.controlSecondary;
        borderColor = colors.strokeSecondary;
        bottomBorderColor = colors.strokeSecondary;
    } else {
        bgColor = colors.controlDefault;
        borderColor = colors.strokeDefault;
        bottomBorderColor = colors.strokeDivider;
    }

    qreal r = radius.control;
    QPainterPath framePath;
    framePath.addRoundedRect(bgRect, r, r);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bgColor);
    painter.drawPath(framePath);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(borderColor, 1));
    painter.drawPath(framePath);

    if (isEnabled() && !isReadOnly()) {
        QPen pen(bottomBorderColor, bottomBorderWidth);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        qreal bottomY = bgRect.bottom() - (bottomBorderWidth > 1 ? (bottomBorderWidth - 1) / 2.0 : 0);
        QPainterPath bottomPath;
        bottomPath.moveTo(bgRect.left() + r, bottomY);
        bottomPath.lineTo(bgRect.right() - r, bottomY);
        painter.drawPath(bottomPath);
    }
}

void TextEdit::enterEvent(FluentEnterEvent* event) {
    m_isHovered = true; update(); QWidget::enterEvent(event);
}

void TextEdit::leaveEvent(QEvent* event) {
    m_isHovered = false; update(); QWidget::leaveEvent(event);
}

void TextEdit::focusInEvent(QFocusEvent* event) {
    m_isFocused = true; update(); QWidget::focusInEvent(event);
}

void TextEdit::focusOutEvent(QFocusEvent* event) {
    m_isFocused = false; update(); QWidget::focusOutEvent(event);
}

bool TextEdit::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_editor) {
        if (event->type() == QEvent::FocusIn) {
            m_isFocused = true; update();
        } else if (event->type() == QEvent::FocusOut) {
            m_isFocused = false; update();
        }
    }
    return QWidget::eventFilter(obj, event);
}

// ── Property setters. zh_CN: 属性 setter ────────────────────────────────────────

void TextEdit::setContentMargins(const QMargins& margins) {
    if (m_contentMargins == margins) return;
    m_contentMargins = margins;
    applyThemeStyle();
    emit contentMarginsChanged();
}

void TextEdit::setFontRole(const QString& role) {
    if (m_fontRole == role) return;
    m_fontRole = role;
    applyThemeStyle();
    if (m_editor && m_editor->viewport())
        m_editor->viewport()->update();
    emit fontRoleChanged();
}

void TextEdit::setFocusedBorderWidth(int width) {
    if (m_focusedBorderWidth == width) return;
    m_focusedBorderWidth = width;
    update();
    emit focusedBorderWidthChanged();
}

void TextEdit::setUnfocusedBorderWidth(int width) {
    if (m_unfocusedBorderWidth == width) return;
    m_unfocusedBorderWidth = width;
    update();
    emit unfocusedBorderWidthChanged();
}

void TextEdit::setLineHeight(int height) {
    if (height <= 0 || m_lineHeight == height) return;
    m_lineHeight = height;
    applyBlockCenterFormat();
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setMinVisibleLines(int lines) {
    if (lines <= 0)
        return;
    const bool maxChanged = m_maxVisibleLines < lines;
    if (m_minVisibleLines == lines && !maxChanged)
        return;
    m_minVisibleLines = lines;
    if (maxChanged)
        m_maxVisibleLines = lines;
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setMaxVisibleLines(int lines) {
    if (lines <= 0)
        return;
    const bool minChanged = m_minVisibleLines > lines;
    if (m_maxVisibleLines == lines && !minChanged)
        return;
    m_maxVisibleLines = lines;
    if (minChanged)
        m_minVisibleLines = lines;
    updateHeightForContent();
    emit layoutMetricsChanged();
}

void TextEdit::setScrollChainingEnabled(bool enabled) {
    if (m_scrollChainingEnabled == enabled) return;
    m_scrollChainingEnabled = enabled;
    emit scrollChainingEnabledChanged();
}

void TextEdit::onThemeUpdated() {
    applyThemeStyle();
}

// ── Core internals. zh_CN: 核心私有方法 ─────────────────────────────────────────

void TextEdit::applyThemeStyle() {
    if (!m_editor) return;

    const auto& c = themeColors();
    QPalette pal = palette();
    pal.setColor(QPalette::Base,             Qt::transparent);
    pal.setColor(QPalette::Window,           Qt::transparent);
    pal.setColor(QPalette::Text,             c.textPrimary);
    pal.setColor(QPalette::PlaceholderText,  c.textSecondary);
    pal.setColor(QPalette::Highlight,        c.accentDefault);
    pal.setColor(QPalette::HighlightedText,  c.textOnAccent);
    pal.setColor(QPalette::Inactive, QPalette::Highlight,        c.accentDefault);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText,  c.textOnAccent);
    pal.setColor(QPalette::Disabled, QPalette::Text,             c.textDisabled);
    pal.setColor(QPalette::Disabled, QPalette::PlaceholderText,  c.textDisabled);
    m_editor->setPalette(pal);
    m_editor->setFont(themeFont(m_fontRole).toQFont());

    QString qss = QString(
                    "QTextEdit { "
                    "background: transparent; "
                    "color: %1; "
                    "selection-background-color: %2; "
                    "selection-color: %3; "
                    "border: none; }")
                    .arg(c.textPrimary.name(QColor::HexArgb))
                    .arg(c.accentDefault.name(QColor::HexArgb))
                    .arg(c.textOnAccent.name(QColor::HexArgb));
    m_editor->setStyleSheet(qss);

    if (auto* vp = m_editor->viewport()) {
        vp->setAutoFillBackground(false);
        QPalette vpal = vp->palette();
        vpal.setColor(QPalette::Base,   Qt::transparent);
        vpal.setColor(QPalette::Window, Qt::transparent);
        vp->setPalette(vpal);
        vp->setStyleSheet("background: transparent; border: none;");
    }

    // Recompute centering margins after font changes (fontLineSpacing may differ).
    // zh_CN: 字体变更后重算居中 margin（fontLineSpacing 可能不同）。
    applyBlockCenterFormat();
}

void TextEdit::applyBlockCenterFormat() {
    if (!m_editor) return;

    m_updatingFormat = true;

    const QFont f = m_editor->font();
    const int topPad = calcTopPad(f, m_lineHeight);
    const int botPad = calcBotPad(f, m_lineHeight);

    // 1. rootFrame topMargin: space above line one for vertical centering.
    // zh_CN: rootFrame topMargin — 首行上方留白，实现垂直居中。
    QTextFrame* rootFrame = m_editor->document()->rootFrame();
    QTextFrameFormat rff = rootFrame->frameFormat();
    const bool frameFormatChanged =
        !formatMetricEquals(rff.topMargin(), topPad)
        || !formatMetricEquals(rff.bottomMargin(), 0)
        || !formatMetricEquals(rff.leftMargin(), 0)
        || !formatMetricEquals(rff.rightMargin(), 0);
    if (frameFormatChanged) {
        rff.setTopMargin(topPad);
        rff.setBottomMargin(0);
        rff.setLeftMargin(0);
        rff.setRightMargin(0);
        rootFrame->setFrameFormat(rff);
    }

    // 2. LineDistanceHeight: each visual line = fontLh + botPad = lineHeight.
    //    bottomMargin is avoided; combined with LineDistanceHeight it would
    //    double the line spacing.
    // zh_CN: LineDistanceHeight — 每个视觉行高 = fontLh + botPad = lineHeight；
    // 不使用 bottomMargin，否则与 LineDistanceHeight 重叠导致行间距翻倍。
    QTextBlockFormat fmt;
    fmt.setLineHeight(botPad, QTextBlockFormat::LineDistanceHeight);
    fmt.setBottomMargin(0);

    // QTextCursor formatting participates in QTextDocument's undo stack even
    // when the merged values are identical. Avoid a no-op merge so theme and
    // palette refreshes never consume the user's next Undo command.
    // zh_CN: QTextCursor 的格式操作即使数值未变化也会进入撤销栈；先判断现有
    // block 格式，避免主题/调色板刷新吞掉用户下一次 Undo。
    bool blockFormatChanged = false;
    for (QTextBlock block = m_editor->document()->begin();
         block.isValid();
         block = block.next()) {
        const QTextBlockFormat current = block.blockFormat();
        if (current.lineHeightType() != fmt.lineHeightType()
            || !formatMetricEquals(current.lineHeight(), fmt.lineHeight())
            || !formatMetricEquals(current.bottomMargin(), fmt.bottomMargin())) {
            blockFormatChanged = true;
            break;
        }
    }

    if (blockFormatChanged) {
        QTextCursor cursor(m_editor->document());
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
        cursor.mergeBlockFormat(fmt);
    }

    // 3. Left/right viewport margins. zh_CN: 左右 viewport margins。
    static_cast<InnerTextEdit*>(m_editor)->setContentViewportMargins(
        m_contentMargins.left(), 0, m_contentMargins.right(), 0);

    m_updatingFormat = false;
}

void TextEdit::scheduleHeightForContentUpdate() {
    if (m_heightUpdateScheduled)
        return;
    m_heightUpdateScheduled = true;
    QTimer::singleShot(0, this, [this]() {
        m_heightUpdateScheduled = false;
        updateHeightForContent();
    });
}

void TextEdit::updateHeightForContent() {
    if (!m_editor || m_updatingHeight)
        return;
    m_updatingHeight = true;

    // Asking the document layout for its size flushes any pending width-driven
    // wrap recalculation before individual QTextLayout line counts are read.
    // zh_CN: 读取各 QTextLayout 行数前先查询文档尺寸，刷新宽度变化引起的
    // 待处理换行布局。
    m_editor->document()->documentLayout()->documentSize();

    // Count all visual lines, including wrap-generated ones. zh_CN: 统计所有可视行数（包括自动换行产生的行）。
    int visualLines = 0;
    QTextBlock block = m_editor->document()->begin();
    while (block.isValid()) {
        int lc = block.layout()->lineCount();
        visualLines += (lc > 0) ? lc : 1;
        block = block.next();
    }
    if (visualLines < 1) visualLines = 1;

    const int minimumLines = qMax(1, m_minVisibleLines);
    const int maximumLines = qMax(minimumLines, m_maxVisibleLines);
    const int clamped = qBound(minimumLines, visualLines, maximumLines);

    // height = clampedLines × lineHeight
    const int targetHeight = clamped * m_lineHeight;
    if (minimumHeight() != targetHeight || maximumHeight() != targetHeight)
        setFixedHeight(targetHeight);

    // The scroll bar only appears once content exceeds maxVisibleLines.
    // zh_CN: 滚动条仅在内容实际超过 maxVisibleLines 时显示。
    m_scrollEnabled = (visualLines > maximumLines);
    if (m_vScrollBar)
        m_vScrollBar->setVisible(m_scrollEnabled);
    // Reset the inner scroll position when not needed to avoid content drift.
    // zh_CN: 无需滚动时重置内部滚动位置，避免内容偏移。
    if (!m_scrollEnabled && m_editor)
        m_editor->verticalScrollBar()->setValue(0);

    m_updatingHeight = false;
    updateGeometry();
}

} // namespace fluent::textfields
