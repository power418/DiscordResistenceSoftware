module;

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <shellapi.h>
#  include <shlobj.h>
#  include <commoncontrols.h>
#  include <objbase.h>
#endif

#include <lodepng.h>

export module rpc.os.icon_extractor;

export namespace rpc {

#if defined(_WIN32)

namespace icon_detail {

#ifndef ICON_SMALL2
#  define ICON_SMALL2 2
#endif

[[nodiscard]] inline std::wstring utf8_to_wide(std::string_view input) {
  if (input.empty()) return {};
  int needed = MultiByteToWideChar(CP_UTF8, 0, input.data(),
                                   static_cast<int>(input.size()), nullptr, 0);
  if (needed <= 0) return {};
  std::wstring output(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                      output.data(), needed);
  return output;
}

// Try to get a high-res icon (256x256 jumbo) via shell image list
[[nodiscard]] inline HICON extract_jumbo_icon(const std::wstring& exe_path) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  SHFILEINFOW sfi{};
  if (SHGetFileInfoW(exe_path.c_str(), 0, &sfi, sizeof(sfi),
                     SHGFI_SYSICONINDEX) == 0) {
    return nullptr;
  }

  IImageList* image_list = nullptr;
  // SHIL_JUMBO = 0x4 (256x256 on Vista+)
  HRESULT hr = SHGetImageList(0x4, IID_IImageList,
                              reinterpret_cast<void**>(&image_list));
  if (FAILED(hr) || !image_list) {
    // Fallback to SHIL_EXTRALARGE = 0x2 (48x48)
    hr = SHGetImageList(0x2, IID_IImageList,
                        reinterpret_cast<void**>(&image_list));
    if (FAILED(hr) || !image_list) return nullptr;
  }

  HICON hIcon = nullptr;
  image_list->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
  image_list->Release();
  return hIcon;
}

// Fallback: ExtractIconExW (32x32)
[[nodiscard]] inline HICON extract_basic_icon(const std::wstring& exe_path) {
  HICON large_icon = nullptr;
  UINT count = ExtractIconExW(exe_path.c_str(), 0, &large_icon, nullptr, 1);
  if (count == 0 || !large_icon) return nullptr;
  return large_icon;
}

[[nodiscard]] inline HICON duplicate_icon(HICON hIcon) {
  if (!hIcon) {
    return nullptr;
  }

  return CopyIcon(hIcon);
}

[[nodiscard]] inline HICON extract_window_icon(HWND hwnd) {
  if (!hwnd) {
    return nullptr;
  }

  constexpr UINT icon_sizes[] = {ICON_SMALL2, ICON_SMALL, ICON_BIG};
  for (UINT icon_size : icon_sizes) {
    DWORD_PTR icon_result = 0;
    if (SendMessageTimeoutW(hwnd, WM_GETICON, icon_size, 0,
                            SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &icon_result) != 0) {
      if (auto* icon = reinterpret_cast<HICON>(icon_result)) {
        if (auto* duplicated = duplicate_icon(icon)) {
          return duplicated;
        }
      }
    }
  }

  if (auto* class_icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICONSM))) {
    if (auto* duplicated = duplicate_icon(class_icon)) {
      return duplicated;
    }
  }

  if (auto* class_icon = reinterpret_cast<HICON>(GetClassLongPtrW(hwnd, GCLP_HICON))) {
    if (auto* duplicated = duplicate_icon(class_icon)) {
      return duplicated;
    }
  }

  return nullptr;
}

