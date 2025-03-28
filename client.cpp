#include <iostream>
#include <winsock2.h>
#include <thread>
#include <string>
#include <atomic>
#include <mutex>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define MAX_LEN 1024
#define PORT 10000
#define SERVER_IP "127.0.0.1"

using namespace std;

atomic<bool> exit_flag(false);
SOCKET client_socket;
thread t_send, t_recv;
mutex cout_mtx;
HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

void set_console_color(int color) {
    SetConsoleTextAttribute(hConsole, color);
}

void reset_console_color() {
    SetConsoleTextAttribute(hConsole, 7); // Default gray
}

void safe_print(const string& msg, bool is_self = false) {
    lock_guard<mutex> guard(cout_mtx);
    if (is_self) {
        set_console_color(10); // Bright green
        cout << "You: " << msg << endl;
    } else {
        set_console_color(11); // Bright cyan
        cout << msg << endl;
    }
    reset_console_color();
}

void send_message() {
    char msg[MAX_LEN];
    while (!exit_flag) {
        cin.getline(msg, MAX_LEN);
        
        if (strlen(msg) == 0) continue;
        
        if (strcmp(msg, "#exit") == 0) {
            if (send(client_socket, msg, strlen(msg) + 1, 0) == SOCKET_ERROR) {
                safe_print("Failed to send exit message", true);
            }
            exit_flag = true;
            break;
        }
        
        if (send(client_socket, msg, strlen(msg) + 1, 0) == SOCKET_ERROR) {
            safe_print("Failed to send message to server", true);
            exit_flag = true;
            break;
        }
        
        safe_print(msg, true);
    }
}

void recv_message() {
    char msg[MAX_LEN];
    while (!exit_flag) {
        int bytes_received = recv(client_socket, msg, MAX_LEN, 0);
        
        if (bytes_received <= 0) {
            if (!exit_flag) {
                safe_print("Server disconnected", true);
            }
            exit_flag = true;
            break;
        }
        
        safe_print(msg);
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return -1;
    }

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        cerr << "Socket creation failed: " << WSAGetLastError() << endl;
        WSACleanup();
        return -1;
    }

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr(SERVER_IP);

    cout << "Connecting to server...";
    if (connect(client_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        cerr << "\nConnect error: " << WSAGetLastError() << endl;
        closesocket(client_socket);
        WSACleanup();
        return -1;
    }
    cout << " connected!" << endl;

    char name[MAX_LEN];
    cout << "Enter your name: ";
    cin.getline(name, MAX_LEN);
    
    if (send(client_socket, name, strlen(name) + 1, 0) == SOCKET_ERROR) {
        cerr << "Name send failed: " << WSAGetLastError() << endl;
        closesocket(client_socket);
        WSACleanup();
        return -1;
    }

    cout << "Connected to chat server! Type '#exit' to leave." << endl;
    cout << "---------------------------------------------" << endl;

    t_send = thread(send_message);
    t_recv = thread(recv_message);

    if (t_send.joinable()) t_send.join();
    if (t_recv.joinable()) t_recv.join();

    closesocket(client_socket);
    WSACleanup();
    return 0;
}