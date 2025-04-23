//! Modulo di crittografia per il protocollo SABER
//! Fornisce funzionalità di cifratura e firma per proteggere la rete mesh

use std::error::Error;
use std::fmt;
use std::time::{SystemTime, UNIX_EPOCH};

// Importiamo le librerie di crittografia necessarie
use aes_gcm::{Aes256Gcm, Key, Nonce};
use aes_gcm::aead::Aead;
use aes_gcm::NewAead;
use ed25519_dalek::{Keypair, PublicKey, Signature, Signer, Verifier};
use rand::rngs::OsRng;

use sha2::{Sha256, Digest};
use x25519_dalek::{PublicKey as X25519PublicKey, StaticSecret};
use hkdf::Hkdf;

/// Errore durante operazioni crittografiche
#[derive(Debug)]
pub enum CryptoError {
    EncryptionError(String),
    DecryptionError(String),
    SignatureError(String),
    VerificationError(String),
    KeyExchangeError(String),
    HashError(String),
}

impl fmt::Display for CryptoError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            CryptoError::EncryptionError(msg) => write!(f, "Encryption error: {}", msg),
            CryptoError::DecryptionError(msg) => write!(f, "Decryption error: {}", msg),
            CryptoError::SignatureError(msg) => write!(f, "Signature error: {}", msg),
            CryptoError::VerificationError(msg) => write!(f, "Verification error: {}", msg),
            CryptoError::KeyExchangeError(msg) => write!(f, "Key exchange error: {}", msg),
            CryptoError::HashError(msg) => write!(f, "Hash error: {}", msg),
        }
    }
}

impl Error for CryptoError {}

/// Risultato delle operazioni crittografiche
pub type CryptoResult<T> = Result<T, CryptoError>;

/// Struct per gestire la crittografia della rete mesh
use pyo3::prelude::*;

#[pyclass]
pub struct MeshCrypto {
    /// Chiave principale della rete (condivisa tra i nodi)
    network_key: [u8; 32],
    /// Coppia di chiavi per la firma ed autenticazione dei messaggi
    signing_keys: Keypair,
    /// Coppia di chiavi per lo scambio di chiavi (key exchange)
    exchange_secret: StaticSecret,
    exchange_public: X25519PublicKey,
    /// Chiavi note di altri nodi (ID nodo -> chiave pubblica)
    known_public_keys: std::collections::HashMap<String, PublicKey>,
    /// Contatore per i nonce incrementali (per evitare replay attacks)
    nonce_counter: u64,
}

impl MeshCrypto {
    /// Crea una nuova istanza di MeshCrypto
    pub fn new() -> Self {
        // Genero una chiave casuale per la rete
        let mut csprng = OsRng {};
        let mut network_key = [0u8; 32];
        rand::RngCore::fill_bytes(&mut csprng, &mut network_key);
        
        // Genero le chiavi di firma
        let signing_keys = Keypair::generate(&mut csprng);
        
        // Genero le chiavi per lo scambio
        let exchange_secret = StaticSecret::new(&mut csprng);
        let exchange_public = X25519PublicKey::from(&exchange_secret);
        
        Self {
            network_key,
            signing_keys,
            exchange_secret,
            exchange_public,
            known_public_keys: std::collections::HashMap::new(),
            nonce_counter: 0,
        }
    }
    
    /// Crea un'istanza con una chiave di rete specifica
    pub fn with_network_key(network_key: [u8; 32]) -> Self {
        let mut crypto = Self::new();
        crypto.network_key = network_key;
        crypto
    }
    
