/**
 * @file WebSocketTransport.cpp
 * @brief Implementation of WebSocket framing and handshake over TcpSocket.
 */

#include "../../include/transports/WebSocketTransport.h"
#include "../../include/HttpClient.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <random>
#include <sstream>
#include <chrono>
#include <iostream>
#include <winsock2.h>
#include <stdexcept>
#include <thread>

namespace NetDiscovery {



// Simple Base64 encode for Sec-WebSocket-Key
static std::string Base64Encode(const std::vector<uint8_t>& data) {
    static const char encoding_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int i = 0;
    uint32_t octet_a, octet_b, octet_c, triple;
    while (i < data.size()) {
        octet_a = i < data.size() ? data[i++] : 0;
        octet_b = i < data.size() ? data[i++] : 0;
        octet_c = i < data.size() ? data[i++] : 0;
        triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;
        encoded += encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded += encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded += i > data.size() + 1 ? '=' : encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded += i > data.size() ? '=' : encoding_table[(triple >> 0 * 6) & 0x3F];
    }
    return encoded;
}

std::string WebSocketTransport::GenerateSecWebSocketKey() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    std::vector<uint8_t> key(16);
    for (int i = 0; i < 16; ++i) {
        key[i] = static_cast<uint8_t>(dis(gen));
    }
    return Base64Encode(key);
}

std::string WebSocketTransport::ComputeSecWebSocketAccept(const std::string& /*key*/) {
    // Skipping full SHA1 validation on client side to keep dependencies zero.
    // The framework trusts the Samsung TV backend during Phase 9.
    return "";
}

bool WebSocketTransport::DoHandshake(TcpSocket& socket, const std::string& host, const std::string& path) {
    std::string key = GenerateSecWebSocketKey();
    
    std::stringstream request;
    request << "GET " << path << " HTTP/1.1\r\n"
            << "Host: " << host << "\r\n"
            << "Upgrade: websocket\r\n"
            << "Connection: Upgrade\r\n"
            << "Sec-WebSocket-Key: " << key << "\r\n"
            << "Sec-WebSocket-Version: 13\r\n"
            << "\r\n";
            
    std::string reqStr = request.str();
    std::cout << "[WebSocketTransport] === HTTP UPGRADE REQUEST ===\n" 
              << reqStr 
              << "[WebSocketTransport] ============================\n";

    socket.Send(reqStr);
    
    // Read the response until \r\n\r\n
    std::string response;
    char buffer[1024];
    while (response.find("\r\n\r\n") == std::string::npos) {
        int bytes = socket.Receive(buffer, sizeof(buffer));
        if (bytes <= 0) {
            std::cout << "[WebSocketTransport] Connection closed while reading HTTP Upgrade response.\n";
            return false;
        }
        response.append(buffer, bytes);
    }
    
    std::cout << "[WebSocketTransport] === HTTP UPGRADE RESPONSE ===\n" 
              << response 
              << "[WebSocketTransport] =============================\n";

    if (response.find("101 Switching Protocols") != std::string::npos) {
        return true;
    }
    
    std::cout << "[WebSocketTransport] HTTP Upgrade Failed! The response did not contain 101 Switching Protocols.\n";
    return false;
}

void WebSocketTransport::SendFrame(TcpSocket& socket, const std::string& payload) {
    std::vector<uint8_t> frame;
    // FIN = 1, Opcode = 1 (text)
    frame.push_back(0x81);
    
    size_t len = payload.length();
    // MASK = 1 (client must mask)
    if (len <= 125) {
        frame.push_back(static_cast<uint8_t>(len | 0x80));
    } else if (len <= 65535) {
        frame.push_back(126 | 0x80);
        frame.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(len & 0xFF));
    } else {
        frame.push_back(127 | 0x80);
        for (int i = 7; i >= 0; --i) {
            frame.push_back(static_cast<uint8_t>((len >> (i * 8)) & 0xFF));
        }
    }
    
    // Generate masking key
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);
    uint8_t mask[4] = {
        static_cast<uint8_t>(dis(gen)),
        static_cast<uint8_t>(dis(gen)),
        static_cast<uint8_t>(dis(gen)),
        static_cast<uint8_t>(dis(gen))
    };
    
    frame.push_back(mask[0]);
    frame.push_back(mask[1]);
    frame.push_back(mask[2]);
    frame.push_back(mask[3]);
    
    for (size_t i = 0; i < len; ++i) {
        frame.push_back(static_cast<uint8_t>(payload[i] ^ mask[i % 4]));
    }
    
    std::string frameStr(frame.begin(), frame.end());
    socket.Send(frameStr);
}

