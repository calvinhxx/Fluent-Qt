#pragma once

#include "components/foundation/WidgetOwnership.h"
#include <QObject>
#include <QPointer>
#include <QRect>
#include <QWidget>

namespace fluent::textfields {
class Label;
}

namespace fluent::detail {

// One optional field adornment; the field still owns its native editing and chrome.
// zh_CN: 管理一个可选字段附加内容；原生编辑与内置按钮仍由字段自身管理。
class FieldTrailingContent final : public QObject {
    Q_OBJECT
public:
    explicit FieldTrailingContent(QWidget* host);
    ~FieldTrailingContent() override;

    QString text() const { return m_text; }
    QWidget* widget() const { return m_widget.data(); }
    WidgetOwnership ownership() const { return m_ownership; }
    void setText(const QString& text);
    bool setWidget(QWidget* widget, WidgetOwnership ownership);
    QWidget* takeWidget();
    void releaseWidget();
    QSize sizeHint() const;
    int layout(const QRect& available, Qt::LayoutDirection direction, int minimumTextWidth);
    void refreshFont();

signals:
    void textChanged();
    void widgetChanged();
    void ownershipChanged();
    void geometryChanged();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    struct State {
        QString text;
        QWidget* widget;
        WidgetOwnership ownership;
    };
    State state() const { return {m_text, m_widget.data(), m_ownership}; }
    void notify(const State& previous);
    QWidget* content() const;
    void release(bool destroyOwned);

    QWidget* m_host;
    QString m_text;
    QPointer<textfields::Label> m_label;
    QPointer<QWidget> m_widget;
    QPointer<QWidget> m_originalParent;
    WidgetOwnership m_ownership = WidgetOwnership::Borrowed;
    QMetaObject::Connection m_destroyed;
    bool m_layingOut = false;
};

} // namespace fluent::detail
