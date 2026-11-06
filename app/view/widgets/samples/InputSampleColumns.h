#pragma once

#include <FluentQt/FluentQt.h>
#include <QBoxLayout>
#include <QWidget>

namespace fluent::gallery {

// A pair of readable field examples stacks when the preview gets narrow.
// zh_CN: 两列字段示例在预览变窄时纵向排列。
class InputSampleColumns final : public QWidget {
    static constexpr int ColumnWidth = 370;
    static constexpr int RowWidth = 2 * ColumnWidth + ::Spacing::Large;

    // Parent layouts query QLayout's height-for-width when a widget has a layout.
    // zh_CN: 控件有布局时，父布局会查询 QLayout 的宽高关系，而非 QWidget 的重载。
    class ColumnsLayout final : public QBoxLayout {
    public:
        explicit ColumnsLayout(QWidget* parent) : QBoxLayout(LeftToRight, parent)
        {
            setContentsMargins(0, 0, 0, 0);
            setSpacing(::Spacing::Large);
            setSizeConstraint(QLayout::SetNoConstraint);
            setAlignment(Qt::AlignTop);
        }
        bool hasHeightForWidth() const override { return true; }
        int heightForWidth(int width) const override
        {
            const bool stacked = width < RowWidth;
            int height = 0;
            for (int index = 0; index < count(); ++index) {
                auto* column = itemAt(index)->widget();
                if (!column)
                    continue;
                const int hint = column->hasHeightForWidth()
                    ? column->heightForWidth(qMin(width, ColumnWidth))
                    : column->sizeHint().height();
                height = stacked ? height + hint : qMax(height, hint);
            }
            return height + (stacked ? qMax(0, count() - 1) * ::Spacing::Large : 0);
        }
        QSize sizeHint() const override { return QSize(RowWidth, heightForWidth(RowWidth)); }
        QSize minimumSize() const override { return QSize(180, heightForWidth(RowWidth)); }
        void setGeometry(const QRect& rect) override
        {
            setDirection(rect.width() < RowWidth ? TopToBottom : LeftToRight);
            QBoxLayout::setGeometry(rect);
        }
    };

public:
    explicit InputSampleColumns(QWidget* parent = nullptr) : QWidget(parent), m_layout(new ColumnsLayout(this))
    {
        setMaximumWidth(RowWidth);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    QWidget* addColumn()
    {
        auto* column = new QWidget(this);
        column->setMaximumWidth(ColumnWidth);
        column->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        auto* layout = new QVBoxLayout(column);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(::Spacing::Small);
        m_layout->addWidget(column, 1, Qt::AlignTop);
        return column;
    }

private:
    ColumnsLayout* m_layout;
};

} // namespace fluent::gallery
