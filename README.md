# 💬 Chatting Application

A simple client-server chat application using **C++**, **Winsock2**, and **multi-threading** for real-time communication.

## 🚀 Features
- 📡 **Real-time messaging** between multiple clients
- 🔄 **Multi-threaded server** to handle multiple clients simultaneously
- 🔐 **Client timeout detection** for inactive users
- 🔥 **Custom server commands** (e.g., `#list`, `#stop`)

## 🛠️ Setup & Installation
### 🏗️ Build & Run
#### Server:
```sh
 g++ server.cpp -o server -lws2_32
 ./server
```
#### Client:
```sh
 g++ client.cpp -o client -lws2_32
 ./client
```

## 📸 Preview
![Console Chat](https://img.icons8.com/external-flat-icons-pause-08/64/000000/external-chat-digital-marketing-flat-icons-pause-08.png)

## 📜 Commands
- `#exit` → Disconnect from the chat
- `#stop` → Shut down the server (admin only)
- `#list` → Show connected users

## 🏗️ Future Improvements
- 📱 GUI version with a better user interface
- 🔒 End-to-end encryption for secure messaging
- 🌍 Support for cross-platform communication
  

Made with ❤️ by KrishnaCodeCrafter
