if(NOT APPLE OR TARGET FluentQt::ApplePlatformDependencies)
    return()
endif()

# Resolve Apple SDK libraries on the machine that configures FluentQt.  The
# imported interface target keeps build-tree and installed-package consumers
# on the same dependency path without exporting this machine's SDK location.
find_library(_FLUENT_QT_COREGRAPHICS_FRAMEWORK CoreGraphics REQUIRED)
find_library(_FLUENT_QT_OBJC_LIBRARY objc REQUIRED)

add_library(FluentQt::ApplePlatformDependencies INTERFACE IMPORTED)
set_target_properties(FluentQt::ApplePlatformDependencies PROPERTIES
    INTERFACE_LINK_LIBRARIES
        "${_FLUENT_QT_COREGRAPHICS_FRAMEWORK};${_FLUENT_QT_OBJC_LIBRARY}")

unset(_FLUENT_QT_COREGRAPHICS_FRAMEWORK CACHE)
unset(_FLUENT_QT_OBJC_LIBRARY CACHE)
