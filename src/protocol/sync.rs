// Implementazione del modulo di sincronizzazione per SABER Protocol
// Basato sul modello descritto in STRUCTURE.md e PAPER.md

use std::time::{Instant, SystemTime, UNIX_EPOCH};
use std::sync::{Arc, Mutex};
use std::collections::HashMap;

// Importo il modulo mesh per integrazione con i nodi
// use crate::protocol::mesh::{Node, NodeRole};

/// Struttura per gestire la sincronizzazione temporale tra i dispositivi
use pyo3::prelude::*;

#[pyclass]
pub struct SyncManager {
    /// Offset per sincronizzare l'orologio locale con il master
    time_offset: Arc<Mutex<i64>>,
    /// Timestamp dell'ultimo beacon ricevuto
    last_beacon: Arc<Mutex<Option<Instant>>>,
    /// Mappa delle latenze dei nodi
    node_latencies: Arc<Mutex<HashMap<String, u32>>>,
    /// Flag che indica se il dispositivo è sincronizzato
    is_synced: Arc<Mutex<bool>>,
    /// Jitter massimo tollerato (in ms)
    max_jitter_ms: u32,
}

impl SyncManager {
    /// Crea una nuova istanza del gestore di sincronizzazione
    pub fn new() -> Self {
        SyncManager {
            time_offset: Arc::new(Mutex::new(0)),
            last_beacon: Arc::new(Mutex::new(None)),
            node_latencies: Arc::new(Mutex::new(HashMap::new())),
            is_synced: Arc::new(Mutex::new(false)),
            // Come da PAPER.md sezione 4.2, la tolleranza jitter è < ±5 ms
            max_jitter_ms: 5,
        }
    }

    /// Ottiene il timestamp corrente sincronizzato
    pub fn now(&self) -> u64 {
        let system_time = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64;

        // Applico l'offset di sincronizzazione
        if let Ok(offset) = self.time_offset.lock() {
            if *offset >= 0 {
                system_time + *offset as u64
            } else {
                system_time - (-*offset as u64)
            }
        } else {
            system_time
        }
    }

    /// Gestisce un beacon temporale ricevuto dal master
    pub fn handle_time_beacon(&self, master_time: u64) -> Result<(), String> {
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64;

        // Calcolo l'offset necessario per sincronizzarsi col master
        let calculated_offset = master_time as i64 - now as i64;

        if let Ok(mut offset) = self.time_offset.lock() {
            *offset = calculated_offset;
        } else {
            return Err("Impossibile acquisire il lock sull'offset temporale".to_string());
        }

        // Aggiorno il timestamp dell'ultimo beacon
        if let Ok(mut last) = self.last_beacon.lock() {
            *last = Some(Instant::now());
        } else {
            return Err("Impossibile acquisire il lock sull'ultimo beacon".to_string());
        }

        // Marco il dispositivo come sincronizzato
        if let Ok(mut synced) = self.is_synced.lock() {
            *synced = true;
        }

        Ok(())
    }

    /// Verifica se il nodo è sincronizzato
    pub fn is_synchronized(&self) -> bool {
        // Controllo se abbiamo ricevuto almeno un beacon
        let has_beacon = if let Ok(last) = self.last_beacon.lock() {
            last.is_some()
        } else {
            false
        };

        // Verifico lo stato del flag di sincronizzazione
        let synced = if let Ok(sync) = self.is_synced.lock() {
            *sync
        } else {
            false
        };

        has_beacon && synced
    }

    /// Calcola e registra la latenza di un nodo
    pub fn update_node_latency(&self, node_id: &str, latency: u32) {
        if let Ok(mut latencies) = self.node_latencies.lock() {
            latencies.insert(node_id.to_string(), latency);
        }
    }

    /// Ottiene la latenza media di tutti i nodi
    pub fn get_average_latency(&self) -> Option<f32> {
        if let Ok(latencies) = self.node_latencies.lock() {
            if latencies.is_empty() {
                return None;
            }

            let sum: u32 = latencies.values().sum();
            Some(sum as f32 / latencies.len() as f32)
        } else {
            None
        }
    }

    /// Verifica se un nodo è desincronizzato (jitter oltre la soglia)
    pub fn is_node_out_of_sync(&self, _node_id: &str, reported_time: u64) -> bool {
        let current_time = self.now();
        let time_diff = if current_time > reported_time {
            current_time - reported_time
        } else {
            reported_time - current_time
        };

        // Se la differenza è maggiore del jitter massimo, il nodo è desincronizzato
        time_diff > self.max_jitter_ms as u64
    }

    /// Calcola il buffer necessario per compensare la latenza
    pub fn calculate_buffer_adjustment(&self, node_latency: u32) -> u32 {
        // Imposta un buffer leggermente superiore alla latenza per evitare interruzioni
        // Mantenendo comunque sotto la soglia dei 40ms (sezione 4.1 del PAPER.md)
        let buffer_size = node_latency + 10;
        if buffer_size > 40 {
            40 // Limito al massimo a 40ms come da specifiche
        } else {
            buffer_size
        }
    }