    /// Ottiene il timestamp corrente in millisecondi
    fn current_timestamp() -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .expect("SystemTime before UNIX EPOCH!")
            .as_millis() as u64
    }
    
    /// Genera un nonce unico basato sul timestamp e un contatore
    fn generate_nonce(&mut self) -> [u8; 12] {
        self.nonce_counter += 1;
        let timestamp = Self::current_timestamp();
        
        // Combino timestamp e contatore per creare un nonce unico
        let mut nonce = [0u8; 12];
        nonce[0..8].copy_from_slice(&timestamp.to_le_bytes());
        nonce[8..12].copy_from_slice(&(self.nonce_counter % u32::MAX as u64).to_le_bytes());
        
        nonce
    }
    
    /// Cifra un payload utilizzando AES-256-GCM
    pub fn encrypt(&mut self, payload: &[u8]) -> CryptoResult<Vec<u8>> {
        // Creo la chiave AES dalla chiave di rete
        let key = Key::from_slice(&self.network_key);
        let cipher = Aes256Gcm::new(key);
        
        // Genero un nonce unico
        let nonce_bytes = self.generate_nonce();
        let nonce = Nonce::from_slice(&nonce_bytes);
        
        // Cifro il payload
        match cipher.encrypt(nonce, payload) {
            Ok(ciphertext) => {
                // Prependo il nonce al testo cifrato
                let mut result = Vec::with_capacity(nonce_bytes.len() + ciphertext.len());
                result.extend_from_slice(&nonce_bytes);
                result.extend_from_slice(&ciphertext);
                Ok(result)
            },
            Err(e) => Err(CryptoError::EncryptionError(e.to_string())),
        }
    }
    
    /// Decifra un payload cifrato con AES-256-GCM
    pub fn decrypt(&self, encrypted_data: &[u8]) -> CryptoResult<Vec<u8>> {
        if encrypted_data.len() < 12 {
            return Err(CryptoError::DecryptionError("Input too short".to_string()));
        }
        
        // Estraggo il nonce e il testo cifrato
        let nonce_bytes = &encrypted_data[0..12];
        let ciphertext = &encrypted_data[12..];
        
        // Creo la chiave AES dalla chiave di rete
        let key = Key::from_slice(&self.network_key);
        let cipher = Aes256Gcm::new(key);
        
        // Decifro il payload
        let nonce = Nonce::from_slice(nonce_bytes);
        match cipher.decrypt(nonce, ciphertext) {
            Ok(plaintext) => Ok(plaintext),
            Err(e) => Err(CryptoError::DecryptionError(e.to_string())),
        }
    }
    
    /// Firma un messaggio con la chiave privata del nodo
    pub fn sign(&self, message: &[u8]) -> CryptoResult<Signature> {
        match self.signing_keys.sign(message) {
            signature => Ok(signature),
        }
    }
    
    /// Verifica la firma di un messaggio con la chiave pubblica del mittente
    pub fn verify(&self, node_id: &str, message: &[u8], signature: &Signature) -> CryptoResult<bool> {
        // Cerco la chiave pubblica del nodo
        match self.known_public_keys.get(node_id) {
            Some(public_key) => {
                // Verifico la firma
                match public_key.verify(message, signature) {
                    Ok(_) => Ok(true),
                    Err(_) => Ok(false),
                }
            },
            None => Err(CryptoError::VerificationError(format!("Unknown node: {}", node_id))),
        }
    }
    
    /// Registra la chiave pubblica di un nodo
    pub fn register_node_key(&mut self, node_id: String, public_key: PublicKey) {
        self.known_public_keys.insert(node_id, public_key);
    }
    
    /// Calcola l'hash SHA-256 di un messaggio
    pub fn hash(&self, data: &[u8]) -> CryptoResult<[u8; 32]> {
        let mut hasher = Sha256::new();
        hasher.update(data);
        
        let result = hasher.finalize();
        let mut hash_bytes = [0u8; 32];
        hash_bytes.copy_from_slice(&result);
        
        Ok(hash_bytes)
    }
    
    /// Esegue uno scambio di chiavi con un altro nodo usando Diffie-Hellman X25519
    pub fn key_exchange(&self, peer_public: &[u8]) -> CryptoResult<[u8; 32]> {
        if peer_public.len() != 32 {
            return Err(CryptoError::KeyExchangeError("Invalid public key length".to_string()));
        }
        
        // Converto i bytes nella chiave pubblica X25519
        let mut public_bytes = [0u8; 32];
        public_bytes.copy_from_slice(peer_public);
        
        let peer_public_key = match X25519PublicKey::from(public_bytes) {
            pk => pk,
        };
        
        // Calcolo la chiave condivisa
        let shared_secret = self.exchange_secret.diffie_hellman(&peer_public_key);
        
        // Uso HKDF per derivare la chiave finale
        let h = Hkdf::<Sha256>::new(None, shared_secret.as_bytes());
        let mut okm = [0u8; 32];
        h.expand(b"SABER-PROTOCOL-KEY", &mut okm)
            .map_err(|e| CryptoError::KeyExchangeError(e.to_string()))?;
        
        Ok(okm)
    }
    
    /// Ottiene la chiave pubblica per la firma
    pub fn get_public_key(&self) -> PublicKey {
        self.signing_keys.public
    }
    
    /// Ottiene la chiave pubblica per lo scambio
    pub fn get_exchange_public_key(&self) -> [u8; 32] {
        self.exchange_public.to_bytes()
    }
    
    /// Genera un token di sicurezza con data di scadenza
    pub fn generate_security_token(&mut self, node_id: &str, ttl_seconds: u64) -> CryptoResult<Vec<u8>> {
        // Creo un token con ID nodo, timestamp e scadenza
        let timestamp = Self::current_timestamp();
        let expiry = timestamp + (ttl_seconds * 1000); // Converto i secondi in ms
        
        // Creo il payload del token
        let mut token_data = Vec::new();
        token_data.extend_from_slice(node_id.as_bytes());
        token_data.extend_from_slice(&timestamp.to_le_bytes());
        token_data.extend_from_slice(&expiry.to_le_bytes());
        
        // Firmo il token
        let signature = self.sign(&token_data)?;
        
        // Aggiungo la firma al token
        token_data.extend_from_slice(&signature.to_bytes());
        
        // Cifro il token
        self.encrypt(&token_data)
    }
    
    /// Verifica un token di sicurezza
    pub fn verify_security_token(&self, token: &[u8]) -> CryptoResult<(String, u64)> {
        // Decifro il token
        let decrypted = self.decrypt(token)?;
        
        if decrypted.len() < 8 + 8 + 64 {  // node_id + timestamp + expiry + signature
            return Err(CryptoError::VerificationError("Invalid token format".to_string()));
        }
        
        // Estraggo la firma
        let signature_bytes = &decrypted[decrypted.len() - 64..];
        let data = &decrypted[..decrypted.len() - 64];
        
        // Ricostruisco la firma
        let signature = Signature::from_bytes(signature_bytes)
            .map_err(|e| CryptoError::VerificationError(e.to_string()))?;
        
        // Estraggo i campi dal token
        let expiry_start = data.len() - 8;
        let timestamp_start = expiry_start - 8;
        
        let node_id_bytes = &data[..timestamp_start];
        let node_id = String::from_utf8_lossy(node_id_bytes).to_string();
        
        let mut expiry_bytes = [0u8; 8];
        expiry_bytes.copy_from_slice(&data[expiry_start..expiry_start + 8]);
        let expiry = u64::from_le_bytes(expiry_bytes);
        
        // Verifico la scadenza
        let current = Self::current_timestamp();
        if current > expiry {
            return Err(CryptoError::VerificationError("Token expired".to_string()));
        }
        
        // Verifico la firma
        if let Some(public_key) = self.known_public_keys.get(&node_id) {
            match public_key.verify(data, &signature) {
                Ok(_) => Ok((node_id, expiry)),
                Err(_) => Err(CryptoError::VerificationError("Invalid signature".to_string())),
            }
        } else {
            Err(CryptoError::VerificationError(format!("Unknown node: {}", node_id)))
        }
    }
}

