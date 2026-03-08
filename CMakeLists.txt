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

# Locate LLC and Clang for execution tests (WITH_LLVM builds only)
if(PL11_WITH_LLVM)
    find_program(LLC   NAMES llc-19   llc-18   llc-17   llc   REQUIRED)
    find_program(CLANG NAMES clang-19 clang-18 clang-17 clang REQUIRED)
    message(STATUS "Test tools: LLC=${LLC}  CLANG=${CLANG}")
endif()

# ── Helper: register lex / parse / sema / ir / exec tests for one source ──
#
# pl11_add_tests(NAME <name> SOURCE <file.pl11>)
#
# Always registers:
#   lex_<name>   — tokenise only
#   parse_<name> — parse to AST
#   sema_<name>  — semantic analysis
#
# When PL11_WITH_LLVM is ON, additionally registers:
#   ir_<name>   — emit LLVM IR (syntax check)
#   exec_<name> — compile + run; if tests/<name>.expected exists, compare stdout
#
function(pl11_add_tests)
    cmake_parse_arguments(ARG "" "NAME;SOURCE" "" ${ARGN})
    set(SRC "${CMAKE_SOURCE_DIR}/${ARG_SOURCE}")

    add_test(NAME lex_${ARG_NAME}
        COMMAND pl11c --lex "${SRC}")
    add_test(NAME parse_${ARG_NAME}
        COMMAND pl11c --parse "${SRC}")
    add_test(NAME sema_${ARG_NAME}
        COMMAND pl11c --sema "${SRC}")

    if(PL11_WITH_LLVM)
        add_test(NAME ir_${ARG_NAME}
            COMMAND pl11c --emit-llvm "${SRC}")

        set(EXPECTED "${CMAKE_SOURCE_DIR}/tests/${ARG_NAME}.expected")
        if(EXISTS "${EXPECTED}")
            set(EXPECTED_ARG "-DEXPECTED_FILE=${EXPECTED}")
        else()
            set(EXPECTED_ARG "")
        endif()

        add_test(NAME exec_${ARG_NAME}
            COMMAND "${CMAKE_COMMAND}"
                -DPLC11C=$<TARGET_FILE:pl11c>
                -DPL11C=$<TARGET_FILE:pl11c>
                -DLLC=${LLC}
                -DCLANG=${CLANG}
                -DSOURCE=${SRC}
                -DTMPDIR=${CMAKE_BINARY_DIR}/test_tmp
                ${EXPECTED_ARG}
                -P "${CMAKE_SOURCE_DIR}/tests/run_pl11_test.cmake"
        )
    endif()
endfunction()

# ── Register all test programs ─────────────────────────────────────────────
pl11_add_tests(NAME hello         SOURCE tests/hello.pl11)
pl11_add_tests(NAME fibonacci     SOURCE tests/fibonacci.pl11)
pl11_add_tests(NAME gcd           SOURCE tests/gcd.pl11)
pl11_add_tests(NAME control_flow  SOURCE tests/control_flow.pl11)
pl11_add_tests(NAME sort          SOURCE tests/sort.pl11)
pl11_add_tests(NAME print_demo    SOURCE tests/print_demo.pl11)
pl11_add_tests(NAME while_until   SOURCE tests/while_until.pl11)
pl11_add_tests(NAME for_until     SOURCE tests/for_until.pl11)
pl11_add_tests(NAME do_loop       SOURCE tests/do_loop.pl11)
pl11_add_tests(NAME primes        SOURCE tests/primes.pl11)
pl11_add_tests(NAME comments      SOURCE tests/comments.pl11)
pl11_add_tests(NAME array_decl    SOURCE tests/array_decl.pl11)
pl11_add_tests(NAME for_step      SOURCE tests/for_step.pl11)
pl11_add_tests(NAME for_nostart   SOURCE tests/for_nostart.pl11)
pl11_add_tests(NAME neq_op        SOURCE tests/neq_op.pl11)
pl11_add_tests(NAME assign_op     SOURCE tests/assign_op.pl11)
pl11_add_tests(NAME push_pop      SOURCE tests/push_pop.pl11)
