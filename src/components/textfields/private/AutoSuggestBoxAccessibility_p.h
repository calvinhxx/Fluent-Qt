#ifndef FLUENTQT_COMPONENTS_TEXTFIELDS_PRIVATE_AUTOSUGGESTBOXACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_TEXTFIELDS_PRIVATE_AUTOSUGGESTBOXACCESSIBILITY_P_H

namespace fluent::textfields {
class AutoSuggestBox;

namespace detail {

void ensureAutoSuggestBoxAccessibilityFactory();
void notifyAutoSuggestSuggestionsChanged(AutoSuggestBox* box);
void notifyAutoSuggestPopupChanged(AutoSuggestBox* box);
void notifyAutoSuggestActiveDescendantChanged(AutoSuggestBox* box);
void notifyAutoSuggestNameChanged(AutoSuggestBox* box);

} // namespace detail
} // namespace fluent::textfields

#endif // FLUENTQT_COMPONENTS_TEXTFIELDS_PRIVATE_AUTOSUGGESTBOXACCESSIBILITY_P_H
