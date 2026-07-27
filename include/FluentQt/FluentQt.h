#ifndef FLUENTQT_FLUENTQT_H
#define FLUENTQT_FLUENTQT_H

namespace fluent {

/**
 * @brief Configures process-wide High-DPI behavior before QApplication exists.
 * zh_CN: 在 QApplication 创建前配置进程级 High-DPI 行为。
 *
 * On Qt 5 this enables device-independent scaling and high-resolution pixmaps.
 * On every supported Qt version it selects pass-through fractional scaling so
 * 125%, 150%, 200%, and 300% keep the operating system's requested size.
 * Call this before constructing QApplication; repeated pre-application calls
 * are harmless. A late call emits a warning and leaves global state unchanged.
 * zh_CN: Qt 5 下会启用设备无关缩放与高分辨率位图；所有受支持 Qt 版本都会采用
 * 分数缩放直通策略，使 125%、150%、200% 与 300% 保持系统请求的实际比例。
 * 必须在构造 QApplication 前调用；应用创建前重复调用是安全的。调用过晚时会输出
 * 警告，且不再修改全局状态。
 */
void prepareHighDpiApplication();

/**
 * @brief Registers Fluent-Qt bundled resources and application fonts.
 * zh_CN: 注册 Fluent-Qt 内置资源与应用字体。
 *
 * Call this once after constructing QApplication in standalone applications so
 * the bundled open-source text and icon faces are available without depending
 * on platform font matching. A call made before QApplication registers the
 * compiled resource collection but returns false without caching a failed font
 * load; call it again after QApplication exists to initialize the fonts.
 * zh_CN: 独立应用应在构造 QApplication 后调用一次，使内置的开源文本与图标字体
 * 可用，且不依赖平台字体匹配。在 QApplication 创建前调用时会先注册编译资源，
 * 但返回 false 且不会缓存字体加载失败；QApplication 创建后应再次调用以初始化字体。
 */
bool initializeResources();

} // namespace fluent

#include <FluentQt/BasicInput.h>
#include <FluentQt/Collections.h>
#include <FluentQt/DateTime.h>
#include <FluentQt/Design.h>
#include <FluentQt/DialogsFlyouts.h>
#include <FluentQt/Foundation.h>
#include <FluentQt/Layout.h>
#include <FluentQt/MenusToolbars.h>
#include <FluentQt/Navigation.h>
#include <FluentQt/Scrolling.h>
#include <FluentQt/StatusInfo.h>
#include <FluentQt/TextFields.h>
#include <FluentQt/Windowing.h>

#endif // FLUENTQT_FLUENTQT_H
