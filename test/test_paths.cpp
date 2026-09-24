#include <catch2/catch_test_macros.hpp>
#include "fs/Path.h"

using namespace misa::fs;

TEST_CASE("Paths: normalize separators, dots and drive letters", "[paths]") {
    REQUIRE(normalize("a/./b/../c") == "a/c");
    REQUIRE(normalize("C:\\Users\\x\\..\\y.asm") == "c:/Users/y.asm");
    REQUIRE(normalize("/home//u/./lib/../main.asm") == "/home/u/main.asm");
    REQUIRE(normalize("/../x") == "/x");      // never above the root
    REQUIRE(normalize("../x") == "../x");     // relative paths keep leading ..
    REQUIRE(normalize("//server/share/x") == "//server/share/x");
}

TEST_CASE("Paths: absolute detection", "[paths]") {
    REQUIRE(isAbsolute("/x"));
    REQUIRE(isAbsolute("c:/x"));
    REQUIRE(isAbsolute("C:\\x"));
    REQUIRE(isAbsolute("//server/x"));
    REQUIRE_FALSE(isAbsolute("x/y.asm"));
    REQUIRE_FALSE(isAbsolute("@u/lib/main.asm"));
}

TEST_CASE("Paths: dirname, filename, extension", "[paths]") {
    REQUIRE(dirname("c:/p/lib/util.asm") == "c:/p/lib");
    REQUIRE(dirname("c:/main.asm") == "c:/");
    REQUIRE(dirname("/main.asm") == "/");
    REQUIRE(filename("c:/p/lib/util.asm") == "util.asm");
    REQUIRE(extensionLower("x/Sprite.PNG") == ".png");
    REQUIRE(extensionLower("x/noext") == "");
}

TEST_CASE("Paths: file URI to path", "[paths]") {
    REQUIRE(uriToPath("file:///c%3A/Users/a%20b/x.asm") == "c:/Users/a b/x.asm");
    REQUIRE(uriToPath("file:///C:/x.asm") == "c:/x.asm");
    REQUIRE(uriToPath("file:///home/u/x.asm") == "/home/u/x.asm");
    REQUIRE(uriToPath("file://server/share/x.asm") == "//server/share/x.asm");
    REQUIRE_FALSE(uriToPath("untitled:Untitled-1").has_value());
}

TEST_CASE("Paths: path to file URI uses the VS Code form", "[paths]") {
    REQUIRE(pathToUri("c:/Program Files (x86)/x.asm") ==
            "file:///c%3A/Program%20Files%20%28x86%29/x.asm");
    REQUIRE(pathToUri("/home/u/x.asm") == "file:///home/u/x.asm");
    REQUIRE(uriToPath(pathToUri("c:/a b/é.asm")) == "c:/a b/é.asm");
}

TEST_CASE("Paths: include resolution", "[paths]") {
    PathConfig cfg{"c:/data/user_projects", "d:/steam/sample_projects"};

    SECTION("relative to the including file") {
        auto r = resolveIncludePath("lib/util.asm", "c:/p", cfg);
        REQUIRE(r.error == ResolveError::None);
        REQUIRE(r.path == "c:/p/lib/util.asm");
        REQUIRE(resolveIncludePath("../shared/x.asm", "c:/p/lib", cfg).path == "c:/p/shared/x.asm");
    }
    SECTION("virtual folders") {
        REQUIRE(resolveIncludePath("@u/gfx/main.asm", "c:/p", cfg).path == "c:/data/user_projects/gfx/main.asm");
        REQUIRE(resolveIncludePath("@s/lander/main.asm", "c:/p", cfg).path == "d:/steam/sample_projects/lander/main.asm");
    }
    SECTION("unset virtual folder") {
        PathConfig empty;
        REQUIRE(resolveIncludePath("@u/gfx/main.asm", "c:/p", empty).error == ResolveError::UserFolderUnset);
        REQUIRE(resolveIncludePath("@s/x.asm", "c:/p", empty).error == ResolveError::SampleFolderUnset);
    }
    SECTION("absolute paths ignore the base directory") {
        REQUIRE(resolveIncludePath("C:\\libs\\x.asm", "c:/p", cfg).path == "c:/libs/x.asm");
    }
    SECTION("relative path without a base (unsaved document)") {
        REQUIRE(resolveIncludePath("x.asm", "", cfg).error == ResolveError::RelativeWithoutBase);
        REQUIRE(resolveIncludePath("", "c:/p", cfg).error == ResolveError::EmptyPath);
    }
}
