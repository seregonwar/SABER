#include "saber_protocol.h"

#include <chrono>
#include <iostream>
#include <random>
#include <thread>

namespace saber {

// Implementazione di SaberConfig
SaberConfig SaberConfig::defaultConfig() {
    // Genera un UUID semplificato per l'ID del nodo
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 0xFFFFFF);
    std::string uuid = "node-" + std::to_string(dis(gen));
    
    return {
        uuid,                // nodeId
        NodeRole::Sink,      // role
        std::nullopt,        // btAddress
        true                 // isMusicMode
    };
}

// Implementazione di SaberProtocol
SaberProtocol::SaberProtocol(const SaberConfig& config)
    : config(config),
      syncManager(std::make_shared<SyncManager>()),
      running(false) {
}

SaberProtocol::~SaberProtocol() {
    // Ferma il thread di runtime se è in esecuzione
    if (running) {
        running = false;
        if (runtimeThread && runtimeThread->joinable()) {
            runtimeThread->join();
        }
    }
}

bool SaberProtocol::initialize() {
    std::cout << "Inizializzazione SABER Protocol con ID " << config.nodeId << std::endl;
    
    // Creazione del nodo locale per la rete mesh
    Node localNode(config.nodeId, config.role);
    
    // Creazione della rete mesh
    meshNetwork = std::make_unique<MeshNetwork>(localNode);
    
    try {
        // Avvio mesh network
        meshNetwork->start();
    } catch (const std::exception& e) {
        std::cerr << "Errore durante l'avvio della rete mesh: " << e.what() << std::endl;
        return false;
    }
    
    // Inizializzazione del sincronizzatore audio
    audioSync = std::make_unique<AudioSync>(syncManager, config.isMusicMode);
    
    // Avvio thread di runtime
    running = true;
    runtimeThread = std::make_unique<std::thread>([this]() {
        while (running) {
            // Esegui operazioni periodiche qui
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    
    std::cout << "Protocollo SABER inizializzato correttamente" << std::endl;
    return true;
}

std::shared_ptr<SyncManager> SaberProtocol::getSyncManager() const {
    return syncManager;
}

bool SaberProtocol::startAudioPlayback() {
    std::lock_guard<std::mutex> lock(protocolMutex);
    
    if (!audioSync) {
        std::cerr << "Sincronizzatore audio non inizializzato" << std::endl;
        return false;
    }
    
    if (audioSync->startPlayback()) {
        std::cout << "Avvio riproduzione audio sincronizzata" << std::endl;
        return true;
    } else {
        return false;
    }
}

bool SaberProtocol::stopAudioPlayback() {
    std::lock_guard<std::mutex> lock(protocolMutex);
    
    if (!audioSync) {
        std::cerr << "Sincronizzatore audio non inizializzato" << std::endl;
        return false;
    }
    
    audioSync->stopPlayback();
    std::cout << "Arresto riproduzione audio" << std::endl;
    return true;
}

bool SaberProtocol::updateTimeSync(uint64_t masterTime) {
    return syncManager->handleTimeBeacon(masterTime);
}

uint32_t SaberProtocol::getCurrentLatency() const {
    std::lock_guard<std::mutex> lock(protocolMutex);
    
    if (audioSync) {
        return audioSync->getCurrentLatency();
    } else {
        return 0;
    }
}

bool SaberProtocol::registerNode(const std::string& nodeId, NodeRole role, 
                             const std::optional<std::string>& address) {
    std::lock_guard<std::mutex> lock(protocolMutex);
    
    if (!meshNetwork) {
        std::cerr << "Rete mesh non inizializzata" << std::endl;
        return false;
    }
    
    // Invece di creare un oggetto Node, passa direttamente i parametri
    meshNetwork->registerNode(nodeId, role);
    return true;
}

std::vector<std::string> SaberProtocol::getActiveNodes() const {
    std::lock_guard<std::mutex> lock(protocolMutex);
    
    if (!meshNetwork) {
        std::cerr << "Rete mesh non inizializzata" << std::endl;
        return {};
    }
    
    // Ora meshNetwork->getActiveNodes() restituisce direttamente un vettore di stringhe
    return meshNetwork->getActiveNodes();
}

bool SaberProtocol::isSynchronized() const {
    return syncManager->isSynchronized();
}

// Funzioni di utilità
std::unique_ptr<SaberProtocol> startMaster(const std::optional<std::string>& nodeId, 
                         const std::optional<std::string>& btAddress) {
    // Genera un ID se non fornito
    std::string id;
    if (nodeId) {
        id = *nodeId;
    } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 0xFFFFFF);
        id = "master-" + std::to_string(dis(gen));
    }
    
    SaberConfig config = {
        id,                  // nodeId
        NodeRole::Master,    // role
        btAddress,           // btAddress
        true                 // isMusicMode
    };
    
    auto protocol = std::make_unique<SaberProtocol>(config);
    if (!protocol->initialize()) {
        throw std::runtime_error("Impossibile inizializzare il protocollo SABER");
    }
    
    std::cout << "Nodo Master (UCB) avviato" << std::endl;
    return protocol;
}

std::unique_ptr<SaberProtocol> startRepeater(const std::optional<std::string>& nodeId,
                           const std::optional<std::string>& btAddress) {
    // Genera un ID se non fornito
    std::string id;
    if (nodeId) {
        id = *nodeId;
    } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 0xFFFFFF);
        id = "repeater-" + std::to_string(dis(gen));
    }
    
    SaberConfig config = {
        id,                  // nodeId
        NodeRole::Repeater,  // role
        btAddress,           // btAddress
        true                 // isMusicMode
    };
    
    auto protocol = std::make_unique<SaberProtocol>(config);
    if (!protocol->initialize()) {
        throw std::runtime_error("Impossibile inizializzare il protocollo SABER");
    }
    
    std::cout << "Nodo Repeater avviato" << std::endl;
    return protocol;
}

std::unique_ptr<SaberProtocol> startSink(const std::optional<std::string>& nodeId,
                       const std::optional<std::string>& btAddress,
                       bool isMusic) {
    // Genera un ID se non fornito
    std::string id;
    if (nodeId) {
        id = *nodeId;
    } else {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 0xFFFFFF);
        id = "sink-" + std::to_string(dis(gen));
    }
    
    SaberConfig config = {
        id,                  // nodeId
        NodeRole::Sink,      // role
        btAddress,           // btAddress
        isMusic              // isMusicMode
    };
    
    auto protocol = std::make_unique<SaberProtocol>(config);
    if (!protocol->initialize()) {
        throw std::runtime_error("Impossibile inizializzare il protocollo SABER");
    }
    
    std::cout << "Nodo Sink avviato" << std::endl;
    return protocol;
}

} // namespace saber
