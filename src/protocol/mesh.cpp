#include "../include/mesh.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>

namespace saber {

// Implementazione di Node
Node::Node(const std::string& id, NodeRole role)
    : id(id), role(role), latency(0), bufferState(100) {
}

void Node::updatePing() {
    lastPing = std::chrono::steady_clock::now();
}

void Node::updateBufferState(uint8_t state) {
    bufferState = state;
}

void Node::setLatency(uint32_t latency) {
    this->latency = latency;
}

uint32_t Node::getLatency() const {
    return latency;
}

bool Node::isActive() const {
    if (!lastPing) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - *lastPing).count();
    
    // Consideriamo attivo un nodo che ha inviato un ping negli ultimi 30 secondi
    return diff < 30;
}

// Implementazione di MeshPacket
MeshPacket::MeshPacket(MeshPacketType type) : type(type) {
    // Inizializzazione della union in base al tipo
    switch (type) {
        case MeshPacketType::Ping:
            new (&data.ping) PingData();
            break;
        case MeshPacketType::Command:
            new (&data.command) CommandData();
            break;
        case MeshPacketType::Status:
            new (&data.status) StatusData();
            break;
        case MeshPacketType::TimeBeacon:
            new (&data.timeBeacon) TimeBeaconData();
            break;
        case MeshPacketType::EmergencySync:
            new (&data.emergencySync) EmergencySyncData();
            break;
    }
}

MeshPacket::~MeshPacket() {
    destroyData();
}

MeshPacket::MeshPacket(const MeshPacket& other) : type(other.type) {
    copyDataFromOther(other);
}

MeshPacket& MeshPacket::operator=(const MeshPacket& other) {
    if (this != &other) {
        destroyData();
        type = other.type;
        copyDataFromOther(other);
    }
    return *this;
}

MeshPacket::MeshPacket(MeshPacket&& other) noexcept : type(other.type) {
    copyDataFromOther(other);
    other.type = MeshPacketType::Ping; // Reset other to a known state
    new (&other.data.ping) PingData(); // Initialize with empty data
}

MeshPacket& MeshPacket::operator=(MeshPacket&& other) noexcept {
    if (this != &other) {
        destroyData();
        type = other.type;
        copyDataFromOther(other);
        other.type = MeshPacketType::Ping; // Reset other to a known state
        new (&other.data.ping) PingData(); // Initialize with empty data
    }
    return *this;
}

void MeshPacket::copyDataFromOther(const MeshPacket& other) {
    switch (type) {
        case MeshPacketType::Ping:
            new (&data.ping) PingData(other.data.ping);
            break;
        case MeshPacketType::Command:
            new (&data.command) CommandData(other.data.command);
            break;
        case MeshPacketType::Status:
            new (&data.status) StatusData(other.data.status);
            break;
        case MeshPacketType::TimeBeacon:
            new (&data.timeBeacon) TimeBeaconData(other.data.timeBeacon);
            break;
        case MeshPacketType::EmergencySync:
            new (&data.emergencySync) EmergencySyncData(other.data.emergencySync);
            break;
    }
}

void MeshPacket::destroyData() {
    switch (type) {
        case MeshPacketType::Ping:
            data.ping.~PingData();
            break;
        case MeshPacketType::Command:
            data.command.~CommandData();
            break;
        case MeshPacketType::Status:
            data.status.~StatusData();
            break;
        case MeshPacketType::TimeBeacon:
            data.timeBeacon.~TimeBeaconData();
            break;
        case MeshPacketType::EmergencySync:
            data.emergencySync.~EmergencySyncData();
            break;
    }
}

MeshPacket MeshPacket::createPing(const std::string& source, uint64_t timestamp) {
    MeshPacket packet(MeshPacketType::Ping);
    packet.data.ping.source = source;
    packet.data.ping.timestamp = timestamp;
    return packet;
}

MeshPacket MeshPacket::createCommand(const std::string& cmdType, 
                                   const std::map<std::string, std::string>& params) {
    MeshPacket packet(MeshPacketType::Command);
    packet.data.command.cmdType = cmdType;
    packet.data.command.params = params;
    return packet;
}

MeshPacket MeshPacket::createStatus(const std::string& nodeId, uint8_t buffer, uint32_t latency) {
    MeshPacket packet(MeshPacketType::Status);
    packet.data.status.nodeId = nodeId;
    packet.data.status.buffer = buffer;
    packet.data.status.latency = latency;
    return packet;
}

MeshPacket MeshPacket::createTimeBeacon(uint64_t masterTime) {
    MeshPacket packet(MeshPacketType::TimeBeacon);
    packet.data.timeBeacon.masterTime = masterTime;
    return packet;
}

MeshPacket MeshPacket::createEmergencySync(uint64_t masterTime, 
                                         const std::vector<std::string>& targetNodes) {
    MeshPacket packet(MeshPacketType::EmergencySync);
    packet.data.emergencySync.masterTime = masterTime;
    packet.data.emergencySync.targetNodes = targetNodes;
    return packet;
}

MeshPacketType MeshPacket::getType() const {
    return type;
}

