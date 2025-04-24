#ifndef SABER_MESH_H
#define SABER_MESH_H

#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace saber {

/**
 * @brief Definizione dei ruoli dei nodi nella rete mesh
 */
enum class NodeRole {
    /// UCB - Unità Centrale di Broadcast: emette flussi BIS LE Audio
    Master,
    /// Nodo intermedio che estende la rete mesh
    Repeater,
    /// DS - Dispositivo Sink: riceve e decodifica il flusso LC3
    Sink
};

/**
 * @brief Struttura dati che rappresenta un nodo nella rete mesh
 */
class Node {
public:
    /**
     * @brief Crea un nuovo nodo con i parametri specificati
     * @param id Identificatore univoco del nodo
     * @param role Ruolo del nodo nella rete mesh
     */
    Node(const std::string& id, NodeRole role);
    
    /**
     * @brief Aggiorna il timestamp dell'ultimo ping ricevuto
     */
    void updatePing();
    
    /**
     * @brief Aggiorna lo stato del buffer
     * @param state Percentuale di buffer disponibile (0-100)
     */
    void updateBufferState(uint8_t state);
    
    /**
     * @brief Imposta la latenza misurata
     * @param latency Latenza in millisecondi
     */
    void setLatency(uint32_t latency);
    
    /**
     * @brief Ottiene la latenza attuale
     * @return Latenza in millisecondi
     */
    uint32_t getLatency() const;
    
    /**
     * @brief Controlla se il nodo è attivo (ha inviato un ping recentemente)
     * @return true se il nodo è attivo, false altrimenti
     */
    bool isActive() const;
    
    /// Identificatore univoco del nodo
    std::string id;
    
    /// Ruolo del nodo nella rete mesh
    NodeRole role;
    
private:
    /// Timestamp dell'ultimo ping ricevuto, usato per sincronizzazione
    std::optional<std::chrono::steady_clock::time_point> lastPing;
    
    /// Latenza misurata in millisecondi
    uint32_t latency;
    
    /// Stato del buffer (percentuale disponibile)
    uint8_t bufferState;
};

/**
 * @brief Tipo di messaggio scambiato nella rete mesh
 */
enum class MeshPacketType {
    Ping,
    Command,
    Status,
    TimeBeacon,
    EmergencySync
};

/**
 * @brief Pacchetto mesh
 */
class MeshPacket {
public:
    /**
     * @brief Crea un pacchetto di tipo Ping
     * @param source ID del nodo sorgente
     * @param timestamp Timestamp del ping
     * @return Pacchetto Ping
     */
    static MeshPacket createPing(const std::string& source, uint64_t timestamp);
    
    /**
     * @brief Crea un pacchetto di tipo Command
     * @param cmdType Tipo di comando
     * @param params Parametri del comando
     * @return Pacchetto Command
     */
    static MeshPacket createCommand(const std::string& cmdType, 
                                   const std::map<std::string, std::string>& params);
    
    /**
     * @brief Crea un pacchetto di tipo Status
     * @param nodeId ID del nodo
     * @param buffer Stato del buffer
     * @param latency Latenza misurata
     * @return Pacchetto Status
     */
    static MeshPacket createStatus(const std::string& nodeId, uint8_t buffer, uint32_t latency);
    
    /**
     * @brief Crea un pacchetto di tipo TimeBeacon
     * @param masterTime Tempo del master
     * @return Pacchetto TimeBeacon
     */
    static MeshPacket createTimeBeacon(uint64_t masterTime);
    
    /**
     * @brief Crea un pacchetto di tipo EmergencySync
     * @param masterTime Tempo del master
     * @param targetNodes Nodi target per la sincronizzazione
     * @return Pacchetto EmergencySync
     */
    static MeshPacket createEmergencySync(uint64_t masterTime, 
                                         const std::vector<std::string>& targetNodes);
    
    /**
     * @brief Costruttore di copia
     * @param other Pacchetto da copiare
     */
    MeshPacket(const MeshPacket& other);
    
    /**
     * @brief Operatore di assegnazione
     * @param other Pacchetto da copiare
     * @return Riferimento a questo pacchetto
     */
    MeshPacket& operator=(const MeshPacket& other);
    
    /**
     * @brief Costruttore di spostamento
     * @param other Pacchetto da spostare
     */
    MeshPacket(MeshPacket&& other) noexcept;
    
    /**
     * @brief Operatore di assegnazione per spostamento
     * @param other Pacchetto da spostare
     * @return Riferimento a questo pacchetto
     */
    MeshPacket& operator=(MeshPacket&& other) noexcept;
    
    /**
     * @brief Distruttore
     */
    ~MeshPacket();
    
    /**
     * @brief Ottiene il tipo del pacchetto
     * @return Tipo del pacchetto
     */
    MeshPacketType getType() const;
    
    /**
     * @brief Ottiene i dati del pacchetto Ping
     * @return Coppia con ID sorgente e timestamp
     * @throws std::runtime_error se il pacchetto non è di tipo Ping
     */
    std::pair<std::string, uint64_t> getPingData() const;
    