// Convert HICON to RGBA pixel data using pure Win32 GDI (no GDI+ needed)
[[nodiscard]] inline std::vector<std::uint8_t>
icon_to_rgba(HICON hIcon, int& out_width, int& out_height) {
  // Get icon dimensions
  ICONINFO icon_info{};
  if (!GetIconInfo(hIcon, &icon_info)) return {};

  BITMAP bm{};
  GetObject(icon_info.hbmColor ? icon_info.hbmColor : icon_info.hbmMask,
            sizeof(BITMAP), &bm);

  out_width = bm.bmWidth;
  out_height = bm.bmHeight;

  if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
  if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);

  if (out_width <= 0 || out_height <= 0) return {};

  // Create a 32-bit top-down DIB section
  HDC screen_dc = GetDC(nullptr);
  HDC mem_dc = CreateCompatibleDC(screen_dc);

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = out_width;
  bmi.bmiHeader.biHeight = -out_height;  // negative = top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* pixels = nullptr;
  HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS,
                                 &pixels, nullptr, 0);
  if (!dib || !pixels) {
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    return {};
  }

  HGDIOBJ old_bmp = SelectObject(mem_dc, dib);

  // Clear to transparent black
  std::memset(pixels, 0,
              static_cast<std::size_t>(out_width) * out_height * 4);

  // Draw the icon onto the DIB
  DrawIconEx(mem_dc, 0, 0, hIcon, out_width, out_height,
             0, nullptr, DI_NORMAL);

  // Convert BGRA → RGBA
  const int pixel_count = out_width * out_height;
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(pixel_count) * 4);
  auto* src = static_cast<std::uint8_t*>(pixels);

  for (int i = 0; i < pixel_count; ++i) {
    const int offset = i * 4;
    rgba[offset + 0] = src[offset + 2];  // R ← B
    rgba[offset + 1] = src[offset + 1];  // G
    rgba[offset + 2] = src[offset + 0];  // B ← R
    rgba[offset + 3] = src[offset + 3];  // A
  }

  // Cleanup GDI
  SelectObject(mem_dc, old_bmp);
  DeleteObject(dib);
  DeleteDC(mem_dc);
  ReleaseDC(nullptr, screen_dc);

  return rgba;
}

} // namespace icon_detail

/// Extract the best available icon for a foreground window/executable and return it as PNG bytes.
/// Tries the window icon first, then falls back to the executable icon.
/// Uses Win32 GDI for icon extraction + lodepng for PNG encoding.
/// Returns empty vector on failure.
[[nodiscard]] inline std::vector<std::uint8_t>
extract_icon_png(std::uintptr_t window_handle, std::string_view exe_path) {
  HICON hIcon = nullptr;

  if (window_handle != 0) {
    hIcon = icon_detail::extract_window_icon(reinterpret_cast<HWND>(window_handle));
  }

  if (!hIcon && !exe_path.empty()) {
    const std::wstring wide_path = icon_detail::utf8_to_wide(exe_path);
    if (!wide_path.empty()) {
      // Try jumbo (256x256) first, then basic (32x32)
      hIcon = icon_detail::extract_jumbo_icon(wide_path);
      if (!hIcon) {
        hIcon = icon_detail::extract_basic_icon(wide_path);
      }
    }
  }
  if (!hIcon) return {};

  // Convert HICON â†’ RGBA pixels
  int width = 0, height = 0;
  auto rgba = icon_detail::icon_to_rgba(hIcon, width, height);
  DestroyIcon(hIcon);

  if (rgba.empty() || width <= 0 || height <= 0) return {};

  // Encode RGBA pixels â†’ PNG using lodepng
  std::vector<std::uint8_t> png_data;
  unsigned error = lodepng::encode(png_data, rgba,
                                   static_cast<unsigned>(width),
                                   static_cast<unsigned>(height));
  if (error != 0) return {};

  return png_data;
}

/// Extract the icon from an executable and return it as PNG bytes.
/// Uses Win32 GDI for icon extraction + lodepng for PNG encoding.
/// Returns empty vector on failure.
[[nodiscard]] inline std::vector<std::uint8_t>
extract_icon_png(std::string_view exe_path) {
  if (exe_path.empty()) return {};

  const std::wstring wide_path = icon_detail::utf8_to_wide(exe_path);
  if (wide_path.empty()) return {};

  // Try jumbo (256x256) first, then basic (32x32)
  HICON hIcon = icon_detail::extract_jumbo_icon(wide_path);
  if (!hIcon) {
    hIcon = icon_detail::extract_basic_icon(wide_path);
  }
  if (!hIcon) return {};

  // Convert HICON → RGBA pixels
  int width = 0, height = 0;
  auto rgba = icon_detail::icon_to_rgba(hIcon, width, height);
  DestroyIcon(hIcon);

  if (rgba.empty() || width <= 0 || height <= 0) return {};

  // Encode RGBA pixels → PNG using lodepng
  std::vector<std::uint8_t> png_data;
  unsigned error = lodepng::encode(png_data, rgba,
                                   static_cast<unsigned>(width),
                                   static_cast<unsigned>(height));
  if (error != 0) return {};

  return png_data;
}

#else

// Linux/macOS stub — icon extraction not supported yet
[[nodiscard]] inline std::vector<std::uint8_t>
extract_icon_png(std::uintptr_t /*window_handle*/, std::string_view /*exe_path*/) {
  return {};
}

[[nodiscard]] inline std::vector<std::uint8_t>
extract_icon_png(std::string_view /*exe_path*/) {
  return {};
}

#endif

} // namespace rpc
