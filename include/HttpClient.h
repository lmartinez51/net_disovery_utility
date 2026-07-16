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

namespace NetDiscovery {

/**
 * @brief Simple synchronous HTTP/1.1 GET client for fetching UPnP descriptions.
 *
 * Intended usage (Phase 3+):
 * @code
 *   HttpClient http;
 *   std::string body = http.Get("http://192.168.1.50:49153/description.xml");
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
     * @brief Perform an HTTP GET request and return the response body.
     *
     * Parses the URL into host, port, and path components, opens a TCP
     * connection, sends an HTTP/1.1 GET request, and returns the body.
     *
     * @param url  Absolute HTTP URL (e.g. "http://192.168.1.50:49153/desc.xml").
     * @return Response body as a raw string.
     *
     * @throws std::runtime_error("Not implemented") — Phase 1/2 stub.
     *
     * TODO (Phase 3):
     *   - Implement TCP connect via WinSock2 / lwIP.
     *   - Send HTTP/1.1 GET with correct headers.
     *   - Read response headers and body.
     *   - Handle chunked transfer encoding.
     *   - Follow redirects up to MAX_REDIRECTS.
     */
    std::string Get(const std::string& url);

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
