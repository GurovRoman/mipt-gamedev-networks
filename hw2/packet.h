#pragma once
#include <cstdint>
#include <enet/enet.h>
#include <spdlog/spdlog.h>

enum class PacketType : uint8_t {
    ServerAddress,
    Start,
    PlayerList,
    Time,
    PlayerListWithPing
};


// Без этого уже жить не получается:)
template<PacketType t>
struct PacketBase {
    PacketType type{t};
};

template<PacketType type>
struct Packet;
#define IMPL_PACKET(Name)               \
    using P##Name = Packet<PacketType::Name>; \
    template<>                                \
    struct Packet<PacketType::Name> : PacketBase<PacketType::Name>

template<PacketType t>
void sendPacket(ENetPeer* peer, enet_uint32 flags, const Packet<t>& data) {
    ENetPacket* packet =
        enet_packet_create(&data, sizeof(data), flags);
    enet_peer_send(peer, 0, packet);
};

PacketType getPacketType(const uint8_t* data) {
    return *reinterpret_cast<const PacketType*>(data);
}

template<PacketType t>
Packet<t> parsePacket(const uint8_t* data) {
    return *reinterpret_cast<const Packet<t>*>(data);
}

IMPL_PACKET(ServerAddress) {
    ENetAddress address;
};

IMPL_PACKET(Start) {};
IMPL_PACKET(PlayerList) {};
IMPL_PACKET(PlayerListWithPing) {};

IMPL_PACKET(Time) {
    uint64_t time;
};

struct Player {
    uint16_t id;
    std::array<char, 7> name;
};

struct PlayerWithPing {
    Player player;
    uint16_t ping;
};