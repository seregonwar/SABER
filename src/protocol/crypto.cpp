#include "crypto.h"

#include <chrono>
#include <cstring>
#include <random>
#include <stdexcept>

// Utilizziamo OpenSSL per le operazioni crittografiche
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

// Utilizziamo libsodium per Ed25519 e X25519
#include <sodium.h>

namespace saber {

// Implementazione di CryptoError
CryptoError::CryptoError(Type type, const std::string& message)
    : std::runtime_error(message), type(type) {}

CryptoError::Type CryptoError::getType() const {
    return type;
}

// Implementazione di MeshCrypto
MeshCrypto::MeshCrypto() 
    : signingKeys(std::make_unique<SigningKeys>()),
      exchangeKeys(std::make_unique<ExchangeKeys>()),
      nonceCounter(0) {
    
    // Inizializza libsodium se necessario
    if (sodium_init() < 0) {
        throw CryptoError(CryptoError::Type::Encryption, "Impossibile inizializzare libsodium");
    }
    
    // Genera una chiave casuale per la rete
    RAND_bytes(networkKey.data(), networkKey.size());
    
    // Genera le chiavi di firma Ed25519
    crypto_sign_keypair(signingKeys->publicKey, signingKeys->secretKey);
    
    // Genera le chiavi per lo scambio X25519
    crypto_box_keypair(exchangeKeys->publicKey, exchangeKeys->secretKey);
}

MeshCrypto MeshCrypto::withNetworkKey(const std::array<uint8_t, 32>& networkKey) {
    MeshCrypto crypto;
    crypto.networkKey = networkKey;
    return crypto;
}

uint64_t MeshCrypto::currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

std::array<uint8_t, 12> MeshCrypto::generateNonce() {
    nonceCounter++;
    uint64_t timestamp = currentTimestamp();
    
    std::array<uint8_t, 12> nonce;
    std::memcpy(nonce.data(), &timestamp, 8);
    std::memcpy(nonce.data() + 8, &nonceCounter, 4);
    
    return nonce;
}

std::vector<uint8_t> MeshCrypto::encrypt(const std::vector<uint8_t>& payload) {
    // Genera un nonce unico
    auto nonce = generateNonce();
    
    // Prepara il contesto EVP per AES-GCM
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw CryptoError(CryptoError::Type::Encryption, "Impossibile creare il contesto di cifratura");
    }
    
    // Inizializza la cifratura
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, networkKey.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Encryption, "Impossibile inizializzare la cifratura");
    }
    
    // Alloca spazio per il testo cifrato
    std::vector<uint8_t> ciphertext(payload.size() + EVP_CIPHER_CTX_block_size(ctx));
    int len = 0;
    
    // Cifra il payload
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, payload.data(), payload.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Encryption, "Errore durante la cifratura");
    }
    
    int ciphertext_len = len;
    
    // Finalizza la cifratura
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Encryption, "Errore durante la finalizzazione della cifratura");
    }
    
    ciphertext_len += len;
    ciphertext.resize(ciphertext_len);
    
    // Ottieni il tag di autenticazione
    std::array<uint8_t, 16> tag;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, tag.size(), tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Encryption, "Impossibile ottenere il tag di autenticazione");
    }
    
    EVP_CIPHER_CTX_free(ctx);
    
    // Prepara il risultato: nonce + ciphertext + tag
    std::vector<uint8_t> result;
    result.reserve(nonce.size() + ciphertext.size() + tag.size());
    
    // Aggiungi il nonce
    result.insert(result.end(), nonce.begin(), nonce.end());
    
    // Aggiungi il testo cifrato
    result.insert(result.end(), ciphertext.begin(), ciphertext.end());
    
    // Aggiungi il tag
    result.insert(result.end(), tag.begin(), tag.end());
    
    return result;
}

