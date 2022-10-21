#include "packet.h"
#include <enet/enet.h>
#include <iostream>
#include <unordered_set>

int main(int argc, const char** argv)
{
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }
    ENetAddress address;

    address.host = ENET_HOST_ANY;
    address.port = 10887;

    ENetHost* server = enet_host_create(&address, 32, 1, 0, 0);

    if (!server) {
        printf("Cannot create ENet server\n");
        return 1;
    }

    bool started_ = false;
    std::unordered_set<ENetPeer*> clients_;
    ENetAddress server_address_ = {};

    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    spdlog::info("Connection with {}:{} established", event.peer->address.host, event.peer->address.port);
                    clients_.emplace(event.peer);
                    if (started_) {
                        spdlog::info("Sending late client to {}:{}", server_address_.host, server_address_.port);
                        sendPacket(event.peer, ENET_PACKET_FLAG_RELIABLE, PServerAddress{.address = server_address_});
                    }
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    spdlog::info("Connection with {}:{} lost", event.peer->address.host, event.peer->address.port);
                    clients_.erase(event.peer);
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    const auto& data = event.packet->data;
                    switch (getPacketType(data)) {
                        case PacketType::ServerAddress:
                            server_address_ = parsePacket<PacketType::ServerAddress>(data).address;
                            spdlog::info("Server registered: {}:{}", server_address_.host, server_address_.port);
                            break;
                        case PacketType::Start: {
                            if (!started_) {
                                if (server_address_.port == 0) {
                                    spdlog::error("Server wasn't started");
                                    break;
                                }

                                spdlog::info(
                                    "Routing {} clients to {}:{}",
                                    clients_.size(),
                                    server_address_.host,
                                    server_address_.port);

                                for (auto& client: clients_) {
                                    sendPacket(
                                        client, ENET_PACKET_FLAG_RELIABLE, PServerAddress{.address = server_address_});
                                }
                                started_ = true;
                                break;
                            }
                        }
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
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}
