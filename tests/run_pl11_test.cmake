# run_pl11_test.cmake — cmake -P script for end-to-end PL-11 execution tests
#
# Required variables (pass via -D on command line):
#   PL11C       — path to pl11c binary
#   LLC         — path to llc binary
#   CLANG       — path to clang binary
#   SOURCE      — path to .pl11 source file
#   TMPDIR      — writable directory for intermediate files
#
# Optional variables:
#   EXPECTED_FILE — path to file containing expected stdout; if set, the
#                   actual output is diff'd against it and the test fails
#                   on mismatch.

cmake_minimum_required(VERSION 3.16)

# Derive a base name for temp files from the source path
get_filename_component(BASE "${SOURCE}" NAME_WE)
set(LL_FILE  "${TMPDIR}/${BASE}.ll")
set(OBJ_FILE "${TMPDIR}/${BASE}.o")
set(EXE_FILE "${TMPDIR}/${BASE}")
set(OUT_FILE "${TMPDIR}/${BASE}.out")

# ── Step 1: emit LLVM IR ───────────────────────────────────────────────────
execute_process(
    COMMAND "${PL11C}" --emit-llvm "${SOURCE}"
    OUTPUT_FILE "${LL_FILE}"
    RESULT_VARIABLE RC
    ERROR_VARIABLE STDERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "pl11c --emit-llvm failed for ${SOURCE}:\n${STDERR}")
endif()

# ── Step 2: assemble to object ─────────────────────────────────────────────
execute_process(
    COMMAND "${LLC}" -filetype=obj "${LL_FILE}" -o "${OBJ_FILE}"
    RESULT_VARIABLE RC
    ERROR_VARIABLE STDERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "llc failed for ${LL_FILE}:\n${STDERR}")
endif()

# ── Step 3: link ───────────────────────────────────────────────────────────
execute_process(
    COMMAND "${CLANG}" "${OBJ_FILE}" -o "${EXE_FILE}"
    RESULT_VARIABLE RC
    ERROR_VARIABLE STDERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "clang link failed for ${OBJ_FILE}:\n${STDERR}")
endif()

# ── Step 4: run ────────────────────────────────────────────────────────────
execute_process(
    COMMAND "${EXE_FILE}"
    OUTPUT_FILE "${OUT_FILE}"
    RESULT_VARIABLE RC
    ERROR_VARIABLE STDERR
)
if(NOT RC EQUAL 0)
    message(FATAL_ERROR "Program exited with code ${RC}: ${EXE_FILE}\n${STDERR}")
endif()

# ── Step 5: compare output (optional) ─────────────────────────────────────
if(DEFINED EXPECTED_FILE)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUT_FILE}" "${EXPECTED_FILE}"
        RESULT_VARIABLE RC
    )
    if(NOT RC EQUAL 0)
        file(READ "${OUT_FILE}"      ACTUAL)
        file(READ "${EXPECTED_FILE}" EXPECTED)
        message(FATAL_ERROR
            "Output mismatch for ${BASE}:\n"
            "--- expected ---\n${EXPECTED}\n"
            "+++ actual ---\n${ACTUAL}\n")
    endif()
endif()
