#ifndef COMBOBOX_H
#define COMBOBOX_H

#include "compatibility/InputEventCompat.h"
#include "compatibility/QtFeatures.h"
#include <QComboBox>
#include <QPoint>
#include <QPointer>
#include <QStyledItemDelegate>
#include <memory>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"
#include "components/foundation/WidgetOwnership.h"
#include "design/Typography.h"
#include "design/Spacing.h"

class QAbstractItemView;
class QPropertyAnimation;
class QKeyEvent;
class QLineEdit;
class QWheelEvent;
namespace fluent::detail {
class FieldTrailingContent;
}

namespace fluent::textfields {
class LineEdit;
}

namespace fluent::basicinput {
namespace detail {
class ComboBoxAccessible;
}

/**
 * @brief Item delegate that renders ComboBox popup rows with Fluent metrics and theme colors.
 * zh_CN: 按 Fluent 尺寸与主题色绘制 ComboBox 下拉列表行的委托。
 */
class ComboBoxItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ComboBoxItemDelegate(FluentElement* themeHost, QAbstractItemView* view, QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private:
    FluentElement* m_themeHost = nullptr;
    QAbstractItemView* m_view = nullptr;
};

/**
 * @brief Fluent combo box with a custom popup list, optional editable input, and trailing slot.
 * zh_CN: 带自定义下拉列表、可选编辑框和尾部内容插槽的 Fluent 组合框。
 */
class ComboBox : public QComboBox, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent Typography preset used for text.
     * zh_CN: 文本使用的预设字号样式。
     */
    Q_PROPERTY(Typography::FontRole fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
    /**
     * @brief Horizontal content padding in pixels.
     * zh_CN: 内容区域水平内边距，单位为像素。
     */
    Q_PROPERTY(int contentPaddingH READ contentPaddingH WRITE setContentPaddingH NOTIFY layoutChanged)
    /**
     * @brief Vertical content padding in pixels.
     * zh_CN: 内容区域垂直内边距，单位为像素。
     */
    Q_PROPERTY(int contentPaddingV READ contentPaddingV WRITE setContentPaddingV NOTIFY layoutChanged)
    /**
     * @brief Iconfont glyph used for the chevron affordance.
     * zh_CN: 下拉箭头使用的 iconfont 字符。
     */
    Q_PROPERTY(QString chevronGlyph READ chevronGlyph WRITE setChevronGlyph NOTIFY chevronChanged)
    /**
     * @brief Chevron glyph pixel size.
     * zh_CN: 下拉箭头图标像素尺寸。
     */
    Q_PROPERTY(int chevronSize READ chevronSize WRITE setChevronSize NOTIFY chevronChanged)
    /**
     * @brief Pixel offset applied to chevron drawing.
     * zh_CN: 下拉箭头绘制时应用的像素偏移。
     */
    Q_PROPERTY(QPoint chevronOffset READ chevronOffset WRITE setChevronOffset NOTIFY chevronChanged)
    /**
     * @brief Popup offset from its anchor in pixels.
     * zh_CN: 弹层相对锚点的像素偏移。
     */
    Q_PROPERTY(int popupOffset READ popupOffset WRITE setPopupOffset NOTIFY layoutChanged)
    /**
     * @brief Animated press progress for chevron click feedback.
     * zh_CN: 按压下拉箭头时的动画进度。
     */
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)
    Q_PROPERTY(QString trailingText READ trailingText WRITE setTrailingText NOTIFY trailingTextChanged)
    Q_PROPERTY(QWidget* trailingWidget READ trailingWidget NOTIFY trailingWidgetChanged)
    Q_PROPERTY(fluent::WidgetOwnership trailingWidgetOwnership
        READ trailingWidgetOwnership
        NOTIFY trailingWidgetOwnershipChanged)

public:
    explicit ComboBox(QWidget* parent = nullptr);
    ~ComboBox() override;

    Typography::FontRole fontRole() const { return m_fontRole; }
    /**
     * @brief Applies the Fluent Typography preset and clears any explicit font override, even if unchanged.
     * zh_CN: 应用预设字号样式；即使样式未变，也会清除之前设置的自定义字体。
     */
    void setFontRole(Typography::FontRole role);
    /**
     * @brief Sets the field, editor and dropdown font independently of theme refreshes.
     * zh_CN: 设置闭合框、编辑框和下拉列表的字体，主题刷新后仍保留。
     */
    void setFont(const QFont& font);

    int contentPaddingH() const { return m_contentPaddingH; }
    void setContentPaddingH(int px);

    int contentPaddingV() const { return m_contentPaddingV; }
    void setContentPaddingV(int px);

    QString chevronGlyph() const { return m_chevronGlyph; }
    void setChevronGlyph(const QString& glyph);

    int chevronSize() const { return m_chevronSize; }
    void setChevronSize(int size);

