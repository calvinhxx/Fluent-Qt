function(fluent_qt_add_particles_library)
    add_library(FluentQtParticles STATIC ${FLUENT_QT_PARTICLE_SOURCES})
    add_library(FluentQt::Particles ALIAS FluentQtParticles)
    fluent_qt_configure_cpp_target(FluentQtParticles)
    fluent_qt_enable_project_warnings(FluentQtParticles)
    fluent_qt_enable_sanitizers(FluentQtParticles)
    target_link_libraries(FluentQtParticles PUBLIC FluentQt::Plugins)
    target_compile_definitions(FluentQtParticles PUBLIC FLUENT_QT_HAS_PARTICLES=1)
    set_target_properties(FluentQtParticles PROPERTIES
        EXPORT_NAME Particles OUTPUT_NAME FluentQtParticles
        ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}"
        AUTOGEN_BUILD_DIR "${PROJECT_BINARY_DIR}/FluentQtParticles_autogen")
endfunction()
