#ifndef FLUENTQT_COMPONENTS_LAYOUT_EXPANDER_H
#define FLUENTQT_COMPONENTS_LAYOUT_EXPANDER_H

#include <QMetaObject>
#include <QPointer>
#include <QString>

#include "components/foundation/WidgetOwnership.h"
#include "components/layout/Card.h"

class QResizeEvent;
class QScrollArea;
class QScrollBar;
class QVariantAnimation;

namespace fluent {
class FontIcon;
}

namespace fluent::basicinput {
class Button;
}

namespace fluent::layout {
class Divider;
}

namespace fluent::textfields {
class Label;
}

namespace fluent::layout {

/**
 * @brief Collapsible Fluent card with an animated content area.
 * zh_CN: 带展开/收起动画的 Fluent 折叠面板。
 *
 * Content lifetime follows WidgetOwnership: Borrowed detaches the widget, Reparented restores its previous parent,
 * and Owned destroys it with the Expander.
 * zh_CN: 正文控件的生命周期由 WidgetOwnership 控制：Borrowed 仅解除父子关系，Reparented 恢复原父对象，
 * Owned 随 Expander 一起销毁。
 */
class Expander : public Card {
    Q_OBJECT
    Q_PROPERTY(QString headerText READ headerText WRITE setHeaderText NOTIFY headerTextChanged)
    Q_PROPERTY(QWidget* contentWidget READ contentWidget WRITE setContentWidget NOTIFY contentWidgetChanged)
    Q_PROPERTY(bool expanded READ isExpanded WRITE setExpanded NOTIFY expandedChanged)
    Q_PROPERTY(bool animationEnabled READ isAnimationEnabled WRITE setAnimationEnabled NOTIFY animationEnabledChanged)

public:
    explicit Expander(QWidget* parent = nullptr);
    ~Expander() override;

    QString headerText() const { return m_headerText; }
    void setHeaderText(const QString& text);

    QWidget* contentWidget() const { return m_contentWidget.data(); }
    void setContentWidget(QWidget* widget);
    bool setContentWidget(QWidget* widget, WidgetOwnership ownership);
    QWidget* takeContentWidget();
    WidgetOwnership contentOwnership() const { return m_contentOwnership; }

    bool isExpanded() const { return m_expanded; }
    void setExpanded(bool expanded);
    void setExpandedAnimated(bool expanded, bool animated);
    void toggleExpanded();

    bool isAnimationEnabled() const { return m_animationEnabled; }
    void setAnimationEnabled(bool enabled);

    basicinput::Button* headerButton() const { return m_headerButton; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    void onThemeUpdated() override;

signals:
    void headerTextChanged(const QString& text);
    void contentWidgetChanged(QWidget* widget);
    void contentOwnershipChanged(WidgetOwnership ownership);
    void expandedChanged(bool expanded);
    void animationEnabledChanged(bool enabled);
    void expansionTransitionStarted(bool expanding);
    void expansionTransitionFinished(bool expanded);
    void layoutHeightChanged(int height);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    int naturalContentHeight(bool invalidateLayout = true) const;
    int currentContentHeight() const;
    int totalHeightForContent(int contentHeight) const;
    void updateChildGeometry();
    void applyFraction(qreal fraction);
    void releaseContent(bool deleteOwned, bool restoreParent);
    void handleContentDestroyed();
    void beginViewportTransition();
    void finishViewportTransition();
    void synchronizeViewportLayout();
    void restoreViewportAnchor();
    void clearViewportAnchor();
    void runTransitionTo(qreal target, bool animated);
    void finishTransitionAt(qreal target);
    void handleMotionPolicyChanged();

    static constexpr int HeaderHeight = 44;
    static constexpr int DividerExtent = 1;

    QString m_headerText;
    basicinput::Button* m_headerButton = nullptr;
    textfields::Label* m_headerLabel = nullptr;
    FontIcon* m_chevron = nullptr;
    Divider* m_divider = nullptr;
    QWidget* m_clip = nullptr;
    QPointer<QWidget> m_contentWidget;
    QPointer<QWidget> m_contentOriginalParent;
    WidgetOwnership m_contentOwnership = WidgetOwnership::Borrowed;
    QMetaObject::Connection m_contentDestroyedConnection;
    QVariantAnimation* m_animation = nullptr;

    bool m_expanded = false;
    bool m_animationEnabled = true;
    qreal m_fraction = 0.0;
    int m_contentTargetHeight = 0;
    int m_lastEmittedLayoutHeight = -1;
    bool m_refreshingContentHeight = false;
    bool m_viewportTransitionActive = false;
    bool m_restoringViewportAnchor = false;
    quint64 m_viewportTransitionGeneration = 0;
    int m_anchorViewportY = 0;
    QPointer<QScrollArea> m_transitionScrollArea;
    QPointer<QScrollBar> m_transitionScrollBar;
    QMetaObject::Connection m_scrollRangeConnection;
    QMetaObject::Connection m_scrollValueConnection;
};

} // namespace fluent::layout

#endif // FLUENTQT_COMPONENTS_LAYOUT_EXPANDER_H
