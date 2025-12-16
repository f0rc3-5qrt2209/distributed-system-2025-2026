#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace std;

#define SERVER_IP "127.0.0.1"
#define PORT 8888

// Helper to send data with length prefix
bool send_message(int sock, const string &data) {
  uint32_t len = htonl(data.size());
  if (send(sock, &len, sizeof(len), 0) < 0) // gửi độ dài của data
    return false;
  if (send(sock, data.c_str(), data.size(), 0) < 0) // gửi data
    return false;
  return true;
}

// Helper to receive data with length prefix
bool receive_message(int sock, string &data) {
  // nhận độ dài của data 32 bit = 4 bytes
  uint32_t len;
  // msg_waitall đảm bảo nhận đủ dữ liệu
  int bytes_received =
      recv(sock, &len, sizeof(len), MSG_WAITALL); // nhận độ dài của data
  if (bytes_received <= 0)
    return false;
  // net to host length
  len = ntohl(len);
  vector<char> buffer(len);
  bytes_received = recv(sock, buffer.data(), len, MSG_WAITALL); // nhận data
  if (bytes_received <= 0)
    return false;

  data.assign(
      buffer.begin(),
      buffer.end()); // chuyển dữ liệu thô trong buffer thành string vào data
  return true;
}

int main() {
  int sock = 0;
  struct sockaddr_in serv_addr;
  // tạo socket
  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    cerr << "Socket creation error" << endl;
    return -1;
  }
  // khởi tạo cấu trúc địa chỉ
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_port = htons(PORT);
  // chuyển địa chỉ IP từ chuỗi thành dạng nhị phân
  if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
    cerr << "Invalid address / Address not supported" << endl;
    return -1;
  }
  // kết nối đến server
  if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    cerr << "Connection Failed. Is the server running?" << endl;
    return -1;
  }

  cout << "Connected to server. Type 'exit' to quit." << endl;
  // lặp lại cho đến khi người dùng nhập exit
  while (true) {
    cout << "Shell> ";
    string command;
    getline(cin, command);

    if (command == "exit" || command == "quit") {
      break;
    }

    if (command.empty())
      continue;

    if (!send_message(sock, command)) {
      cerr << "Send failed" << endl;
      break;
    }

    string response;
    if (!receive_message(sock, response)) {
      cerr << "Server disconnected" << endl;
      break;
    }

    cout << response << endl;
  }

  close(sock);
  return 0;
}