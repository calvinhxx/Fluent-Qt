#pragma once

#include <QAbstractItemDelegate>
#include <QAbstractItemModel>
#include <QHash>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <functional>
#include <utility>

namespace compatibility::detail {

// Keep one delegate installed per view while borrowing the application's paint
// and edit policy. Only signals for editors created by this adapter are forwarded.
// zh_CN: 每个视图安装独立代理，借用应用的绘制与编辑策略，只转发本代理创建的编辑器信号。
class ForwardingItemDelegate final : public QAbstractItemDelegate {
public:
    using Resolver = std::function<QAbstractItemDelegate*(const QModelIndex&)>;
    ForwardingItemDelegate(Resolver resolver, QObject* parent)
        : QAbstractItemDelegate(parent), m_resolver(std::move(resolver))
    {}

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        if (auto* delegate = resolve(index))
            delegate->paint(painter, option, index);
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        const auto* delegate = resolve(index);
        return delegate ? delegate->sizeHint(option, index) : QSize();
    }
    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        auto* delegate = resolve(index);
        if (!delegate)
            return nullptr;
        auto* editor = delegate->createEditor(parent, option, index);
        if (editor) {
            m_editors.insert(editor, delegate);
            auto* self = const_cast<ForwardingItemDelegate*>(this);
            QObject::connect(editor, &QObject::destroyed, self, [self, editor] { self->m_editors.remove(editor); });
        }
        return editor;
    }
    void destroyEditor(QWidget* editor, const QModelIndex& index) const override
    {
        const QPointer<QAbstractItemDelegate> delegate = m_editors.take(editor);
        if (delegate)
            delegate->destroyEditor(editor, index);
        else
            QAbstractItemDelegate::destroyEditor(editor, index);
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override
    {
        if (auto* delegate = m_editors.value(editor).data())
            delegate->setEditorData(editor, index);
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override
    {
        if (auto* delegate = m_editors.value(editor).data())
            delegate->setModelData(editor, model, index);
    }
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index)
        const override
    {
        if (auto* delegate = m_editors.value(editor).data())
            delegate->updateEditorGeometry(editor, option, index);
    }
    bool editorEvent(QEvent* event,
        QAbstractItemModel* model,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) override
    {
        auto* delegate = resolve(index);
        return delegate && delegate->editorEvent(event, model, option, index);
    }
    bool helpEvent(QHelpEvent* event,
        QAbstractItemView* view,
        const QStyleOptionViewItem& option,
        const QModelIndex& index) override
    {
        auto* delegate = resolve(index);
        return delegate && delegate->helpEvent(event, view, option, index);
    }
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const QPointer<ForwardingItemDelegate> guard(this);
        auto* delegate = m_editors.value(qobject_cast<QWidget*>(watched)).data();
        const bool handled = delegate && delegate->eventFilter(watched, event);
        return !guard || handled;
    }

private:
    QAbstractItemDelegate* resolve(const QModelIndex& index) const
    {
        auto* delegate = m_resolver(index);
        if (!delegate || delegate == this)
            return nullptr;
        if (!m_observed.contains(delegate)) {
            m_observed.insert(delegate);
            auto* self = const_cast<ForwardingItemDelegate*>(this);
            QObject::connect(delegate, &QAbstractItemDelegate::commitData, self, [self](QWidget* editor) {
                if (self->m_editors.contains(editor))
                    emit self->commitData(editor);
            });
            QObject::connect(delegate, &QAbstractItemDelegate::closeEditor, self,
                [self](QWidget* editor, EndEditHint hint) {
                    if (self->m_editors.contains(editor))
                        emit self->closeEditor(editor, hint);
                });
            QObject::connect(delegate, &QAbstractItemDelegate::sizeHintChanged, self,
                &QAbstractItemDelegate::sizeHintChanged);
            QObject::connect(delegate, &QObject::destroyed, self, [self, delegate] {
                self->m_observed.remove(delegate);
            });
        }
        return delegate;
    }
    Resolver m_resolver;
    mutable QHash<QWidget*, QPointer<QAbstractItemDelegate>> m_editors;
    mutable QSet<QAbstractItemDelegate*> m_observed;
};

} // namespace compatibility::detail
