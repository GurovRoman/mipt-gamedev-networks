#include "packet.h"
#include <enet/enet.h>
#include <iostream>
#include <thread>
#include <vector>

class StdioThread {
public:
    StdioThread() : thread_([this]() { perform(); }) { }

    ~StdioThread()
    {
        exit_ = true;
        thread_.join();
    }

    template<class Func>
    void poll(Func&& func)
    {
        decltype(lines_) lines;
        {
            std::lock_guard lock{mtx_};
            lines.swap(lines_);
        }

        for (const auto& line: lines) {
            func(line);
        }
    }

private:
    void perform()
    {
        while (!exit_) {
            std::string line;
            std::getline(std::cin, line);

            std::lock_guard lock{mtx_};
            lines_.emplace_back(std::move(line));
        }
    }

private:
    std::vector<std::string> lines_;
    std::mutex mtx_;

    std::atomic<bool> exit_{false};
    std::thread thread_;
};

template<class T>
std::vector<T> parsePlayerList(uint8_t* data, size_t size)
{
    size -= sizeof(PacketType);
    assert(size % sizeof(T) == 0);
    T* begin = reinterpret_cast<T*>(data + sizeof(PacketType));
    std::vector<T> vec(begin, begin + size / sizeof(T));

    return vec;
}

int main(int argc, const char** argv)
{
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }

    ENetHost* client = enet_host_create(nullptr, 2, 1, 0, 0);
    if (!client) {
        printf("Cannot create ENet client\n");
        return 1;
    }

    ENetAddress address;
    enet_address_set_host(&address, "localhost");
    address.port = 10887;

    ENetPeer* serverPeer;
    ENetPeer* lobbyPeer = enet_host_connect(client, &address, 1, 0);
    if (!lobbyPeer) {
        printf("Cannot connect to lobby");
        return 1;
    }

    StdioThread input;

    uint32_t timeStart = enet_time_get();
    uint32_t lastTimed = timeStart;

    bool lobbyConnected = false;
    bool serverConnected = false;
    while (true) {
        ENetEvent event;
        while (enet_host_service(client, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    spdlog::info("Connection with {}:{} established", event.peer->address.host, event.peer->address.port);
                    if (event.peer == lobbyPeer)
                        lobbyConnected = true;
                    else
                        serverConnected = true;
                    lastTimed = enet_time_get();
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    const auto& data = event.packet->data;
                    switch (getPacketType(data)) {
                        case PacketType::ServerAddress: {
                            auto addr = parsePacket<PacketType::ServerAddress>(data).address;
                            spdlog::info("Connecting to server {}:{}", addr.host, addr.port);
                            serverPeer = enet_host_connect(client, &addr, 1, 0);
                            if (!serverPeer) {
                                spdlog::error("Cannot connect to server");
                                return 1;
                            }
                            break;
                        }
                        case PacketType::PlayerList:
                            spdlog::info("Players online:");
                            for (const auto& pl: parsePlayerList<Player>(data, event.packet->dataLength))
                                spdlog::info("(id: {}, name: {})", pl.id, pl.name.data());
                            break;
                        case PacketType::PlayerListWithPing:
                            spdlog::info("Players stats:");
                            for (const auto& pl: parsePlayerList<PlayerWithPing>(data, event.packet->dataLength))
                                spdlog::info("(id: {}, name: {}, ping: {})", pl.player.id, pl.player.name.data(), pl.ping);
                            break;
                        case PacketType::Time:
                            spdlog::info(
                                "Current server timestamp (with random offset): {}",
                                parsePacket<PacketType::Time>(data).time);
                            break;
                        default:
                            spdlog::info("Unexpected packet id: {}", getPacketType(data));
                    }

                    enet_packet_destroy(event.packet);
                    break;
                }
                default:
                    break;
            };
        }
        if (lobbyConnected) {
            input.poll([&](const auto& line) {
                if (line == "start") {
                    sendPacket(lobbyPeer, ENET_PACKET_FLAG_RELIABLE, PStart{});
                }
            });
        }
        if (serverConnected) {
            uint32_t curTime = enet_time_get();
            int8_t randOffset = rand() % 512 - 256;

            if (curTime > lastTimed + 10000) {
                sendPacket(serverPeer, {}, PTime{.time = curTime + randOffset});
                lastTimed = curTime;
            }
        }
    }
    return 0;
}
