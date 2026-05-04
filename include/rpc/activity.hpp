#pragma once

#include <cstdint>
#include <string>

namespace rpc {

struct ActivityPayload {
  std::string details;
  std::string state;
  std::uint64_t start_timestamp_unix = 0;
  std::string large_image;  // Asset key uploaded to Discord Developer Portal
  std::string large_text;   // Tooltip shown on hover over the large image
};

} // namespace rpc

