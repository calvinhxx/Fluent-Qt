#pragma once
#include "components/basicinput/Button.h"
namespace fluent::gallery {
class GalleryFeedbackButton final : public basicinput::Button {
public:
    explicit GalleryFeedbackButton(QWidget* parent = nullptr);
};
} // namespace fluent::gallery
