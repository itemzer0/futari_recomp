/**
 * @file        codegen/codegen_flags.cpp
 * @brief       CVar definitions for the codegen pipeline
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include <rex/cvar.h>

// clang-format off

//=============================================================================
// Codegen/Output
//=============================================================================

REXCVAR_DEFINE_UINT32(max_file_size_bytes, 2097152, "Codegen",
                      "Target maximum source file size in bytes")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(65536, 67108864);

REXCVAR_DEFINE_UINT32(progress_log_frequency, 100, "Codegen",
                      "Log progress every N functions")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

//=============================================================================
// Codegen/Analysis
//=============================================================================

REXCVAR_DEFINE_UINT32(max_discovery_iterations, 1000, "Codegen",
                      "Max iterations for function discovery convergence")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_UINT32(max_vtable_iterations, 100, "Codegen",
                      "Max iterations for vtable discovery")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_BOOL(enable_data_ptr_scan, true, "Codegen",
                    "Harvest function pointers from non-RTTI data pointer-tables/vtables "
                    "(runs of >=2 consecutive code pointers in data sections)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

// Default OFF: a lis/addi pair that materializes a code address has no
// corroboration that the address is a FUNCTION (vs a jump-table base, a case
// body, or a data address), so it forges bogus functions that split real ones
// and orphan internal gotos (e.g. futari shell sub_820DFFD0, a computed
// jump-table base). It also never paid off in practice -- the Class-2 targets
// it was meant to catch turned out to be runtime-populated table pointers, not
// lis/ori-computed -- so it stays opt-in.
REXCVAR_DEFINE_BOOL(enable_lis_ori_fn_scan, false, "Codegen",
                    "Harvest functions reached via runtime-computed indirect calls by scanning "
                    "for lis/addi and lis/ori pairs that materialize a code address (opt-in: "
                    "prone to false positives on jump-table bases)")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REXCVAR_DEFINE_UINT32(max_resolve_iterations, 100, "Codegen",
                      "Max iterations for call target resolution")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_UINT32(max_eh_states, 100, "Codegen",
                      "Max C++ EH states before rejecting handler")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_UINT32(max_eh_try_blocks, 50, "Codegen",
                      "Max try blocks before rejecting handler")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_UINT32(max_eh_ip_map_entries, 200, "Codegen",
                      "Max IP-to-state map entries")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_UINT32(max_seh_scope_entries, 100, "Codegen",
                      "Max SEH scope table entries")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

//=============================================================================
// Codegen/Discovery
//=============================================================================

REXCVAR_DEFINE_UINT32(backward_scan_limit, 64, "Codegen",
                      "Max instructions to scan backward for jump table patterns")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 10000);

REXCVAR_DEFINE_UINT32(max_jump_table_entries, 512, "Codegen",
                      "Max entries per detected jump table")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 100000);

REXCVAR_DEFINE_UINT32(max_blocks_per_function, 10000, "Codegen",
                      "Safety limit on blocks per function")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly)
    .range(1, 1000000);

// clang-format on
