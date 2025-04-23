// Entry point principale per il modulo di protocollo SABER
// Basato sul modello descritto in STRUCTURE.md e PAPER.md

mod mesh;
mod sync;
mod crypto;

use std::sync::Arc;
use tokio::runtime::Runtime;

use mesh::{MeshNetwork, Node, NodeRole};
use sync::{SyncManager, AudioSync};

/// Configurazione per il nodo SABER
pub struct SaberConfig {
    /// ID univoco del nodo
    pub node_id: String,
    /// Ruolo del nodo nella rete
    pub role: NodeRole,
    /// Indirizzo Bluetooth (opzionale)
    pub bt_address: Option<String>,
    /// Flag che indica se il nodo riproduce audio musicale (48kHz) o vocale (16kHz)
    pub is_music_mode: bool,
}

impl Default for SaberConfig {
    fn default() -> Self {
        SaberConfig {
            node_id: format!("node-{}", uuid::Uuid::new_v4().to_string()[..8].to_string()),
            role: NodeRole::Sink,
            bt_address: None,
            is_music_mode: true,
        }
    }
}

/// Gestore principale del protocollo SABER
pub struct SaberProtocol {
    /// Configurazione del nodo
    config: SaberConfig,
    /// Rete mesh per gestione dei nodi
    mesh_network: Option<MeshNetwork>,
    /// Manager per la sincronizzazione
    sync_manager: Arc<SyncManager>,
    /// Sincronizzatore audio
    audio_sync: Option<AudioSync>,
    /// Runtime asincrono Tokio
    runtime: Runtime,
}

impl SaberProtocol {
    /// Crea una nuova istanza del protocollo SABER
    pub fn new(config: SaberConfig) -> Self {
        let sync_manager = Arc::new(SyncManager::new());
        
        // Inizializzo il runtime Tokio per le operazioni asincrone
        let runtime = Runtime::new().expect("Impossibile creare il runtime Tokio");
        
        SaberProtocol {
            config,
            mesh_network: None,
            sync_manager,
            audio_sync: None,
            runtime,
        }
    }
    
    /// Inizializza e avvia il protocollo
    pub fn initialize(&mut self) -> Result<(), String> {
        println!("Inizializzazione SABER Protocol con ID {}", self.config.node_id);
        
        // Creazione del nodo locale per la rete mesh
        let local_node = Node::new(
            self.config.node_id.clone(),
            self.config.role.clone(),
            self.config.bt_address.clone(),
        );
        
        // Creazione della rete mesh
        let mut mesh_network = MeshNetwork::new(local_node);
        
        // Avvio mesh network in modo asincrono
        self.runtime.block_on(async {
            mesh_network.start().await
        }).map_err(|e| format!("Errore durante l'avvio della rete mesh: {}", e))?;
        
        self.mesh_network = Some(mesh_network);
        
        // Inizializzazione del sincronizzatore audio
        let audio_sync = AudioSync::new(
            self.sync_manager.clone(),
            self.config.is_music_mode,
        );
        
        self.audio_sync = Some(audio_sync);
        
        println!("Protocollo SABER inizializzato correttamente");
        Ok(())
    }
    
    /// Ottiene il manager di sincronizzazione
    pub fn get_sync_manager(&self) -> Arc<SyncManager> {
        self.sync_manager.clone()
    }
    
    /// Avvia la riproduzione audio sincronizzata
    pub fn start_audio_playback(&mut self) -> Result<(), String> {
        if let Some(audio_sync) = &mut self.audio_sync {
            audio_sync.start_playback()?;
            println!("Avvio riproduzione audio sincronizzata");
            Ok(())
        } else {
            Err("Sincronizzatore audio non inizializzato".to_string())
        }
    }
    
    /// Ferma la riproduzione audio
    pub fn stop_audio_playback(&mut self) -> Result<(), String> {
        if let Some(audio_sync) = &mut self.audio_sync {
            audio_sync.stop_playback();
            println!("Arresto riproduzione audio");
            Ok(())
        } else {
            Err("Sincronizzatore audio non inizializzato".to_string())
        }
    }
    
