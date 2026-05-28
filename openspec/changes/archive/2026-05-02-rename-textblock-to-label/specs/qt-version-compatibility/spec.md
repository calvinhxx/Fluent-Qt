## MODIFIED Requirements

### Requirement: TextBlock 头文件 SHALL NOT 引入 `Label` 类型别名
After the rename, `src/view/textfields/Label.h` MUST declare a real `view::textfields::Label` class and MUST NOT introduce `using Label = TextBlock` or `using TextBlock = Label`. The old `TextBlock.h` public header is removed by the rename.

#### Scenario: Label.h 在 Qt5 / MSVC 编译通过
- **WHEN** 在 Qt 5.15 + MSVC 环境编译包含 `Label.h` 的翻译单元
- **THEN** 编译 MUST 不出现 `C2371: Label 重定义`

#### Scenario: 不保留 TextBlock 别名
- **WHEN** 引入 `view/textfields/Label.h`
- **THEN** `Label` MUST be a class declaration, not an alias to `TextBlock`
- **AND** `src/view/textfields/TextBlock.h` MUST NOT exist
