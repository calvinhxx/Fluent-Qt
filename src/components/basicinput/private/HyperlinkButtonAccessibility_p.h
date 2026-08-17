#ifndef FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_HYPERLINKBUTTONACCESSIBILITY_P_H
#define FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_HYPERLINKBUTTONACCESSIBILITY_P_H

class QString;

namespace fluent::basicinput {

class HyperlinkButton;

namespace detail {

class HyperlinkButtonAccessible;

const QString& prepareHyperlinkButtonAccessibility(
    const QString& text);
void notifyHyperlinkButtonAccessibilityUrlChanged(
    HyperlinkButton* button, bool visitedChanged);
void notifyHyperlinkButtonAccessibilityVisited(
    HyperlinkButton* button);

} // namespace detail
} // namespace fluent::basicinput

#endif // FLUENTQT_COMPONENTS_BASICINPUT_PRIVATE_HYPERLINKBUTTONACCESSIBILITY_P_H
