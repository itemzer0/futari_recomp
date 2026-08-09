/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string_view>

#include <rex/assert.h>
#include <rex/graphics/flags.h>
#include <rex/graphics/vulkan/command_processor.h>
#include <rex/graphics/vulkan/deferred_command_buffer.h>
#include <rex/graphics/vulkan/primitive_processor.h>
#include <rex/logging.h>
#include <rex/ui/vulkan/util.h>

REXCVAR_DEFINE_BOOL(vulkan_force_expand_point_sprites_in_vs, false, "GPU/Vulkan",
                    "Force Vulkan point sprite expansion in the vertex shader, even when geometry "
                    "shaders are available")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(
    vulkan_force_expand_rectangle_lists_in_vs, false, "GPU/Vulkan",
    "Force Vulkan rectangle list expansion in the vertex shader, even when geometry "
    "shaders are available")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(vulkan_force_convert_quad_lists_to_triangle_lists, false, "GPU/Vulkan",
                    "Force Vulkan quad list conversion to triangle lists in primitive processing, "
                    "even when geometry shaders are available")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_INT32(
    vulkan_primitive_vs_expansion, 0, "GPU/Vulkan",
    "Expand Xenos point sprites, rectangle lists and quad lists in the vertex shader instead of "
    "using geometry shaders, to avoid the low-occupancy legacy geometry-shader path on RDNA2. "
    "0 = off (use geometry shaders where the host supports them), 1 = auto (force vertex-shader "
    "expansion only on the Steam Deck APU - RADV Van Gogh/Sephiroth), 2 = force on for every "
    "device (correctness / A-B testing on a non-Deck GPU).")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

namespace rex::graphics::vulkan {

VulkanPrimitiveProcessor::~VulkanPrimitiveProcessor() {
  Shutdown(true);
}

bool VulkanPrimitiveProcessor::Initialize() {
  const ui::vulkan::VulkanDevice* const vulkan_device = command_processor_.GetVulkanDevice();
  const ui::vulkan::VulkanDevice::Properties& device_properties = vulkan_device->properties();
  // Keep triangle fan handling aligned with D3D12 by always converting fans to
  // lists in the shared primitive processor path.
  constexpr bool triangle_fans_supported_without_conversion = false;

  // Default to D3D12-like geometry-shader-based handling when available, but
  // allow opting into Vulkan vertex-shader expansion fallbacks for the Xenos
  // primitive types that would otherwise be emulated with a geometry shader
  // (point sprites, rectangle lists, quad lists).
  bool geometry_shader_primitive_emulation_allowed = device_properties.geometryShader;

  // On RDNA2 - notably the Steam Deck "Van Gogh" APU - the legacy geometry-shader
  // path used for those primitives runs at very low wavefront occupancy (the GPU
  // is starved rather than busy), so expanding them in the vertex shader instead
  // can claw back GPU headroom. vulkan_primitive_vs_expansion gates this:
  //   0 = off  : use geometry shaders where the host supports them (default).
  //   1 = auto : force vertex-shader expansion only on the Steam Deck APU,
  //              detected by the RADV marketing name (as in the tessellation
  //              workaround in the command processor), leaving desktop GPUs on
  //              the geometry-shader path.
  //   2 = on   : force vertex-shader expansion on every device (correctness /
  //              A-B testing on a dev box that is not the Deck).
  // The per-type force_* cvars below still apply independently of this aggregate.
  bool force_vs_primitive_expansion = false;
  {
    const int32_t mode = REXCVAR_GET(vulkan_primitive_vs_expansion);
    const std::string_view device_name(device_properties.deviceName);
    const bool is_steam_deck_apu = device_name.find("VANGOGH") != std::string_view::npos ||
                                   device_name.find("SEPHIROTH") != std::string_view::npos;
    force_vs_primitive_expansion = (mode == 2) || (mode == 1 && is_steam_deck_apu);
    if (force_vs_primitive_expansion) {
      REXGPU_INFO(
          "Expanding Xenos point/rectangle/quad primitives in the vertex shader instead of "
          "geometry shaders on \"{}\" (low-occupancy-GS workaround; vulkan_primitive_vs_expansion="
          "{}).",
          device_properties.deviceName, mode);
    }
  }

  bool quad_lists_supported_without_conversion =
      geometry_shader_primitive_emulation_allowed && !force_vs_primitive_expansion &&
      !REXCVAR_GET(vulkan_force_convert_quad_lists_to_triangle_lists);
  bool point_sprites_supported_without_vs_expansion =
      geometry_shader_primitive_emulation_allowed && !force_vs_primitive_expansion &&
      !REXCVAR_GET(vulkan_force_expand_point_sprites_in_vs);
  bool rectangle_lists_supported_without_vs_expansion =
      geometry_shader_primitive_emulation_allowed && !force_vs_primitive_expansion &&
      !REXCVAR_GET(vulkan_force_expand_rectangle_lists_in_vs);

  if (!InitializeCommon(
          device_properties.fullDrawIndexUint32, triangle_fans_supported_without_conversion, false,
          quad_lists_supported_without_conversion, point_sprites_supported_without_vs_expansion,
          rectangle_lists_supported_without_vs_expansion)) {
    Shutdown();
    return false;
  }
  frame_index_buffer_pool_ = std::make_unique<ui::vulkan::VulkanUploadBufferPool>(
      vulkan_device, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
      std::max(size_t(kMinRequiredConvertedIndexBufferSize),
               ui::GraphicsUploadBufferPool::kDefaultPageSize));
  return true;
}

void VulkanPrimitiveProcessor::Shutdown(bool from_destructor) {
  const ui::vulkan::VulkanDevice* const vulkan_device = command_processor_.GetVulkanDevice();
  const ui::vulkan::VulkanDevice::Functions& dfn = vulkan_device->functions();
  const VkDevice device = vulkan_device->device();

  frame_index_buffers_.clear();
  frame_index_buffer_pool_.reset();
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device, builtin_index_buffer_upload_);
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                         builtin_index_buffer_upload_memory_);
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device, builtin_index_buffer_);
  ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device, builtin_index_buffer_memory_);

  if (!from_destructor) {
    ShutdownCommon();
  }
}

