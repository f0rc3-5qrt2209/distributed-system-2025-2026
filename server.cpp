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
    while(count>0){
        //listen to the assign socket
        cout << "Listening to request: ..." << endl;
        listen(serverSocket, 5);

        //accepting connection request
        int clientSocket = accept(serverSocket, nullptr, nullptr);
        //create a socket for answering, careless about client's IP and port

        //receiving data
        char buffer[1024] = {0};
        recv(clientSocket, buffer, sizeof(buffer), 0);
        cout << "A ha! Found a client's message. It said: \"" << buffer << "\"" << endl;

        count--;
    }
    cout << "That is enough for today! See you later!" << endl;

    //closeing socket
    close(serverSocket);

    return 0;
}