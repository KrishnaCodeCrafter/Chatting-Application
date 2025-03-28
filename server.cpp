#include <iostream>
#include <winsock2.h>
#include <vector>
#include <thread>
#include <mutex>
#include <string>
#include <atomic>
#include <algorithm>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

#define MAX_LEN 1024
#define PORT 10000
#define SERVER_IP "0.0.0.0"
#define MAX_CLIENTS 100

using namespace std;

struct ClientInfo {
    int id;
    SOCKET socket;
    string name;
    thread th;
    time_t last_activity;
};

vector<ClientInfo> clients;
mutex clients_mtx, cout_mtx;
atomic<bool> server_running(true);

void safe_print(const string& message, bool is_error = false) {
    lock_guard<mutex> guard(cout_mtx);
    if (is_error) {
        cerr << message << endl;
    } else {
        cout << message << endl;
    }
}

void broadcast_message(const string& message, int sender_id = -1) {
    lock_guard<mutex> guard(clients_mtx);
    
    time_t current_time = time(nullptr);
    string formatted_msg = "[" + string(ctime(&current_time));
    formatted_msg.pop_back(); // Remove newline from ctime
    formatted_msg += "] " + message;
    
    for (auto& client : clients) {
        if (client.id != sender_id && client.socket != INVALID_SOCKET) {
            if (send(client.socket, formatted_msg.c_str(), formatted_msg.size() + 1, 0) == SOCKET_ERROR) {
                safe_print("Failed to send message to client " + to_string(client.id), true);
            }
        }
    }
}

void end_connection(int id) {
    lock_guard<mutex> guard(clients_mtx);
    auto it = find_if(clients.begin(), clients.end(), [id](const ClientInfo& c) { return c.id == id; });
    
    if (it != clients.end()) {
        if (it->socket != INVALID_SOCKET) {
            shutdown(it->socket, SD_BOTH);
            closesocket(it->socket);
        }
        if (it->th.joinable()) {
            it->th.detach();
        }
        safe_print("Client " + it->name + " (ID: " + to_string(it->id) + ") disconnected");
        clients.erase(it);
    }
}

void handle_client(SOCKET client_socket, int id) {
    char buffer[MAX_LEN];
    string client_name;
    
    // Get client name
    int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received <= 0) {
        safe_print("Failed to get client name for ID: " + to_string(id), true);
        end_connection(id);
        return;
    }
    
    client_name = buffer;
    
    // Update client info with name
    {
        lock_guard<mutex> guard(clients_mtx);
        for (auto& client : clients) {
            if (client.id == id) {
                client.name = client_name;
                client.last_activity = time(nullptr);
                break;
            }
        }
    }
    
    safe_print(client_name + " (ID: " + to_string(id) + ") connected");
    broadcast_message(client_name + " has joined the chat!", id);
    
    // Main message loop
    while (server_running) {
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
        
        if (bytes_received <= 0) {
            break;
        }
        
        if (strcmp(buffer, "#exit") == 0) {
            break;
        }
        
        // Update last activity time
        {
            lock_guard<mutex> guard(clients_mtx);
            for (auto& client : clients) {
                if (client.id == id) {
                    client.last_activity = time(nullptr);
                    break;
                }
            }
        }
        
        string full_message = client_name + ": " + buffer;
        broadcast_message(full_message, id);
    }
    
    string leave_msg = client_name + " has left the chat.";
    broadcast_message(leave_msg, id);
    end_connection(id);
}

void monitor_clients() {
    while (server_running) {
        this_thread::sleep_for(chrono::seconds(10));
        
        time_t current_time = time(nullptr);
        vector<int> to_remove;
        
        {
            lock_guard<mutex> guard(clients_mtx);
            for (const auto& client : clients) {
                if (difftime(current_time, client.last_activity) > 30) {
                    to_remove.push_back(client.id);
                }
            }
        }
        
        for (int id : to_remove) {
            safe_print("Client ID " + to_string(id) + " timed out", true);
            end_connection(id);
        }
    }
}

void server_commands() {
    string command;
    while (server_running) {
        getline(cin, command);
        
        if (command == "#stop") {
            server_running = false;
            safe_print("Server shutting down...");
            
            // Close all client connections
            lock_guard<mutex> guard(clients_mtx);
            for (auto& client : clients) {
                shutdown(client.socket, SD_BOTH);
                closesocket(client.socket);
                if (client.th.joinable()) {
                    client.th.detach();
                }
            }
            clients.clear();
            break;
        } else if (command == "#list") {
            lock_guard<mutex> guard(clients_mtx);
            safe_print("Connected clients (" + to_string(clients.size()) + "):");
            for (const auto& client : clients) {
                safe_print("ID: " + to_string(client.id) + " - " + client.name);
            }
        } else {
            safe_print("Unknown command: " + command);
        }
    }
}

int main() {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        safe_print("WSAStartup failed: " + to_string(WSAGetLastError()), true);
        return -1;
    }

    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        safe_print("Socket creation failed: " + to_string(WSAGetLastError()), true);
        WSACleanup();
        return -1;
    }

    sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(SERVER_IP);
    server.sin_port = htons(PORT);

    if (bind(server_socket, (sockaddr*)&server, sizeof(server)) == SOCKET_ERROR) {
        safe_print("Bind failed: " + to_string(WSAGetLastError()), true);
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }

    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        safe_print("Listen failed: " + to_string(WSAGetLastError()), true);
        closesocket(server_socket);
        WSACleanup();
        return -1;
    }

    safe_print("Server started on " + string(SERVER_IP) + ":" + to_string(PORT));
    safe_print("Type '#stop' to shutdown server or '#list' to show connected clients");

    thread command_thread(server_commands);
    thread monitor_thread(monitor_clients);

    int client_id = 0;
    while (server_running) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &addr_len);
        if (client_socket == INVALID_SOCKET) {
            if (server_running) {
                safe_print("Accept failed: " + to_string(WSAGetLastError()), true);
            }
            continue;
        }

        if (!server_running) {
            closesocket(client_socket);
            break;
        }

        client_id++;
        thread t(handle_client, client_socket, client_id);
        
        lock_guard<mutex> guard(clients_mtx);
        clients.push_back({client_id, client_socket, "", move(t), time(nullptr)});
        
        if (clients.size() >= MAX_CLIENTS) {
            safe_print("Maximum client limit reached (" + to_string(MAX_CLIENTS) + ")", true);
            string msg = "Server is full. Please try again later.";
            send(client_socket, msg.c_str(), msg.size() + 1, 0);
            end_connection(client_id);
        }
    }

    if (command_thread.joinable()) command_thread.join();
    if (monitor_thread.joinable()) monitor_thread.join();

    closesocket(server_socket);
    WSACleanup();
    return 0;
}