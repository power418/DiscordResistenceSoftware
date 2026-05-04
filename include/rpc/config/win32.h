#pragma once

// Win32 desktop baseline for the app shell.
// Keep this aligned with the Win32 shell and any future WinRT/WinUI adapter.
#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif

#  ifndef RPC_WINDOWS_TARGET_VERSION
#    define RPC_WINDOWS_TARGET_VERSION 0x0A00
#  endif

#  ifndef WINVER
#    define WINVER RPC_WINDOWS_TARGET_VERSION
#  endif

#  ifndef _WIN32_WINNT
#    define _WIN32_WINNT RPC_WINDOWS_TARGET_VERSION
#  endif

#  define RPC_WINDOWS_COMMON_CONTROLS_MANIFEST \
    "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' " \
    "version='6.0.0.0' processorArchitecture='*' " \
    "publicKeyToken='6595b64144ccf1df' language='*'\""

#  pragma comment(linker, RPC_WINDOWS_COMMON_CONTROLS_MANIFEST)
#endif
