#include "defines.h"

int readn(int &sock, char *message, int mesasge_size) {
    char buf[mesasge_size];
    bzero(buf, mesasge_size);
    bzero(message, mesasge_size);
    int received = 0;
    do {
        auto &&nbytes = recv(sock, buf, mesasge_size - received, 0);
        if(nbytes == -1){
            if (errno == EAGAIN) return RECV_TIMEOUT;
            std::cerr << "Error in recv" << std::endl;
            return RECV_ERROR;
        }
        if (nbytes == 0) {
            std::cout << "socket " << sock << " closed" << std::endl;
            return -1;
        }
        memcpy(message + received, buf, nbytes);
        received += nbytes;
    } while (received < mesasge_size);

    if (received != mesasge_size) {
        std::cerr << "Incorrect message received" << std::endl;
        return ERROR_MESSAGE_SIZE;
    }
    return 0;
}
