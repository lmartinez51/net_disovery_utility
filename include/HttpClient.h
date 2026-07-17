/**
 * @file HttpClient.h
 * @brief Minimal synchronous HTTP/1.1 client interface (Phase 3 stub).
 *
 * HttpClient will be used to fetch the UPnP device description XML
 * document from the URL advertised in the SSDP LOCATION header.
 *
 * Phase status:
 *   - Phase 1-2 : STUB. All methods throw std::runtime_error("Not implemented").
 *   - Phase 3   : Full implementation using raw WinSock2 TCP sockets.
 *
 * Design notes:
 *   - No external HTTP libraries are used. Implementation will use raw
 *     TCP sockets and hand-written HTTP/1.1 GET request construction.
 *   - Redirects (301/302) will be handled up to a configurable limit.
 *   - Only plain HTTP is required; HTTPS is out of scope for this library.
 *   - The interface is designed to be portable to ESP-IDF's BSD socket API
 *     with minimal changes to the header.
 */

#pragma once

#include <string>
#include <map>

namespace NetDiscovery {

/**
 * @brief Represents an HTTP response.
 */
struct HttpResponse {
    int statusCode = 0;
    std::map<std::string, std::string> headers;
    std::string body;
};

/**
 * @brief Simple synchronous HTTP/1.1 client for fetching UPnP descriptions and executing REST commands.
 *
 * Intended usage (Phase 3+):
 * @code
 *   HttpClient http;
 *   HttpResponse res = http.Get("http://192.168.1.50:49153/description.xml");
 * @endcode
 */
class HttpClient {
public:
    /// Default connection and read timeout in seconds.
    static constexpr int DEFAULT_TIMEOUT_SECONDS = 5;

    /// Maximum number of HTTP redirects to follow.
    static constexpr int MAX_REDIRECTS = 3;

    /**
     * @brief Default constructor.
     */
    HttpClient() = default;

    /**
     * @brief Perform an HTTP request.
     */
    HttpResponse SendRequest(const std::string& method, const std::string& url, const std::string& body = "", const std::map<std::string, std::string>& extraHeaders = {});

    /**
     * @brief Convenience wrapper for GET.
     */
    HttpResponse Get(const std::string& url, const std::map<std::string, std::string>& extraHeaders = {});

    /**
     * @brief Convenience wrapper for POST.
     */
    HttpResponse Post(const std::string& url, const std::string& body = "", const std::map<std::string, std::string>& extraHeaders = {});

    /**
     * @brief Convenience wrapper for DELETE.
     */
    HttpResponse Delete(const std::string& url, const std::map<std::string, std::string>& extraHeaders = {});

private:
    /**
     * @brief Parse a URL into its components.
     *
     * TODO (Phase 3): Implement URL parsing.
     *
     * @param url    Input URL string.
     * @param host   Output: hostname or IP address.
     * @param port   Output: port number (default 80 if absent).
     * @param path   Output: request path (e.g. "/description.xml").
     */
    void ParseUrl(const std::string& url,
                  std::string&       host,
                  int&               port,
                  std::string&       path) const;
};

} // namespace NetDiscovery