    /// Aggiorna lo stato di sincronizzazione con un beacon temporale
    pub fn update_time_sync(&self, master_time: u64) -> Result<(), String> {
        self.sync_manager.handle_time_beacon(master_time)
    }
    
    /// Ottiene la latenza corrente
    pub fn get_current_latency(&self) -> u32 {
        if let Some(audio_sync) = &self.audio_sync {
            audio_sync.get_current_latency()
        } else {
            0
        }
    }
    
    /// Registra un nuovo nodo nella rete mesh
    pub fn register_node(&self, node_id: String, role: NodeRole, address: Option<String>) -> Result<(), String> {
        if let Some(mesh) = &self.mesh_network {
            let node = Node::new(node_id, role);
            mesh.register_node(node);
            Ok(())
        } else {
            Err("Rete mesh non inizializzata".to_string())
        }
    }
    
    /// Ottiene tutti i nodi attivi
    pub fn get_active_nodes(&self) -> Result<Vec<String>, String> {
        if let Some(mesh) = &self.mesh_network {
            let nodes = mesh.get_active_nodes();
            Ok(nodes.iter().map(|n| n.id.clone()).collect())
        } else {
            Err("Rete mesh non inizializzata".to_string())
        }
    }
    
    /// Verifica se il nodo è sincronizzato
    pub fn is_synchronized(&self) -> bool {
        self.sync_manager.is_synchronized()
    }
}

/// Funzione principale per l'inizializzazione di SABER in modalità Master (UCB)
pub fn start_master(node_id: Option<String>, bt_address: Option<String>) -> Result<SaberProtocol, String> {
    let config = SaberConfig {
        node_id: node_id.unwrap_or_else(|| format!("master-{}", uuid::Uuid::new_v4().to_string()[..8].to_string())),
        role: NodeRole::Master,
        bt_address,
        is_music_mode: true,
    };
    
    let mut protocol = SaberProtocol::new(config);
    protocol.initialize()?;
    
    println!("Nodo Master (UCB) avviato");
    Ok(protocol)
}

/// Funzione principale per l'inizializzazione di SABER in modalità Repeater
pub fn start_repeater(node_id: Option<String>, bt_address: Option<String>) -> Result<SaberProtocol, String> {
    let config = SaberConfig {
        node_id: node_id.unwrap_or_else(|| format!("repeater-{}", uuid::Uuid::new_v4().to_string()[..8].to_string())),
        role: NodeRole::Repeater,
        bt_address,
        is_music_mode: true,
    };
    
    let mut protocol = SaberProtocol::new(config);
    protocol.initialize()?;
    
    println!("Nodo Repeater avviato");
    Ok(protocol)
}

/// Funzione principale per l'inizializzazione di SABER in modalità Sink (ricevitore)
pub fn start_sink(node_id: Option<String>, bt_address: Option<String>, is_music: bool) -> Result<SaberProtocol, String> {
    let config = SaberConfig {
        node_id: node_id.unwrap_or_else(|| format!("sink-{}", uuid::Uuid::new_v4().to_string()[..8].to_string())),
        role: NodeRole::Sink,
        bt_address,
        is_music_mode: is_music,
    };
    
    let mut protocol = SaberProtocol::new(config);
    protocol.initialize()?;
    
    println!("Nodo Sink avviato");
    Ok(protocol)
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_protocol_creation() {
        let config = SaberConfig::default();
        let protocol = SaberProtocol::new(config);
        
        assert!(protocol.sync_manager.is_synchronized() == false);
    }
    
    #[test]
    fn test_node_roles() {
        let master = start_master(Some("test-master".to_string()), None).unwrap();
        let repeater = start_repeater(Some("test-repeater".to_string()), None).unwrap();
        let sink = start_sink(Some("test-sink".to_string()), None, true).unwrap();
        
        // Verifico che i ruoli siano stati assegnati correttamente
        assert_eq!(master.config.role, NodeRole::Master);
        assert_eq!(repeater.config.role, NodeRole::Repeater);
        assert_eq!(sink.config.role, NodeRole::Sink);
    }
}
