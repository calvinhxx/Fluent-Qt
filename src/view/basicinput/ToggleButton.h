#ifndef TOGGLEBUTTON_H
#define TOGGLEBUTTON_H

#include "Button.h"

namespace view::basicinput {

/**
 * @brief Fluent button with persistent checked or tri-state behavior.
 * zh_CN: 支持持久选中态或三态的 Fluent 按钮。
 *
 * ToggleButton reuses the Button visual contract while making check state part
 * of the public API for toolbar commands, filters, and selectable actions.
 * zh_CN: ToggleButton 复用 Button 的视觉契约，并把 check state 暴露为公开 API，
 * 适合工具栏命令、筛选项和可选操作。
 */
class ToggleButton : public Button {
    Q_OBJECT
    /**
     * @brief Whether the toggle button supports partially checked state.
     * zh_CN: 切换按钮是否支持部分选中的三态。
     */
    Q_PROPERTY(bool threeState READ isThreeState WRITE setThreeState NOTIFY threeStateChanged)
    /**
     * @brief Current Qt check state exposed by the toggle button.
     * zh_CN: 切换按钮暴露的当前 Qt 选中状态。
     */
    Q_PROPERTY(Qt::CheckState checkState READ checkState WRITE setCheckState NOTIFY checkStateChanged)

public:
    explicit ToggleButton(const QString& text = "", QWidget* parent = nullptr);
    explicit ToggleButton(QWidget* parent = nullptr);

    bool isThreeState() const { return m_threeState; }
    void setThreeState(bool threeState);

    Qt::CheckState checkState() const;
    void setCheckState(Qt::CheckState state);

    void onThemeUpdated() override;

signals:
    void threeStateChanged();
    void checkStateChanged(Qt::CheckState state);

protected:
    void paintEvent(QPaintEvent* event) override;
    void nextCheckState() override; // 处理三态循环

private:
    bool m_threeState = false;
    Qt::CheckState m_checkState = Qt::Unchecked;
};

} // namespace view::basicinput

#endif // TOGGLEBUTTON_H
