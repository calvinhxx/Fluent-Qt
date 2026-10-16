add_qt_test_module(test_spatial_view TestSpatialView.cpp)
target_link_libraries(test_spatial_view PRIVATE FluentQt::Spatial)
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(test_spatial_view PRIVATE Qt6::OpenGLWidgets)
endif()

add_qt_test_module(test_particle_layer TestParticleLayer.cpp)
target_link_libraries(test_particle_layer PRIVATE FluentQt::Spatial)
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(test_particle_layer PRIVATE Qt6::OpenGLWidgets)
endif()

add_qt_test_module(test_spatial_runtime TestSpatialRuntime.cpp)
target_link_libraries(test_spatial_runtime PRIVATE FluentQt::Spatial)
# Compile the same backend without the renderer target's RHI definition. This
# exercises the older-Qt/unsupported-platform fallback even on a modern SDK.
add_qt_test_module(test_spatial_texture_backend_fallback TestSpatialTextureBackend.cpp
    "${PROJECT_SOURCE_DIR}/src/compatibility/private/spatial/renderer/SpatialTextureBackend_p.cpp")
add_qt_test_module(test_spatial_plugin TestSpatialPlugin.cpp)
target_link_libraries(test_spatial_plugin PRIVATE FluentQt::Spatial)
if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(test_spatial_runtime PRIVATE Qt6::OpenGLWidgets)
endif()
