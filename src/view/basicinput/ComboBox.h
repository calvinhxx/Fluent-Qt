#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>
#include <QPoint>
#include <QStyledItemDelegate>
#include "view/foundation/FluentElement.h"
#include "view/foundation/QMLPlus.h"
#include "view/dialogs_flyouts/Flyout.h"
#include "design/Typography.h"
#include "design/Spacing.h"

class QPropertyAnimation;

namespace view::collections {
class ListView;
}
namespace view::textfields { class LineEdit; }

namespace view::basicinput {

// ─── ComboBox 弹层代理 ──────────────────────────────────────────────────────

/**
 * @brief Delegate that paints ComboBox popup rows with Fluent metrics.
 * zh_CN: 使用 Fluent 尺寸和颜色绘制 ComboBox 弹层行的 delegate。
 *
 * ComboBoxItemDelegate keeps popup row rendering aligned with the owning
 * ComboBox theme host instead of relying on the platform item delegate.
 * zh_CN: ComboBoxItemDelegate 通过所属 ComboBox 的主题宿主对齐弹层行渲染，
 * 避免依赖平台默认 item delegate。
 */
class ComboBoxItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ComboBoxItemDelegate(FluentElement* themeHost, QAbstractItemView* view,
                                  QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    FluentElement* m_themeHost = nullptr;
    QAbstractItemView* m_view = nullptr;
};

/**
 * @brief Fluent combo box with custom popup and token-driven button surface.
 * zh_CN: 使用自定义弹层和 token 驱动按钮表面的 Fluent 组合框。
 *
 * ComboBox keeps QComboBox model semantics while replacing the closed surface,
 * chevron affordance, editable line edit, and popup chrome with repository controls.
 * zh_CN: ComboBox 保留 QComboBox 的 model 语义，同时用项目控件接管闭合表面、
 * 下拉箭头、可编辑输入框和弹层外观。
 */
class ComboBox : public QComboBox, public FluentElement, public QMLPlus {
    Q_OBJECT
    /**
     * @brief Fluent typography role used for text rendering.
     * zh_CN: 文本绘制使用的 Fluent 排版角色。
     */
    Q_PROPERTY(QString fontRole READ fontRole WRITE setFontRole NOTIFY fontRoleChanged)
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
     * @brief Animated press progress used by the painted surface.
     * zh_CN: 自绘表面使用的按压动画进度。
     */
    Q_PROPERTY(qreal pressProgress READ pressProgress WRITE setPressProgress)

public:
    explicit ComboBox(QWidget* parent = nullptr);
    ~ComboBox() override;

    // --- Appearance ---
    QString fontRole() const { return m_fontRole; }
    void setFontRole(const QString& role);

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

    // --- Editable ---
    void setEditable(bool editable);

    // --- QComboBox overrides ---
    void showPopup() override;
    void hidePopup() override;

    QSize sizeHint() const override;

signals:
    void fontRoleChanged();
    void layoutChanged();
    void chevronChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    void onThemeUpdated() override;

private:
    friend class ComboBoxPopup;
    void initAnimation();
    void onPopupHidden();
    void layoutLineEdit();
    void applyLineEditStyle();
    void validateLineEditText();

    // --- Configurable design tokens ---
    QString m_fontRole       = Typography::FontRole::Body;
    int     m_contentPaddingH = ::Spacing::Padding::ComboBoxHorizontal;
    int     m_contentPaddingV = ::Spacing::Padding::ComboBoxVertical;
    QString m_chevronGlyph   = Typography::Icons::ChevronDownMed;
    int     m_chevronSize    = 12;
    QPoint  m_chevronOffset  {::Spacing::Padding::ComboBoxHorizontal, 0};
    int     m_popupOffset    = ::Spacing::XSmall; // 4px gap between combo and popup

    // --- State ---
    bool  m_hovered  = false;
    bool  m_pressed  = false;
    bool  m_chevronHovered = false;
    bool  m_popupVisible = false;
    bool  m_ignoreNextPopupPress = false;
    qreal m_pressProgress = 0.0;

    QPropertyAnimation* m_pressAnimation = nullptr;

    // --- Editable ---
    view::textfields::LineEdit* m_lineEdit = nullptr;

    // --- Popup ---
    class ComboBoxPopup;
    ComboBoxPopup* m_popup = nullptr;
};

// ─── ComboBox 弹层窗口 ──────────────────────────────────────────────────────

/**
 * @brief Flyout-backed dropdown host used internally by ComboBox.
 * zh_CN: ComboBox 内部使用的 Flyout 下拉宿主。
 *
 * ComboBoxPopup hosts the popup ListView inside shared overlay behavior so
 * placement, light-dismiss, and clipping follow the common flyout contract.
 * zh_CN: ComboBoxPopup 在统一浮层行为中承载下拉 ListView，使定位、light-dismiss
 * 和裁剪遵循通用 Flyout 契约。
 */
class ComboBox::ComboBoxPopup : public view::dialogs_flyouts::Flyout {
public:
    explicit ComboBoxPopup(ComboBox* comboBox);

    void showForComboBox();
    void onThemeUpdated() override;

protected:
    QPoint computePosition() const override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    ComboBox* m_comboBox;
    view::collections::ListView* m_listView;
    ComboBoxItemDelegate* m_delegate;
};

} // namespace view::basicinput

#endif // COMBOBOX_H
