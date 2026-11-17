#ifndef FLUENTELEMENT_P_H
#define FLUENTELEMENT_P_H

#include <QObject>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include "components/foundation/FluentElement.h"

namespace fluent {

// Internal singleton broadcasting global Fluent theme changes to live FluentElement instances.
class FluentThemeManager : public QObject {
    Q_OBJECT
public:
    static FluentThemeManager* instance()
    {
        static FluentThemeManager inst;
        return &inst;
    }

    FluentElement::Theme currentTheme = FluentElement::Light;
    QSet<FluentElement*> elements;

    // Bumped on every theme change so hidden deferred subtrees can check staleness on show.
    int generation() const { return notificationGeneration; }

    void notifyAll()
    {
        const int generation = ++notificationGeneration;
        deferredElements.clear();
        deferredIndex = 0;
        // Iterate a copy because onThemeUpdated() callbacks may destroy elements.
        auto copy = elements;
        for (auto* e : copy) {
            if (elements.contains(e)) {
                e->onThemeUpdated();
                if (generation != notificationGeneration)
                    return;
            }
        }
    }

    void notifyVisibleThenDeferred()
    {
        const int generation = ++notificationGeneration;
        deferredElements.clear();
        deferredIndex = 0;

        const auto copy = elements;
        QVector<FluentElement*> visibleElements;
        deferredElements.reserve(copy.size());
        visibleElements.reserve(copy.size());
        for (auto* element : copy) {
            if (!elements.contains(element))
                continue;
            auto* widget = dynamic_cast<QWidget*>(element);
            if (widget && !widget->isVisible())
                deferredElements.append(element);
            else
                visibleElements.append(element);
        }

        // Refresh visible elements synchronously so on-screen switches repaint atomically;
        // hidden subtrees are queued in batches and also refreshed on show (e.g. StackContentHost).
        for (auto* element : visibleElements) {
            if (elements.contains(element)) {
                element->onThemeUpdated();
                if (generation != notificationGeneration)
                    return;
            }
        }

        if (!deferredElements.isEmpty()) {
            QTimer::singleShot(0, this, [this, generation]() { notifyDeferredBatch(generation); });
        }
    }

private:
    void notifyDeferredBatch(int generation)
    {
        if (generation != notificationGeneration)
            return;

        // Small batch size keeps background timer ticks under a frame while catching hidden
        // widgets without an explicit on-show refresh (e.g. collapsed nav rows).
        constexpr int batchSize = 8;
        const int end = qMin(deferredIndex + batchSize, deferredElements.size());
        while (deferredIndex < end) {
            auto* element = deferredElements.at(deferredIndex++);
            if (elements.contains(element))
                element->onThemeUpdated();
            // Stop if a callback started a newer theme switch.
            if (generation != notificationGeneration)
                return;
        }

        if (deferredIndex < deferredElements.size()) {
            QTimer::singleShot(1, this, [this, generation]() { notifyDeferredBatch(generation); });
        } else {
            deferredElements.clear();
            deferredIndex = 0;
        }
    }

    QVector<FluentElement*> deferredElements;
    int deferredIndex = 0;
    int notificationGeneration = 0;
};

} // namespace fluent

#endif // FLUENTELEMENT_P_H