    /**
     * @brief Ottiene i dati del pacchetto Command
     * @return Coppia con tipo di comando e parametri
     * @throws std::runtime_error se il pacchetto non è di tipo Command
     */
    std::pair<std::string, std::map<std::string, std::string>> getCommandData() const;
    
    /**
     * @brief Ottiene i dati del pacchetto Status
     * @return Tupla con ID nodo, stato buffer e latenza
     * @throws std::runtime_error se il pacchetto non è di tipo Status
     */
    std::tuple<std::string, uint8_t, uint32_t> getStatusData() const;
    
    /**
     * @brief Ottiene i dati del pacchetto TimeBeacon
     * @return Tempo del master
     * @throws std::runtime_error se il pacchetto non è di tipo TimeBeacon
     */
    uint64_t getTimeBeaconData() const;
    
    /**
     * @brief Ottiene i dati del pacchetto EmergencySync
     * @return Coppia con tempo del master e nodi target
     * @throws std::runtime_error se il pacchetto non è di tipo EmergencySync
     */
    std::pair<uint64_t, std::vector<std::string>> getEmergencySyncData() const;
    
private:
    MeshPacket(MeshPacketType type);
    
    MeshPacketType type;
    
    // Dati specifici per ogni tipo di pacchetto
    struct PingData {
        std::string source;
        uint64_t timestamp;
    };
    
    struct CommandData {
        std::string cmdType;
        std::map<std::string, std::string> params;
    };
    
    struct StatusData {
        std::string nodeId;
        uint8_t buffer;
        uint32_t latency;
    };
    
    struct TimeBeaconData {
        uint64_t masterTime;
    };
    
    struct EmergencySyncData {
        uint64_t masterTime;
        std::vector<std::string> targetNodes;
    };
    
    // Utilizziamo std::variant in C++17, ma per semplicità qui usiamo union
    union PacketData {
        PingData ping;
        CommandData command;
        StatusData status;
        TimeBeaconData timeBeacon;
        EmergencySyncData emergencySync;
        
        PacketData() {} // Default constructor
        ~PacketData() {} // Default destructor
    };
    
    // Funzione helper per copiare i dati in base al tipo
    void copyDataFromOther(const MeshPacket& other);
    
    // Funzione helper per distruggere i dati in base al tipo
    void destroyData();
    
    PacketData data;
};

/**
 * @brief Gestore della rete mesh
 */
class MeshNetwork {
public:
    /**
     * @brief Tipo di callback per gestione pacchetti
     */
    using PacketHandler = std::function<void(const MeshPacket&)>;

    /**
     * @brief Crea una nuova istanza della rete mesh
     * @param localNode Nodo locale
     */
    MeshNetwork(const Node& localNode);
    
    /**
     * @brief Distruttore
     */
    ~MeshNetwork();
    
    /**
     * @brief Avvia la rete mesh
     */
    void start();
    
    /**
     * @brief Ferma la rete mesh
     */
    void stop();
    
    /**
     * @brief Invia un pacchetto nella rete mesh
     * @param packet Pacchetto da inviare
     */
    void sendPacket(const MeshPacket& packet);
    
    /**
     * @brief Registra un nuovo nodo nella rete
     * @param nodeId ID del nodo da registrare
     * @param role Ruolo del nodo nella rete
     */
    void registerNode(const std::string& nodeId, NodeRole role);
    
    /**
     * @brief Aggiorna lo stato di un nodo
     * @param nodeId ID del nodo da aggiornare
     * @param bufferState Stato del buffer del nodo
     * @param latency Latenza del nodo
     */
    void updateNodeStatus(const std::string& nodeId, uint8_t bufferState, uint32_t latency);
    
    /**
     * @brief Ottiene la lista dei nodi attivi
     * @return Vettore di ID dei nodi attivi
     */
    std::vector<std::string> getActiveNodes() const;
    
    /**
     * @brief Imposta il gestore di pacchetti
     * @param handler Funzione di callback per gestire i pacchetti
     */
    void setPacketHandler(PacketHandler handler);
    
private:
    /// Nodo locale
    Node localNode;
    
    /// Mappa dei nodi connessi
    std::map<std::string, Node> nodes;
    
    /// Flag per il thread di gestione pacchetti
    bool running;
    
    /// Thread di gestione pacchetti
    std::unique_ptr<std::thread> networkThread;
    
    /// Coda di pacchetti da processare
    std::vector<MeshPacket> packetQueue;
    
    /// Mutex per la rete
    mutable std::mutex networkMutex;
    
    /// Mutex per la coda di pacchetti
    mutable std::mutex queueMutex;
    
    /// Condition variable per la coda di pacchetti
    std::condition_variable queueCondition;
    
    /// Handler per i pacchetti
    PacketHandler packetHandler;
    
    /**
     * @brief Loop principale per la gestione della rete
     */
    void runNetworkLoop();
    
    /**
     * @brief Processa un pacchetto
     * @param packet Pacchetto da processare
     */
    void processPacket(const MeshPacket& packet);
};

/**
 * @brief Funzione di utilità per gestire un pacchetto ricevuto dalla rete
 * @param pkt Pacchetto ricevuto
 */
void handlePacket(const std::vector<uint8_t>& pkt);

} // namespace saber

#endif // SABER_MESH_H
