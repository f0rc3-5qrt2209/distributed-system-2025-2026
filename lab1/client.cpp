#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main(){
    //creating socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    //specifying socket
    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    //sending connection request
    connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

    char choice;
    cout << "Enter M to send message or F to send file: ";
    cin >> choice;
    cin.ignore();

    if(choice == 'M' || choice == 'm'){
        char header = 'M';
        send(clientSocket, &header, 1, 0);
        string msg;
        cout << "Enter your message: ";
        getline(cin, msg);
        send(clientSocket, msg.c_str(), msg.size(), 0);
        cout << "Message sent!" << endl;
    } else if (choice == 'F' || choice == 'f'){
        char header = 'F';
        send(clientSocket, &header, 1, 0);

        FILE* fd = fopen("hello.txt", "rb");
        if (!fd) {
            cout << "Cannot open file!" << endl;
            close(clientSocket);
            return 0;
        }

        const int BUFFER_SIZE = 4096;
        char buffer[BUFFER_SIZE];

        size_t readBytes;
        while ((readBytes = fread(buffer, 1, BUFFER_SIZE, fd)) > 0) {
            send(clientSocket, buffer, readBytes, 0);
        }

        fclose(fd);

        cout << "File sent!" << endl;
    }

    else {
        cout << "Invalid choice." << endl;
    }

    //closing socket
    close(clientSocket);

    return 0;
}