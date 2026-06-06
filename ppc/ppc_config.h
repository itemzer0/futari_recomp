#pragma once

// Mushihimesama Futari Ver. 1.5 — Xbox 360 memory map
//
// TODO: Run scripts/analyse_xex.py and fill in real values.
// The defaults below are placeholders based on typical Xbox 360 disc titles.
//
// Title ID: (check default.xex header)
// Entry point: (output of analyse_xex.py)

// Title: cave360.exe  (from XEX execution ID)
// Entry point: 0x8208A2B8  (from XEX optional header 0x00010100)
#define PPC_IMAGE_BASE  0x82000000U
#define PPC_IMAGE_SIZE  0x002B0000U
#define PPC_CODE_BASE   0x82060000U  // from XenonRecomp generated/ppc_config.h
#define PPC_CODE_SIZE   0x00175BF4U  // from XenonRecomp generated/ppc_config.h

#ifdef PPC_INCLUDE_DETAIL
#include "ppc_detail.h"
#endif
