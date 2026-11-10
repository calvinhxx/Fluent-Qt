#pragma once

#include <functional>
#include <QMetaObject>
#include <QSize>
#include "components/dialogs_flyouts/TeachingTip.h"

class QHBoxLayout;
class QVBoxLayout;
class QPropertyAnimation;
class QVariantAnimation;

namespace fluent {
class FontIcon;
namespace basicinput {
class Button;
}
namespace layout {
class Card;
}
namespace textfields {
class Label;
}
} // namespace fluent

namespace fluent::gallery {
// Gallery content and motion policy; TeachingTip owns tracking, containment, dismissal and painting.
// zh_CN: Gallery 负责反馈内容和动效策略，TeachingTip 负责跟随、边界约束、关闭和绘制。
class GalleryFeedbackTip final : public dialogs_flyouts::TeachingTip {
public:
    explicit GalleryFeedbackTip(QWidget* parent = nullptr);
    void setFeedback(const QString& title, const QString& message, bool preparing, bool error);
    void setArtifact(const QString& fileName, qint64 byteCount, bool downloaded = false);
    void setAction(const QString& caption, const QString& glyph, QObject* context, std::function<void()> handler);
    void showFor(QWidget* anchor);
    QString title() const;
    QString message() const;
    QString artifactName() const;
    QString artifactState() const;
    QSize targetCardSize() const { return m_targetCardSize; }
    basicinput::Button* actionButton() const { return m_action; }
    void onThemeUpdated() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void syncPlacement();
    void updateBodyIndentation();
    void updateAccessibleSummary();
    void applyTargetCardSize(const QSize& targetSize);
    int resolvedRestMargin() const;
    void applyRevealProgress();
    void updateSpinState();
    void updateColors();

    QVBoxLayout* m_rootLayout = nullptr;
    QHBoxLayout* m_messageLayout = nullptr;
    QHBoxLayout* m_actionLayout = nullptr;
    layout::Card* m_statusBadge = nullptr;
    FontIcon* m_icon = nullptr;
    textfields::Label* m_title = nullptr;
    QWidget* m_artifactHost = nullptr;
    layout::Card* m_artifactCard = nullptr;
    layout::Card* m_artifactIconBox = nullptr;
    FontIcon* m_artifactIcon = nullptr;
    textfields::Label* m_artifactName = nullptr;
    textfields::Label* m_artifactSize = nullptr;
    FontIcon* m_artifactStateIcon = nullptr;
    textfields::Label* m_artifactState = nullptr;
    textfields::Label* m_message = nullptr;
    QWidget* m_actionHost = nullptr;
    basicinput::Button* m_action = nullptr;
    QPropertyAnimation* m_spinAnimation = nullptr;
    QPropertyAnimation* m_actionFadeAnimation = nullptr;
    QVariantAnimation* m_sizeAnimation = nullptr;
    QSize m_targetCardSize{244, 66};
    QMetaObject::Connection m_actionConnection;
    bool m_preparing = false;
    bool m_error = false;
    bool m_placementPending = false;
};
} // namespace fluent::gallery
