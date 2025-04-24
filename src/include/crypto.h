#ifndef SABER_CRYPTO_H
#define SABER_CRYPTO_H

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// Definizione delle costanti per le dimensioni delle chiavi
#define CRYPTO_SIGN_PUBLICKEYBYTES 32
#define CRYPTO_SIGN_SECRETKEYBYTES 64
#define CRYPTO_SCALARMULT_BYTES 32
#define CRYPTO_SCALARMULT_SCALARBYTES 32

namespace saber {

/**
 * @brief Errore durante operazioni crittografiche
 */
class CryptoError : public std::runtime_error {
public:
    enum class Type {
        Encryption,
        Decryption,
        Signature,
        Verification,
        KeyExchange,
        Hash
    };
    
    CryptoError(Type type, const std::string& message);
    Type getType() const;
    
private:
    Type type;
};

/**
 * @brief Gestore della crittografia per la rete mesh
 */
class MeshCrypto {
public:
    // Strutture per le chiavi di firma (Ed25519)
    struct SigningKeys {
        unsigned char publicKey[CRYPTO_SIGN_PUBLICKEYBYTES];
        unsigned char secretKey[CRYPTO_SIGN_SECRETKEYBYTES];
    };
    
    // Strutture per le chiavi di scambio (X25519)
    struct ExchangeKeys {
        unsigned char publicKey[CRYPTO_SCALARMULT_BYTES];
        unsigned char secretKey[CRYPTO_SCALARMULT_SCALARBYTES];
    };

    /**
     * @brief Crea una nuova istanza di MeshCrypto con chiave casuale
     */
    MeshCrypto();
    
    /**
     * @brief Crea un'istanza con una chiave di rete specifica
     * @param networkKey Chiave di rete da utilizzare
     * @return Nuova istanza di MeshCrypto
     */
    static MeshCrypto withNetworkKey(const std::array<uint8_t, 32>& networkKey);
    
    /**
     * @brief Cifra un payload utilizzando AES-256-GCM
     * @param payload Dati da cifrare
     * @return Dati cifrati con nonce preposto
     * @throws CryptoError in caso di errore di cifratura
     */
    std::vector<uint8_t> encrypt(const std::vector<uint8_t>& payload);
    
    /**
     * @brief Decifra un payload cifrato con AES-256-GCM
     * @param encryptedData Dati cifrati con nonce preposto
     * @return Dati decifrati
     * @throws CryptoError in caso di errore di decifratura
     */
    std::vector<uint8_t> decrypt(const std::vector<uint8_t>& encryptedData);
    
    /**
     * @brief Firma un messaggio con la chiave privata del nodo
     * @param message Messaggio da firmare
     * @return Firma digitale
     * @throws CryptoError in caso di errore di firma
     */
    std::vector<uint8_t> sign(const std::vector<uint8_t>& message);
    
    /**
     * @brief Verifica la firma di un messaggio con la chiave pubblica del mittente
     * @param nodeId ID del nodo mittente
     * @param message Messaggio originale
     * @param signature Firma da verificare
     * @return true se la firma è valida, false altrimenti
     * @throws CryptoError se il nodo non è conosciuto
     */
    bool verify(const std::string& nodeId, const std::vector<uint8_t>& message, 
                const std::vector<uint8_t>& signature);
    
    /**
     * @brief Registra la chiave pubblica di un nodo
     * @param nodeId ID del nodo
     * @param publicKey Chiave pubblica del nodo
     */
    void registerNodeKey(const std::string& nodeId, const std::vector<uint8_t>& publicKey);
    
    /**
     * @brief Calcola l'hash SHA-256 di un messaggio
     * @param data Dati da hashare
     * @return Hash SHA-256
     * @throws CryptoError in caso di errore di hashing
     */
    std::array<uint8_t, 32> hash(const std::vector<uint8_t>& data);
    
    /**
     * @brief Esegue uno scambio di chiavi con un altro nodo usando Diffie-Hellman X25519
     * @param peerPublic Chiave pubblica del peer
     * @return Chiave condivisa
     * @throws CryptoError in caso di errore nello scambio
     */
    std::array<uint8_t, 32> keyExchange(const std::vector<uint8_t>& peerPublic);
    
    /**
     * @brief Ottiene la chiave pubblica per la firma
     * @return Chiave pubblica
     */
    std::vector<uint8_t> getPublicKey() const;
    
    /**
     * @brief Ottiene la chiave pubblica per lo scambio
     * @return Chiave pubblica X25519
     */
    std::array<uint8_t, 32> getExchangePublicKey() const;
    
    /**
     * @brief Genera un token di sicurezza con data di scadenza
     * @param nodeId ID del nodo
     * @param ttlSeconds Durata di validità in secondi
     * @return Token cifrato
     * @throws CryptoError in caso di errore
     */
    std::vector<uint8_t> generateSecurityToken(const std::string& nodeId, uint64_t ttlSeconds);
    
    /**
     * @brief Verifica un token di sicurezza
     * @param token Token cifrato
     * @return Coppia con ID nodo e timestamp di scadenza
     * @throws CryptoError se il token è invalido o scaduto
     */
    std::pair<std::string, uint64_t> verifySecurityToken(const std::vector<uint8_t>& token);
    
private:
    // Chiave principale della rete
    std::array<uint8_t, 32> networkKey;
    
    // Puntatori alle strutture per le chiavi
    std::unique_ptr<SigningKeys> signingKeys;
    std::unique_ptr<ExchangeKeys> exchangeKeys;
    
    // Chiavi note di altri nodi (ID nodo -> chiave pubblica)
    std::map<std::string, std::vector<uint8_t>> knownPublicKeys;
    
    // Contatore per i nonce incrementali
    uint64_t nonceCounter;
    
    /**
     * @brief Ottiene il timestamp corrente in millisecondi
     * @return Timestamp in millisecondi
     */
    static uint64_t currentTimestamp();
    
    /**
     * @brief Genera un nonce unico basato sul timestamp e un contatore
     * @return Nonce di 12 byte
     */
    std::array<uint8_t, 12> generateNonce();
};

} // namespace saber

#endif // SABER_CRYPTO_H
