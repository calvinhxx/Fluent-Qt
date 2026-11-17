#ifndef FLUENTQT_FLUENTQT_H
#define FLUENTQT_FLUENTQT_H

namespace fluent {

/**
 * @brief Configures High-DPI scaling before QApplication is created.
 * zh_CN: 在创建 QApplication 之前开启高分屏（High-DPI）适配。
 *
 * Enables device-independent scaling on Qt 5 and pass-through fractional
 * scaling (such as 125%, 150%, 200%) across supported Qt versions. Call this
 * before constructing QApplication; repeated pre-application calls are safe,
 * while late calls warn and have no effect.
 * zh_CN: 在 Qt 5 上启用设备无关缩放与高清位图，并在所有受支持版本中直接沿用
 * 系统的缩放比例（如 125%、150%、200%）。必须在创建 QApplication 之前调用；
 * 重复调用无副作用，创建之后再调用则会打印警告且不生效。
 */
void prepareHighDpiApplication();

/**
 * @brief Loads FluentQt's bundled fonts and icon resources.
 * zh_CN: 加载 FluentQt 内置的文本字体、图标字体和资源文件。
 *
 * Call once after constructing QApplication so bundled fonts are ready without
 * OS font matching. If called before QApplication exists, Qt resources are
 * registered and false is returned; call it again afterward to load the fonts.
 * zh_CN: 请在创建 QApplication 之后调用一次，使内置字体和图标立即可用，不必
 * 依赖系统字体匹配。若在 QApplication 创建前调用，会先注册编译资源并
 * 返回 false；创建后请再调用一次以初始化字体。
 */
bool initializeResources();

} // namespace fluent

#include <FluentQt/BasicInput.h>
#include <FluentQt/Charts.h>
#include <FluentQt/Collections.h>
#include <FluentQt/DateTime.h>
#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/Navigation.h>
#ifdef FLUENT_QT_HAS_PARTICLES
#include <FluentQt/Particles.h>
#endif
#ifdef FLUENT_QT_HAS_PLUGINS
#include <FluentQt/Plugins.h>
#endif
#ifdef FLUENT_QT_HAS_QML_BRIDGE
#include <FluentQt/QmlBridge.h>
#endif
#include <FluentQt/Scrolling.h>
#ifdef FLUENT_QT_HAS_SPATIAL
#include <FluentQt/Spatial.h>
#endif
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Windowing.h>

#endif // FLUENTQT_FLUENTQT_H
