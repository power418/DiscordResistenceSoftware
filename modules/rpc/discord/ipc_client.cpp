#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
#  include <rpc/config/win32.h>
#  include <windows.h>
#else
#  include <sys/socket.h>
#  include <sys/un.h>
#  include <unistd.h>
#endif

#include <modules/rpc/activity.cppm>
#include <modules/rpc/core.cppm>
#include <modules/rpc/discord/ipc_client.cppm>

namespace rpc::discord {

IpcClient::~IpcClient() { close(); }

IpcClient::IpcClient(IpcClient&& other) noexcept
    : native_handle_(other.native_handle_),
      nonce_counter_(other.nonce_counter_) {
  other.native_handle_ = invalid_handle();
}

IpcClient& IpcClient::operator=(IpcClient&& other) noexcept {
  if (this != &other) {
    close();
    native_handle_ = other.native_handle_;
    nonce_counter_ = other.nonce_counter_;
    other.native_handle_ = invalid_handle();
  }
  return *this;
}

bool IpcClient::connected() const { return is_valid_handle(native_handle_); }
std::string_view IpcClient::last_error() const { return last_error_; }

bool IpcClient::is_valid_handle(std::intptr_t handle) {
    return handle != invalid_handle();
}

std::uint32_t IpcClient::current_process_id() {
#if defined(_WIN32)
    return static_cast<std::uint32_t>(GetCurrentProcessId());
#else
    return static_cast<std::uint32_t>(getpid());
#endif
}

std::array<std::uint8_t, 8> IpcClient::make_header(std::uint32_t opcode,
                                                   std::uint32_t length) {
    return {
      static_cast<std::uint8_t>((opcode >> 0U) & 0xFFU),
      static_cast<std::uint8_t>((opcode >> 8U) & 0xFFU),
      static_cast<std::uint8_t>((opcode >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((opcode >> 24U) & 0xFFU),
      static_cast<std::uint8_t>((length >> 0U) & 0xFFU),
      static_cast<std::uint8_t>((length >> 8U) & 0xFFU),
      static_cast<std::uint8_t>((length >> 16U) & 0xFFU),
      static_cast<std::uint8_t>((length >> 24U) & 0xFFU),
    };
}

bool IpcClient::connect(std::string_view client_id) {
    close();
    last_error_.clear();
    if (client_id.empty()) {
      last_error_ = "client id is empty";
      return false;
    }

    if (!open_first_available_pipe()) {
      if (last_error_.empty()) {
        last_error_ = "discord ipc pipe not found";
      }
      return false;
    }

    const nlohmann::json handshake = {
      {"v", 1},
      {"client_id", std::string(client_id)},
    };

    if (!send_frame(0, handshake.dump()) || !response_ok(read_frame())) {
      close();
      return false;
    }

    return true;
}

bool IpcClient::set_activity(const rpc::ActivityPayload& activity) {
    if (!connected()) {
      return false;
    }

    nlohmann::json activity_json = {
      {"details", activity.details},
      {"state", activity.state},
    };

    if (activity.start_timestamp_unix != 0) {
      activity_json["timestamps"] = {{"start", activity.start_timestamp_unix}};
    }

    if (!activity.large_image.empty()) {
      nlohmann::json assets = {{"large_image", activity.large_image}};
      if (!activity.large_text.empty()) {
        assets["large_text"] = activity.large_text;
      }
      if (!activity.small_image.empty()) {
        assets["small_image"] = activity.small_image;
      }
      if (!activity.small_text.empty()) {
        assets["small_text"] = activity.small_text;
      }
      activity_json["assets"] = assets;
    }

    const nlohmann::json command = {
      {"cmd", "SET_ACTIVITY"},
      {"args", {
        {"pid", current_process_id()},
        {"activity", activity_json},
      }},
      {"nonce", next_nonce()},
    };

    if (!send_frame(1, command.dump()) || !response_ok(read_frame())) {
      close();
      return false;
    }

    return true;
}

bool IpcClient::clear_activity() {
    if (!connected()) {
      return false;
    }

    const nlohmann::json command = {
      {"cmd", "SET_ACTIVITY"},
      {"args", {
        {"pid", current_process_id()},
        {"activity", nullptr},
      }},
      {"nonce", next_nonce()},
    };

    if (!send_frame(1, command.dump()) || !response_ok(read_frame())) {
      close();
      return false;
    }

    return true;
}

void IpcClient::close() {
    if (!connected()) {
      return;
    }

#if defined(_WIN32)
    CloseHandle(reinterpret_cast<HANDLE>(native_handle_));
#else
    ::close(static_cast<int>(native_handle_));
#endif
    native_handle_ = invalid_handle();
}

std::string IpcClient::next_nonce() {
    ++nonce_counter_;
    return std::to_string(nonce_counter_);
}

bool IpcClient::send_frame(std::uint32_t opcode, const std::string& payload) {
    if (!connected() || payload.size() > (std::numeric_limits<std::uint32_t>::max)()) {
      return false;
    }

    const auto header = make_header(opcode, static_cast<std::uint32_t>(payload.size()));
    return write_all(header.data(), header.size()) && write_all(payload.data(), payload.size());
}

bool IpcClient::write_all(const void* data, std::size_t size) {
    const auto* cursor = static_cast<const std::uint8_t*>(data);
    std::size_t remaining = size;

    while (remaining > 0) {
#if defined(_WIN32)
      DWORD written = 0;
      const DWORD chunk_size = static_cast<DWORD>(
        std::min<std::size_t>(remaining, (std::numeric_limits<DWORD>::max)()));
      if (WriteFile(reinterpret_cast<HANDLE>(native_handle_), cursor, chunk_size,
                    &written, nullptr) == FALSE || written == 0) {
        return false;
      }
#else
      const ssize_t written = ::write(static_cast<int>(native_handle_), cursor, remaining);
      if (written < 0) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }
      if (written == 0) {
        return false;
      }
#endif
      cursor += written;
      remaining -= static_cast<std::size_t>(written);
    }

    return true;
}

std::optional<IpcClient::IpcFrame> IpcClient::read_frame() {
    std::array<std::uint8_t, 8> header{};
    if (!read_all(header.data(), header.size())) {
      return std::nullopt;
    }

    const std::uint32_t opcode =
      (static_cast<std::uint32_t>(header[0]) << 0U) |
      (static_cast<std::uint32_t>(header[1]) << 8U) |
      (static_cast<std::uint32_t>(header[2]) << 16U) |
      (static_cast<std::uint32_t>(header[3]) << 24U);

    const std::uint32_t length =
      (static_cast<std::uint32_t>(header[4]) << 0U) |
      (static_cast<std::uint32_t>(header[5]) << 8U) |
      (static_cast<std::uint32_t>(header[6]) << 16U) |
      (static_cast<std::uint32_t>(header[7]) << 24U);

    std::string payload(length, '\0');
    if (length > 0 && !read_all(payload.data(), payload.size())) {
      return std::nullopt;
    }

    return IpcFrame{opcode, std::move(payload)};
}

bool IpcClient::response_ok(const std::optional<IpcFrame>& frame) {
    if (!frame.has_value()) {
      last_error_ = "response read failed";
      return false;
    }

    if (frame->opcode == 2) {
      last_error_ = frame->payload.empty() ? "discord closed ipc pipe" : frame->payload;
      return false;
    }

    return true;
}

bool IpcClient::read_all(void* data, std::size_t size) {
    auto* cursor = static_cast<std::uint8_t*>(data);
    std::size_t remaining = size;

    while (remaining > 0) {
#if defined(_WIN32)
      DWORD bytes_read = 0;
      const DWORD chunk_size = static_cast<DWORD>(
        std::min<std::size_t>(remaining, (std::numeric_limits<DWORD>::max)()));
      if (ReadFile(reinterpret_cast<HANDLE>(native_handle_), cursor, chunk_size,
                   &bytes_read, nullptr) == FALSE || bytes_read == 0) {
        return false;
      }
#else
      const ssize_t bytes_read = ::read(static_cast<int>(native_handle_), cursor, remaining);
      if (bytes_read < 0) {
        if (errno == EINTR) {
          continue;
        }
        return false;
      }
      if (bytes_read == 0) {
        return false;
      }
#endif
      cursor += bytes_read;
      remaining -= static_cast<std::size_t>(bytes_read);
    }

    return true;
}

bool IpcClient::open_first_available_pipe() {
#if defined(_WIN32)
    for (int index = 0; index < 10; ++index) {
      const std::wstring pipe_name = LR"(\\?\pipe\discord-ipc-)" + std::to_wstring(index);
      HANDLE pipe = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                nullptr, OPEN_EXISTING, 0, nullptr);
      if (pipe == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY) {
        if (WaitNamedPipeW(pipe_name.c_str(), 250) != FALSE) {
          pipe = CreateFileW(pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                             nullptr, OPEN_EXISTING, 0, nullptr);
        }
      }

      if (pipe != INVALID_HANDLE_VALUE) {
        DWORD pipe_mode = PIPE_READMODE_BYTE;
        SetNamedPipeHandleState(pipe, &pipe_mode, nullptr, nullptr);
        native_handle_ = reinterpret_cast<std::intptr_t>(pipe);
        return true;
      }
    }
#else
    for (const auto& path : candidate_socket_paths()) {
      const int socket_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
      if (socket_fd < 0) {
        continue;
      }

      sockaddr_un address{};
      address.sun_family = AF_UNIX;
      if (path.size() >= sizeof(address.sun_path)) {
        ::close(socket_fd);
        continue;
      }
      std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);

      if (::connect(socket_fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
        native_handle_ = socket_fd;
        return true;
      }

      ::close(socket_fd);
    }
#endif
    last_error_ = "discord ipc pipe not found";
    return false;
}

#if !defined(_WIN32)
std::vector<std::string> IpcClient::candidate_socket_paths() {
    std::vector<std::string> base_paths;
    if (const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR")) {
      base_paths.emplace_back(runtime_dir);
    }
    if (const char* tmp_dir = std::getenv("TMPDIR")) {
      base_paths.emplace_back(tmp_dir);
    }
    base_paths.emplace_back("/tmp");

    std::vector<std::string> paths;
    for (const auto& base_path : base_paths) {
      for (int index = 0; index < 10; ++index) {
        paths.emplace_back((std::filesystem::path(base_path) /
                            ("discord-ipc-" + std::to_string(index))).string());
      }
    }
    return paths;
}
#endif

} // namespace rpc::discord
