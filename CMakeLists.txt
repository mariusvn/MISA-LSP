cmake_minimum_required(VERSION 3.20)
project(misa-lsp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

option(BUILD_TESTING "Build tests" ON)

include(cmake/Dependencies.cmake)

# Strict warnings
if(MSVC)
    add_compile_options(/W4 /wd4100 /wd4505 /utf-8)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

set(CORE_SOURCES
    src/transport/JsonRpcStream.cpp
    src/protocol/Serialization.cpp
    src/server/Server.cpp
    src/server/Workspace.cpp
    src/fs/Path.cpp
    src/fs/SourceProvider.cpp
    src/text/TextDocument.cpp
    src/kb/KnowledgeBase.cpp
    src/lang/Ast.cpp
    src/lang/Lexer.cpp
    src/lang/ExprParser.cpp
    src/lang/Parser.cpp
    src/lang/SymbolTable.cpp
    src/lang/SemanticAnalyzer.cpp
    src/lang/Compilation.cpp
    src/features/Diagnostics.cpp
    src/features/Hover.cpp
    src/features/Completion.cpp
    src/features/DocumentSymbols.cpp
    src/features/Definition.cpp
    src/features/References.cpp
    src/features/SignatureHelp.cpp
    src/features/Folding.cpp
    src/features/DocumentLinks.cpp
)

add_library(misa-core STATIC ${CORE_SOURCES})
target_include_directories(misa-core PUBLIC src)
target_link_libraries(misa-core PUBLIC nlohmann_json::nlohmann_json)

add_executable(misa-lsp src/main.cpp)
target_link_libraries(misa-lsp PRIVATE misa-core)

add_executable(misa-lint tools/lint.cpp)
target_link_libraries(misa-lint PRIVATE misa-core)

if(BUILD_TESTING)
    enable_testing()
    add_executable(misa-tests
        test/test_lexer.cpp
        test/test_parser.cpp
        test/test_expr.cpp
        test/test_semantic.cpp
        test/test_features.cpp
        test/test_paths.cpp
        test/test_include.cpp
        test/test_workspace.cpp
    )
    target_link_libraries(misa-tests PRIVATE misa-core Catch2::Catch2WithMain)
    include(CTest)
    include(Catch)
    catch_discover_tests(misa-tests)
endif()