    /// Determina la dimensione del buffer audio ottimale per tutti i nodi
    pub fn get_optimal_buffer_size(&self) -> u32 {
        let avg_latency = self.get_average_latency();
        match avg_latency {
            Some(latency) => self.calculate_buffer_adjustment(latency as u32),
            None => 20, // Valore di default in assenza di misurazioni
        }
    }

    /// Effettua una sincronizzazione di emergenza (quando la connessione BIS è persa)
    pub fn emergency_sync(&self, master_time: u64) -> Result<(), String> {
        // In caso di emergenza, forza la sincronizzazione
        self.handle_time_beacon(master_time)?;
        
        // Reset delle latenze 
        if let Ok(mut latencies) = self.node_latencies.lock() {
            latencies.clear();
        }
        
        Ok(())
    }
}

/// Struttura per la sincronizzazione dell'audio
#[pyclass]
pub struct AudioSync {
    /// Manager di sincronizzazione globale
    sync_manager: Arc<SyncManager>,
    /// Buffer di jitter in millisecondi
    jitter_buffer: u32,
    /// Flag che indica se l'audio è in riproduzione
    is_playing: bool,
    /// Formato audio (sezione 4.1 del PAPER.md)
    sample_rate: u32,
    /// Bitrate in kbps
    bitrate: u32,
}

impl AudioSync {
    /// Crea una nuova istanza del sincronizzatore audio
    pub fn new(sync_manager: Arc<SyncManager>, is_music: bool) -> Self {
        // Configurazione come da PAPER.md sezione 4.1
        let sample_rate = if is_music { 48000 } else { 16000 };
        let bitrate = if is_music { 128 } else { 64 };
        
        AudioSync {
            sync_manager,
            jitter_buffer: 20, // Valore iniziale di default
            is_playing: false,
            sample_rate,
            bitrate,
        }
    }
    
    /// Avvia la riproduzione sincronizzata
    pub fn start_playback(&mut self) -> Result<(), String> {
        if !self.sync_manager.is_synchronized() {
            return Err("Impossibile avviare la riproduzione: dispositivo non sincronizzato".to_string());
        }
        
        // Aggiorno il buffer di jitter in base alle latenze attuali
        self.jitter_buffer = self.sync_manager.get_optimal_buffer_size();
        
        self.is_playing = true;
        println!("Avvio riproduzione con buffer di {}ms", self.jitter_buffer);
        
        Ok(())
    }
    
    /// Interrompe la riproduzione
    pub fn stop_playback(&mut self) {
        self.is_playing = false;
    }
    
    /// Aggiusta il bitrate in base alle condizioni della rete
    pub fn adjust_bitrate(&mut self, network_quality: f32) {
        // network_quality è un valore da 0.0 a 1.0
        if network_quality < 0.5 {
            // Riduco il bitrate in caso di rete debole
            self.bitrate = if self.sample_rate == 48000 { 64 } else { 32 };
        } else {
            // Ripristino il bitrate normale
            self.bitrate = if self.sample_rate == 48000 { 128 } else { 64 };
        }
        
        println!("Bitrate aggiustato a {}kbps", self.bitrate);
    }
    
    /// Ottiene la latenza corrente
    pub fn get_current_latency(&self) -> u32 {
        match self.sync_manager.get_average_latency() {
            Some(latency) => latency as u32,
            None => 0,
        }
    }
    
    /// Verifica se la riproduzione è sincronizzata
    pub fn is_playback_synchronized(&self) -> bool {
        self.sync_manager.is_synchronized() && self.is_playing
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_sync_manager_creation() {
        let manager = SyncManager::new();
        assert_eq!(manager.is_synchronized(), false);
    }
    
    #[test]
    fn test_time_beacon_handling() {
        let manager = SyncManager::new();
        let now = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_millis() as u64;
        
        // Simulo un beacon dal master con un offset di +100ms
        let master_time = now + 100;
        manager.handle_time_beacon(master_time).unwrap();
        
        // Verifico che il manager sia ora sincronizzato
        assert_eq!(manager.is_synchronized(), true);
        
        // Verifico che l'offset sia stato applicato
        // Il timestamp sincronizzato dovrebbe essere circa uguale a master_time
        let synced_time = manager.now();
        let diff = if synced_time > master_time {
            synced_time - master_time
        } else {
            master_time - synced_time
        };
        
        // Tollero una piccola differenza dovuta al tempo di esecuzione del test
        assert!(diff < 5, "Difference too large: {}", diff);
    }
    
    #[test]
    fn test_buffer_calculation() {
        let manager = SyncManager::new();
        
        // Test con 15ms di latenza
        let buffer_15ms = manager.calculate_buffer_adjustment(15);
        assert_eq!(buffer_15ms, 25); // 15 + 10
        
        // Test con 35ms di latenza (dovrebbe essere limitato a 40ms)
        let buffer_35ms = manager.calculate_buffer_adjustment(35);
        assert_eq!(buffer_35ms, 40); // Limitato a 40ms
    }
}
