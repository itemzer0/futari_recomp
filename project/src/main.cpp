#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

// ReXGlue runtime
#include <rex/rex.h>

// PPC image layout constants
#define PPC_INCLUDE_DETAIL
#include "ppc_config.h"

// Forward declarations for generated entry point and stubs
void PPCFunc_EntryPoint(PPCContext& ctx, uint8_t* base);
void RegisterStubs();

namespace fs = std::filesystem;

static fs::path g_game_data_root;

static void print_usage(const char* argv0) {
    fprintf(stderr,
        "Usage: %s --game_data_root <path>\n"
        "\n"
        "  --game_data_root   Path to the extracted game data directory\n"
        "                     (the folder containing default.xex and game files)\n",
        argv0);
}

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--game_data_root") == 0 && i + 1 < argc) {
            g_game_data_root = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (g_game_data_root.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    if (!fs::is_directory(g_game_data_root)) {
        fprintf(stderr, "ERROR: game_data_root does not exist: %s\n",
                g_game_data_root.string().c_str());
        return 1;
    }

    printf("futari_recomp  --  Mushihimesama Futari Ver. 1.5\n");
    printf("game_data_root: %s\n", g_game_data_root.string().c_str());

    // Initialise ReXGlue runtime
    RexConfig cfg{};
    cfg.image_base = PPC_IMAGE_BASE;
    cfg.image_size = PPC_IMAGE_SIZE;
    cfg.game_data_root = g_game_data_root.string().c_str();

    if (!RexInit(&cfg)) {
        fprintf(stderr, "ERROR: RexInit failed\n");
        return 1;
    }

    // Register Xbox 360 kernel/XAM stubs
    RegisterStubs();

    // Run the game
    PPCContext ctx{};
    uint8_t* base = RexGetMemoryBase();
    PPCFunc_EntryPoint(ctx, base);

    RexShutdown();
    return 0;
}
