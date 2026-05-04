#if defined(_WIN32)

#include <rpc/platform/icon.hpp>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <objbase.h>
#include <wincodec.h>

namespace {

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
void release_com(T*& ptr) {
  if (ptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

[[nodiscard]] std::filesystem::path executable_directory() {
  std::wstring buffer(MAX_PATH, L'\0');
  while (true) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return {};
    }

    if (length < buffer.size() - 1) {
      buffer.resize(length);
      break;
    }

    buffer.resize(buffer.size() * 2);
  }

  return std::filesystem::path(buffer).parent_path();
}

[[nodiscard]] std::filesystem::path app_icon_path() {
  const auto base = executable_directory() / L"res";
  const auto transparent = base / L"discord_rpc_transparent.png";
  if (std::filesystem::exists(transparent)) {
    return transparent;
  }

  const auto opaque = base / L"discord_rpc.png";
  if (std::filesystem::exists(opaque)) {
    return opaque;
  }

  return transparent;
}

[[nodiscard]] HICON create_icon_from_png(const std::filesystem::path& path) {
  ComApartment com_apartment{};

  IWICImagingFactory* factory = nullptr;
  IWICBitmapDecoder* decoder = nullptr;
  IWICBitmapFrameDecode* frame = nullptr;
  IWICFormatConverter* converter = nullptr;

  HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IWICImagingFactory,
                                reinterpret_cast<void**>(&factory));
  if (FAILED(hr)) {
    return nullptr;
  }

  hr = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                         WICDecodeMetadataCacheOnLoad, &decoder);
  if (SUCCEEDED(hr)) {
    hr = decoder->GetFrame(0, &frame);
  }
  if (SUCCEEDED(hr)) {
    hr = factory->CreateFormatConverter(&converter);
  }
  if (SUCCEEDED(hr)) {
    hr = converter->Initialize(frame, GUID_WICPixelFormat32bppPBGRA,
                               WICBitmapDitherTypeNone, nullptr, 0.0,
                               WICBitmapPaletteTypeCustom);
  }

  UINT width = 0;
  UINT height = 0;
  if (SUCCEEDED(hr)) {
    hr = frame->GetSize(&width, &height);
  }

  std::vector<std::uint8_t> pixels;
  if (SUCCEEDED(hr)) {
    const std::size_t pixel_bytes = static_cast<std::size_t>(width) *
                                    static_cast<std::size_t>(height) * 4u;
    pixels.resize(pixel_bytes);
    hr = converter->CopyPixels(nullptr, width * 4u,
                               static_cast<UINT>(pixels.size()),
                               pixels.data());
  }

  HICON icon = nullptr;
  if (SUCCEEDED(hr) && width > 0 && height > 0 && !pixels.empty()) {
    HDC screen_dc = GetDC(nullptr);
    if (screen_dc) {
      BITMAPINFO bmi{};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = static_cast<LONG>(width);
      bmi.bmiHeader.biHeight = -static_cast<LONG>(height);
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;

      void* dib_pixels = nullptr;
      HBITMAP color_bitmap = CreateDIBSection(screen_dc, &bmi, DIB_RGB_COLORS,
                                              &dib_pixels, nullptr, 0);
      if (color_bitmap && dib_pixels) {
        std::memcpy(dib_pixels, pixels.data(), pixels.size());

        const std::size_t mask_stride = ((static_cast<std::size_t>(width) + 31u) / 32u) * 4u;
        std::vector<std::uint8_t> mask_bits(mask_stride * static_cast<std::size_t>(height), 0);
        HBITMAP mask_bitmap = CreateBitmap(
          static_cast<int>(width),
          static_cast<int>(height),
          1,
          1,
          mask_bits.data());

        if (mask_bitmap) {
          ICONINFO icon_info{};
          icon_info.fIcon = TRUE;
          icon_info.hbmColor = color_bitmap;
          icon_info.hbmMask = mask_bitmap;
          icon = CreateIconIndirect(&icon_info);
          DeleteObject(mask_bitmap);
        }

        DeleteObject(color_bitmap);
      }

      ReleaseDC(nullptr, screen_dc);
    }
  }

  release_com(converter);
  release_com(frame);
  release_com(decoder);
  release_com(factory);

  return icon;
}

} // namespace

namespace rpc::platform {

HICON load_app_icon() {
  const std::filesystem::path path = app_icon_path();
  if (path.empty()) {
    return nullptr;
  }

  return create_icon_from_png(path);
}

} // namespace rpc::platform

#endif
