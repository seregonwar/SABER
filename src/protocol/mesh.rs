// Implementazione del modulo Mesh per SABER Protocol
// Basato sul modello descritto in STRUCTURE.md e PAPER.md

use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::{Duration, Instant};

// Necessario per la comunicazione Bluetooth LE
// Dovrà essere aggiunto come dipendenza in Cargo.toml

use tokio::sync::mpsc;
use tokio::time;

/// Definizioni dei ruoli dei nodi nella rete mesh
use pyo3::prelude::*;

#[pyclass]
#[derive(Debug, Clone, PartialEq)]
pub enum NodeRole {
    /// UCB - Unità Centrale di Broadcast: emette flussi BIS LE Audio
    Master,
    /// Nodo intermedio che estende la rete mesh
    Repeater,
    /// DS - Dispositivo Sink: riceve e decodifica il flusso LC3
    Sink,
}

/// Struttura dati che rappresenta un nodo nella rete mesh
#[pyclass]
#[derive(Debug, Clone)]
pub struct Node {
    /// Identificatore univoco del nodo
    pub id: String,
    /// Ruolo del nodo nella rete mesh
    pub role: NodeRole,
    /// Timestamp dell'ultimo ping ricevuto, usato per sincronizzazione
    last_ping: Option<Instant>,
    /// Latenza misurata in millisecondi
    latency: u32,

    /// Stato del buffer (percentuale disponibile)
    buffer_state: u8,
}

#[pymethods]
impl Node {
    /// Crea un nuovo nodo con i parametri specificati
    #[new]
    pub fn new(id: &str, role: NodeRole) -> Self {
        Node {
            id: id.to_string(),
            role,
            last_ping: None,
            latency: 0,
            buffer_state: 100,
        }
    }

    /// Aggiorna il timestamp dell'ultimo ping ricevuto
    pub fn update_ping(&mut self) {
        self.last_ping = Some(Instant::now());
    }

    /// Aggiorna lo stato del buffer
    pub fn update_buffer_state(&mut self, state: u8) {
        self.buffer_state = state;
    }

    /// Imposta la latenza misurata
    pub fn set_latency(&mut self, latency: u32) {
        self.latency = latency;
    }

    /// Ottiene la latenza attuale
    pub fn get_latency(&self) -> u32 {
        self.latency
    }

    /// Controlla se il nodo è attivo (ha inviato un ping recentemente)
    pub fn is_active(&self) -> bool {
        match self.last_ping {
            Some(time) => time.elapsed() < Duration::from_secs(5),
            None => false,
        }
    }
}

/// Tipo di messaggio scambiato nella rete mesh
#[derive(Debug, Clone)]
pub enum MeshPacket {
    /// Ping per verifica connettività e sincronizzazione
    Ping { source: String, timestamp: u64 },
    /// Comando per controllo riproduzione
    Command { cmd_type: String, params: HashMap<String, String> },
    /// Pacchetto contenente informazioni sullo stato 
    Status { node_id: String, buffer: u8, latency: u32 },
    /// Beacon temporale per sincronizzazione
    TimeBeacon { master_time: u64 },
    /// Pacchetto di emergenza per ri-sincronizzazione
    EmergencySync { master_time: u64, target_nodes: Vec<String> },
}

/// Gestore della rete mesh
pub struct MeshNetwork {
    /// Nodo locale
    local_node: Node,
    /// Mappa dei nodi connessi
    nodes: Arc<Mutex<HashMap<String, Node>>>,
    /// Canale per invio messaggi
    tx: mpsc::Sender<MeshPacket>,
    /// Canale per ricezione messaggi
    rx: Option<mpsc::Receiver<MeshPacket>>,
}

impl MeshNetwork {
    /// Crea una nuova istanza della rete mesh
    pub fn new(local_node: Node) -> Self {
        let (tx, rx) = mpsc::channel(32);
        MeshNetwork {
            local_node,
            nodes: Arc::new(Mutex::new(HashMap::new())),
            tx,
            rx: Some(rx),
        }
    }