/// Test unitari per il modulo crypto
#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_encryption_decryption() {
        let mut crypto = MeshCrypto::new();
        let data = b"Test secure audio packet";
        
        let encrypted = crypto.encrypt(data).unwrap();
        let decrypted = crypto.decrypt(&encrypted).unwrap();
        
        assert_eq!(data.to_vec(), decrypted);
    }
    
    #[test]
    fn test_signature_verification() {
        let crypto = MeshCrypto::new();
        let message = b"Secure mesh message";
        
        let signature = crypto.sign(message).unwrap();
        
        // Registro la chiave del nodo stesso per il test
        let mut crypto2 = MeshCrypto::new();
        crypto2.register_node_key("self".to_string(), crypto.get_public_key());
        
        let verified = crypto2.verify("self", message, &signature).unwrap();
        assert!(verified);
    }
    
    #[test]
    fn test_key_exchange() {
        let crypto1 = MeshCrypto::new();
        let crypto2 = MeshCrypto::new();
        
        let pubkey1 = crypto1.get_exchange_public_key();
        let pubkey2 = crypto2.get_exchange_public_key();
        
        let shared1 = crypto1.key_exchange(&pubkey2).unwrap();
        let shared2 = crypto2.key_exchange(&pubkey1).unwrap();
        
        assert_eq!(shared1, shared2);
    }
    
    #[test]
    fn test_security_token() {
        let mut crypto1 = MeshCrypto::new();
        let public_key = crypto1.get_public_key();
        
        let mut crypto2 = MeshCrypto::new();
        crypto2.register_node_key("test-node".to_string(), public_key);
        
        // Genero un token valido per 60 secondi
        let token = crypto1.generate_security_token("test-node", 60).unwrap();
        
        // Condivido la chiave di rete con crypto2
        crypto2.network_key = crypto1.network_key;
        
        // Verifico il token
        let (node_id, _) = crypto2.verify_security_token(&token).unwrap();
        assert_eq!(node_id, "test-node");
    }
}
