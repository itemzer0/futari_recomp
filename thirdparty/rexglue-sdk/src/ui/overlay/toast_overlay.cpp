/**
 * @file        ui/overlay/toast_overlay.cpp
 *
 * @brief       Toast notification implementation. See toast_overlay.h for details.
 *
 * @copyright   Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *              All rights reserved.
 *
 * @license     BSD 3-Clause License
 *              See LICENSE file in the project root for full license text.
 */
#include <rex/ui/overlay/toast_overlay.h>

#include <imgui.h>

#include <utility>

namespace rex::ui {

ToastDialog* ToastDialog::Show(ImGuiDrawer* imgui_drawer, std::string text,
                               float duration_seconds) {
  return new ToastDialog(imgui_drawer, std::move(text), duration_seconds);
}

ToastDialog::ToastDialog(ImGuiDrawer* imgui_drawer, std::string text, float duration_seconds)
    : ImGuiDialog(imgui_drawer), text_(std::move(text)), duration_(duration_seconds) {}

void ToastDialog::OnDraw(ImGuiIO& io) {
  if (start_time_ < 0.0) {
    start_time_ = ImGui::GetTime();
  }
  const float elapsed = static_cast<float>(ImGui::GetTime() - start_time_);
  if (elapsed >= duration_) {
    Close();
    return;
  }

  constexpr float kFadeInSeconds = 0.3f;
  constexpr float kFadeOutSeconds = 1.0f;
  float alpha = 1.0f;
  if (elapsed < kFadeInSeconds) {
    alpha = elapsed / kFadeInSeconds;
  } else if (elapsed > duration_ - kFadeOutSeconds) {
    alpha = (duration_ - elapsed) / kFadeOutSeconds;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
  ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, 16.0f), ImGuiCond_Always,
                          ImVec2(0.5f, 0.0f));
  ImGui::SetNextWindowBgAlpha(0.65f * alpha);
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
      ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing |
      ImGuiWindowFlags_NoSavedSettings;
  if (ImGui::Begin("##rex_toast", nullptr, kFlags)) {
    ImGui::TextUnformatted(text_.c_str());
  }
  ImGui::End();
  ImGui::PopStyleVar();
}

}  // namespace rex::ui
