#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

using namespace std;

int main(){
    //create socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
                        //    IPv4,    TCP,        protocol

    //specifying address
    sockaddr_in serverAddress; //struct
    serverAddress.sin_family = AF_INET; //declare IPv4
    serverAddress.sin_port = htons(8080); //htons convert big edian to little edian
    serverAddress.sin_addr.s_addr = INADDR_ANY; //listen to all connect

    //binding socket
    bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    //  use this socket - import serverAddress & type cast - identify addr

    int count = 5;
    int fileIndex = 1;

    while(count > 0){
        //listen
        cout << "Listening to request: ..." << endl;
        listen(serverSocket, 5);

        //accepting connection request
        int clientSocket = accept(serverSocket, nullptr, nullptr);

        // === RECEIVING FILE HERE ===
        cout << "Client connected!" << endl;

        char header;
        recv(clientSocket, &header, 1, 0);
        
        if(header == 'M'){
            char buffer[1024] = {0};
            recv(clientSocket, buffer, sizeof(buffer), 0);
            cout << "Client message: \"" << buffer << "\"" << endl;
        } else if(header == 'F'){
            const int BUFFER_SIZE = 4096;
            char buffer[BUFFER_SIZE];

            // file name changes each loop
            string fileName = "received_" + to_string(fileIndex) + ".bin";
            FILE* fd = fopen(fileName.c_str(), "wb");

            int bytes_read;

            while ((bytes_read = recv(clientSocket, buffer, BUFFER_SIZE, 0)) > 0){
                fwrite(buffer, 1, bytes_read, fd);
            }
            fclose(fd);

            cout << "Saved file as: " << fileName << endl;
            fileIndex++;
        }
        close(clientSocket);            
        count--;
        cout << "-----------------" << endl;
    }
    cout << "That is enough for today! See you later!" << endl;

    //closeing socket
    close(serverSocket);

    return 0;
}