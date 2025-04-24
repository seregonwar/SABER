#include "sync.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>

namespace saber {

// Implementazione di SyncManager
SyncManager::SyncManager()
    : timeOffset(std::make_shared<int64_t>(0)),
      lastBeacon(std::make_shared<std::optional<std::chrono::steady_clock::time_point>>(std::nullopt)),
      nodeLatencies(std::make_shared<std::map<std::string, uint32_t>>()),
      isSynced(std::make_shared<bool>(false)),
      maxJitterMs(5) { // Come da PAPER.md sezione 4.2, la tolleranza jitter è < ±5 ms
}

uint64_t SyncManager::now() const {
    auto systemTime = std::chrono::system_clock::now();
    auto duration = systemTime.time_since_epoch();
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // Applico l'offset di sincronizzazione
    std::lock_guard<std::mutex> lock(syncMutex);
    int64_t offset = *timeOffset;
    
    if (offset >= 0) {
        return currentTime + static_cast<uint64_t>(offset);
    } else {
        return currentTime - static_cast<uint64_t>(-offset);
    }
}

bool SyncManager::handleTimeBeacon(uint64_t masterTime) {
    auto systemTime = std::chrono::system_clock::now();
    auto duration = systemTime.time_since_epoch();
    uint64_t currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    // Calcolo l'offset necessario per sincronizzarsi col master
    int64_t calculatedOffset = static_cast<int64_t>(masterTime) - static_cast<int64_t>(currentTime);
    
    {
        std::lock_guard<std::mutex> lock(syncMutex);
        
        // Aggiorno l'offset
        *timeOffset = calculatedOffset;
        
        // Aggiorno il timestamp dell'ultimo beacon
        *lastBeacon = std::chrono::steady_clock::now();
        
        // Marco il dispositivo come sincronizzato
        *isSynced = true;
    }
    
    return true;
}

bool SyncManager::isSynchronized() const {
    std::lock_guard<std::mutex> lock(syncMutex);
    
    // Controllo se abbiamo ricevuto almeno un beacon
    bool hasBeacon = lastBeacon->has_value();
    
    // Verifico lo stato del flag di sincronizzazione
    bool synced = *isSynced;
    
    return hasBeacon && synced;
}

void SyncManager::updateNodeLatency(const std::string& nodeId, uint32_t latency) {
    std::lock_guard<std::mutex> lock(syncMutex);
    (*nodeLatencies)[nodeId] = latency;
}

std::optional<float> SyncManager::getAverageLatency() const {
    std::lock_guard<std::mutex> lock(syncMutex);
    
    if (nodeLatencies->empty()) {
        return std::nullopt;
    }
    
    uint32_t sum = 0;
    for (const auto& pair : *nodeLatencies) {
        sum += pair.second;
    }
    
    return static_cast<float>(sum) / nodeLatencies->size();
}

bool SyncManager::isNodeOutOfSync(const std::string& nodeId, uint64_t reportedTime) const {
    uint64_t currentTime = now();
    uint64_t timeDiff = (currentTime > reportedTime) ? 
                        (currentTime - reportedTime) : 
                        (reportedTime - currentTime);
    
    // Se la differenza è maggiore del jitter massimo, il nodo è desincronizzato
    return timeDiff > maxJitterMs;
}

uint32_t SyncManager::calculateBufferAdjustment(uint32_t nodeLatency) const {
    // Imposta un buffer leggermente superiore alla latenza per evitare interruzioni
    // Mantenendo comunque sotto la soglia dei 40ms (sezione 4.1 del PAPER.md)
    uint32_t bufferSize = nodeLatency + 10;
    return std::min(bufferSize, 40u); // Limito al massimo a 40ms come da specifiche
}

uint32_t SyncManager::getOptimalBufferSize() const {
    auto avgLatency = getAverageLatency();
    if (avgLatency) {
        return calculateBufferAdjustment(static_cast<uint32_t>(*avgLatency));
    } else {
        return 20; // Valore di default in assenza di misurazioni
    }
}

bool SyncManager::emergencySync(uint64_t masterTime) {
    // In caso di emergenza, forza la sincronizzazione
    bool result = handleTimeBeacon(masterTime);
    
    // Reset delle latenze
    std::lock_guard<std::mutex> lock(syncMutex);
    nodeLatencies->clear();
    
    return result;
}

// Implementazione di AudioSync
AudioSync::AudioSync(std::shared_ptr<SyncManager> syncManager, bool isMusic)
    : syncManager(syncManager),
      jitterBuffer(20), // Valore iniziale di default
      isPlaying(false),
      sampleRate(isMusic ? 48000 : 16000),
      bitrate(isMusic ? 128 : 64) {
}

bool AudioSync::startPlayback() {
    if (!syncManager->isSynchronized()) {
        std::cerr << "Impossibile avviare la riproduzione: dispositivo non sincronizzato" << std::endl;
        return false;
    }
    
    // Aggiorno il buffer di jitter in base alle latenze attuali
    jitterBuffer = syncManager->getOptimalBufferSize();
    
    isPlaying = true;
    std::cout << "Avvio riproduzione con buffer di " << jitterBuffer << "ms" << std::endl;
    
    return true;
}

void AudioSync::stopPlayback() {
    isPlaying = false;
}

void AudioSync::adjustBitrate(float networkQuality) {
    // networkQuality è un valore da 0.0 a 1.0
    if (networkQuality < 0.5) {
        // Riduco il bitrate in caso di rete debole
        bitrate = (sampleRate == 48000) ? 64 : 32;
    } else {
        // Ripristino il bitrate normale
        bitrate = (sampleRate == 48000) ? 128 : 64;
    }
    
    std::cout << "Bitrate aggiustato a " << bitrate << "kbps" << std::endl;
}

uint32_t AudioSync::getCurrentLatency() const {
    auto avgLatency = syncManager->getAverageLatency();
    return avgLatency ? static_cast<uint32_t>(*avgLatency) : 0;
}

bool AudioSync::isPlaybackSynchronized() const {
    return syncManager->isSynchronized() && isPlaying;
}

} // namespace saber
