#ifndef FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_SPLITVIEWACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_SPLITVIEWACCESSIBILITY_P_H

#include <QRect>
#include <QWidget>

class QFocusEvent;
class QKeyEvent;
class QPaintEvent;

namespace fluent::collections {

class SplitView;

namespace detail {

inline QRect centeredSplitHandleVisualRect(
    const QRect& handleRect,
    Qt::Orientation orientation,
    int thickness,
    int crossAxisInset = 4)
{
    if (handleRect.isEmpty())
        return {};

    const int inset = qMax(0, crossAxisInset);
    if (orientation == Qt::Horizontal) {
        const int visualWidth = qBound(1, thickness, handleRect.width());
        return QRect(
            handleRect.left() + (handleRect.width() - visualWidth) / 2,
            handleRect.top() + inset,
            visualWidth,
            qMax(0, handleRect.height() - inset * 2));
    }

    const int visualHeight = qBound(1, thickness, handleRect.height());
    return QRect(
        handleRect.left() + inset,
        handleRect.top() + (handleRect.height() - visualHeight) / 2,
        qMax(0, handleRect.width() - inset * 2),
        visualHeight);
}

class SplitViewHandle final : public QWidget {
public:
    SplitViewHandle(SplitView* splitView, int handleIndex);

    SplitView* splitView() const { return m_splitView; }
    int handleIndex() const { return m_handleIndex; }
    void setHandleIndex(int index) { m_handleIndex = index; }

    int currentValue() const;
    int minimumValue() const;
    int maximumValue() const;
    int leadingPaneIndex() const;
    int trailingPaneIndex() const;
    bool setValue(int value);
    bool stepBy(int delta);
    bool consumeSemanticChange(bool* actionsChanged);
    void resetSemanticSnapshot();

protected:
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    SplitView* m_splitView = nullptr;
    int m_handleIndex = -1;
    int m_lastCurrentValue = 0;
    int m_lastMinimumValue = 0;
    int m_lastMaximumValue = 0;
};

void ensureSplitViewAccessibilityFactory();
void notifySplitViewAccessibilityStructureChanged(SplitView* splitView);
void notifySplitViewAccessibilityOrientationChanged(SplitView* splitView);
void notifySplitViewAccessibilityHandleValueChanged(
    SplitView* splitView, int handleIndex);
void notifySplitViewAccessibilityAllHandleValuesChanged(
    SplitView* splitView);

} // namespace detail
} // namespace fluent::collections

#endif // FLUENTQT_COMPONENTS_COLLECTIONS_PRIVATE_SPLITVIEWACCESSIBILITY_P_H