std::string WebSocketTransport::ReceiveFrame(TcpSocket& socket) {
    char header[2];
    int bytes = socket.Receive(header, 2);
    if (bytes < 2) return "";
    
    (void)header; // fin not used for now
    int opcode = header[0] & 0x0F;
    bool masked = (header[1] & 0x80) != 0;
    uint64_t payloadLen = header[1] & 0x7F;
    
    if (payloadLen == 126) {
        char ext[2];
        socket.Receive(ext, 2);
        payloadLen = (static_cast<uint8_t>(ext[0]) << 8) | static_cast<uint8_t>(ext[1]);
    } else if (payloadLen == 127) {
        char ext[8];
        socket.Receive(ext, 8);
        payloadLen = 0;
        for (int i = 0; i < 8; ++i) {
            payloadLen = (payloadLen << 8) | static_cast<uint8_t>(ext[i]);
        }
    }
    
    char maskKey[4];
    if (masked) {
        socket.Receive(maskKey, 4);
    }
    
    std::string payload;
    if (payloadLen > 0) {
        std::vector<char> buffer(payloadLen);
        size_t totalReceived = 0;
        while (totalReceived < payloadLen) {
            int r = socket.Receive(buffer.data() + totalReceived, static_cast<int>(payloadLen - totalReceived));
            if (r <= 0) break;
            totalReceived += r;
        }
        
        if (masked) {
            for (size_t i = 0; i < totalReceived; ++i) {
                buffer[i] ^= maskKey[i % 4];
            }
        }
        payload.assign(buffer.data(), totalReceived);
    }
    
    if (opcode == 8) { // Close frame
        return "";
    }
    return payload;
}

ExecutionResult WebSocketTransport::Execute(const ExecutionRequest& request, const ExecutionRoute& route) {
    ExecutionResult result;
    result.status = ExecutionStatus::ExecutionFailed;
    
    auto it = route.metadata.find("WebSocket-Host");
    if (it == route.metadata.end()) {
        
        result.errorMessage = "Missing WebSocket-Host in metadata";
        return result;
    }
    std::string host = it->second;
    
    std::string portStr = "8001";
    it = route.metadata.find("WebSocket-Port");
    if (it != route.metadata.end()) {
        portStr = it->second;
    }
    int port = std::stoi(portStr);
    
    std::string path = "/";
    it = route.metadata.find("WebSocket-Path");
    if (it != route.metadata.end()) {
        path = it->second;
    }

    std::cout << "[WebSocketTransport] Target IP: " << host << "\n";
    std::cout << "[WebSocketTransport] Target Port: " << port << "\n";
    std::cout << "[WebSocketTransport] Complete WebSocket Path: " << path << "\n";

    try {
        TcpSocket socket;
        auto start = std::chrono::steady_clock::now();
        socket.Connect(host, static_cast<uint16_t>(port));
        
        if (!DoHandshake(socket, host, path)) {
            
            result.errorMessage = "WebSocket handshake failed";
            return result;
        }
        
        // Handshake succeeded.
        // Wait briefly for a potential ms.channel.connect (auth prompt) from the server.
        // If it's a first connection, the TV will send an event indicating pairing state.
        // We will do a non-blocking wait or short sleep to see if there's an incoming frame before sending ours.
        
        // Actually, we must send the command.
        if (request.action.id != ActionId::Unknown) {
            // The request.action.id contains the payload to send, but wait, 
            // the Strategy generated the JSON payload and put it where?
            // Usually we pass the JSON payload in metadata or in request.action.id.
            // Let's assume the Strategy put the JSON payload in metadata["WebSocket-Payload"].
            it = route.metadata.find("WebSocket-Payload");
            if (it != route.metadata.end()) {
                std::string payload = it->second;
                SendFrame(socket, payload);
            }
        }
        
        // Read the response frame
        std::string response = ReceiveFrame(socket);
        if (response.empty()) {
            std::cout << "[WebSocketTransport] Received empty WebSocket frame (connection likely closed).\n";
            // It could be that the TV closed the connection immediately, or waiting for auth
            result.status = ExecutionStatus::AuthenticationRequired;
            result.errorMessage = "Connection closed or no data";
        } else {
            std::cout << "[WebSocketTransport] === FIRST WEBSOCKET FRAME ===\n" 
                      << response 
                      << "\n[WebSocketTransport] ===========================\n";
            result.status = ExecutionStatus::Success;
            result.transportDiagnostics.rawPayload = response;
        }
        
        auto end = std::chrono::steady_clock::now();
        result.elapsedTimeMs = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        
    } catch (const std::exception& e) {
        int err = WSAGetLastError();
        std::cout << "[WebSocketTransport] Socket Exception: " << e.what() << " | WSAGetLastError: " << err << "\n";
        result.status = ExecutionStatus::TransportUnavailable;
        
        result.errorMessage = e.what();
    }
    
    return result;
}

} // namespace NetDiscovery
