#ifndef FLUENTQT_COMPONENTS_NAVIGATION_STEPPER_H
#define FLUENTQT_COMPONENTS_NAVIGATION_STEPPER_H

#include <memory>
#include <QVariant>
#include <QVector>
#include <QWidget>
#include "components/foundation/FluentElement.h"
#include "components/foundation/QMLPlus.h"

namespace fluent::navigation {
struct StepperItem;
class StepperAccessible;

/**
 * @brief Multi-step progress indicator whose state is controlled by the caller.
 * Clicks only emit stepRequested(); update the current step and completion or error states via methods.
 * zh_CN: 多步骤进度指示条，点击仅发出 stepRequested()，当前步骤与完成/错误状态由调用方通过接口更新。
 */
class Stepper : public QWidget, public FluentElement, public QMLPlus {
    Q_OBJECT
    Q_PROPERTY(int itemCount READ itemCount NOTIFY itemCountChanged)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation NOTIFY orientationChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)
public:
    /**
     * @brief Step completion state managed by the application.
     * zh_CN: 由应用设置的步骤状态。
     */
    enum State { Pending, Completed, Error };
    Q_ENUM(State)
    explicit Stepper(QWidget* parent = nullptr);
    ~Stepper() override;
    int itemCount() const;
    StepperItem itemAt(int index) const;
    QVector<StepperItem> items() const;
    int addItem(const QString& text);
    int addItem(const StepperItem& item);
    bool insertItem(int index, const StepperItem& item);
    bool removeItem(int index);
    void clearItems();
    /**
     * @brief Updates step metadata without advancing the current index.
     * zh_CN: 更新步骤内容与元数据，不改变当前步骤索引。
     */
    bool setItem(int index, const StepperItem& item);
    bool setItemState(int index, State state);
    bool setItemEnabled(int index, bool enabled);
    int currentIndex() const;
    /**
     * @brief Sets the current step index (including disabled steps), or -1 to clear; out-of-range indexes are ignored.
     * zh_CN: 设置当前步骤索引（允许选中禁用项），传 -1 清空；越界索引会被忽略。
     */
    void setCurrentIndex(int index);
    Qt::Orientation orientation() const;
    void setOrientation(Qt::Orientation orientation);
    bool isAnimationEnabled() const;
    void setAnimationEnabled(bool enabled);
    /**
     * @brief Returns step geometry in this widget's coordinates.
     * zh_CN: 返回控件坐标中的步骤几何。
     */
    QRect itemGeometry(int index) const;
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
signals:
    void itemCountChanged(int count);
    void itemsChanged();
    void currentIndexChanged(int index);
    void orientationChanged(Qt::Orientation orientation);
    void animationEnabledChanged(bool enabled);
    /**
     * @brief Requests navigation; a caller decides whether to accept it.
     * zh_CN: 请求导航，由调用方决定是否接受。
     */
    void stepRequested(int index);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void changeEvent(QEvent* event) override;
    void onThemeUpdated() override;

private:
    friend class StepperAccessible;
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @brief Text, icon, state and application data for a step.
 * zh_CN: 步骤文字、图标、状态和应用数据。
 */
struct StepperItem {
    QString text;
    QString description;
    QString iconGlyph;
    Stepper::State state = Stepper::Pending;
    bool enabled = true;
    QVariant data;
    QString accessibleName;
    StepperItem() = default;
    explicit StepperItem(const QString& text, const QString& description = QString());
    bool operator==(const StepperItem& other) const;
    bool operator!=(const StepperItem& other) const { return !(*this == other); }
};
} // namespace fluent::navigation
Q_DECLARE_METATYPE(fluent::navigation::StepperItem)
#endif
