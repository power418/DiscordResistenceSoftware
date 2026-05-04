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
#  include <wincrypt.h>
#  include <winhttp.h>
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
      default: output += c; break;
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

[[nodiscard]] inline std::string trim_copy(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }

  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

[[nodiscard]] inline std::optional<std::string> read_response_body(HINTERNET request) {
  std::string response_body;
  DWORD bytes_available = 0;
  while (WinHttpQueryDataAvailable(request, &bytes_available) && bytes_available > 0) {
    std::string chunk(bytes_available, '\0');
    DWORD bytes_read = 0;
    if (WinHttpReadData(request, chunk.data(), bytes_available, &bytes_read) == FALSE) {
      return std::nullopt;
    }
    response_body.append(chunk.data(), bytes_read);
    bytes_available = 0;
  }

  if (response_body.empty()) {
    return std::nullopt;
  }

  return response_body;
}

[[nodiscard]] inline std::optional<std::string> perform_request(
  const std::wstring& host,
  const std::wstring& path,
  const std::wstring& content_type,
  const std::string& body,
  const std::wstring& extra_headers = {}) {
  HINTERNET session = WinHttpOpen(
    L"SoftwareDiscordRPC/1.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) return std::nullopt;

  HINTERNET connection = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!connection) {
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  HINTERNET request = WinHttpOpenRequest(
    connection, L"POST", path.c_str(),
    nullptr, WINHTTP_NO_REFERER,
    WINHTTP_DEFAULT_ACCEPT_TYPES,
    WINHTTP_FLAG_SECURE);
  if (!request) {
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  const std::wstring content_type_header = L"Content-Type: " + content_type;
  const std::wstring user_agent_header = L"User-Agent: SoftwareDiscordRPC/1.0";

  if (WinHttpAddRequestHeaders(request, content_type_header.c_str(),
                               static_cast<DWORD>(content_type_header.size()),
                               WINHTTP_ADDREQ_FLAG_ADD) == FALSE ||
      WinHttpAddRequestHeaders(request, user_agent_header.c_str(),
                               static_cast<DWORD>(user_agent_header.size()),
                               WINHTTP_ADDREQ_FLAG_ADD) == FALSE ||
      (!extra_headers.empty() &&
       WinHttpAddRequestHeaders(request, extra_headers.c_str(),
                                static_cast<DWORD>(extra_headers.size()),
                                WINHTTP_ADDREQ_FLAG_ADD) == FALSE)) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  const BOOL sent = WinHttpSendRequest(
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

  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  if (WinHttpQueryHeaders(request,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX,
                          &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) == FALSE) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  auto response_body = read_response_body(request);

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);

  if (!response_body.has_value() || status_code / 100U != 2U) {
    return std::nullopt;
  }

  return trim_copy(*response_body);
}

[[nodiscard]] inline std::optional<std::string>
upload_to_0x0st(const std::vector<std::uint8_t>& image_data) {
  if (image_data.empty()) {
    return std::nullopt;
  }

  constexpr char boundary[] = "----SoftwareDiscordRPC0x0Boundary7d3e8c21";
  std::string body;
  body.reserve(image_data.size() + 512);
  body.append("--");
  body.append(boundary);
  body.append("\r\n");
  body.append("Content-Disposition: form-data; name=\"file\"; filename=\"icon.png\"\r\n");
  body.append("Content-Type: image/png\r\n\r\n");
  body.append(reinterpret_cast<const char*>(image_data.data()), image_data.size());
  body.append("\r\n--");
  body.append(boundary);
  body.append("--\r\n");

  const std::wstring content_type = L"multipart/form-data; boundary=" + to_wide(boundary);
  return perform_request(L"0x0.st", L"/", content_type, body);
}

[[nodiscard]] inline std::optional<std::string>
upload_to_imgur_internal(const std::vector<std::uint8_t>& image_data,
                         std::string_view client_id) {
  if (image_data.empty() || client_id.empty()) return std::nullopt;

  std::string b64 = base64_encode(image_data);
  if (b64.empty()) return std::nullopt;

  std::string body = "image=" + url_encode_base64(b64) + "&type=base64";

  HINTERNET session = WinHttpOpen(
    L"SoftwareDiscordRPC/1.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
    WINHTTP_NO_PROXY_NAME,
    WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) return std::nullopt;

  HINTERNET connection = WinHttpConnect(session, L"api.imgur.com",
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

  const std::wstring auth_header = L"Authorization: Client-ID " + to_wide(client_id);
  const std::wstring content_type_header = L"Content-Type: application/x-www-form-urlencoded";

  if (WinHttpAddRequestHeaders(request, auth_header.c_str(),
                               static_cast<DWORD>(auth_header.size()),
                               WINHTTP_ADDREQ_FLAG_ADD) == FALSE ||
      WinHttpAddRequestHeaders(request, content_type_header.c_str(),
                               static_cast<DWORD>(content_type_header.size()),
                               WINHTTP_ADDREQ_FLAG_ADD) == FALSE) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  const BOOL sent = WinHttpSendRequest(
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

  DWORD status_code = 0;
  DWORD status_size = sizeof(status_code);
  if (WinHttpQueryHeaders(request,
                          WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                          WINHTTP_HEADER_NAME_BY_INDEX,
                          &status_code, &status_size, WINHTTP_NO_HEADER_INDEX) == FALSE) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connection);
    WinHttpCloseHandle(session);
    return std::nullopt;
  }

  auto response_body = read_response_body(request);

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connection);
  WinHttpCloseHandle(session);

  if (!response_body.has_value() || status_code / 100U != 2U) {
    return std::nullopt;
  }

  try {
    auto json = nlohmann::json::parse(*response_body);
    if (json.contains("data") && json["data"].contains("link")) {
      return json["data"]["link"].get<std::string>();
    }
  } catch (...) {
    // JSON parse failure
  }

  return std::nullopt;
}

} // namespace upload_detail

/// Upload image data to a public host and return the public URL.
/// Uses Imgur when a client ID is provided, otherwise falls back to anonymous upload.
[[nodiscard]] inline std::optional<std::string>
upload_to_imgur(const std::vector<std::uint8_t>& image_data,
                std::string_view client_id) {
  if (image_data.empty()) return std::nullopt;

  if (!client_id.empty()) {
    if (auto url = upload_detail::upload_to_imgur_internal(image_data, client_id);
        url.has_value() && !url->empty()) {
      return url;
    }
  }

  return upload_detail::upload_to_0x0st(image_data);
}

#else

// Linux/macOS stub
[[nodiscard]] inline std::optional<std::string>
upload_to_imgur(const std::vector<std::uint8_t>& /*image_data*/,
                std::string_view /*client_id*/) {
  return std::nullopt;
}

#endif

} // namespace rpc::net
