#ifndef SABER_SYNC_H
#define SABER_SYNC_H

#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace saber {

/**
 * @brief Struttura per gestire la sincronizzazione temporale tra i dispositivi
 */
class SyncManager {
public:
    /**
     * @brief Crea una nuova istanza del gestore di sincronizzazione
     */
    SyncManager();
    
    /**
     * @brief Ottiene il timestamp corrente sincronizzato
     * @return Timestamp in millisecondi
     */
    uint64_t now() const;
    
    /**
     * @brief Gestisce un beacon temporale ricevuto dal master
     * @param masterTime Tempo del master
     * @return true se la sincronizzazione è avvenuta con successo, false altrimenti
     */
    bool handleTimeBeacon(uint64_t masterTime);
    
    /**
     * @brief Verifica se il nodo è sincronizzato
     * @return true se il nodo è sincronizzato, false altrimenti
     */
    bool isSynchronized() const;
    
    /**
     * @brief Calcola e registra la latenza di un nodo
     * @param nodeId ID del nodo
     * @param latency Latenza in millisecondi
     */
    void updateNodeLatency(const std::string& nodeId, uint32_t latency);
    
    /**
     * @brief Ottiene la latenza media di tutti i nodi
     * @return Latenza media in millisecondi, o nullopt se non ci sono nodi
     */
    std::optional<float> getAverageLatency() const;
    
    /**
     * @brief Verifica se un nodo è desincronizzato (jitter oltre la soglia)
     * @param nodeId ID del nodo
     * @param reportedTime Tempo riportato dal nodo
     * @return true se il nodo è desincronizzato, false altrimenti
     */
    bool isNodeOutOfSync(const std::string& nodeId, uint64_t reportedTime) const;
    
    /**
     * @brief Calcola il buffer necessario per compensare la latenza
     * @param nodeLatency Latenza del nodo in millisecondi
     * @return Dimensione del buffer in millisecondi
     */
    uint32_t calculateBufferAdjustment(uint32_t nodeLatency) const;
    
    /**
     * @brief Determina la dimensione del buffer audio ottimale per tutti i nodi
     * @return Dimensione ottimale del buffer in millisecondi
     */
    uint32_t getOptimalBufferSize() const;
    
    /**
     * @brief Effettua una sincronizzazione di emergenza (quando la connessione BIS è persa)
     * @param masterTime Tempo del master
     * @return true se la sincronizzazione è avvenuta con successo, false altrimenti
     */
    bool emergencySync(uint64_t masterTime);
    
private:
    /// Offset per sincronizzare l'orologio locale con il master
    std::shared_ptr<int64_t> timeOffset;
    
    /// Timestamp dell'ultimo beacon ricevuto
    std::shared_ptr<std::optional<std::chrono::steady_clock::time_point>> lastBeacon;
    
    /// Mappa delle latenze dei nodi
    std::shared_ptr<std::map<std::string, uint32_t>> nodeLatencies;
    
    /// Flag che indica se il dispositivo è sincronizzato
    std::shared_ptr<bool> isSynced;
    
    /// Jitter massimo tollerato (in ms)
    uint32_t maxJitterMs;
    
    /// Mutex per proteggere l'accesso concorrente
    mutable std::mutex syncMutex;
};

/**
 * @brief Struttura per la sincronizzazione dell'audio
 */
class AudioSync {
public:
    /**
     * @brief Crea una nuova istanza del sincronizzatore audio
     * @param syncManager Manager di sincronizzazione
     * @param isMusic Flag che indica se l'audio è musicale (48kHz) o vocale (16kHz)
     */
    AudioSync(std::shared_ptr<SyncManager> syncManager, bool isMusic);
    
    /**
     * @brief Avvia la riproduzione sincronizzata
     * @return true se l'avvio è avvenuto con successo, false altrimenti
     */
    bool startPlayback();
    
    /**
     * @brief Interrompe la riproduzione
     */
    void stopPlayback();
    
    /**
     * @brief Aggiusta il bitrate in base alle condizioni della rete
     * @param networkQuality Qualità della rete (0.0-1.0)
     */
    void adjustBitrate(float networkQuality);
    
    /**
     * @brief Ottiene la latenza corrente
     * @return Latenza in millisecondi
     */
    uint32_t getCurrentLatency() const;
    
    /**
     * @brief Verifica se la riproduzione è sincronizzata
     * @return true se la riproduzione è sincronizzata, false altrimenti
     */
    bool isPlaybackSynchronized() const;
    
private:
    /// Manager di sincronizzazione globale
    std::shared_ptr<SyncManager> syncManager;
    
    /// Buffer di jitter in millisecondi
    uint32_t jitterBuffer;
    
    /// Flag che indica se l'audio è in riproduzione
    bool isPlaying;
    
    /// Formato audio (sezione 4.1 del PAPER.md)
    uint32_t sampleRate;
    
    /// Bitrate in kbps
    uint32_t bitrate;
};

} // namespace saber

#endif // SABER_SYNC_H
