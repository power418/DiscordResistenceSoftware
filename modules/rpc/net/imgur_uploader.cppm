module;

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

export module rpc.net.imgur_uploader;

export namespace rpc::net {

/// Upload image data to a public host and return the public URL.
/// Uses Imgur when a client ID is provided, otherwise falls back to anonymous
/// hosts in order: Catbox, Uguu, then 0x0.st.
[[nodiscard]] std::optional<std::string>
upload_to_imgur(const std::vector<std::uint8_t>& image_data,
                std::string_view client_id,
                std::string_view file_name,
                std::string_view content_type);

[[nodiscard]] inline std::optional<std::string>
upload_to_imgur(const std::vector<std::uint8_t>& image_data,
                std::string_view client_id) {
  return upload_to_imgur(image_data, client_id, "icon.png", "image/png");
}

} // namespace rpc::net
