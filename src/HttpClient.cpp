/**
 * @file HttpClient.cpp
 * @brief Simple synchronous HTTP/1.1 GET client.
 */

#include "../include/HttpClient.h"
#include "../include/TcpSocket.h"

#include <stdexcept>
#include <vector>
#include <algorithm>
#include <cctype>

namespace NetDiscovery {

namespace {
    // Helper to extract a single line ending in \r\n
    bool GetLine(std::string& buffer, std::string& line) {
        size_t pos = buffer.find("\r\n");
        if (pos == std::string::npos) return false;
        line = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);
        return true;
    }

    // Helper for case-insensitive string compare
    bool IEquals(const std::string& a, const std::string& b) {
        if (a.length() != b.length()) return false;
        for (size_t i = 0; i < a.length(); ++i) {
            if (std::tolower((unsigned char)a[i]) != std::tolower((unsigned char)b[i])) return false;
        }
        return true;
    }
}

void HttpClient::ParseUrl(const std::string& url, std::string& host, int& port, std::string& path) const
{
    std::string u = url;
    size_t schemePos = u.find("://");
    if (schemePos != std::string::npos) {
        u = u.substr(schemePos + 3);
    } else {
        throw std::runtime_error("Invalid URL scheme: " + url);
    }

    size_t pathPos = u.find('/');
    std::string hostPort = (pathPos == std::string::npos) ? u : u.substr(0, pathPos);
    path = (pathPos == std::string::npos) ? "/" : u.substr(pathPos);

    size_t colonPos = hostPort.find(':');
    if (colonPos != std::string::npos) {
        host = hostPort.substr(0, colonPos);
        port = std::stoi(hostPort.substr(colonPos + 1));
    } else {
        host = hostPort;
        port = 80;
    }
}

std::string HttpClient::Get(const std::string& url)
{
    std::string currentUrl = url;
    int redirects = 0;

    while (redirects <= MAX_REDIRECTS) {
        std::string host;
        int port = 80;
        std::string path;
        ParseUrl(currentUrl, host, port, path);

        TcpSocket sock;
        sock.Connect(host, static_cast<uint16_t>(port));

        std::string request = "GET " + path + " HTTP/1.1\r\n"
                              "Host: " + host + ":" + std::to_string(port) + "\r\n"
                              "Connection: close\r\n"
                              "Accept: */*\r\n\r\n";

        sock.Send(request);

        std::string buffer;
        char chunk[4096];
        while (true) {
            int r = sock.Receive(chunk, sizeof(chunk));
            if (r <= 0) break;
            buffer.append(chunk, r);
        }

        // Parse HTTP response
        std::string line;
        if (!GetLine(buffer, line)) {
            throw std::runtime_error("Invalid HTTP response (no status line)");
        }

        // Parse status line (e.g. HTTP/1.1 200 OK)
        size_t sp1 = line.find(' ');
        if (sp1 == std::string::npos) throw std::runtime_error("Invalid HTTP status line");
        size_t sp2 = line.find(' ', sp1 + 1);
        std::string statusCodeStr = line.substr(sp1 + 1, sp2 - sp1 - 1);
        int statusCode = std::stoi(statusCodeStr);

        // Parse headers
        int contentLength = -1;
        bool chunked = false;
        std::string location;

        while (GetLine(buffer, line)) {
            if (line.empty()) break; // End of headers
            
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string headerName = line.substr(0, colon);
                std::string headerVal = line.substr(colon + 1);
                
                // Trim spaces
                size_t first = headerVal.find_first_not_of(" \t");
                if (first != std::string::npos) {
                    headerVal = headerVal.substr(first);
                }

                if (IEquals(headerName, "Content-Length")) {
                    contentLength = std::stoi(headerVal);
                } else if (IEquals(headerName, "Transfer-Encoding") && headerVal.find("chunked") != std::string::npos) {
                    chunked = true;
                } else if (IEquals(headerName, "Location")) {
                    location = headerVal;
                }
            }
        }

        if (statusCode >= 300 && statusCode < 400 && !location.empty()) {
            // Redirect
            currentUrl = location;
            redirects++;
            continue;
        }

        if (statusCode < 200 || statusCode >= 300) {
            throw std::runtime_error("HTTP request failed with status: " + std::to_string(statusCode));
        }

        // Handle chunked body
        if (chunked) {
            std::string decodedBody;
            while (true) {
                if (!GetLine(buffer, line)) break;
                // Parse hex size
                int size = 0;
                try {
                    size = std::stoi(line, nullptr, 16);
                } catch (...) { break; }
                
                if (size == 0) break;
                
                if (buffer.length() < size) {
                    // Not enough data for the chunk, wait for more (not implemented fully for streaming)
                    // Since we read all available data upfront, if it's not here, it's malformed.
                    break;
                }
                
                decodedBody.append(buffer.substr(0, size));
                buffer.erase(0, size);
                
                // Read the trailing CRLF after the chunk
                GetLine(buffer, line); 
            }
            return decodedBody;
        }

        if (contentLength >= 0 && buffer.length() > static_cast<size_t>(contentLength)) {
            buffer.resize(contentLength);
        }

        return buffer;
    }

    throw std::runtime_error("HttpClient::Get — Too many redirects");
}

} // namespace NetDiscovery
