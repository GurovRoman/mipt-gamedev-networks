#include "socket_tools.h"
#include <netdb.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <vector>

using namespace std::chrono_literals;

namespace std {
template<>
struct hash<sockaddr_in> {
    std::size_t operator()(sockaddr_in const& a) const
    {
        return std::hash<decltype(a.sin_addr.s_addr)>()(a.sin_addr.s_addr) ^
               (std::hash<decltype(a.sin_family)>()(a.sin_family) << 2) ^
               (std::hash<decltype(a.sin_port)>()(a.sin_port) << 1);
    }
};
} // namespace std

// kms
bool operator==(sockaddr_in a, sockaddr_in b)
{
    return a.sin_port == b.sin_port && a.sin_family == b.sin_family &&
           a.sin_addr.s_addr == b.sin_addr.s_addr;
}

std::string format_client(sockaddr_in client)
{
    auto ip = client.sin_addr.s_addr;
    auto port = client.sin_port;

    std::ostringstream res;
    res << (ip & 0xFF);
    res << '.';
    res << ((ip >>= 8) & 0xFF);
    res << '.';
    res << ((ip >>= 8) & 0xFF);
    res << '.';
    res << ((ip >>= 8) & 0xFF);
    res << ':';
    res << port;

    return res.str();
}

const std::chrono::duration PING_TIME = 500ms;
const std::chrono::duration TIMEOUT = 5s;

void handle_keepalive(
    std::unordered_map<sockaddr_in, std::chrono::steady_clock::time_point>& last_seen,
    int ping_sock)
{
    auto time = std::chrono::steady_clock::now();
    std::vector<sockaddr_in> dead;
    for (const auto& client: last_seen) {
        if (time > client.second + TIMEOUT) {
            dead.emplace_back(client.first);
            std::cout << "Client " << format_client(client.first)
                      << " timed out\n";
            continue;
        }
        if (time > client.second + PING_TIME) {
            std::string payload = "k";
            ssize_t res = sendto(
                ping_sock,
                payload.c_str(),
                payload.size(),
                0,
                (sockaddr*)&client.first,
                sizeof(client.first));
            if (res == -1) {
                dead.emplace_back(client.first);
                std::cout << "Unable to ping client "
                          << format_client(client.first)
                          << " with err=" << strerror(errno) << std::endl;
            }
        }
    }
    for (const auto& ded: dead) {
        last_seen.erase(ded);
    }
}

int main(int argc, const char** argv)
{
    std::unordered_map<sockaddr_in, std::chrono::steady_clock::time_point> last_seen;

    const char* port = "2024";

    int sfd = create_dgram_socket(nullptr, port, nullptr);

    if (sfd == -1)
        return 1;
    printf("listening!\n");

    fd_set readSet;
    FD_ZERO(&readSet);

    while (true) {
        FD_SET(sfd, &readSet);

        timeval timeout = {
            .tv_usec = 100000 // 100msec
        };
        select(sfd + 1, &readSet, nullptr, nullptr, &timeout);

        handle_keepalive(last_seen, sfd);

        if (FD_ISSET(sfd, &readSet)) {
            constexpr size_t buf_size = 1000;
            static char buffer[buf_size];
            memset(buffer, 0, buf_size);

            sockaddr_in from{};
            socklen_t from_len = sizeof(from);

            ssize_t numBytes = recvfrom(
                sfd, buffer, buf_size - 1, 0, (sockaddr*)&from, &from_len);

            if (last_seen.count(from) == 0)
                std::cout << "Client " << format_client(from) << " connected\n";

            last_seen[from] = std::chrono::steady_clock::now();

            if (numBytes > 0 && buffer[0] == 'm') {
                std::cout << "Received from client " << format_client(from)
                          << ": " << buffer + 1 << '\n';
                auto oldlen = strlen(buffer);
                snprintf(buffer + oldlen, sizeof(buffer) - oldlen, "%s", " too");

                ssize_t res = sendto(
                    sfd, buffer, strlen(buffer), 0, (sockaddr*)&from, from_len);
                if (res == -1)
                    std::cout << strerror(errno) << std::endl;
            }
        }
    }
}
