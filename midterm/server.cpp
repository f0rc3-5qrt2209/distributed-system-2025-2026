#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

using namespace std;

#define PORT 8888
#define BUFFER_SIZE 4096

// Structure to pass client socket to thread
struct ClientArgs {
  int socket;
  struct sockaddr_in address;
};

// Helper to send data with length prefix
bool send_message(int sock, const string &data) {
  uint32_t len = htonl(data.size());
  if (send(sock, &len, sizeof(len), 0) < 0)
    return false;
  if (send(sock, data.c_str(), data.size(), 0) < 0)
    return false;
  return true;
}

// Helper to receive data with length prefix
bool receive_message(int sock, string &data) {
  uint32_t len;
  int bytes_received = recv(sock, &len, sizeof(len), MSG_WAITALL);
  if (bytes_received <= 0)
    return false;

  len = ntohl(len);
  vector<char> buffer(len);
  bytes_received = recv(sock, buffer.data(), len, MSG_WAITALL);
  if (bytes_received <= 0)
    return false;

  data.assign(buffer.begin(), buffer.end());
  return true;
}

// Function to execute command and return output
string execute_command(const string &cmd, const string &cwd) {
  int pipefd[2];
  // tạo pipe
  // pipefd[0] là read end, pipefd[1] là write end
  if (pipe(pipefd) == -1)
    return "Error: pipe failed";
  // tạo process con
  pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return "Error: fork failed";
  }

  if (pid == 0) {       // Child process
    close(pipefd[0]); // Close read end
    // gửi tất cả output của process con vào pipe, stdout_fileno: đầu ra chuẩn,
    // stderr_fileno: đầu ra lỗi chuẩn
    dup2(pipefd[1], STDOUT_FILENO); // Redirect stdout to pipe
    dup2(pipefd[1], STDERR_FILENO); // Redirect stderr to pipe
    close(pipefd[1]);

    // Change directory in child process
    // chdir = change directory, cwd = current working directory
    if (chdir(cwd.c_str()) == -1) {
      cerr << "Error: chdir failed to " << cwd << endl;
      exit(1);
    }

    execl("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
    // nếu không tìm thấy command
    exit(127);        // Should not reach here
  } else {            // Parent process
    close(pipefd[1]); // Close write end

    string result;
    char buffer[1024];
    ssize_t count;
    while ((count = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
      result.append(buffer, count);
    }
    close(pipefd[0]);
    waitpid(pid, NULL, 0);

    if (result.empty())
      return "(Command executed with no output)";
    return result;
  }
}

// Client handler thread function
void *handle_client(void *arg) {
  ClientArgs *args = (ClientArgs *)arg;
  int client_sock = args->socket;
  struct sockaddr_in client_addr = args->address;
  delete args; // Free memory allocated in main

  char cwd_buf[PATH_MAX];
  if (getcwd(cwd_buf, sizeof(cwd_buf)) == NULL) {
    perror("getcwd");
    close(client_sock);
    return NULL;
  }
  string current_dir = cwd_buf;

  cout << "[NEW CONNECTION] Client connected." << endl;

  while (true) {
    string command;
    if (!receive_message(client_sock, command)) {
      break;
    }

    // Trim whitespace
    size_t first = command.find_first_not_of(" \t\n\r");
    if (first == string::npos)
      continue;
    size_t last = command.find_last_not_of(" \t\n\r");
    command = command.substr(first, (last - first + 1));

    cout << "[RECEIVED] " << command << endl;

    string response;

    if (command.substr(0, 3) == "cd ") {
      string path = command.substr(3);
      string new_dir;

      // Handle absolute vs relative path
      if (path[0] == '/') {
        new_dir = path;
      } else {
        new_dir = current_dir + "/" + path;
      }

      // Resolve path using realpath to handle '..' and check existence
      char resolved_path[PATH_MAX];
      if (realpath(new_dir.c_str(), resolved_path) != NULL) {
        // Check if it's a directory
        struct stat statbuf;
        if (stat(resolved_path, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
          current_dir = resolved_path;
          response = "Changed directory to: " + current_dir;
        } else {
          response = "Error: Not a directory: " + path;
        }
      } else {
        response = "Error: Directory not found: " + path;
      }
    } else if (command == "cd") {
      // Go to home directory
      const char *home = getenv("HOME");
      if (home) {
        current_dir = home;
        response = "Changed directory to: " + current_dir;
      } else {
        response = "Error: HOME environment variable not set";
      }
    } else {
      response = execute_command(command, current_dir);
    }

    if (!send_message(client_sock, response)) {
      break;
    }
  }

  cout << "[DISCONNECTED] Client disconnected." << endl;
  close(client_sock);
  return NULL;
}

int main() {
  int server_fd;
  struct sockaddr_in address;
  int opt = 1;

  // Create socket
  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // Attach socket to port
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PORT);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd, 10) < 0) {
    perror("listen");
    exit(EXIT_FAILURE);
  }

  cout << "[LISTENING] Server is listening on port " << PORT << endl;

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t addrlen = sizeof(client_addr);
    int client_sock =
        accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);

    if (client_sock < 0) {
      perror("accept");
      continue;
    }

    // Create thread for client
    pthread_t thread_id;
    ClientArgs *args = new ClientArgs;
    args->socket = client_sock;
    args->address = client_addr;

    if (pthread_create(&thread_id, NULL, handle_client, (void *)args) != 0) {
      perror("pthread_create");
      delete args;
      close(client_sock);
    } else {
      pthread_detach(thread_id); // Detach thread to cleanup automatically
    }
  }

  return 0;
}