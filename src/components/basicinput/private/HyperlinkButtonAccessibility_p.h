#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_HYPERLINKBUTTONACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_HYPERLINKBUTTONACCESSIBILITY_P_H

namespace fluent::basicinput {

class HyperlinkButton;

namespace detail {

class HyperlinkButtonAccessible;

void ensureHyperlinkButtonAccessibilityFactory();
void notifyHyperlinkButtonAccessibilityUrlChanged(
    HyperlinkButton* button, bool visitedChanged);
void notifyHyperlinkButtonAccessibilityVisited(
    HyperlinkButton* button);

} // namespace detail
} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_HYPERLINKBUTTONACCESSIBILITY_P_H
