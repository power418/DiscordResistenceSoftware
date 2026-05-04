#pragma once

#include <cstdint>
#include <string>

namespace rpc {

struct ActivityPayload {
  std::string details;
  std::string state;
  std::uint64_t start_timestamp_unix = 0;
  std::string large_image;  // Asset key or external URL
  std::string large_text;   // Tooltip shown on hover over the large image
  std::string small_image;  // Small overlay icon (asset key or external URL)
  std::string small_text;   // Tooltip shown on hover over the small image
};

} // namespace rpc

