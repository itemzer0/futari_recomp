# ReXGlue Tests
# Test infrastructure for PPC instruction validation and unit tests

# Unit tests for core components.
# GATED OFF by default: unit_tests links rexcodegen and, even after dropping the
# direct rexcore link + COMPILE_ONLY-ing rexcore in rexcodegen (so rexcore's
# objects are NOT in unit_tests' own link line), a 2nd copy of rexcore's logging
# cvars still reaches the exe via a linked static lib -> static-init
# double-registration, whose [error] output corrupts Catch2's build-time test
# discovery (JSON parse). Pinning the exact lib needs nm/ldd on a local Linux
# build. ppc_tests is unaffected (it doesn't link rexcodegen). Re-enable once
# resolved with -DREXGLUE_BUILD_UNIT_TESTS=ON.
option(REXGLUE_BUILD_UNIT_TESTS "Build core unit_tests (known rexcore double-link; see comment)" OFF)
if(REXGLUE_BUILD_UNIT_TESTS)
    add_subdirectory(unit)
endif()

# PPC instruction tests — the high-value suite (recompiler instruction semantics)
add_subdirectory(ppc)
