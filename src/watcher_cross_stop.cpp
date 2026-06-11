#include <arpa/inet.h>
#include <sys/resource.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>

#include <uv.h>

namespace {

constexpr size_t kSocketCount = 4;
constexpr size_t kFileLimitHeadroom = 32;
constexpr int kTestPort = 9123;

struct ReceiverContext {
  uv_udp_t socket;
  std::array<char, sizeof(std::uint32_t)> receive_buffer;
  size_t id;
  size_t receive_count;
};

struct SenderContext {
  uv_udp_t socket;
  uv_udp_send_t request;
  std::array<char, sizeof(std::uint32_t)> payload;
  size_t id;
};

std::array<ReceiverContext, kSocketCount> g_receivers;
std::array<SenderContext, kSocketCount> g_senders;
std::array<bool, kSocketCount> g_received_senders;
size_t g_receive_callbacks;
size_t g_send_callbacks;
size_t g_close_callbacks;
bool g_validation_failed;

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

void AllocateBuffer(uv_handle_t* handle, size_t, uv_buf_t* buffer) {
  auto* receiver = static_cast<ReceiverContext*>(handle->data);
  buffer->base = receiver->receive_buffer.data();
  buffer->len = receiver->receive_buffer.size();
}

void ReceiveCallback(uv_udp_t* handle,
                     ssize_t nread,
                     const uv_buf_t* buffer,
                     const sockaddr* source,
                     unsigned int) {
  if (nread < 0) {
    PrintUvError("UDP receive", static_cast<int>(nread));
    g_validation_failed = true;
    return;
  }
  if (nread == 0) {
    return;
  }

  auto* receiver = static_cast<ReceiverContext*>(handle->data);
  ++g_receive_callbacks;
  ++receiver->receive_count;

  if (nread != static_cast<ssize_t>(sizeof(std::uint32_t))) {
    std::cerr << "socket " << receiver->id << " received an invalid payload "
              << "of " << nread << " bytes\n";
    g_validation_failed = true;
    return;
  }

  std::uint32_t sender_id;
  std::memcpy(&sender_id, buffer->base, sizeof(sender_id));

  if (source == nullptr || source->sa_family != AF_INET) {
    std::cerr << "sender id " << sender_id
              << " has an invalid source address\n";
    g_validation_failed = true;
    return;
  }

  const auto* source_address =
      reinterpret_cast<const sockaddr_in*>(source);
  const unsigned int source_port = ntohs(source_address->sin_port);
  std::cout << "sender socket " << sender_id << " (source port "
            << source_port << ") -> receiver socket " << receiver->id << '\n';

  if (sender_id >= kSocketCount) {
    std::cerr << "invalid sender id " << sender_id << '\n';
    g_validation_failed = true;
    return;
  }
  if (g_received_senders[sender_id]) {
    std::cerr << "sender id " << sender_id << " was received more than once\n";
    g_validation_failed = true;
    return;
  }

  g_received_senders[sender_id] = true;
}

void SendCallback(uv_udp_send_t* request, int status) {
  auto* sender = static_cast<SenderContext*>(request->data);
  if (status != 0) {
    std::cerr << "send from socket " << sender->id
              << " failed: " << uv_strerror(status) << '\n';
    g_validation_failed = true;
  }
  ++g_send_callbacks;
  std::cout << "send from socket " << sender->id << " completed successfully\n";
}

void CloseCallback(uv_handle_t*) {
  ++g_close_callbacks;
}

void CloseInitializedSockets(uv_loop_t* loop,
                             size_t initialized_receivers,
                             size_t initialized_senders) {
  for (size_t i = 0; i < initialized_receivers; ++i) {
    uv_handle_t* handle =
        reinterpret_cast<uv_handle_t*>(&g_receivers[i].socket);
    if (!uv_is_closing(handle)) {
      uv_close(handle, CloseCallback);
    }
  }

  for (size_t i = 0; i < initialized_senders; ++i) {
    uv_handle_t* handle =
        reinterpret_cast<uv_handle_t*>(&g_senders[i].socket);
    if (!uv_is_closing(handle)) {
      uv_close(handle, CloseCallback);
    }
  }

  uv_run(loop, UV_RUN_DEFAULT);
}

int RunWatcherCrossStop() {
  const size_t required_file_limit = kSocketCount * 2 + kFileLimitHeadroom;
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

  size_t initialized_receivers = 0;
  for (size_t i = 0; i < kSocketCount; ++i) {
    ReceiverContext& receiver = g_receivers[i];
    receiver.id = i;

    err = uv_udp_init(loop, &receiver.socket);
    if (err != 0) {
      PrintUvError("receiver uv_udp_init", err);
      CloseInitializedSockets(loop, initialized_receivers, 0);
      return 1;
    }
    ++initialized_receivers;
    receiver.socket.data = &receiver;

    err =
        uv_udp_bind(&receiver.socket,
                    reinterpret_cast<const sockaddr*>(&address),
                    UV_UDP_REUSEADDR);
    if (err != 0) {
      std::cerr << "receiver uv_udp_bind[" << i
                << "] failed: " << uv_strerror(err) << '\n';
      CloseInitializedSockets(loop, initialized_receivers, 0);
      return 1;
    }

    err =
        uv_udp_recv_start(&receiver.socket, AllocateBuffer, ReceiveCallback);
    if (err != 0) {
      PrintUvError("uv_udp_recv_start", err);
      CloseInitializedSockets(loop, initialized_receivers, 0);
      return 1;
    }
  }

  size_t initialized_senders = 0;
  for (size_t i = 0; i < kSocketCount; ++i) {
    SenderContext& sender = g_senders[i];
    sender.id = i;

    err = uv_udp_init(loop, &sender.socket);
    if (err != 0) {
      PrintUvError("sender uv_udp_init", err);
      CloseInitializedSockets(loop, initialized_receivers,
                              initialized_senders);
      return 1;
    }
    ++initialized_senders;
    sender.request.data = &sender;

    const std::uint32_t sender_id = static_cast<std::uint32_t>(i);
    std::memcpy(sender.payload.data(), &sender_id, sizeof(sender_id));
    uv_buf_t buffer =
        uv_buf_init(sender.payload.data(),
                    static_cast<unsigned int>(sender.payload.size()));

    err = uv_udp_send(&sender.request, &sender.socket, &buffer, 1,
                      reinterpret_cast<const sockaddr*>(&address),
                      SendCallback);
    if (err != 0) {
      PrintUvError("uv_udp_send", err);
      CloseInitializedSockets(loop, initialized_receivers,
                              initialized_senders);
      return 1;
    }
  }

  while (g_receive_callbacks < kSocketCount && !g_validation_failed) {
    uv_run(loop, UV_RUN_ONCE);
  }

  CloseInitializedSockets(loop, kSocketCount, kSocketCount);

  if (g_send_callbacks != kSocketCount) {
    std::cerr << "expected " << kSocketCount
              << " send callbacks, received " << g_send_callbacks << '\n';
    return 1;
  }

  const size_t expected_close_callbacks = kSocketCount * 2;
  if (g_close_callbacks != expected_close_callbacks) {
    std::cerr << "expected " << expected_close_callbacks
              << " close callbacks, received " << g_close_callbacks << '\n';
    return 1;
  }

  if (!std::all_of(g_received_senders.begin(), g_received_senders.end(),
                   [](bool received) { return received; })) {
    std::cerr << "not every sender id was received\n";
    return 1;
  }

  if (g_validation_failed) {
    return 1;
  }

  for (const ReceiverContext& receiver : g_receivers) {
    std::cout << "receiver socket " << receiver.id << " handled "
              << receiver.receive_count << " datagram(s)\n";
  }

  std::cout << "watcher_cross_stop: " << g_receive_callbacks << " receive, "
            << g_send_callbacks << " send, " << g_close_callbacks
            << " close callbacks\n";
  return 0;
}

}  // namespace

int main() {
  const int result = RunWatcherCrossStop();

  const int err = uv_loop_close(uv_default_loop());
  if (err != 0) {
    PrintUvError("uv_loop_close", err);
    return 1;
  }

  return result;
}
