# The Conan recipe.
#
#   conan create . --build=missing -tf packaging/conan/test_package
#
# The library's own build does the work: this recipe turns Conan's
# settings into the CMake options the build already has, and describes
# what was installed. The package carries the library and its headers -
# Doctor, the tests and the examples are the project's, not a consumer's.
#
# Every CF_WITH_* option is a recipe option with the build's default, and
# with_yaml brings libyaml from ConanCenter, the one dependency any build
# of this library can have (docs/Dependencies.md).

import os
import re

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load

FEATURES = {
    "ini": True,
    "json": True,
    "toml": True,
    "properties": True,
    "yaml": False,
    "xml": False,
    "sqlite": False,
    "real": True,
    "dir": True,
    "dotenv": True,
    "emit": True,
    "image": True,
    "report": True,
}


class ConfigForgeConan(ConanFile):
    name = "configforge"
    author = "Mustafa Selçuk Çağlar"
    license = "Zlib"
    url = "https://github.com/develooper1994/configforge"
    homepage = url
    description = "Configuration for C whose every value can say why it is what it is"
    topics = ("configuration", "provenance", "c89", "embedded")
    package_type = "library"
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], "fPIC": [True, False]}
    options.update({"with_" + f: [True, False] for f in FEATURES})
    default_options = {"shared": False, "fPIC": True}
    default_options.update({"with_" + f: d for f, d in FEATURES.items()})

    exports_sources = (
        "CMakeLists.txt", "CMakePresets.json", "LICENSE", "cmake/*", "include/*",
        "src/*", "scripts/*", "tools/*", "tests/*", "examples/*", "docs/*")

    def set_version(self):
        # cf_version.h is the one place the version is written.
        header = load(self, os.path.join(self.recipe_folder, "include", "configforge",
                                         "cf_version.h"))
        parts = [re.search(r"#define CF_VERSION_%s\s+(\d+)" % p, header).group(1)
                 for p in ("MAJOR", "MINOR", "PATCH")]
        self.version = ".".join(parts)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")
        # C, with a C++03 wrapper that is a header: no C++ runtime to pin.
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")

    def requirements(self):
        if self.options.with_yaml:
            self.requires("libyaml/0.2.5")
        if self.options.with_xml:
            self.requires("expat/[>=2.6 <3]")
        if self.options.with_sqlite:
            self.requires("sqlite3/[>=3.35 <4]")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.cache_variables["CF_BUILD_TESTING"] = False
        tc.cache_variables["CF_BUILD_TOOLS"] = False
        tc.cache_variables["CF_BUILD_EXAMPLES"] = False
        tc.cache_variables["CF_ENABLE_GATES"] = False
        for f in FEATURES:
            tc.cache_variables["CF_WITH_" + f.upper()] = bool(self.options.get_safe("with_" + f))
        tc.generate()
        CMakeDeps(self).generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, self.package_folder + "/licenses")
        CMake(self).install()

    def package_info(self):
        self.cpp_info.libs = ["configforge"]
        self.cpp_info.set_property("cmake_file_name", "ConfigForge")
        self.cpp_info.set_property("cmake_target_name", "ConfigForge::configforge")
        self.cpp_info.set_property("pkg_config_name", "configforge")
        # No defines: the installed cf_build_options.h states this build's
        # options, CF_SHARED among them, so a consumer cannot disagree.
