#pragma once

namespace fluent::basicinput {
class ComboBox;
namespace detail {
class ComboBoxAccessible;
void ensureComboBoxAccessibilityFactory();
void notifyComboBoxPopupChanged(ComboBox* box);
void notifyComboBoxContentChanged(ComboBox* box);
} // namespace detail
} // namespace fluent::basicinput
