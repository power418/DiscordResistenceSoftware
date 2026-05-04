module;

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#if defined(_WIN32)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <winhttp.h>
#  include <wincrypt.h>
#endif

#include <nlohmann/json.hpp>

export module rpc.net.imgur_uploader;

export namespace rpc::net {

#if defined(_WIN32)

namespace upload_detail {

[[nodiscard]] inline std::string base64_encode(const std::vector<std::uint8_t>& data) {
  if (data.empty()) return {};

  DWORD b64_size = 0;
  CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()),
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                       nullptr, &b64_size);
  if (b64_size == 0) return {};

  std::string result(b64_size, '\0');
  CryptBinaryToStringA(data.data(), static_cast<DWORD>(data.size()),
                       CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                       result.data(), &b64_size);
  result.resize(b64_size);
  return result;
}

[[nodiscard]] inline std::string url_encode_base64(std::string_view input) {
  std::string output;
  output.reserve(input.size() + input.size() / 4);
  for (char c : input) {
    switch (c) {
      case '+': output += "%2B"; break;
      case '/': output += "%2F"; break;
      case '=': output += "%3D"; break;
      default:  output += c;     break;
    }
  }
  return output;
}

[[nodiscard]] inline std::wstring to_wide(std::string_view input) {
  if (input.empty()) return {};
  int needed = MultiByteToWideChar(CP_UTF8, 0, input.data(),
                                   static_cast<int>(input.size()), nullptr, 0);
  if (needed <= 0) return {};
  std::wstring output(static_cast<std::size_t>(needed), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                      output.data(), needed);
  return output;
}

} // namespace upload_detail

/// Upload PNG image data to Imgur (anonymous) and return the public URL.
/// Requires a valid Imgur Client-ID.
[[nodiscard]] inline std::optional<std::string>
upload_to_imgur(const std::vector<std::uint8_t>& png_data,
               std::string_view client_id) {
  if (png_data.empty() || client_id.empty()) return std::nullopt;

  // Base64 encode the PNG data
  std::string b64 = upload_detail::base64_encode(png_data);
  if (b64.empty()) return std::nullopt;

  // Build POST body: image=<url_encoded_base64>&type=base64
  std::string body = "image=" + upload_detail::url_encode_base64(b64) + "&type=base64";

  // Open WinHTTP session
  HINTERNET session = WinHttpOpen(
    L"SoftwareDiscordRPC/1.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) return std::nullopt;

  HINTERNET connection = WinHttpConnect(
    session, L"api.imgur.com",
    INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!connection) {
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  HINTERNET request = WinHttpOpenRequest(
    connection, L"POST", L"/3/image",
    nullptr, WINHTTP_NO_REFERER,
    WINHTTP_DEFAULT_ACCEPT_TYPES,
    WINHTTP_FLAG_SECURE);
  if (!request) {
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  // Set headers
  std::wstring auth_header = L"Authorization: Client-ID " +
                             upload_detail::to_wide(client_id);
  WinHttpAddRequestHeaders(request, auth_header.c_str(),
                           static_cast<DWORD>(auth_header.size()),
                           WINHTTP_ADDREQ_FLAG_ADD);

  const wchar_t* content_type = L"Content-Type: application/x-www-form-urlencoded";
  WinHttpAddRequestHeaders(request, content_type,
                           static_cast<DWORD>(wcslen(content_type)),
                           WINHTTP_ADDREQ_FLAG_ADD);

  // Send request
  BOOL sent = WinHttpSendRequest(
    request,
    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
    const_cast<char*>(body.data()),
    static_cast<DWORD>(body.size()),
    static_cast<DWORD>(body.size()), 0);

  if (!sent || !WinHttpReceiveResponse(request, nullptr)) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  // Check HTTP status
  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  WinHttpQueryHeaders(request,
                      WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                      WINHTTP_HEADER_NAME_BY_INDEX,
                      &status_code, &status_size, WINHTTP_NO_HEADER_INDEX);

  // Read response body
  std::string response_body;
  DWORD bytes_available = 0;
  while (WinHttpQueryDataAvailable(request, &bytes_available) && bytes_available > 0) {
    std::string chunk(bytes_available, '\0');
    DWORD bytes_read = 0;
    WinHttpReadData(request, chunk.data(), bytes_available, &bytes_read);
    response_body.append(chunk.data(), bytes_read);
    bytes_available = 0;
  }

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);

  if (status_code != 200 || response_body.empty()) {
    return std::nullopt;
  }

  // Parse JSON response → data.link
  try {
    auto json = nlohmann::json::parse(response_body);
    if (json.contains("data") && json["data"].contains("link")) {
      return json["data"]["link"].get<std::string>();
    }
  } catch (...) {
    // JSON parse failure
  }

  return std::nullopt;
}

#else

// Linux/macOS stub
[[nodiscard]] inline std::optional<std::string>
upload_to_imgur(const std::vector<std::uint8_t>& /*png_data*/,
               std::string_view /*client_id*/) {
  return std::nullopt;
}

#endif

} // namespace rpc::net
