/**
 * @file        rex/ui/overlay/toast_overlay.h
 *
 * @brief       Transient non-interactive toast notification drawn over the game.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#pragma once
#include <string>
#include <rex/ui/imgui_dialog.h>

namespace rex::ui {

// Fire-and-forget toast: fades in at the top of the window, holds, fades out,
// then deletes itself (same self-owning lifecycle as ImGuiDialog::ShowMessageBox).
// Accepts no input and never takes focus.
class ToastDialog final : public ImGuiDialog {
 public:
  static ToastDialog* Show(ImGuiDrawer* imgui_drawer, std::string text,
                           float duration_seconds = 10.0f);

 protected:
  void OnDraw(ImGuiIO& io) override;

 private:
  ToastDialog(ImGuiDrawer* imgui_drawer, std::string text, float duration_seconds);

  std::string text_;
  float duration_;
  // Set on the first drawn frame so the timer excludes startup/shader stalls.
  double start_time_ = -1.0;
};

}  // namespace rex::ui
