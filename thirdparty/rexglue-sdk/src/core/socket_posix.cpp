#include <rex/net/socket.h>
#include <rex/platform.h>

static_assert(REX_PLATFORM_LINUX || REX_PLATFORM_MAC, "This file is POSIX-only");

#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

namespace rex::net {

int socket_close(SocketHandle handle) {
  return close(static_cast<int>(handle));
}

int socket_ioctl(SocketHandle handle, uint32_t cmd, uint8_t* arg) {
  int fd = static_cast<int>(handle);
  // The guest passes Winsock ioctl command codes (e.g. FIONBIO=0x8004667E),
  // whose numeric values differ from the POSIX equivalents (Linux FIONBIO is
  // 0x5421). Passing them straight to ioctl() silently no-ops, so e.g. the
  // guest's "set non-blocking" request is lost and the socket stays blocking.
  // Translate the codes the titles actually use.
  switch (cmd) {
    case 0x8004667Eu: {  // Winsock FIONBIO
      uint32_t nonblocking = arg ? *reinterpret_cast<uint32_t*>(arg) : 0;
      int flags = fcntl(fd, F_GETFL, 0);
      if (flags < 0) {
        return -1;
      }
      if (nonblocking) {
        flags |= O_NONBLOCK;
      } else {
        flags &= ~O_NONBLOCK;
      }
      return fcntl(fd, F_SETFL, flags);
    }
    case 0x4004667Fu:  // Winsock FIONREAD
      return ioctl(fd, FIONREAD, arg);
    default:
      return ioctl(fd, cmd, arg);
  }
}

}  // namespace rex::net