std::vector<uint8_t> MeshCrypto::decrypt(const std::vector<uint8_t>& encryptedData) {
    if (encryptedData.size() < 12 + 16) { // nonce + tag minimo
        throw CryptoError(CryptoError::Type::Decryption, "Dati cifrati troppo corti");
    }
    
    // Estrai nonce, ciphertext e tag
    std::array<uint8_t, 12> nonce;
    std::copy_n(encryptedData.begin(), nonce.size(), nonce.begin());
    
    std::array<uint8_t, 16> tag;
    std::copy_n(encryptedData.end() - tag.size(), tag.size(), tag.begin());
    
    std::vector<uint8_t> ciphertext(encryptedData.begin() + nonce.size(), 
                                   encryptedData.end() - tag.size());
    
    // Prepara il contesto EVP per AES-GCM
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        throw CryptoError(CryptoError::Type::Decryption, "Impossibile creare il contesto di decifratura");
    }
    
    // Inizializza la decifratura
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, networkKey.data(), nonce.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Decryption, "Impossibile inizializzare la decifratura");
    }
    
    // Imposta il tag di autenticazione
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, tag.size(), tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Decryption, "Impossibile impostare il tag di autenticazione");
    }
    
    // Alloca spazio per il testo in chiaro
    std::vector<uint8_t> plaintext(ciphertext.size());
    int len = 0;
    
    // Decifra il ciphertext
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), ciphertext.size()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        throw CryptoError(CryptoError::Type::Decryption, "Errore durante la decifratura");
    }
    
    int plaintext_len = len;
    
    // Finalizza la decifratura e verifica il tag
    int ret = EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    
    if (ret != 1) {
        throw CryptoError(CryptoError::Type::Decryption, "Verifica dell'autenticazione fallita");
    }
    
    plaintext_len += len;
    plaintext.resize(plaintext_len);
    
    return plaintext;
}

std::vector<uint8_t> MeshCrypto::sign(const std::vector<uint8_t>& message) {
    std::vector<uint8_t> signature(crypto_sign_BYTES);
    unsigned long long signatureLen;
    
    if (crypto_sign_detached(signature.data(), &signatureLen, 
                            message.data(), message.size(), 
                            signingKeys->secretKey) != 0) {
        throw CryptoError(CryptoError::Type::Signature, "Errore durante la firma del messaggio");
    }
    
    signature.resize(signatureLen);
    return signature;
}

bool MeshCrypto::verify(const std::string& nodeId, const std::vector<uint8_t>& message, 
                       const std::vector<uint8_t>& signature) {
    auto it = knownPublicKeys.find(nodeId);
    if (it == knownPublicKeys.end()) {
        throw CryptoError(CryptoError::Type::Verification, 
                         "Nodo sconosciuto: " + nodeId);
    }
    
    const auto& publicKey = it->second;
    
    if (publicKey.size() != crypto_sign_PUBLICKEYBYTES) {
        throw CryptoError(CryptoError::Type::Verification, 
                         "Formato chiave pubblica non valido");
    }
    
    if (signature.size() != crypto_sign_BYTES) {
        throw CryptoError(CryptoError::Type::Verification, 
                         "Formato firma non valido");
    }
    
    return crypto_sign_verify_detached(signature.data(), 
                                      message.data(), message.size(), 
                                      publicKey.data()) == 0;
}

void MeshCrypto::registerNodeKey(const std::string& nodeId, const std::vector<uint8_t>& publicKey) {
    knownPublicKeys[nodeId] = publicKey;
}

std::array<uint8_t, 32> MeshCrypto::hash(const std::vector<uint8_t>& data) {
    std::array<uint8_t, 32> hashResult;
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.data(), data.size());
    SHA256_Final(hashResult.data(), &sha256);
    
    return hashResult;
}

