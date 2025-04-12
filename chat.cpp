#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define INET_ADDRSTRLEN 16

#include <string>
#include <stdio.h>
#include <stdlib.h>
#include <WinSock2.h>
#include <windows.h>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <sstream>
#pragma comment(lib, "ws2_32.lib")

std::string user = "user";
std::string message = "message";

std::atomic<bool> server_running(true); // ���Ʒ������߳�����״̬
std::mutex chat_mutex;
std::deque<std::pair<std::string, std::string>> chat_messages; // �洢 (IP, ��Ϣ) ��

// ��ȡ�ͻ���IP��ַ 
std::string get_client_ip(SOCKET client_socket) {
    sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    if (getpeername(client_socket, (sockaddr*)&client_addr, &addr_len) == SOCKET_ERROR) {
        return "Unknown IP";
    }

    char ip_str[INET_ADDRSTRLEN];
    strcpy_s(ip_str, INET_ADDRSTRLEN, inet_ntoa(client_addr.sin_addr));

    return std::string(ip_str);
}

// URL���� 
std::string url_decode(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            int value = 0;
            sscanf_s(str.substr(i + 1, 2).c_str(), "%x", &value);
            result += static_cast<char>(value);
            i += 2;
        }
        else if (str[i] == '+') {
            result += ' ';
        }
        else {
            result += str[i];
        }
    }
    return result;
}

// �����Ϣ�������¼
void add_message(const std::string& user, const std::string& message) {
    std::lock_guard<std::mutex> lock(chat_mutex);
    chat_messages.push_back(std::make_pair(user, message));
    if (chat_messages.size() > 100) { // ���������¼���� 
        chat_messages.pop_front();
    }
}

// ����HTMLҳ��
std::string generate_html_page() {
    std::stringstream html;
    html << "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
        << "<!DOCTYPE html>"
        << "<html><head><title>Chat Room</title></head><body>"
        << "<h1>Chat Room</h1>"
        << "<div id='messages'>";

    // ��������¼
    std::lock_guard<std::mutex> lock(chat_mutex);
    for (const auto& pair : chat_messages) {
        const std::string& user = pair.first;
        const std::string& message = pair.second;
        html << "<p><strong>" << user << ":</strong> " << message << "</p>";
    }

    html << "</div>"
        << "<form action='/' method='POST'>"
        << "<input type='text' name='message' placeholder='Type your message...'>"
        << "<input type='submit' value='Send'>"
        << "</form>"
        << "</body></html>";

    return html.str();
}

// ����ͻ�������ĺ���
void client_handler(SOCKET client_socket) {
    char buffer[4096] = { 0 };
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        closesocket(client_socket);
        return;
    }

    std::string request(buffer, bytes_received);
    std::string response;

    // ����������ͣ�GET �� POST��
    if (request.find("GET") != std::string::npos) {
        // ����HTMLҳ��
        response = generate_html_page();
    }
    else if (request.find("POST") != std::string::npos) {
        // ����POST�����е���Ϣ 
        size_t start = request.find("message=");
        if (start != std::string::npos) {
            std::string message = request.substr(start + 8);
            message = url_decode(message); // URL����
            add_message(get_client_ip(client_socket), message);
            // ������ҳ��
            response = generate_html_page();
        }
        else {
            response = "HTTP/1.1 400 Bad Request\r\n\r\n";
        }
    }
    else {
        response = "HTTP/1.1 400 Bad Request\r\n\r\n";
    }

    send(client_socket, response.c_str(), response.length(), 0);
    closesocket(client_socket);
}

// ��HTTP�������̺߳���
void http_server_thread() {
    WSADATA wsa;
    SOCKET server_fd;
    sockaddr_in address;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == INVALID_SOCKET) {
        WSACleanup();
        return;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8000);

    if (bind(server_fd, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR) {
        closesocket(server_fd);
        WSACleanup();
        return;
    }

    listen(server_fd, 10);

    while (server_running) {
        SOCKET client_socket = accept(server_fd, NULL, NULL);
        if (client_socket == INVALID_SOCKET) continue;

        // �������̴߳���ͻ�������
        std::thread(client_handler, client_socket).detach();
    }

    closesocket(server_fd);
    WSACleanup();
}

int main() {
    // ����HTTP�������߳�
    std::thread server_thread(http_server_thread);
    server_thread.detach();

    // ���߳̿���������ִ�����������ȴ�
    while (true) {
        Sleep(1000); // ���߳����ߣ�����ռ�ù���CPU��Դ
    }

    server_running = false;
    return 0;
}