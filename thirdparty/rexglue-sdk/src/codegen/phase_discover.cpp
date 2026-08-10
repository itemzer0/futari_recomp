/**
 * @file        codegen/phase_discover.cpp
 * @brief       Discover phase: iterative function block discovery
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */

#include "codegen_flags.h"
#include "decoded_binary.h"
#include <rex/codegen/function_scanner.h>

#include <array>
#include <bitset>
#include <cstdlib>
#include <unordered_set>

#include <rex/codegen/phases.h>
#include "phase_helpers.h"

#include <rex/codegen/vtable_scanner.h>
#include <rex/logging.h>

#include "codegen_logging.h"
#include <rex/memory/utils.h>

#include <ppc.h>

using rex::codegen::ppc::Opcode;
using rex::memory::load_and_swap;

namespace rex::codegen {

namespace {

//=============================================================================
// Discover Phase: iterative function block discovery
//=============================================================================

void discoverFunction(CodegenContext& ctx, uint32_t funcAddr,
                      const std::unordered_set<uint32_t>& knownFunctions) {
  auto& graph = ctx.graph;
  auto& binary = ctx.binary();
  auto& decoded = ctx.decoded();

  auto* node = graph.getFunction(funcAddr);
  if (!node)
    return;

  // Skip if already discovered
  if (!node->canDiscover()) {
    REXCODEGEN_TRACE("Analyze: function 0x{:08X} already discovered, skipping", funcAddr);
    return;
  }

  // Imports don't need block discovery
  if (node->isImport()) {
    node->discoverAsImport();
    return;
  }

  REXCODEGEN_TRACE("Analyze: discovering function 0x{:08X} ({})", funcAddr, node->name());

  // Lookup pdataSize for exception handler boundary
  uint32_t pdataSize = 0;

  // For CONFIG functions: use only the explicitly declared size (if any)
  // If no size specified (size=0), let discovery find natural boundaries via region
  // Don't inherit PDATA sizes for CONFIG functions - they're user hints for entry points
  if (node->authority() == FunctionAuthority::CONFIG) {
    pdataSize = node->size();  // 0 if not specified, which is correct
    REXCODEGEN_TRACE("Analyze: 0x{:08X} is CONFIG, using declared size={}", funcAddr, pdataSize);
  } else {
    // For non-CONFIG functions, use PDATA size if available
    auto pdataIt = ctx.scan.pdataSizes.find(funcAddr);
    if (pdataIt != ctx.scan.pdataSizes.end()) {
      pdataSize = pdataIt->second;
      REXCODEGEN_TRACE("Analyze: 0x{:08X} using PDATA size={}", funcAddr, pdataSize);
    }
  }

  // Find the code region containing this function
  const CodeRegion* region = nullptr;
  for (const auto& r : ctx.scan.codeRegions) {
    if (r.contains(funcAddr)) {
      region = &r;
      break;
    }
  }
  if (!region) {
    REXCODEGEN_WARN("Analyze: function 0x{:08X} not in any code region", funcAddr);
    return;
  }

  // Pass pdataSize so forward branches within function extent are correctly identified
  auto result = discoverBlocks(decoded, funcAddr, *region, knownFunctions, pdataSize,
                               &ctx.Config().switchTables);

  if (result.blocks.empty()) {
    REXCODEGEN_WARN("Analyze: no blocks found for function 0x{:08X}", funcAddr);
    return;
  }

  // snooper the function with the discovered blocks and instructions
  node->discover(std::move(result.blocks), std::move(result.instructions),
                 std::move(result.labels));

  // Add jump tables (targets become labels in the function)
  for (const auto& jt : result.jumpTables) {
    graph.addJumpTableToFunction(funcAddr, jt);
  }

  // Register external call targets as new functions (bl only, not b)
  for (uint32_t target : result.externalCalls) {
    if (!graph.isEntryPoint(target) && !graph.isImport(target)) {
      if (binary.isInImportExportRange(target)) {
        continue;
      }
      graph.addFunction(target, 4, FunctionAuthority::DISCOVERED, true);
    }
  }

  // Add unresolved branches for later resolution
  for (const auto& branch : result.unresolvedBranches) {
    graph.addUnresolvedJumpToFunction(funcAddr, branch.site, branch.target, branch.isCall,
                                      branch.isConditional);
  }

  // Scan exception handler regions for branches not in discovered blocks
  if (pdataSize > 0) {
    std::unordered_set<uint32_t> discoveredAddrs;
    for (const auto& block : result.blocks) {
      for (uint32_t addr = block.base; addr < block.base + block.size; addr += 4) {
        discoveredAddrs.insert(addr);
      }
    }

    uint32_t pdataEnd = funcAddr + pdataSize;
    const uint8_t* funcData = binary.translate(funcAddr);
    if (funcData) {
      for (uint32_t offset = 0; offset < pdataSize; offset += 4) {
        uint32_t site = funcAddr + offset;

        // Skip if already discovered by normal control flow
        if (discoveredAddrs.count(site))
          continue;

        // Skip if marked invalid
        auto invalidIt = ctx.analysisState().invalidInstructions.find(site);
        if (invalidIt != ctx.analysisState().invalidInstructions.end()) {
          continue;
        }

        uint32_t insn = load_and_swap<uint32_t>(funcData + offset);
        uint32_t opcode = PPC_OP(insn);

        if (opcode != PPC_OP_B && opcode != PPC_OP_BC)
          continue;

        uint32_t target = 0;
        bool isCall = PPC_BL(insn);
        bool isAbsolute = PPC_BA(insn);

        if (opcode == PPC_OP_B) {
          int32_t branchOffset = PPC_BI(insn);
          target = isAbsolute ? static_cast<uint32_t>(branchOffset) : site + branchOffset;
        } else {
          int32_t branchOffset = PPC_BD(insn);
          target = isAbsolute ? static_cast<uint32_t>(branchOffset) : site + branchOffset;
        }

        // Skip internal jumps within pdata region
        if (!isCall && target >= funcAddr && target < pdataEnd) {
          continue;
        }

        graph.addUnresolvedJumpToFunction(funcAddr, site, target, isCall, false);

        // Register call targets as new functions
        if (isCall && !graph.isEntryPoint(target) && !graph.isImport(target)) {
          if (binary.isInImportExportRange(target)) {
            continue;
          }
          graph.addFunction(target, 4, FunctionAuthority::DISCOVERED, true);
        }
      }
    }
  }
}

//=============================================================================
// Shared helpers for the heuristic function-pointer scans below.
//=============================================================================

// A plausible function-pointer value: 4-aligned, in an executable section,
// outside the import/export thunk table, and not a 64KB-"round" address (those
// are section/page addresses that appear in data tables, not real entries).
// Deliberately does NOT require a *decoded* code region -- the targets these
// scans exist to find are exactly the ones missing from decoded regions.
inline bool isPlausibleFnPtr(const BinaryView& binary, uint32_t a) {
  if ((a & 0x3u) != 0)
    return false;
  if (!binary.isExecutable(a) || binary.isInImportExportRange(a))
    return false;
  if ((a & 0xFFFFu) == 0)
    return false;
  return true;
}

// A real function entry is immediately preceded by the previous function's
// terminator (or inter-function padding). Requiring this rejects candidates
// that point into the MIDDLE of a real function -- registering those would
// split the function and orphan its internal branches ("unresolved branch"
// FATALs). Reads the raw preceding word from the image (not the decoder) so it
// works for targets outside any decoded code region.
inline bool precededByFnBoundary(const BinaryView& binary, uint32_t target, bool strict) {
  const uint8_t* p = binary.translate(target - 4);
  if (!p)
    return false;  // can't verify -> conservatively reject
  uint32_t prev = load_and_swap<uint32_t>(p);
  // Definitive function boundaries, trusted in both modes:
  if (prev == 0x00000000u || prev == 0x60000000u)
    return true;  // zero / nop padding between functions
  if (prev == 0x4E800020u)
    return true;  // unconditional blr -- the previous function returned

  // Strict mode (the lis/ori scan, which has no run>=2 corroboration) stops
  // here. The endings below are AMBIGUOUS -- they can end a function OR sit
  // mid-function -- so trusting them with no corroboration forges bogus
  // functions: e.g. a `bctr` is usually a jump-table dispatch whose case bodies
  // follow it, so the address after it (the table base / case 0, computed by a
  // lis/addi) is NOT a function entry; registering it splits the function and
  // orphans the internal `goto` into it (futari shell sub_820DFFD0).
  if (strict)
    return false;

  // Loose mode (the data-table scan) has the run>=2 contiguity corroboration,
  // so it can also accept these ambiguous endings.
  if ((prev & 0xFC0007FEu) == 0x4C000020u)
    return true;  // bclr family (conditional returns)
  if ((prev & 0xFC0007FEu) == 0x4C000420u)
    return true;  // bcctr family (bctr, computed tail-jumps)
  if ((prev & 0xFC000001u) == 0x48000000u)
    return true;  // b / ba (unconditional branch / tail branch)
  return false;
}

//=============================================================================
// Data Pointer-Table Scan: harvest non-RTTI vtables / dispatch tables.
//
// The RTTI VTableScanner only finds vtables reachable from a Complete Object
// Locator. Classes compiled without RTTI -- and plain function-pointer tables
// (mode/state dispatch, callback arrays) -- leave runs of code pointers in data
// sections that the call-graph walk never reaches, so their targets are never
// recompiled and a runtime dispatch through them FATALs on an unregistered
// address. Harvest runs of >=2 consecutive 4-aligned pointers into executable
// memory as function entry points.
//
// Unlike the (disabled) lis/addi instruction scan, the >=2 contiguity
// requirement makes false positives unlikely: a lone code-range dword in data
// is usually not a function pointer, but two-plus in a row almost always are.
//=============================================================================
size_t dataPointerTableScan(CodegenContext& ctx) {
  if (!REXCVAR_GET(enable_data_ptr_scan))
    return 0;
  if (!ctx.hasDecoded())
    return 0;

  auto& graph = ctx.graph;
  auto& binary = ctx.binary();
  auto& decoded = ctx.decoded();

  auto isCodePtr = [&](uint32_t a) { return isPlausibleFnPtr(binary, a); };

  size_t newFunctions = 0;

  for (const auto& section : binary.sections()) {
    if (!section.data || section.size < 8)
      continue;

    const uint8_t* data = section.data;
    uint32_t base = section.baseAddress;
    size_t size = section.size & ~size_t{3};

    size_t i = 0;
    while (i + 4 <= size) {
      uint32_t addr = base + static_cast<uint32_t>(i);
      // Skip dwords inside decoded code: there, load/store opcodes (e.g.
      // lwz r,d(rA)) alias code-range values and would forge thousands of
      // bogus tables. Pointer tables live in data, never in the code stream.
      if (decoded.isInCodeRegion(addr)) {
        i += 4;
        continue;
      }
      if (!isCodePtr(load_and_swap<uint32_t>(data + i))) {
        i += 4;
        continue;
      }
      // Measure the run of consecutive code pointers.
      size_t j = i;
      while (j + 4 <= size && !decoded.isInCodeRegion(base + static_cast<uint32_t>(j)) &&
             isCodePtr(load_and_swap<uint32_t>(data + j))) {
        j += 4;
      }
      if ((j - i) / 4 >= 2) {
        for (size_t k = i; k < j; k += 4) {
          uint32_t target = load_and_swap<uint32_t>(data + k);
          if (graph.isEntryPoint(target))
            continue;
          if (graph.isChunkClaimed(target)) {
            // A config chunk says this address is a parent's interior, not a
            // function. Say where the pointer came from — a real dispatch
            // table pointing at a chunk would mean the chunk config is wrong.
            REXCODEGEN_WARN(
                "pointer-table scan: 0x{:08X} (at data 0x{:08X}) is inside a config chunk, skipping",
                target, base + static_cast<uint32_t>(k));
            continue;
          }
          if (!precededByFnBoundary(binary, target, /*strict=*/false))
            continue;  // not a real function entry -> would split a function
          graph.addFunction(target, 4, FunctionAuthority::VTABLE, true);
          newFunctions++;
        }
      }
      i = j;
    }
  }

  REXCODEGEN_TRACE("Analyze: data pointer-table scan registered {} new functions", newFunctions);
  return newFunctions;
}

size_t functionPointerScan(CodegenContext& ctx);  // defined below

void discoverAllFunctions(CodegenContext& ctx) {
  REXCODEGEN_TRACE("Analyze: starting iterative discovery...");

  auto& graph = ctx.graph;
  auto& binary = ctx.binary();

  // Harvest heuristic function-pointer targets BEFORE the discovery loop. Both
  // scans find functions reached only through pointers/computed indirect calls;
  // those same functions are also targets of direct branches/tail-calls from
  // other code, so registering them up front (like manual seeds) lets the loop
  // resolve those branches instead of baking in "unresolved branch" traps.
  // Registering them afterwards is too late -- the callers are already final.
  {
    size_t nTable = dataPointerTableScan(ctx);
    size_t nLisOri = functionPointerScan(ctx);
    REXCODEGEN_TRACE(
        "Analyze: pre-discovery scans seeded {} (pointer-table) + {} (lis/ori) "
        "functions",
        nTable, nLisOri);
  }

  // Iterative discovery
  size_t iteration = 0;
  size_t lastFunctionCount = 0;
  const size_t maxIterations = REXCVAR_GET(max_discovery_iterations);

  while (iteration < maxIterations) {
    iteration++;

    size_t currentFunctionCount = graph.functionCount();
    if (currentFunctionCount == lastFunctionCount && iteration > 1) {
      REXCODEGEN_DEBUG("Analyze: fixed point at iteration {} ({} functions)", iteration,
                       currentFunctionCount);
      break;
    }

    lastFunctionCount = currentFunctionCount;

    auto knownFunctions = buildKnownFunctions(graph);
    if (discoverPendingFunctions(ctx, knownFunctions) == 0) {
      break;
    }
  }

  REXCODEGEN_TRACE("Analyze: {} functions after call graph expansion", graph.functionCount());

  // VTable scanning
  {
    VTableScanner vtScanner(binary);
    auto vtables = vtScanner.scan();

    size_t newFunctions = 0;

    for (const auto& vt : vtables) {
      for (size_t i = 0; i < vt.slots.size(); i++) {
        uint32_t funcAddr = vt.slots[i];

        if (graph.isEntryPoint(funcAddr))
          continue;
        if (binary.isInImportExportRange(funcAddr))
          continue;

        graph.addFunction(funcAddr, 4, FunctionAuthority::VTABLE, true);
        newFunctions++;
      }
    }

    REXCODEGEN_TRACE("Analyze: VTable scan found {} vtables, {} new functions", vtables.size(),
                     newFunctions);

    // Continue discovery for vtable functions
    if (newFunctions > 0) {
      size_t vtableIteration = 0;
      const size_t maxVtableIterations = REXCVAR_GET(max_vtable_iterations);

      while (vtableIteration < maxVtableIterations) {
        vtableIteration++;

        auto knownFunctions = buildKnownFunctions(graph);
        if (discoverPendingFunctions(ctx, knownFunctions) == 0)
          break;

        if (graph.functionCount() == lastFunctionCount)
          break;
        lastFunctionCount = graph.functionCount();
      }
    }
  }

  REXCODEGEN_TRACE("Analyze: {} total functions after vtable scan", graph.functionCount());
}

//=============================================================================
// Function Pointer Scan: find lis/addi and lis/ori pairs that materialize a
// code address. These catch functions reached only through a runtime-computed
// indirect call (e.g. mtctr/bctr through a register built by lis+ori) -- they
// have no static call/branch xref and aren't in any data pointer-table, so
// neither the call-graph walk nor dataPointerTableScan finds them, and a
// dispatch through them FATALs as "call to unregistered function".
//
// This was historically disabled for false positives: a lis/addi pair can
// compute a data address, a return address, or a within-function label. The
// guards below (executable + non-round + preceded by a function boundary, and
// run before discovery so callers resolve) make it safe enough to enable.
//=============================================================================
size_t functionPointerScan(CodegenContext& ctx) {
  if (!REXCVAR_GET(enable_lis_ori_fn_scan))
    return 0;
  if (!ctx.hasDecoded()) {
    REXCODEGEN_WARN("functionPointerScan: DecodedBinary not initialized, skipping");
    return 0;
  }

  auto& graph = ctx.graph;
  auto& binary = ctx.binary();
  auto& decoded = ctx.decoded();
  const auto& codeRegions = decoded.codeRegions();

  if (codeRegions.empty()) {
    REXCODEGEN_WARN("functionPointerScan: no code regions, skipping");
    return 0;
  }

  // Build set of existing functions to avoid duplicates
  std::unordered_set<uint32_t> existingFunctions;
  for (const auto& [addr, node] : graph.functions()) {
    existingFunctions.insert(addr);
  }

  // Track lis values: register -> (high_value, lis_address)
  // We scan linearly and track the most recent lis for each register
  // PPC has exactly 32 GPRs, so a fixed-size array is more efficient than a map
  std::array<std::pair<uint32_t, uint32_t>, 32> lisValues{};
  std::bitset<32> lisValid;

  size_t foundCount = 0;

  for (const auto& region : codeRegions) {
    lisValid.reset();  // Reset tracking at region boundaries

    for (uint32_t addr = region.start; addr < region.end; addr += 4) {
      auto* insn = decoded.get(addr);
      if (!insn)
        continue;

      // Track lis rD, IMM
      if (isLis(*insn)) {
        uint8_t rd = static_cast<uint8_t>(insn->D.RT);
        uint32_t hi = static_cast<uint32_t>(static_cast<int16_t>(insn->D.d)) << 16;
        lisValues[rd] = {hi, addr};
        lisValid.set(rd);
        continue;
      }

      // Check for addi rD, rA, IMM where rA was set by lis
      if (insn->opcode == rex::codegen::ppc::Opcode::addi) {
        uint8_t ra = static_cast<uint8_t>(insn->D.RA);
        if (ra == 0)
          continue;  // li pseudo-op, not addi

        if (!lisValid.test(ra))
          continue;

        uint32_t hi = lisValues[ra].first;
        int16_t lo = static_cast<int16_t>(insn->D.d);
        uint32_t fullAddr = hi + lo;  // Sign-extended add

        if (existingFunctions.contains(fullAddr))
          continue;
        // A target computed within ~4KB of the computing instruction is almost
        // always the address of nearby data / a local label / a return point,
        // not a pointer to a distant function (the classic lis/addi-of-local
        // false positive). Real cross-subsystem function pointers are far away.
        if (std::abs(static_cast<int32_t>(fullAddr - addr)) < 0x1000)
          continue;
        if (!isPlausibleFnPtr(binary, fullAddr) ||
            !precededByFnBoundary(binary, fullAddr, /*strict=*/true))
          continue;
        if (graph.isChunkClaimed(fullAddr)) {
          REXCODEGEN_WARN("functionPointerScan: 0x{:08X} (lis/addi at 0x{:08X}) is inside a config chunk, skipping",
                          fullAddr, addr);
          continue;
        }

        // Register as function with DISCOVERED authority and hasXrefs=true
        graph.addFunction(fullAddr, 4, FunctionAuthority::DISCOVERED, true);
        existingFunctions.insert(fullAddr);
        foundCount++;

        REXCODEGEN_TRACE("functionPointerScan: found 0x{:08X} via lis/addi at 0x{:08X}", fullAddr,
                         addr);
      }

      // Also check ori rD, rA, IMM (alternative to addi for unsigned)
      if (insn->opcode == rex::codegen::ppc::Opcode::ori) {
        uint8_t ra = static_cast<uint8_t>(insn->D.RA);
        if (!lisValid.test(ra))
          continue;

        uint32_t hi = lisValues[ra].first;
        uint16_t lo = static_cast<uint16_t>(insn->D.d);
        uint32_t fullAddr = hi | lo;  // Unsigned OR

        if (existingFunctions.contains(fullAddr))
          continue;
        if (std::abs(static_cast<int32_t>(fullAddr - addr)) < 0x1000)
          continue;  // local data/label/return point, not a distant fn pointer
        if (!isPlausibleFnPtr(binary, fullAddr) ||
            !precededByFnBoundary(binary, fullAddr, /*strict=*/true))
          continue;
        if (graph.isChunkClaimed(fullAddr)) {
          REXCODEGEN_WARN("functionPointerScan: 0x{:08X} (lis/ori at 0x{:08X}) is inside a config chunk, skipping",
                          fullAddr, addr);
          continue;
        }

        graph.addFunction(fullAddr, 4, FunctionAuthority::DISCOVERED, true);
        existingFunctions.insert(fullAddr);
        foundCount++;

        REXCODEGEN_TRACE("functionPointerScan: found 0x{:08X} via lis/ori at 0x{:08X}", fullAddr,
                         addr);
      }

      // Clear lis tracking if register is overwritten by other instruction
      // (Simplified: we clear on any write to the register)
      // This is conservative - could miss some patterns but avoids false positives
    }
  }

  REXCODEGEN_TRACE("functionPointerScan: found {} new function pointer targets", foundCount);
  return foundCount;
}

}  // anonymous namespace

/// Discover blocks for all pending functions (shared helper, declared in phase_helpers.h).
size_t discoverPendingFunctions(CodegenContext& ctx,
                                const std::unordered_set<uint32_t>& knownFunctions) {
  std::vector<uint32_t> pending;
  for (const auto& [addr, node] : ctx.graph.functions()) {
    if (node->canDiscover()) {
      pending.push_back(addr);
    }
  }
  for (uint32_t funcAddr : pending) {
    discoverFunction(ctx, funcAddr, knownFunctions);
  }
  return pending.size();
}

namespace phases {

VoidResult Discover(CodegenContext& ctx, ProgressReporter* reporter) {
  (void)reporter;
  discoverAllFunctions(ctx);
  return Ok();
}

}  // namespace phases

}  // namespace rex::codegen
