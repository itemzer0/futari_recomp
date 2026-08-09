/**
 * @file        rex/ui/overlay/settings_overlay.h
 *
 * @brief       ImGui settings overlay dialog for cvar editing with save-to-config.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once
#include <filesystem>
#include <string>
#include <rex/ui/imgui_dialog.h>

namespace rex::cvar {
struct FlagEntry;
}

namespace rex::ui {

class SettingsDialog : public ImGuiDialog {
 public:
  // config_path: where "Save to config" writes (e.g. exe_dir / "app.toml")
  SettingsDialog(ImGuiDrawer* imgui_drawer, std::filesystem::path config_path);
  ~SettingsDialog();

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  // Quick-access display settings (resolution preset, fullscreen, vsync) shown
  // above the generic cvar list when the root category is selected.
  void DrawDisplaySection();

  // Render one settings row into the currently-open 2-column table.
  // DrawCvarRow handles the generic value widget (+ modified marker / Reset);
  // DrawKeybindRow handles key capture, rebind/reset, and conflict detection.
  void DrawCvarRow(const rex::cvar::FlagEntry& entry);
  void DrawKeybindRow(const rex::cvar::FlagEntry& entry);

  std::filesystem::path config_path_;
  char search_buf_[128] = {};
  std::string selected_category_;
  std::string capturing_bind_name_;
  // Why the last captured key was rejected (already bound elsewhere); shown
  // below the capture button until a free key is pressed or capture ends.
  std::string capture_conflict_;
};

}  // namespace rex::ui
