#pragma once

#include "components/scrolling/ScrollView.h"

namespace fluent::menus_toolbars::detail {

// Menus paint one shared surface; neither the viewport nor its content adds a canvas fill.
class CommandScrollView final : public scrolling::ScrollView {
public:
    explicit CommandScrollView(QWidget* parent) : scrolling::ScrollView(parent) { keepTransparent(); }

    void setMenuContent(QWidget* content)
    {
        setWidget(content);
        // QScrollArea enables auto-fill during setWidget(), after caller setup.
        content->setAutoFillBackground(false);
        keepTransparent();
    }

protected:
    void onThemeUpdated() override
    {
        scrolling::ScrollView::onThemeUpdated();
        keepTransparent();
    }

private:
    void keepTransparent()
    {
        viewport()->setAutoFillBackground(false);
        viewport()->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }
};

} // namespace fluent::menus_toolbars::detail
