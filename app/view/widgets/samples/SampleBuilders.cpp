#include "SampleBuilders.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QWidget>

namespace fluent::gallery::samples {

GallerySample makeSample(const QString& id,
                         const QString& title,
                         const QString& description,
                         const QString& codeSnippet,
                         std::function<QWidget*(QWidget*)> createPreview)
{
    GallerySample sample;
    sample.id = id;
    sample.title = title;
    sample.description = description;
    sample.codeSnippet = codeSnippet;
    sample.createPreview = std::move(createPreview);
    return sample;
}

QWidget* verticalGroup(QWidget* parent, int spacing)
{
    auto* group = new QWidget(parent);
    auto* layout = new QVBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(spacing);
    layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    return group;
}

QWidget* horizontalGroup(QWidget* parent, int spacing)
{
    auto* group = new QWidget(parent);
    auto* layout = new QHBoxLayout(group);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(spacing);
    layout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return group;
}

} // namespace fluent::gallery::samples
