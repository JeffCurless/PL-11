cmake_minimum_required(VERSION 3.16)
project(pl11c CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================================
# Option: build with LLVM IR codegen backend
# ============================================================
option(PL11_WITH_LLVM "Enable LLVM codegen backend" ON)

# ============================================================
# Source files (lexer + parser + sema — always built)
# ============================================================
set(PL11_SOURCES
    src/lexer/pl11_lexer.cpp
    src/parser/pl11_parser.cpp
    src/sema/pl11_sema.cpp
    src/main.cpp
)

if(PL11_WITH_LLVM)
    list(APPEND PL11_SOURCES src/codegen/pl11_codegen.cpp)
endif()

add_executable(pl11c ${PL11_SOURCES})

target_include_directories(pl11c PRIVATE src)

# ============================================================
# LLVM linkage
# ============================================================
if(PL11_WITH_LLVM)
    # Prefer llvm-config over find_package to avoid cmake packaging issues
    # (e.g. missing zstd cmake targets on some Debian/Ubuntu installs)
    find_program(LLVM_CONFIG
        NAMES llvm-config llvm-config-19 llvm-config-18 llvm-config-17
        REQUIRED)

    execute_process(COMMAND ${LLVM_CONFIG} --version
        OUTPUT_VARIABLE LLVM_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
    message(STATUS "Found LLVM ${LLVM_VERSION} via ${LLVM_CONFIG}")

    execute_process(COMMAND ${LLVM_CONFIG} --includedir
        OUTPUT_VARIABLE LLVM_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND ${LLVM_CONFIG} --cppflags
        OUTPUT_VARIABLE LLVM_CPPFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND ${LLVM_CONFIG} --ldflags
        OUTPUT_VARIABLE LLVM_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(
        COMMAND ${LLVM_CONFIG} --libs core support analysis target mc passes irreader
        OUTPUT_VARIABLE LLVM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

    execute_process(COMMAND ${LLVM_CONFIG} --system-libs
        OUTPUT_VARIABLE LLVM_SYSTEM_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

    # Strip leading/trailing whitespace; convert space-separated flags to list
    separate_arguments(LLVM_CPPFLAGS  UNIX_COMMAND "${LLVM_CPPFLAGS}")
    separate_arguments(LLVM_LDFLAGS   UNIX_COMMAND "${LLVM_LDFLAGS}")
    separate_arguments(LLVM_LIBS      UNIX_COMMAND "${LLVM_LIBS}")
    separate_arguments(LLVM_SYSTEM_LIBS UNIX_COMMAND "${LLVM_SYSTEM_LIBS}")

    target_compile_definitions(pl11c PRIVATE PL11_WITH_LLVM)
    target_compile_options(pl11c PRIVATE ${LLVM_CPPFLAGS})
    target_include_directories(pl11c PRIVATE ${LLVM_INCLUDE_DIRS})
    # Add local lib dir for workaround libraries (e.g. libzstd.so symlink when libzstd-dev is absent)
    target_link_options(pl11c PRIVATE ${LLVM_LDFLAGS}
        -L${CMAKE_BINARY_DIR}/lib)
    target_link_libraries(pl11c PRIVATE ${LLVM_LIBS} ${LLVM_SYSTEM_LIBS})
endif()

# ============================================================
# Compiler warnings
# ============================================================
if(MSVC)
    target_compile_options(pl11c PRIVATE /W4)
else()
    target_compile_options(pl11c PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
    )
endif()

# ============================================================
# Install
# ============================================================
install(TARGETS pl11c DESTINATION bin)

# ============================================================
# Tests (optional — requires CTest)
# ============================================================
enable_testing()

# Test: lex a sample program
add_test(NAME lex_hello
    COMMAND pl11c --lex ${CMAKE_SOURCE_DIR}/tests/hello.pl11
)

# Test: parse a sample program
add_test(NAME parse_hello
    COMMAND pl11c --parse ${CMAKE_SOURCE_DIR}/tests/hello.pl11
)

# Test: semantic analysis
add_test(NAME sema_hello
    COMMAND pl11c --sema ${CMAKE_SOURCE_DIR}/tests/hello.pl11
)

# Test: emit LLVM IR (only if built with LLVM)
if(PL11_WITH_LLVM)
    add_test(NAME emit_llvm_hello
        COMMAND pl11c --emit-llvm ${CMAKE_SOURCE_DIR}/tests/hello.pl11
    )
endif()
