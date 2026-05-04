module;

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cwctype>
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
#  include <wincodec.h>
#endif

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

[[nodiscard]] inline bool equals_path_case_insensitive(std::wstring_view a,
                                                       std::wstring_view b) {
  if (a.size() != b.size()) {
    return false;
  }

  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::towlower(a[i]) != std::towlower(b[i])) {
      return false;
    }
  }

  return true;
}

[[nodiscard]] inline std::wstring get_process_image_path(DWORD pid) {
  HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
  if (!process) {
    return {};
  }

  std::wstring buffer(512, L'\0');
  DWORD size = static_cast<DWORD>(buffer.size());

  for (int attempt = 0; attempt < 4; ++attempt) {
    if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size) != 0) {
      CloseHandle(process);
      buffer.resize(size);
      return buffer;
    }

    buffer.resize(buffer.size() * 2);
    size = static_cast<DWORD>(buffer.size());
  }

  CloseHandle(process);
  return {};
}

[[nodiscard]] inline std::wstring get_window_process_path(HWND hwnd) {
  if (!hwnd) {
    return {};
  }

  DWORD pid = 0;
  GetWindowThreadProcessId(hwnd, &pid);
  if (pid == 0) {
    return {};
  }

  return get_process_image_path(pid);
}

class ComApartment {
public:
  ComApartment() {
    const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    initialized_ = (hr == S_OK || hr == S_FALSE);
  }

  ~ComApartment() {
    if (initialized_) {
      CoUninitialize();
    }
  }

  ComApartment(const ComApartment&) = delete;
  ComApartment& operator=(const ComApartment&) = delete;

private:
  bool initialized_ = false;
};

template <typename T>
inline void release_com(T*& ptr) {
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

[[nodiscard]] inline HICON extract_jumbo_icon(const std::wstring& exe_path);
[[nodiscard]] inline HICON extract_basic_icon(const std::wstring& exe_path);
[[nodiscard]] inline HICON extract_window_icon(HWND hwnd);
[[nodiscard]] inline bool window_icon_matches_exe(HWND hwnd, std::string_view exe_path);

struct IconCandidate {
  HICON handle = nullptr;
  int width = 0;
  int height = 0;
  int priority = 0;
};

[[nodiscard]] inline IconCandidate make_icon_candidate(HICON icon, int priority) {
  IconCandidate candidate{};
  candidate.handle = icon;
  candidate.priority = priority;

  if (!icon) {
    return candidate;
  }

  ICONINFO icon_info{};
  if (!GetIconInfo(icon, &icon_info)) {
    return candidate;
  }

  BITMAP bm{};
  if (GetObject(icon_info.hbmColor ? icon_info.hbmColor : icon_info.hbmMask,
                sizeof(BITMAP), &bm) != 0) {
    candidate.width = bm.bmWidth;
    candidate.height = icon_info.hbmColor ? bm.bmHeight : bm.bmHeight / 2;
  }

  if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
  if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);

  return candidate;
}

[[nodiscard]] inline HICON pick_best_icon(std::vector<IconCandidate>& candidates) {
  std::size_t best_index = candidates.size();
  std::uint64_t best_area = 0;
  int best_priority = -1;

  for (std::size_t index = 0; index < candidates.size(); ++index) {
    const auto& candidate = candidates[index];
    if (!candidate.handle || candidate.width <= 0 || candidate.height <= 0) {
      continue;
    }

    const std::uint64_t area = static_cast<std::uint64_t>(candidate.width) *
                               static_cast<std::uint64_t>(candidate.height);
    if (best_index == candidates.size() ||
        area > best_area ||
        (area == best_area && candidate.priority > best_priority)) {
      best_index = index;
      best_area = area;
      best_priority = candidate.priority;
    }
  }

  if (best_index == candidates.size()) {
    for (auto& candidate : candidates) {
      if (candidate.handle) {
        DestroyIcon(candidate.handle);
        candidate.handle = nullptr;
      }
    }
    return nullptr;
  }

  const HICON selected = candidates[best_index].handle;
  for (std::size_t index = 0; index < candidates.size(); ++index) {
    if (index != best_index && candidates[index].handle) {
      DestroyIcon(candidates[index].handle);
      candidates[index].handle = nullptr;
    }
  }

  return selected;
}