void VulkanPrimitiveProcessor::CompletedSubmissionUpdated() {
  if (builtin_index_buffer_upload_ != VK_NULL_HANDLE &&
      command_processor_.GetCompletedSubmission() >= builtin_index_buffer_upload_submission_) {
    const ui::vulkan::VulkanDevice* const vulkan_device = command_processor_.GetVulkanDevice();
    const ui::vulkan::VulkanDevice::Functions& dfn = vulkan_device->functions();
    const VkDevice device = vulkan_device->device();
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                           builtin_index_buffer_upload_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                           builtin_index_buffer_upload_memory_);
  }
}

void VulkanPrimitiveProcessor::BeginSubmission() {
  if (builtin_index_buffer_upload_ != VK_NULL_HANDLE &&
      builtin_index_buffer_upload_submission_ == UINT64_MAX) {
    // No need to submit deferred barriers - builtin_index_buffer_ has never
    // been used yet, and builtin_index_buffer_upload_ is written before
    // submitting commands reading it.

    command_processor_.EndRenderPass();

    DeferredCommandBuffer& command_buffer = command_processor_.deferred_command_buffer();

    VkBufferCopy* copy_region =
        command_buffer.CmdCopyBufferEmplace(builtin_index_buffer_upload_, builtin_index_buffer_, 1);
    copy_region->srcOffset = 0;
    copy_region->dstOffset = 0;
    copy_region->size = builtin_index_buffer_size_;

    command_processor_.PushBufferMemoryBarrier(
        builtin_index_buffer_, 0, VK_WHOLE_SIZE, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_INDEX_READ_BIT);

    builtin_index_buffer_upload_submission_ = command_processor_.GetCurrentSubmission();
  }
}

void VulkanPrimitiveProcessor::BeginFrame() {
  frame_index_buffer_pool_->Reclaim(command_processor_.GetCompletedFrame());
}

void VulkanPrimitiveProcessor::EndSubmission() {
  frame_index_buffer_pool_->FlushWrites();
}

void VulkanPrimitiveProcessor::EndFrame() {
  ClearPerFrameCache();
  frame_index_buffers_.clear();
}

