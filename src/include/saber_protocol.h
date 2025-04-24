#ifndef SABER_PROTOCOL_H
#define SABER_PROTOCOL_H

#include "crypto.h"
#include "mesh.h"
#include "sync.h"

#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace saber {

/**
 * @brief Configurazione per il nodo SABER
 */
struct SaberConfig {
    /// ID univoco del nodo
    std::string nodeId;
    
    /// Ruolo del nodo nella rete
    NodeRole role;
    
    /// Indirizzo Bluetooth (opzionale)
    std::optional<std::string> btAddress;
    
    /// Flag che indica se il nodo riproduce audio musicale (48kHz) o vocale (16kHz)
    bool isMusicMode;
    
    /**
     * @brief Crea una configurazione di default
     * @return Configurazione di default
     */
    static SaberConfig defaultConfig();
};

/**
 * @brief Gestore principale del protocollo SABER
 */
class SaberProtocol {
public:
    /**
     * @brief Crea una nuova istanza del protocollo SABER
     * @param config Configurazione del nodo
     */
    SaberProtocol(const SaberConfig& config);
    
    /**
     * @brief Distruttore
     */
    ~SaberProtocol();
    
    /**
     * @brief Inizializza e avvia il protocollo
     * @return true se l'inizializzazione è avvenuta con successo, false altrimenti
     */
    bool initialize();
    
    /**
     * @brief Ottiene il manager di sincronizzazione
     * @return Puntatore condiviso al manager di sincronizzazione
     */
    std::shared_ptr<SyncManager> getSyncManager() const;
    
    /**
     * @brief Avvia la riproduzione audio sincronizzata
     * @return true se l'avvio è avvenuto con successo, false altrimenti
     */
    bool startAudioPlayback();
    
    /**
     * @brief Ferma la riproduzione audio
     * @return true se l'arresto è avvenuto con successo, false altrimenti
     */
    bool stopAudioPlayback();
    
    /**
     * @brief Aggiorna lo stato di sincronizzazione con un beacon temporale
     * @param masterTime Tempo del master
     * @return true se l'aggiornamento è avvenuto con successo, false altrimenti
     */
    bool updateTimeSync(uint64_t masterTime);
    
    /**
     * @brief Ottiene la latenza corrente
     * @return Latenza in millisecondi
     */
    uint32_t getCurrentLatency() const;
    
    /**
     * @brief Registra un nuovo nodo nella rete mesh
     * @param nodeId ID del nodo
     * @param role Ruolo del nodo
     * @param address Indirizzo Bluetooth (opzionale)
     * @return true se la registrazione è avvenuta con successo, false altrimenti
     */
    bool registerNode(const std::string& nodeId, NodeRole role, 
                     const std::optional<std::string>& address = std::nullopt);
    
    /**
     * @brief Ottiene tutti i nodi attivi
     * @return Vettore di ID dei nodi attivi
     */
    std::vector<std::string> getActiveNodes() const;
    
    /**
     * @brief Verifica se il nodo è sincronizzato
     * @return true se il nodo è sincronizzato, false altrimenti
     */
    bool isSynchronized() const;
    
private:
    /// Configurazione del nodo
    SaberConfig config;
    
    /// Rete mesh per gestione dei nodi
    std::unique_ptr<MeshNetwork> meshNetwork;
    
    /// Manager per la sincronizzazione
    std::shared_ptr<SyncManager> syncManager;
    
    /// Sincronizzatore audio
    std::unique_ptr<AudioSync> audioSync;
    
    /// Thread per il runtime asincrono
    std::unique_ptr<std::thread> runtimeThread;
    
    /// Flag per il thread di runtime
    bool running;
    
    /// Mutex per proteggere l'accesso concorrente
    mutable std::mutex protocolMutex;
};

/**
 * @brief Funzione principale per l'inizializzazione di SABER in modalità Master (UCB)
 * @param nodeId ID del nodo (opzionale)
 * @param btAddress Indirizzo Bluetooth (opzionale)
 * @return Puntatore unico all'istanza del protocollo SABER
 */
std::unique_ptr<SaberProtocol> startMaster(const std::optional<std::string>& nodeId = std::nullopt, 
                         const std::optional<std::string>& btAddress = std::nullopt);

/**
 * @brief Funzione principale per l'inizializzazione di SABER in modalità Repeater
 * @param nodeId ID del nodo (opzionale)
 * @param btAddress Indirizzo Bluetooth (opzionale)
 * @return Puntatore unico all'istanza del protocollo SABER
 */
std::unique_ptr<SaberProtocol> startRepeater(const std::optional<std::string>& nodeId = std::nullopt,
                           const std::optional<std::string>& btAddress = std::nullopt);

/**
 * @brief Funzione principale per l'inizializzazione di SABER in modalità Sink (ricevitore)
 * @param nodeId ID del nodo (opzionale)
 * @param btAddress Indirizzo Bluetooth (opzionale)
 * @param isMusic Flag che indica se l'audio è musicale (true) o vocale (false)
 * @return Puntatore unico all'istanza del protocollo SABER
 */
std::unique_ptr<SaberProtocol> startSink(const std::optional<std::string>& nodeId = std::nullopt,
                       const std::optional<std::string>& btAddress = std::nullopt,
                       bool isMusic = true);

} // namespace saber

#endif // SABER_PROTOCOL_H