    /// Avvia la rete mesh
    pub async fn start(&mut self) -> Result<(), Box<dyn std::error::Error>> {
        println!("Avvio rete mesh con nodo: {:?}", self.local_node);
        
        // Avvia thread separato per gestione ping periodici
        if self.local_node.role == NodeRole::Master {
            let tx_clone = self.tx.clone();
            let node_id = self.local_node.id.clone();
            tokio::spawn(async move {
                loop {
                    // Invia ping ogni 10ms come specificato nel paper (3.3 Sincronizzazione Temporale)
                    let timestamp = std::time::SystemTime::now()
                        .duration_since(std::time::UNIX_EPOCH)
                        .unwrap()
                        .as_millis() as u64;
                    
                    let ping = MeshPacket::Ping {
                        source: node_id.clone(),
                        timestamp,
                    };
                    
                    if let Err(e) = tx_clone.send(ping).await {
                        eprintln!("Errore invio ping: {}", e);
                    }
                    
                    time::sleep(Duration::from_millis(10)).await;
                }
            });
        }
        
        // Loop principale di gestione pacchetti
        let rx = self.rx.take().expect("rx already taken");
        let nodes = self.nodes.clone();
        tokio::spawn(async move {
            Self::packet_handler(rx, nodes).await;
        });
        
        Ok(())
    }
    
    /// Gestisce i pacchetti in arrivo
    async fn packet_handler(
        mut rx: mpsc::Receiver<MeshPacket>,
        nodes: Arc<Mutex<HashMap<String, Node>>>,
    ) {
        while let Some(packet) = rx.recv().await {
            match packet {
                MeshPacket::Ping { source, timestamp } => {
                    // Aggiorna stato del nodo al ping
                    if let Ok(mut nodes_lock) = nodes.lock() {
                        if let Some(node) = nodes_lock.get_mut(&source) {
                            node.update_ping();
                        }
                    }
                    
                    // Calcola latenza basata sul timestamp
                    let now = std::time::SystemTime::now()
                        .duration_since(std::time::UNIX_EPOCH)
                        .unwrap()
                        .as_millis() as u64;
                    
                    let latency = now - timestamp;
                    println!("Ping da nodo {} con latenza {}ms", source, latency);
                }
                
                MeshPacket::Command { cmd_type, params } => {
                    println!("Comando ricevuto: {} con parametri {:?}", cmd_type, params);
                    // Implementazione gestione comandi
                }
                
                MeshPacket::Status { node_id, buffer, latency } => {
                    if let Ok(mut nodes_lock) = nodes.lock() {
                        if let Some(node) = nodes_lock.get_mut(&node_id) {
                            node.update_buffer_state(buffer);
                            node.set_latency(latency);
                        }
                    }
                }
                
                MeshPacket::TimeBeacon { master_time } => {
                    println!("Time beacon ricevuto: {}", master_time);
                    // Implementazione sincronizzazione
                }
                
                MeshPacket::EmergencySync { master_time, target_nodes } => {
                    println!("Sincronizzazione di emergenza: {} per nodi {:?}", 
                             master_time, target_nodes);
                    // Implementazione ri-sincronizzazione
                }
            }
        }
    }
    
    /// Invia un pacchetto nella rete mesh
    pub async fn send_packet(&self, packet: MeshPacket) -> Result<(), mpsc::error::SendError<MeshPacket>> {
        self.tx.send(packet).await
    }
    
    /// Registra un nuovo nodo nella rete
    pub fn register_node(&self, node: Node) {
        if let Ok(mut nodes_lock) = self.nodes.lock() {
            nodes_lock.insert(node.id.clone(), node);
        }
    }
    
    /// Ottiene la lista dei nodi attivi
    pub fn get_active_nodes(&self) -> Vec<Node> {
        if let Ok(nodes_lock) = self.nodes.lock() {
            nodes_lock.values()
                .filter(|node| node.is_active())
                .cloned()
                .collect()
        } else {
            Vec::new()
        }
    }
}

/// Funzione di utilità per gestire un pacchetto ricevuto dalla rete
pub fn handle_packet(pkt: Vec<u8>) {
    // Decodifica + inoltro mesh
    println!("Pacchetto ricevuto: {} bytes", pkt.len());
    // Implementazione da completare con logica di decodifica
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_node_creation() {
        let node = Node::new("device1".to_string(), NodeRole::Sink);
        assert_eq!(node.id, "device1");
        assert_eq!(node.role, NodeRole::Sink);
        assert_eq!(node.is_active(), false);
    }
    
    #[test]
    fn test_node_ping() {
        let mut node = Node::new("device1".to_string(), NodeRole::Sink);
        assert_eq!(node.is_active(), false);
        
        node.update_ping();
        assert_eq!(node.is_active(), true);
    }
}
