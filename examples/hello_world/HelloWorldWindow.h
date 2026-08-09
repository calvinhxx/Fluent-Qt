#ifndef FLUENTQT_HELLOWORLD_WINDOW_H
#define FLUENTQT_HELLOWORLD_WINDOW_H

#include <memory>

namespace fluent::windowing {
class Window;
}

namespace fluentqt::hello_world {

std::unique_ptr<fluent::windowing::Window> createWindow();

} // namespace fluentqt::hello_world

#endif // FLUENTQT_HELLOWORLD_WINDOW_H
