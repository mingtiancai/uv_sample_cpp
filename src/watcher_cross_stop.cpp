#include <uv.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/resource.h>

namespace {

constexpr size_t kSocketCount = 2500;
constexpr size_t kDatagramSize = 1024;
constexpr size_t kFileLimitHeadroom = 32;
constexpr int kTestPort = 9123;

std::array<uv_udp_t, kSocketCount> g_sockets;
std::array<uv_udp_send_t, kSocketCount> g_requests;
std::array<char, 1> g_receive_buffer;
size_t g_receive_callbacks;
size_t g_send_callbacks;
size_t g_close_callbacks;

void PrintUvError(const char* operation, int err) {
  std::cerr << operation << " failed: " << uv_strerror(err) << '\n';
}

void PrintSystemError(const char* operation) {
  std::cerr << operation << " failed: " << std::strerror(errno) << '\n';
}

bool EnsureFileLimit(size_t minimum) {
  rlimit limit;
  if (getrlimit(RLIMIT_NOFILE, &limit) != 0) {
    PrintSystemError("getrlimit");
    return false;
  }

  if (limit.rlim_cur >= minimum) {
    return true;
  }

  limit.rlim_cur = static_cast<rlim_t>(minimum);
  if (limit.rlim_max < limit.rlim_cur) {
    limit.rlim_max = limit.rlim_cur;
  }

  if (setrlimit(RLIMIT_NOFILE, &limit) != 0) {
    PrintSystemError("setrlimit");
    return false;
  }

  return true;
}

void AllocateBuffer(uv_handle_t*, size_t, uv_buf_t* buffer) {
  buffer->base = g_receive_buffer.data();
  buffer->len = g_receive_buffer.size();
}

void ReceiveCallback(uv_udp_t*,
                     ssize_t,
                     const uv_buf_t*,
                     const sockaddr*,
                     unsigned int) {
  ++g_receive_callbacks;
}

void SendCallback(uv_udp_send_t*, int) {
  ++g_send_callbacks;
}

void CloseCallback(uv_handle_t*) {
  ++g_close_callbacks;
}

void CloseInitializedSockets(uv_loop_t* loop, size_t initialized_count) {
  for (size_t i = 0; i < initialized_count; ++i) {
    uv_handle_t* handle =
        reinterpret_cast<uv_handle_t*>(&g_sockets[i]);
    if (!uv_is_closing(handle)) {
      uv_close(handle, CloseCallback);
    }
  }

  uv_run(loop, UV_RUN_DEFAULT);
}

int RunWatcherCrossStop() {
  const size_t required_file_limit = kSocketCount + kFileLimitHeadroom;
  if (!EnsureFileLimit(required_file_limit)) {
    std::cerr << "skipped: file descriptor limit must be at least "
              << required_file_limit << '\n';
    return 0;
  }

  uv_loop_t* loop = uv_default_loop();
  sockaddr_in address;
  int err = uv_ip4_addr("127.0.0.1", kTestPort, &address);
  if (err != 0) {
    PrintUvError("uv_ip4_addr", err);
    return 1;
  }

  std::array<char, kDatagramSize> datagram;
  std::fill(datagram.begin(), datagram.end(), 'A');
  uv_buf_t buffer =
      uv_buf_init(datagram.data(), static_cast<unsigned int>(datagram.size()));

  size_t initialized_count = 0;
  for (size_t i = 0; i < kSocketCount; ++i) {
    err = uv_udp_init(loop, &g_sockets[i]);
    if (err != 0) {
      PrintUvError("uv_udp_init", err);
      CloseInitializedSockets(loop, initialized_count);
      return 1;
    }
    ++initialized_count;

    err = uv_udp_bind(
        &g_sockets[i],
        reinterpret_cast<const sockaddr*>(&address),
        UV_UDP_REUSEADDR);
    if (err != 0) {
      std::cerr << "uv_udp_bind[" << i
                << "] failed: " << uv_strerror(err) << '\n';
      CloseInitializedSockets(loop, initialized_count);
      return 1;
    }

    err = uv_udp_recv_start(
        &g_sockets[i], AllocateBuffer, ReceiveCallback);
    if (err != 0) {
      PrintUvError("uv_udp_recv_start", err);
      CloseInitializedSockets(loop, initialized_count);
      return 1;
    }

    err = uv_udp_send(
        &g_requests[i],
        &g_sockets[i],
        &buffer,
        1,
        reinterpret_cast<const sockaddr*>(&address),
        SendCallback);
    if (err != 0) {
      PrintUvError("uv_udp_send", err);
      CloseInitializedSockets(loop, initialized_count);
      return 1;
    }
  }

  while (g_receive_callbacks == 0) {
    uv_run(loop, UV_RUN_ONCE);
  }

  CloseInitializedSockets(loop, kSocketCount);

  if (g_send_callbacks != kSocketCount) {
    std::cerr << "expected " << kSocketCount
              << " send callbacks, received " << g_send_callbacks << '\n';
    return 1;
  }

  if (g_close_callbacks != kSocketCount) {
    std::cerr << "expected " << kSocketCount
              << " close callbacks, received " << g_close_callbacks << '\n';
    return 1;
  }

  std::cout << "watcher_cross_stop: " << g_receive_callbacks << " receive, "
            << g_send_callbacks << " send, " << g_close_callbacks
            << " close callbacks\n";
  return 0;
}

}  // namespace

int main() {
  int result = RunWatcherCrossStop();

  int err = uv_loop_close(uv_default_loop());
  if (err != 0) {
    PrintUvError("uv_loop_close", err);
    return 1;
  }

  return result;
}
