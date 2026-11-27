import os

from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, download, get, patch, rmdir


class FluentQtConan(ConanFile):
    name = "fluent-qt"
    package_type = "static-library"
    license = "MIT", "OFL-1.1"
    homepage = "https://github.com/calvinhxx/Fluent-Qt"
    description = "Fluent UI components for native Qt Widgets desktop applications."
    settings = "os", "arch", "compiler", "build_type"
    options = {"fPIC": [True, False]}
    default_options = {"fPIC": True}

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def requirements(self):
        self.requires("qt/[>=6.2 <7]", transitive_headers=True, transitive_libs=True)

    def validate(self):
        if self.settings.os not in ("Windows", "Linux", "Macos"):
            raise ConanInvalidConfiguration("The Widgets SDK recipe targets Windows, Linux and macOS")
        if self.settings.compiler.get_safe("cppstd"):
            check_min_cppstd(self, "17")

    def layout(self):
        cmake_layout(self, src_folder="src")

    def source(self):
        version = str(self.version)
        get(self, **self.conan_data["sources"][version], strip_root=True)
        backport = self.conan_data.get("direct_include_backport", {}).get(version)
        if backport:
            patch_path = os.path.join(self.source_folder, "direct-include.patch")
            download(self, filename=patch_path, **backport)
            patch(self, patch_file=patch_path)

    def generate(self):
        CMakeDeps(self).generate()
        tc = CMakeToolchain(self)
        tc.variables["FLUENT_QT_BUILD_EXAMPLES"] = False
        tc.variables["FLUENT_QT_DEBUG_RELEASE_RUNTIME"] = False
        tc.variables["CMAKE_INSTALL_LIBDIR"] = "lib"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        CMake(self).install(component="Development")
        share = os.path.join(self.package_folder, "share")
        copy(self, "*", os.path.join(share, "licenses", "FluentQt"),
             os.path.join(self.package_folder, "licenses"))
        rmdir(self, share)

    def package_info(self):
        self.cpp_info.set_property("cmake_file_name", "FluentQt")
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs = [os.path.join("lib", "cmake", "FluentQt")]
        self.cpp_info.includedirs = ["include", "include/FluentQt"]
        self.cpp_info.libs = ["FluentQt"]
        if self.settings.os == "Macos":
            self.cpp_info.frameworks = ["CoreGraphics"]
            self.cpp_info.system_libs = ["objc"]
