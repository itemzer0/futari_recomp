#include <rex/platform.h>
#include <rex/platform/dynlib.h>

static_assert(REX_PLATFORM_LINUX || REX_PLATFORM_MAC, "This file is POSIX-only");

#include <dlfcn.h>

#include <rex/filesystem.h>

namespace rex::platform {

DynamicLibrary::~DynamicLibrary() {
  Close();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_) {
  other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
  if (this != &other) {
    Close();
    handle_ = other.handle_;
    other.handle_ = nullptr;
  }
  return *this;
}

bool DynamicLibrary::Load(const std::filesystem::path& path, SymbolResolution mode) {
  Close();
  int flags = (mode == SymbolResolution::kImmediate) ? RTLD_NOW : RTLD_LAZY;
  handle_ = dlopen(path.c_str(), flags);
  if (handle_) {
    return true;
  }

  // Fallback for a bare module name (e.g. "futari_reco_normal"). Callers that
  // load recompiled guest modules pass the undecorated name, relying on
  // Windows LoadLibrary-style resolution. dlopen() does NOT add a "lib" prefix
  // or platform suffix, and on macOS it does not search the executable's own
  // directory either. Recompiled modules colocate with the host, so resolve
  // <exe_dir>/lib<name>.<ext> explicitly.
  if (!path.has_parent_path() && !path.has_extension()) {
#if defined(__APPLE__)
    constexpr const char* kExt = ".dylib";
#else
    constexpr const char* kExt = ".so";
#endif
    const std::filesystem::path exe_dir = rex::filesystem::GetExecutableFolder();
    if (!exe_dir.empty()) {
      const std::filesystem::path candidate = exe_dir / ("lib" + path.string() + kExt);
      handle_ = dlopen(candidate.c_str(), flags);
    }
  }
  return handle_ != nullptr;
}

bool DynamicLibrary::LoadIfPresent(const std::filesystem::path& path) {
  Close();
  // RTLD_NOLOAD: return a handle only if the library is already resident in the
  // process (and bump its refcount so Close() stays balanced); never load it
  // fresh. Without this, dlopen would load a system-installed librenderdoc.so
  // and activate RenderDoc (and its capture overlay) even when not debugging.
  handle_ = dlopen(path.c_str(), RTLD_LAZY | RTLD_NOLOAD);
  return handle_ != nullptr;
}

void DynamicLibrary::Close() {
  if (handle_) {
    dlclose(handle_);
    handle_ = nullptr;
  }
}

void* DynamicLibrary::GetRawSymbol(const char* name) const {
  if (!handle_)
    return nullptr;
  return dlsym(handle_, name);
}

}  // namespace rex::platform