// Try to get a high-res icon (256x256 jumbo) via the shell image list.
[[nodiscard]] inline HICON extract_jumbo_icon(const std::wstring& exe_path) {
  ComApartment com_apartment{};

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

// Fallback: ExtractIconExW (usually 32x32).
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

[[nodiscard]] inline HICON extract_best_icon(std::uintptr_t window_handle,
                                             std::string_view exe_path) {
  std::vector<IconCandidate> candidates;

  if (window_handle != 0 && icon_detail::window_icon_matches_exe(
        reinterpret_cast<HWND>(window_handle), exe_path)) {
    if (HICON window_icon = icon_detail::extract_window_icon(reinterpret_cast<HWND>(window_handle))) {
      candidates.push_back(make_icon_candidate(window_icon, 2));
    }
  }

  if (!exe_path.empty()) {
    const std::wstring wide_path = utf8_to_wide(exe_path);
    if (!wide_path.empty()) {
      if (HICON jumbo_icon = extract_jumbo_icon(wide_path)) {
        candidates.push_back(make_icon_candidate(jumbo_icon, 3));
      }

      if (HICON basic_icon = extract_basic_icon(wide_path)) {
        candidates.push_back(make_icon_candidate(basic_icon, 1));
      }
    }
  }

  return pick_best_icon(candidates);
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

[[nodiscard]] inline bool window_icon_matches_exe(HWND hwnd, std::string_view exe_path) {
  if (!hwnd || exe_path.empty()) {
    return false;
  }

  const std::wstring window_exe_path = get_window_process_path(hwnd);
  const std::wstring target_exe_path = utf8_to_wide(exe_path);
  if (window_exe_path.empty() || target_exe_path.empty()) {
    return false;
  }

  return equals_path_case_insensitive(window_exe_path, target_exe_path);
}

// Convert HICON to BGRA pixel data using pure Win32 GDI.
[[nodiscard]] inline std::vector<std::uint8_t>
icon_to_bgra(HICON hIcon, int& out_width, int& out_height) {
  ICONINFO icon_info{};
  if (!GetIconInfo(hIcon, &icon_info)) {
    return {};
  }

  BITMAP bm{};
  GetObject(icon_info.hbmColor ? icon_info.hbmColor : icon_info.hbmMask,
            sizeof(BITMAP), &bm);

  out_width = bm.bmWidth;
  out_height = bm.bmHeight;

  if (icon_info.hbmColor) DeleteObject(icon_info.hbmColor);
  if (icon_info.hbmMask) DeleteObject(icon_info.hbmMask);

  if (out_width <= 0 || out_height <= 0) return {};

  HDC screen_dc = GetDC(nullptr);
  if (!screen_dc) return {};

  HDC mem_dc = CreateCompatibleDC(screen_dc);
  if (!mem_dc) {
    ReleaseDC(nullptr, screen_dc);
    return {};
  }

  BITMAPINFO bmi{};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = out_width;
  bmi.bmiHeader.biHeight = -out_height;  // negative = top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* pixels = nullptr;
  HBITMAP dib = CreateDIBSection(mem_dc, &bmi, DIB_RGB_COLORS, &pixels, nullptr, 0);
  if (!dib || !pixels) {
    if (dib) {
      DeleteObject(dib);
    }
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    return {};
  }

  HGDIOBJ old_bmp = SelectObject(mem_dc, dib);
  if (!old_bmp || old_bmp == HGDI_ERROR) {
    DeleteObject(dib);
    DeleteDC(mem_dc);
    ReleaseDC(nullptr, screen_dc);
    return {};
  }

  // Clear to transparent black.
  std::memset(pixels, 0, static_cast<std::size_t>(out_width) * out_height * 4);

  // Draw the icon onto the DIB.
  DrawIconEx(mem_dc, 0, 0, hIcon, out_width, out_height, 0, nullptr, DI_NORMAL);

  const std::size_t byte_count = static_cast<std::size_t>(out_width) * out_height * 4;
  std::vector<std::uint8_t> bgra(byte_count);
  auto* src = static_cast<std::uint8_t*>(pixels);
  std::memcpy(bgra.data(), src, byte_count);

  SelectObject(mem_dc, old_bmp);
  DeleteObject(dib);
  DeleteDC(mem_dc);
  ReleaseDC(nullptr, screen_dc);

  return bgra;
}

[[nodiscard]] inline std::vector<std::uint8_t>
encode_png_bgra(const std::vector<std::uint8_t>& bgra, int width, int height) {
  if (bgra.empty() || width <= 0 || height <= 0) {
    return {};
  }

  // Discord recommends 1024x1024 art assets. Upscaling here helps the hosted
  // image stay sharp after Discord fetches and resizes it for the profile card.
  constexpr UINT target_size = 1024;

  ComApartment com_apartment{};

  IWICImagingFactory* factory = nullptr;
  IWICBitmapEncoder* encoder = nullptr;
  IWICBitmapFrameEncode* frame = nullptr;
  IPropertyBag2* properties = nullptr;
  IStream* stream = nullptr;
  IWICBitmap* source_bitmap = nullptr;
  IWICBitmapScaler* scaler = nullptr;

  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory,
                                reinterpret_cast<void**>(&factory));
  if (FAILED(hr)) {
    return {};
  }

  hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
  if (SUCCEEDED(hr)) {
    hr = factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
  }
  if (SUCCEEDED(hr)) {
    hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
  }
  if (SUCCEEDED(hr)) {
    hr = factory->CreateBitmapFromMemory(
      static_cast<UINT>(width), static_cast<UINT>(height),
      GUID_WICPixelFormat32bppBGRA, static_cast<UINT>(width * 4),
      static_cast<UINT>(bgra.size()), const_cast<BYTE*>(bgra.data()), &source_bitmap);
  }
  if (SUCCEEDED(hr) && (width != static_cast<int>(target_size) || height != static_cast<int>(target_size))) {
    hr = factory->CreateBitmapScaler(&scaler);
  }
  if (SUCCEEDED(hr) && scaler) {
    hr = scaler->Initialize(source_bitmap, target_size, target_size,
                            WICBitmapInterpolationModeHighQualityCubic);
  }
  if (SUCCEEDED(hr)) {
    hr = encoder->CreateNewFrame(&frame, &properties);
  }
  if (SUCCEEDED(hr)) {
    hr = frame->Initialize(properties);
  }
  if (SUCCEEDED(hr)) {
    hr = frame->SetSize(target_size, target_size);
  }
  if (SUCCEEDED(hr)) {
    WICPixelFormatGUID pixel_format = GUID_WICPixelFormat32bppBGRA;
    hr = frame->SetPixelFormat(&pixel_format);
    if (SUCCEEDED(hr) && !IsEqualGUID(pixel_format, GUID_WICPixelFormat32bppBGRA)) {
      hr = E_FAIL;
    }
  }
  if (SUCCEEDED(hr)) {
    IWICBitmapSource* bitmap_source = scaler
      ? reinterpret_cast<IWICBitmapSource*>(scaler)
      : reinterpret_cast<IWICBitmapSource*>(source_bitmap);
    hr = frame->WriteSource(bitmap_source, nullptr);
  }
  if (SUCCEEDED(hr)) {
    hr = frame->Commit();
  }
  if (SUCCEEDED(hr)) {
    hr = encoder->Commit();
  }

  std::vector<std::uint8_t> png_data;
  if (SUCCEEDED(hr)) {
    HGLOBAL memory = nullptr;
    hr = GetHGlobalFromStream(stream, &memory);
    if (SUCCEEDED(hr) && memory) {
      const SIZE_T size = GlobalSize(memory);
      if (size > 0) {
        if (void* bytes = GlobalLock(memory)) {
          const auto* first = static_cast<const std::uint8_t*>(bytes);
          png_data.assign(first, first + size);
          GlobalUnlock(memory);
        }
      }
    }
  }

  release_com(properties);
  release_com(frame);
  release_com(scaler);
  release_com(source_bitmap);
  release_com(encoder);
  release_com(stream);
  release_com(factory);

  return png_data;
}

} // namespace icon_detail

