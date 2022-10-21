#include "packet.h"
#include <enet/enet.h>
#include <iostream>
#include <span>
#include <unordered_set>

static uint16_t next_id = 0;

std::array<char, 7> generateName()
{
    static const char alphanum[] = "abcdefghijklmnopqrstuvwxyz";
    std::array<char, 7> str{};

    for (int i = 0; i < 7; ++i) {
        str[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
    }
    str[0] += 'A' - 'a';

    return str;
}

template<PacketType t, class T>
void sendPlayerList(ENetPeer* peer, enet_uint32 flags, std::span<const T> players)
{
    std::vector<uint8_t> vec(players.size() * sizeof(T) + sizeof(PacketType));

    std::memcpy(vec.data() + sizeof(PacketType), players.data(), players.size() * sizeof(T));
    *reinterpret_cast<PacketType*>(vec.data()) = t;

    enet_peer_send(peer, 0, enet_packet_create(vec.data(), vec.size(), flags));
}

int main(int argc, const char** argv)
{
    if (enet_initialize() != 0) {
        printf("Cannot init ENet");
        return 1;
    }
    ENetAddress address;

    address.host = ENET_HOST_ANY;
    address.port = 10889;

    ENetHost* server = enet_host_create(&address, 32, 1, 0, 0);

    if (!server) {
        printf("Cannot create ENet server\n");
        return 1;
    }

    ENetPeer* lobbyPeer;
    {
        ENetAddress lobby_address;
        enet_address_set_host(&lobby_address, "localhost");
        lobby_address.port = 10887;

        lobbyPeer = enet_host_connect(server, &lobby_address, 1, 0);
        if (!lobbyPeer) {
            printf("Cannot connect to lobby");
            return 1;
        }
    }

    std::unordered_map<ENetPeer*, Player> clients_;
    uint32_t lastTimed = enet_time_get();
    uint32_t lastStats = lastTimed;

    while (true) {
        ENetEvent event;
        while (enet_host_service(server, &event, 10) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    spdlog::info("Connection with {}:{} established", event.peer->address.host, event.peer->address.port);
                    if (event.peer == lobbyPeer) {
                        ENetAddress addr;
                        enet_address_set_host(&addr, "localhost");
                        addr.port = address.port;

                        spdlog::info("Sending address to the lobby... {}:{}", addr.host, addr.port);
                        sendPacket(event.peer, ENET_PACKET_FLAG_RELIABLE, PServerAddress{.address = addr});
                        enet_peer_disconnect_later(event.peer, {});
                        lobbyPeer = nullptr;
                    } else {
                        const auto& player =
                            clients_.try_emplace(event.peer, Player{.id = next_id++, .name = generateName()}).first->second;
                        spdlog::info("Player {}: {} enters the game", player.id, player.name.data());
                        std::vector<Player> playerlist;
                        for (const auto& cl: clients_)
                            playerlist.emplace_back(cl.second);
                        sendPlayerList<PacketType::PlayerList, Player>(event.peer, ENET_PACKET_FLAG_RELIABLE, playerlist);
                    }
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    spdlog::info("Connection with {}:{} lost", event.peer->address.host, event.peer->address.port);
                    clients_.erase(event.peer);
                    break;
                case ENET_EVENT_TYPE_RECEIVE: {
                    const auto& data = event.packet->data;
                    switch (getPacketType(data)) {
                        case PacketType::Time: {
                            const auto& plr = clients_[event.peer];
                            spdlog::info(
                                "Player {}: {} reports timestamp: {}",
                                plr.id,
                                plr.name.data(),
                                parsePacket<PacketType::Time>(data).time);
                            break;
                        }
                        default:
                            spdlog::info("Unexpected packet id: {}", getPacketType(data));
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }
                default:
                    break;
            }

            {
                uint32_t curTime = enet_time_get();
                int8_t randOffset = rand() % 256 - 128;

                if (curTime > lastTimed + 5000) {
                    for (const auto& cl: clients_)
                        sendPacket(cl.first, {}, PTime{.time = curTime + randOffset});
                    lastTimed = curTime;
                }

                if (curTime > lastStats + 25000) {
                    std::vector<PlayerWithPing> playerlist;
                    for (const auto& cl: clients_)
                        playerlist.emplace_back(PlayerWithPing{
                            .player = cl.second, .ping = static_cast<uint16_t>(cl.first->roundTripTime / 2)});
                    for (const auto& cl: clients_)
                        sendPlayerList<PacketType::PlayerListWithPing, PlayerWithPing>(cl.first, {}, playerlist);
                    lastStats = curTime;
                }
            }
        };
    }

    enet_host_destroy(server);

    atexit(enet_deinitialize);
    return 0;
}
