#pragma once

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

namespace rpc::platform {

#if defined(_WIN32)
[[nodiscard]] HICON load_app_icon();
#endif

} // namespace rpc::platform
