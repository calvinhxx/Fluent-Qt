#ifndef FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERWHEEL_P_H
#define FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERWHEEL_P_H

#include <QFont>
#include <QString>
#include <QVector>
#include <QWidget>
#include <Qt>

#include <functional>

#include "compatibility/QtCompat.h"
#include "components/date_time/private/PickerAccessibility_p.h"
#include "components/foundation/FluentElement.h"

class QFocusEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QVariantAnimation;
class QWheelEvent;

namespace fluent::basicinput {
class Button;
}

namespace fluent::date_time::detail {

int pickerEntryHeight(const QFont& font);
int pickerRowHeight(const QFont& font);
int pickerColumnHeight(const QFont& font);
QVector<int> distributedPickerWidths(const QVector<int>& preferredWidths, int availableWidth);
Qt::Alignment normalizedPickerHorizontalAlignment(Qt::Alignment alignment, Qt::Alignment fallback);
int wrappedPickerValue(int value, int minimum, int maximum);

/** Private shared interaction and rendering shell for DatePicker/TimePicker columns. */
class PickerWheelColumn : public QWidget,
                          public FluentElement,
                          public PickerColumnAccessibilityHost {
public:
    explicit PickerWheelColumn(int initialWidthHint, QWidget* parent = nullptr);

    QSize sizeHint() const override;
    void setWidthHint(int width);

    QWidget* pickerColumnWidget() override { return this; }
    bool pickerColumnCanShift(int direction) const override;
    void pickerColumnShift(int direction) override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(FluentEnterEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

    void onThemeUpdated() override { update(); }

    virtual bool canShiftBy(int offset) const = 0;
    virtual void shiftBy(int offset) = 0;
    virtual void commitPickerValue() = 0;
    virtual void cancelPickerValue() = 0;
    virtual QString displayTextForOffset(int offset) const = 0;
    virtual bool isRowSelectable(int offset) const = 0;
    virtual bool isRowTextEnabled(int offset) const = 0;
    virtual Qt::Alignment columnTextAlignment() const = 0;
    virtual bool isFirstVisibleColumn() const = 0;
    virtual bool isLastVisibleColumn() const = 0;
    virtual int visibleItemCountProperty() const { return -1; }
    virtual bool refreshPropertiesOnFocus() const { return false; }
    void refreshColumnProperties() { refreshProperties(); }

private:
    enum class HitKind { None, Previous, Next, Row };

    struct HitInfo {
        HitKind kind = HitKind::None;
        int offset = 0;
    };

    HitInfo hitTest(const QPoint& pos) const;
    QRect rowRect(int row) const;
    QRect previousButtonRect() const;
    QRect nextButtonRect() const;
    void setColumnHovered(bool hovered);
    void resetWheelState();
    void refreshProperties();

    int m_widthHint = 100;
    HitInfo m_hoverHit;
    bool m_columnHovered = false;
    qreal m_navButtonOpacity = 0.0;
    qreal m_navButtonTargetOpacity = 0.0;
    QVariantAnimation* m_navButtonAnimation = nullptr;
    qreal m_wheelAccum = 0.0;
    int m_wheelDir = 0;
    qint64 m_lastWheelTs = 0;
};

/** Private shared geometry and action surface for DatePicker/TimePicker flyouts. */
class PickerWheelPanel : public QWidget, public FluentElement {
public:
    PickerWheelPanel(const QString& panelObjectName, int themeMinimumWidth,
                     QWidget* parent = nullptr);

    QSize sizeHint() const override;
    PickerWheelColumn* firstVisibleColumn() const;
    int selectedRowCenterY() const;
    void setColumns(const QVector<PickerWheelColumn*>& columns);
    void initializeActions(const QString& confirmButtonObjectName,
                           const QString& cancelButtonObjectName, std::function<void()> commit,
                           std::function<void()> cancel);
    void configureColumns(const QFont& pickerFont, const QVector<bool>& visible,
                          const QVector<int>& preferredWidths);
    void setActionAccessibleNames(const QString& confirmName, const QString& cancelName);
    void updateColumns();
    void refreshTheme();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void onThemeUpdated() override;

private:
    QVector<PickerWheelColumn*> visibleColumns() const;
    QVector<int> columnWidths() const;
    void layoutContent();

    int m_themeMinimumWidth = 0;
    QVector<PickerWheelColumn*> m_columns;
    fluent::basicinput::Button* m_confirmButton = nullptr;
    fluent::basicinput::Button* m_cancelButton = nullptr;
};

} // namespace fluent::date_time::detail

#endif // FLUENTQT_COMPONENTS_DATE_TIME_PRIVATE_PICKERWHEEL_P_H
