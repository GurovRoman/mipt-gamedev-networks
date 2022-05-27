#include "socket_tools.h"
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <thread>

int main(int argc, const char** argv)
{
    const char* port = "2024";

    addrinfo resAddrInfo;
    int sfd = create_dgram_socket("localhost", port, &resAddrInfo);

    if (sfd == -1) {
        printf("Cannot create a socket\n");
        return 1;
    }

    fd_set readSet;
    FD_ZERO(&readSet);

    while (true) {
        FD_SET(0, &readSet);
        FD_SET(sfd, &readSet);

        if (select(sfd + 1, &readSet, nullptr, nullptr, nullptr) == -1)
            break;

        if (FD_ISSET(0, &readSet)) {
            std::string input;
            std::getline(std::cin, input);
            input = "m" + input;
            ssize_t res = sendto(
                sfd,
                input.c_str(),
                input.size(),
                0,
                resAddrInfo.ai_addr,
                resAddrInfo.ai_addrlen);
            if (res == -1)
                std::cout << strerror(errno) << std::endl;
        }

        if (FD_ISSET(sfd, &readSet)) {
            constexpr size_t buf_size = 1000;
            static char buffer[buf_size];
            memset(buffer, 0, buf_size);

            ssize_t numBytes =
                recvfrom(sfd, buffer, buf_size - 1, 0, nullptr, nullptr);

            if (numBytes > 0) {
                if (buffer[0] == 'm') {
                    std::cout << "The server replies: " << buffer + 1 << '\n';
                } else if (buffer[0] == 'k') {
                    ssize_t res = sendto(
                        sfd,
                        buffer,
                        strlen(buffer),
                        0,
                        resAddrInfo.ai_addr,
                        resAddrInfo.ai_addrlen);
                    if (res == -1)
                        std::cout << "Unable to respond to keepalive ping: "
                                  << strerror(errno) << std::endl;
                }
            }
        }
    }
}