    QPoint chevronOffset() const { return m_chevronOffset; }
    void setChevronOffset(const QPoint& offset);

    int popupOffset() const { return m_popupOffset; }
    void setPopupOffset(int offset);

    qreal pressProgress() const { return m_pressProgress; }
    void setPressProgress(qreal p);

    /**
     * @brief Enables or disables editable mode using a Fluent LineEdit.
     * zh_CN: 启用或关闭可编辑模式（默认使用 Fluent LineEdit）。
     */
    void setEditable(bool editable);

    /**
     * @brief Replaces the QComboBox editor and synchronizes Fluent layout state.
     * zh_CN: 替换 QComboBox 编辑器并同步 Fluent 布局状态。
     */
    void setLineEdit(QLineEdit* edit);

    fluent::textfields::LineEdit* fluentLineEdit() const;

    QString trailingText() const;
    /** @brief Displays a plain-text suffix in both editable and non-editable modes without changing item values.
     * zh_CN: 在只读和可编辑模式下显示纯文本尾缀，不影响选项值。 */
    void setTrailingText(const QString& text);
    QWidget* trailingWidget() const;
    WidgetOwnership trailingWidgetOwnership() const;
    /**
     * @brief Places a trailing widget beside the text or editor, replacing any suffix text.
     * Pass WidgetOwnership to specify cleanup; call takeTrailingWidget() before changing it.
     * zh_CN: 在文本或编辑区右侧放置一个尾部控件（会替换文本尾缀），并传入 WidgetOwnership 指定销毁方式；更换方式前请先调用 takeTrailingWidget()。
     */
    bool setTrailingWidget(QWidget* widget, WidgetOwnership ownership);
    /** @brief Removes and returns the trailing widget for the caller to manage or delete.
     * zh_CN: 移出并返回尾部控件，改由调用方自行销毁。 */
    QWidget* takeTrailingWidget();
    /** @brief Removes the trailing widget and handles cleanup according to its WidgetOwnership mode.
     * zh_CN: 移除尾部控件，并按其 WidgetOwnership 指定的销毁方式处理。 */
    void releaseTrailingWidget();

    QAbstractItemModel* model() const { return QComboBox::model(); }
    /** @brief Sets the item model (still managed by the caller; not deleted by this control).
     * zh_CN: 设置数据源模型（仍由应用管理，不会随控件销毁）。 */
    void setModel(QAbstractItemModel* model) FLUENT_QT6_ONLY_OVERRIDE;
    void showPopup() override;
    void hidePopup() override;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void fontRoleChanged();
    void layoutChanged();
    void chevronChanged();
    void trailingTextChanged();
    void trailingWidgetChanged();
    void trailingWidgetOwnershipChanged();

protected:
    bool event(QEvent* event) override;
    void changeEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class ComboBoxPopup;
    friend class detail::ComboBoxAccessible;
    QWidget* accessiblePopupWidget() const;
    void initAnimation();
    void onPopupHidden();
    void synchronizeLineEdit();
    QLineEdit* observedLineEdit() const;
    void layoutLineEdit();
    void applyLineEditStyle();
    void applyFontRole();
    void synchronizeFont();
    int chevronAreaWidth() const;
    QRect chevronButtonRect() const;

    // --- Configurable design tokens ---
    Typography::FontRole m_fontRole = Typography::FontRole::Body;
    bool m_hasExplicitFont = false;
    bool m_applyingFontRole = false;
    int m_autoHeight = ::Spacing::ControlHeight::Standard;
    int m_contentPaddingH = ::Spacing::Padding::TextFieldHorizontal;
    int m_contentPaddingV = ::Spacing::Padding::ComboBoxVertical;
    QString m_chevronGlyph = Typography::Icons::ChevronDownMed;
    int m_chevronSize = Typography::IconSize::Compact;
    QPoint m_chevronOffset{::Spacing::Padding::ComboBoxHorizontal, 0};
    int m_popupOffset = ::Spacing::Small; // 8px keeps the dropdown shadow clear of its anchor

    // --- State ---
    bool m_hovered = false;
    bool m_pressed = false;
    bool m_chevronHovered = false;
    bool m_popupVisible = false;
    bool m_ignoreNextPopupPress = false;
    qreal m_pressProgress = 0.0;

    QPropertyAnimation* m_pressAnimation = nullptr;

    // --- Editable ---
    QPointer<QObject> m_observedLineEdit;
    bool m_editorMutationInProgress = false;
    bool m_editorLayoutInProgress = false;
    std::unique_ptr<::fluent::detail::FieldTrailingContent> m_trailingContent;
    int m_trailingContentWidth = 0;

    // --- Popup ---
    class ComboBoxPopup;
    QPointer<ComboBoxPopup> m_popup;
};

} // namespace fluent::basicinput

#endif // COMBOBOX_H
