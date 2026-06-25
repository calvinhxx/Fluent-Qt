#ifndef GALLERYCLOSEBEHAVIORPROMPT_H
#define GALLERYCLOSEBEHAVIORPROMPT_H

#include <QSize>
#include <QString>
#include <QVector>
#include <QWidget>

#include "components/foundation/FluentElement.h"
#include "viewmodel/GallerySettings.h"

class QVBoxLayout;

namespace fluent::gallery {

/**
 * @brief Compact first-close prompt content for choosing the gallery close behavior.
 * zh_CN: 首次关闭时用于选择 Gallery 关闭行为的紧凑弹窗内容。
 */
class CloseBehaviorPromptContent final : public QWidget, public FluentElement {
public:
    explicit CloseBehaviorPromptContent(GallerySettings::CloseBehavior current,
                                        QWidget* parent = nullptr);

    GallerySettings::CloseBehavior selectedBehavior() const;

    void onThemeUpdated() override;

private:
    void addChoice(QVBoxLayout* root,
                   GallerySettings::CloseBehavior behavior,
                   const QString& glyph,
                   const QString& title,
                   const QString& description);
    void syncSelection();

    GallerySettings::CloseBehavior m_selected{GallerySettings::CloseBehavior::Tray};
    QVector<QWidget*> m_rows;
};

QSize closeBehaviorDialogSize();

} // namespace fluent::gallery

#endif // GALLERYCLOSEBEHAVIORPROMPT_H