bool VulkanPrimitiveProcessor::InitializeBuiltinIndexBuffer(
    size_t size_bytes, std::function<void(void*)> fill_callback) {
  assert_not_zero(size_bytes);
  assert_true(builtin_index_buffer_ == VK_NULL_HANDLE);
  assert_true(builtin_index_buffer_memory_ == VK_NULL_HANDLE);
  assert_true(builtin_index_buffer_upload_ == VK_NULL_HANDLE);
  assert_true(builtin_index_buffer_upload_memory_ == VK_NULL_HANDLE);

  const ui::vulkan::VulkanDevice* const vulkan_device = command_processor_.GetVulkanDevice();
  const ui::vulkan::VulkanDevice::Functions& dfn = vulkan_device->functions();
  const VkDevice device = vulkan_device->device();

  builtin_index_buffer_size_ = VkDeviceSize(size_bytes);
  if (!ui::vulkan::util::CreateDedicatedAllocationBuffer(
          vulkan_device, builtin_index_buffer_size_,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
          ui::vulkan::util::MemoryPurpose::kDeviceLocal, builtin_index_buffer_,
          builtin_index_buffer_memory_)) {
    REXGPU_ERROR(
        "Vulkan primitive processor: Failed to create the built-in index "
        "buffer GPU resource with {} bytes",
        size_bytes);
    return false;
  }
  uint32_t upload_memory_type;
  if (!ui::vulkan::util::CreateDedicatedAllocationBuffer(
          vulkan_device, builtin_index_buffer_size_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
          ui::vulkan::util::MemoryPurpose::kUpload, builtin_index_buffer_upload_,
          builtin_index_buffer_upload_memory_, &upload_memory_type)) {
    REXGPU_ERROR(
        "Vulkan primitive processor: Failed to create the built-in index "
        "buffer upload resource with {} bytes",
        size_bytes);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device, builtin_index_buffer_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device, builtin_index_buffer_memory_);
    return false;
  }

  void* mapping;
  if (dfn.vkMapMemory(device, builtin_index_buffer_upload_memory_, 0, VK_WHOLE_SIZE, 0, &mapping) !=
      VK_SUCCESS) {
    REXGPU_ERROR(
        "Vulkan primitive processor: Failed to map the built-in index buffer "
        "upload resource with {} bytes",
        size_bytes);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device,
                                           builtin_index_buffer_upload_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device,
                                           builtin_index_buffer_upload_memory_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkDestroyBuffer, device, builtin_index_buffer_);
    ui::vulkan::util::DestroyAndNullHandle(dfn.vkFreeMemory, device, builtin_index_buffer_memory_);
    return false;
  }
  fill_callback(mapping);
  ui::vulkan::util::FlushMappedMemoryRange(vulkan_device, builtin_index_buffer_memory_,
                                           upload_memory_type);
  dfn.vkUnmapMemory(device, builtin_index_buffer_upload_memory_);

  // Schedule uploading in the first submission.
  builtin_index_buffer_upload_submission_ = UINT64_MAX;
  return true;
}

void* VulkanPrimitiveProcessor::RequestHostConvertedIndexBufferForCurrentFrame(
    xenos::IndexFormat format, uint32_t index_count, bool coalign_for_simd,
    uint32_t coalignment_original_address, size_t& backend_handle_out) {
  size_t index_size = format == xenos::IndexFormat::kInt16 ? sizeof(uint16_t) : sizeof(uint32_t);
  VkBuffer buffer;
  VkDeviceSize offset;
  uint8_t* mapping = frame_index_buffer_pool_->Request(
      command_processor_.GetCurrentFrame(),
      index_size * index_count + (coalign_for_simd ? XE_GPU_PRIMITIVE_PROCESSOR_SIMD_SIZE : 0),
      index_size, buffer, offset);
  if (!mapping) {
    return nullptr;
  }
  if (coalign_for_simd) {
    ptrdiff_t coalignment_offset = GetSimdCoalignmentOffset(mapping, coalignment_original_address);
    mapping += coalignment_offset;
    offset = VkDeviceSize(offset + coalignment_offset);
  }
  backend_handle_out = frame_index_buffers_.size();
  frame_index_buffers_.emplace_back(buffer, offset);
  return mapping;
}

}  // namespace rex::graphics::vulkan