/// Extract the best available icon for a foreground window/executable and return it as PNG bytes.
/// Picks the highest-resolution icon available from the window or the executable,
/// then upscales the final PNG to a Discord-friendly asset size.
/// Uses Win32 GDI for icon extraction + Windows Imaging Component for PNG encoding.
/// Returns empty vector on failure.
[[nodiscard]] inline std::vector<std::uint8_t>
extract_icon_png(std::uintptr_t window_handle, std::string_view exe_path) {
  HICON hIcon = icon_detail::extract_best_icon(window_handle, exe_path);

  if (!hIcon) return {};

  int width = 0;
  int height = 0;
  auto bgra = icon_detail::icon_to_bgra(hIcon, width, height);
  DestroyIcon(hIcon);

  if (bgra.empty() || width <= 0 || height <= 0) return {};

  return icon_detail::encode_png_bgra(bgra, width, height);
}

/// Extract the icon from an executable and return it as PNG bytes.
/// Uses Win32 GDI for icon extraction + Windows Imaging Component for PNG encoding.
/// Returns empty vector on failure.
[[nodiscard]] inline std::vector<std::uint8_t>
extract_icon_png(std::string_view exe_path) {
  if (exe_path.empty()) return {};

  HICON hIcon = icon_detail::extract_best_icon(0, exe_path);
  if (!hIcon) return {};

  int width = 0;
  int height = 0;
  auto bgra = icon_detail::icon_to_bgra(hIcon, width, height);
  DestroyIcon(hIcon);

  if (bgra.empty() || width <= 0 || height <= 0) return {};

  return icon_detail::encode_png_bgra(bgra, width, height);
}

#else

// Linux/macOS stub - icon extraction not supported yet.
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