std::array<uint8_t, 32> MeshCrypto::keyExchange(const std::vector<uint8_t>& peerPublic) {
    if (peerPublic.size() != crypto_scalarmult_BYTES) {
        throw CryptoError(CryptoError::Type::KeyExchange, 
                         "Lunghezza chiave pubblica non valida");
    }
    
    std::array<uint8_t, 32> sharedSecret;
    
    if (crypto_scalarmult(sharedSecret.data(), 
                         exchangeKeys->secretKey, 
                         peerPublic.data()) != 0) {
        throw CryptoError(CryptoError::Type::KeyExchange, 
                         "Errore durante lo scambio di chiavi");
    }
    
    // Derivazione della chiave con HKDF
    std::array<uint8_t, 32> derivedKey;
    
    // Utilizziamo HMAC-SHA256 come funzione PRF per HKDF
    unsigned int keyLen = derivedKey.size();
    HMAC(EVP_sha256(), "SABER-PROTOCOL-KEY", 17, 
         sharedSecret.data(), sharedSecret.size(), 
         derivedKey.data(), &keyLen);
    
    return derivedKey;
}

std::vector<uint8_t> MeshCrypto::getPublicKey() const {
    return std::vector<uint8_t>(signingKeys->publicKey, 
                               signingKeys->publicKey + crypto_sign_PUBLICKEYBYTES);
}

std::array<uint8_t, 32> MeshCrypto::getExchangePublicKey() const {
    std::array<uint8_t, 32> publicKey;
    std::memcpy(publicKey.data(), exchangeKeys->publicKey, publicKey.size());
    return publicKey;
}

std::vector<uint8_t> MeshCrypto::generateSecurityToken(const std::string& nodeId, uint64_t ttlSeconds) {
    // Crea un token con ID nodo, timestamp e scadenza
    uint64_t timestamp = currentTimestamp();
    uint64_t expiry = timestamp + (ttlSeconds * 1000); // Converto i secondi in ms
    
    // Crea il payload del token
    std::vector<uint8_t> tokenData;
    
    // Aggiungi l'ID del nodo
    tokenData.insert(tokenData.end(), nodeId.begin(), nodeId.end());
    
    // Aggiungi il timestamp
    const uint8_t* timestampBytes = reinterpret_cast<const uint8_t*>(&timestamp);
    tokenData.insert(tokenData.end(), timestampBytes, timestampBytes + sizeof(timestamp));
    
    // Aggiungi la scadenza
    const uint8_t* expiryBytes = reinterpret_cast<const uint8_t*>(&expiry);
    tokenData.insert(tokenData.end(), expiryBytes, expiryBytes + sizeof(expiry));
    
    // Firma il token
    auto signature = sign(tokenData);
    
    // Aggiungi la firma al token
    tokenData.insert(tokenData.end(), signature.begin(), signature.end());
    
    // Cifra il token
    return encrypt(tokenData);
}

std::pair<std::string, uint64_t> MeshCrypto::verifySecurityToken(const std::vector<uint8_t>& token) {
    // Decifra il token
    auto decrypted = decrypt(token);
    
    const size_t minSize = 8 + 8 + crypto_sign_BYTES; // timestamp + expiry + signature
    if (decrypted.size() < minSize) {
        throw CryptoError(CryptoError::Type::Verification, "Formato token non valido");
    }
    
    // Estrai la firma
    std::vector<uint8_t> signature(decrypted.end() - crypto_sign_BYTES, decrypted.end());
    std::vector<uint8_t> data(decrypted.begin(), decrypted.end() - crypto_sign_BYTES);
    
    // Estrai i campi dal token
    size_t nodeIdSize = data.size() - 16; // 8 byte timestamp + 8 byte expiry
    std::string nodeId(data.begin(), data.begin() + nodeIdSize);
    
    uint64_t expiry;
    std::memcpy(&expiry, data.data() + nodeIdSize + 8, sizeof(expiry));
    
    // Verifica la scadenza
    uint64_t current = currentTimestamp();
    if (current > expiry) {
        throw CryptoError(CryptoError::Type::Verification, "Token scaduto");
    }
    
    // Verifica la firma
    if (!verify(nodeId, data, signature)) {
        throw CryptoError(CryptoError::Type::Verification, "Firma non valida");
    }
    
    return {nodeId, expiry};
}

} // namespace saber
