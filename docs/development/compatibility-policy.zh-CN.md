# 兼容性策略

[English](compatibility-policy.md)

本策略从 FluentQt 1.7 之后开始执行。目标是稳定公开接口，但不承诺 Qt 工具链
无法可靠保证的二进制兼容。

## 公开 API

- 1.x 的补丁版和次版本保持源码兼容；可以兼容地增加重载、属性、信号和组件。
- 删除或改变已经公开的 API，需要等到下一个主版本。1.7 的 Fluent-only 清理是
  本策略生效前最后一次已明确说明的例外。
- `cmake/FluentQtInstallHeaders.cmake` 中的安装头文件、导出的 CMake target 和
  文档中的 PySide6 名称属于公开接口。私有头、Gallery 内部实现、测试和生成的
  实现细节不属于公开接口。

## 二进制 ABI

FluentQt 不承诺不同版本、编译器、标准库、构建模式或 Qt 版本之间保持 C++
二进制 ABI。升级时应使用同一套 Qt 和工具链重新构建 FluentQt 与应用。发布包和
PySide6 Wheel 只匹配其注明的平台、架构、Qt、Python 与 Shiboken 运行时。

## 弃用

- 计划删除公开 API 时，先加入能指出替代项的编译期或运行期弃用提示，并写入
  Release Notes。
- 弃用接口至少保留一个次版本，在下一个主版本删除之前继续测试。
- 安全问题、未定义行为或上游 Qt 破坏可能要求提前修改；Release Notes 必须说明
  原因和迁移方法。

## Qt 支持范围

- 原生 C++：C++17，Qt Widgets 5.15+ 或 Qt 6.2+。
- CI 同时抽检 Qt 5.15.2、Qt 6.2 和当前 Qt 6 版本；各平台与架构的安装包范围以
  README 和 Release 资产为准。
- PySide6 源码兼容从 Qt/PySide/Shiboken 6.2.4 开始；官方 Wheel 使用
  [绑定兼容策略](../../bindings/pyside6/API_COMPATIBILITY.md)中列出的配套运行时。
- WebAssembly 使用独立固定工具链：Qt 6.9.3 `wasm_singlethread` 与
  Emscripten 3.1.70。

提高最低 Qt、C++、CMake 或 Python 版本属于不兼容的构建契约变更，通常只能在
主版本执行；上游安全或发行环境限制导致无法继续支持时除外。
