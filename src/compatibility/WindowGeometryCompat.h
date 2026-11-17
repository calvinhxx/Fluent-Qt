#pragma once

#include <QPoint>
#include <QRect>

class QScreen;
class QWidget;

/**
 * @brief Synchronizes a hidden window's normal geometry before maximized display without showing or activating it.
 * zh_CN: 在以最大化状态显示前同步隐藏窗口的还原尺寸，不触发显示或激活。
 */
void fluentPrepareWindowNormalGeometry(QWidget* window, const QRect& geometry);

/**
 * @brief Constrains a popup's native frame to the screen's available pixel bounds.
 * zh_CN: 将弹窗的原生窗框位置限制在屏幕可用像素区域内。
 */
QPoint fluentClampPopupPosition(const QWidget* popup, const QPoint& position, const QScreen* screen);