std::pair<std::string, uint64_t> MeshPacket::getPingData() const {
    if (type != MeshPacketType::Ping) {
        throw std::runtime_error("Pacchetto non è di tipo Ping");
    }
    return {data.ping.source, data.ping.timestamp};
}

std::pair<std::string, std::map<std::string, std::string>> MeshPacket::getCommandData() const {
    if (type != MeshPacketType::Command) {
        throw std::runtime_error("Pacchetto non è di tipo Command");
    }
    return {data.command.cmdType, data.command.params};
}

std::tuple<std::string, uint8_t, uint32_t> MeshPacket::getStatusData() const {
    if (type != MeshPacketType::Status) {
        throw std::runtime_error("Pacchetto non è di tipo Status");
    }
    return {data.status.nodeId, data.status.buffer, data.status.latency};
}

uint64_t MeshPacket::getTimeBeaconData() const {
    if (type != MeshPacketType::TimeBeacon) {
        throw std::runtime_error("Pacchetto non è di tipo TimeBeacon");
    }
    return data.timeBeacon.masterTime;
}

std::pair<uint64_t, std::vector<std::string>> MeshPacket::getEmergencySyncData() const {
    if (type != MeshPacketType::EmergencySync) {
        throw std::runtime_error("Pacchetto non è di tipo EmergencySync");
    }
    return {data.emergencySync.masterTime, data.emergencySync.targetNodes};
}

// Implementazione di MeshNetwork
MeshNetwork::MeshNetwork(const Node& localNode) 
    : localNode(localNode), running(false) {
    // Registra il nodo locale
    nodes[localNode.id] = localNode;
}

MeshNetwork::~MeshNetwork() {
    stop();
}

void MeshNetwork::start() {
    std::lock_guard<std::mutex> lock(networkMutex);
    if (running) {
        return;
    }
    
    running = true;
    networkThread = std::make_unique<std::thread>(&MeshNetwork::runNetworkLoop, this);
}

void MeshNetwork::stop() {
    {
        std::lock_guard<std::mutex> lock(networkMutex);
        if (!running) {
            return;
        }
        
        running = false;
    }
    
    // Notifica il thread di uscire
    queueCondition.notify_all();
    
    if (networkThread && networkThread->joinable()) {
        networkThread->join();
    }
}

void MeshNetwork::sendPacket(const MeshPacket& packet) {
    std::lock_guard<std::mutex> lock(queueMutex);
    packetQueue.push_back(packet);
    queueCondition.notify_one();
}

void MeshNetwork::registerNode(const std::string& nodeId, NodeRole role) {
    std::lock_guard<std::mutex> lock(networkMutex);
    if (nodes.find(nodeId) == nodes.end()) {
        nodes[nodeId] = Node(nodeId, role);
    }
}

void MeshNetwork::updateNodeStatus(const std::string& nodeId, uint8_t bufferState, uint32_t latency) {
    std::lock_guard<std::mutex> lock(networkMutex);
    auto it = nodes.find(nodeId);
    if (it != nodes.end()) {
        it->second.updateBufferState(bufferState);
        it->second.setLatency(latency);
        it->second.updatePing();
    }
}

std::vector<std::string> MeshNetwork::getActiveNodes() const {
    std::lock_guard<std::mutex> lock(networkMutex);
    std::vector<std::string> activeNodes;
    
    for (const auto& pair : nodes) {
        if (pair.second.isActive()) {
            activeNodes.push_back(pair.first);
        }
    }
    
    return activeNodes;
}

void MeshNetwork::setPacketHandler(PacketHandler handler) {
    std::lock_guard<std::mutex> lock(networkMutex);
    packetHandler = handler;
}

void MeshNetwork::runNetworkLoop() {
    while (running) {
        std::vector<MeshPacket> packetsToProcess;
        
        {
            std::unique_lock<std::mutex> lock(queueMutex);
            queueCondition.wait_for(lock, std::chrono::milliseconds(100), 
                                   [this] { return !packetQueue.empty() || !running; });
            
            if (!running) {
                break;
            }
            
            if (!packetQueue.empty()) {
                packetsToProcess.swap(packetQueue);
            }
        }
        
        for (const auto& packet : packetsToProcess) {
            processPacket(packet);
        }
    }
}

void MeshNetwork::processPacket(const MeshPacket& packet) {
    std::lock_guard<std::mutex> lock(networkMutex);
    
    // Elabora il pacchetto in base al tipo
    switch (packet.getType()) {
        case MeshPacketType::Ping: {
            auto [source, timestamp] = packet.getPingData();
            auto it = nodes.find(source);
            if (it != nodes.end()) {
                it->second.updatePing();
            }
            break;
        }
        case MeshPacketType::Status: {
            auto [nodeId, buffer, latency] = packet.getStatusData();
            updateNodeStatus(nodeId, buffer, latency);
            break;
        }
        default:
            break;
    }
    
    // Inoltra il pacchetto al gestore registrato
    if (packetHandler) {
        packetHandler(packet);
    }
}

} // namespace saber
