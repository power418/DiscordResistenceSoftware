#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <modules/rpc/activity.cppm>
#include <modules/rpc/core.cppm>

namespace rpc::discord {

class IpcClient {
public:
  IpcClient() = default;
  ~IpcClient();

  IpcClient(const IpcClient&) = delete;
  IpcClient& operator=(const IpcClient&) = delete;

  IpcClient(IpcClient&& other) noexcept;
  IpcClient& operator=(IpcClient&& other) noexcept;

  [[nodiscard]] bool connected() const;
  [[nodiscard]] std::string_view last_error() const;

  bool connect(std::string_view client_id);
  bool set_activity(const rpc::ActivityPayload& activity);
  bool clear_activity();
  void close();

private:
  struct IpcFrame {
    std::uint32_t opcode = 0;
    std::string payload;
  };

  std::intptr_t native_handle_ = invalid_handle();
  std::uint64_t nonce_counter_ = 0;
  std::string last_error_;

  [[nodiscard]] static constexpr std::intptr_t invalid_handle() {
    return -1;
  }

  [[nodiscard]] static bool is_valid_handle(std::intptr_t handle);
  [[nodiscard]] static std::uint32_t current_process_id();

  [[nodiscard]] std::string next_nonce();

  [[nodiscard]] static std::array<std::uint8_t, 8> make_header(std::uint32_t opcode,
                                                               std::uint32_t length);

  bool send_frame(std::uint32_t opcode, const std::string& payload);
  bool write_all(const void* data, std::size_t size);

  [[nodiscard]] std::optional<IpcFrame> read_frame();
  [[nodiscard]] bool response_ok(const std::optional<IpcFrame>& frame);
  bool read_all(void* data, std::size_t size);

  bool open_first_available_pipe();

#if !defined(_WIN32)
  [[nodiscard]] static std::vector<std::string> candidate_socket_paths();
#endif
};

} // namespace rpc::discord
