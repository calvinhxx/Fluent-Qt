#pragma once

#include <QPoint>
#include <QRect>

class QScreen;
class QWidget;

/**
 * @brief Synchronizes a hidden window's normal geometry before maximized display.
 * zh_CN: 在以最大化状态显示前同步隐藏窗口的普通状态几何，不显示或激活窗口。
 */
void fluentPrepareWindowNormalGeometry(QWidget* window, const QRect& geometry);

/**
 * @brief Constrains a popup's native frame to the screen's available pixel bounds.
 * zh_CN: 按屏幕的可用像素边界约束弹窗原生窗框，借用弹窗与屏幕，不改变其所有权。
 */
QPoint fluentClampPopupPosition(const QWidget* popup, const QPoint& position, const QScreen* screen);
