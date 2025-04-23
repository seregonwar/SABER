//! Test per il modulo mesh del protocollo SABER
//! Verifica la corretta creazione e gestione della rete mesh tra nodi

use std::time::{Duration, Instant};
use std::thread;
use std::sync::{Arc, Mutex};

// Importiamo i moduli da testare dal crate principale
use saber::mesh::{Node, NodeRole, MeshNetwork, MeshPacket, PacketType};
use saber::main::{SaberProtocol, SaberConfig};

/// Test della creazione di un nodo master
#[test]
fn test_create_master_node() {
    let node_id = "test-master-node";
    let config = SaberConfig {
        node_id: node_id.to_string(),
        role: NodeRole::Master,
        bt_address: None,
        is_music_mode: true,
    };
    
    let node = Node::new(config.node_id.clone(), NodeRole::Master);
    
    assert_eq!(node.id, node_id);
    assert_eq!(node.role, NodeRole::Master);
    assert_eq!(node.is_active(), true);
}

/// Test della creazione di pacchetti mesh
#[test]
fn test_mesh_packet_creation() {
    let source = "source-node";
    let destination = "dest-node";
    let payload = vec![1, 2, 3, 4];
    
    let packet = MeshPacket::new(
        source.to_string(),
        destination.to_string(),
        PacketType::Data,
        payload.clone()
    );
    
    assert_eq!(packet.source, source);
    assert_eq!(packet.destination, destination);
    assert_eq!(packet.packet_type, PacketType::Data);
    assert_eq!(packet.payload, payload);
    assert!(packet.timestamp > 0); // Il timestamp dovrebbe essere valido
}

/// Test del routing dei pacchetti nella rete mesh
#[test]
fn test_packet_routing() {
    // Creo una rete mesh con tre nodi: master, repeater, sink
    let mut network = MeshNetwork::new();
    
    let master = Node::new("master-1".to_string(), NodeRole::Master);
    let repeater = Node::new("repeater-1".to_string(), NodeRole::Repeater);
    let sink = Node::new("sink-1".to_string(), NodeRole::Sink);
    
    // Aggiungo i nodi alla rete
    network.add_node(master);
    network.add_node(repeater);
    network.add_node(sink);
    
    // Creo un pacchetto da master a sink
    let packet = MeshPacket::new(
        "master-1".to_string(),
        "sink-1".to_string(),
        PacketType::Data,
        vec![1, 2, 3, 4]
    );
    
    // Simulo il routing attraverso il repeater
    let route = network.find_route(&packet.source, &packet.destination);
    
    // La route dovrebbe passare per il repeater
    assert!(route.len() >= 2); // Almeno source e destination
    
    // Verifico che il pacchetto venga inoltrato correttamente
    let forwarded = network.forward_packet(&packet);
    assert!(forwarded);
    
    // Verifico che il pacchetto sia arrivato alla destinazione
    let delivered = network.deliver_packet(&packet);
    assert!(delivered);
}

/// Test di creazione del protocollo SABER come master
#[test]
fn test_saber_protocol_master() {
    match saber::main::start_master(Some("test-master".to_string()), None) {
        Ok(protocol) => {
            assert_eq!(protocol.config.node_id, "test-master");
            assert_eq!(protocol.config.role, NodeRole::Master);
        },
        Err(e) => {
            panic!("Errore creazione protocollo master: {}", e);
        }
    }
}

/// Test di sincronizzazione tra nodi
#[test]
fn test_node_synchronization() {
    // Creo un protocollo master
    let master_result = saber::main::start_master(Some("test-sync-master".to_string()), None);
    
    // Creo un protocollo sink
    let sink_result = saber::main::start_sink(Some("test-sync-sink".to_string()), None, true);
    
    if let (Ok(mut master), Ok(mut sink)) = (master_result, sink_result) {
        // Registro il sink nel master
        match master.register_node("test-sync-sink".to_string(), NodeRole::Sink, None) {
            Ok(_) => {
                // Avvio la sincronizzazione
                assert!(master.is_synchronized());
                
                // Attendo che il sink si sincronizzi (timeout di 5 secondi)
                let start = Instant::now();
                let timeout = Duration::from_secs(5);
                
                while !sink.is_synchronized() && start.elapsed() < timeout {
                    thread::sleep(Duration::from_millis(100));
                }
                
                // Verifico che il sink si sia sincronizzato
                assert!(sink.is_synchronized(), "Il sink non si è sincronizzato entro il timeout");
                
                // Verifico che le latenze siano ragionevoli
                assert!(master.get_current_latency() < 100);
                assert!(sink.get_current_latency() < 100);
            },
            Err(e) => {
                panic!("Errore registrazione nodo: {}", e);
            }
        }
    } else {
        // Ignoro il test se non è possibile creare i protocolli
        // (ad esempio in ambiente CI senza hardware Bluetooth)
        println!("Test ignorato: impossibile creare i protocolli");
    }
}

/// Test di comunicazione audio
#[test]
fn test_audio_transmission() {
    // Creo un protocollo master
    let master_result = saber::main::start_master(Some("test-audio-master".to_string()), None);
    
    // Creo un protocollo sink
    let sink_result = saber::main::start_sink(Some("test-audio-sink".to_string()), None, true);
    
    if let (Ok(mut master), Ok(mut sink)) = (master_result, sink_result) {
        // Registro il sink nel master
        match master.register_node("test-audio-sink".to_string(), NodeRole::Sink, None) {
            Ok(_) => {
                // Avvio la riproduzione audio sul master
                match master.start_audio_playback() {
                    Ok(_) => {
                        // Avvio la riproduzione audio sul sink
                        match sink.start_audio_playback() {
                            Ok(_) => {
                                // Attendo che l'audio venga trasmesso
                                thread::sleep(Duration::from_secs(1));
                                
                                // Verifico che entrambi siano attivi e sincronizzati
                                assert!(master.is_synchronized());
                                assert!(sink.is_synchronized());
                                
                                // Arresto la riproduzione
                                let _ = master.stop_audio_playback();
                                let _ = sink.stop_audio_playback();
                            },
                            Err(e) => {
                                panic!("Errore avvio audio sink: {}", e);
                            }
                        }
                    },
                    Err(e) => {
                        panic!("Errore avvio audio master: {}", e);
                    }
                }
            },
            Err(e) => {
                panic!("Errore registrazione nodo: {}", e);
            }
        }
    } else {
        // Ignoro il test se non è possibile creare i protocolli
        println!("Test ignorato: impossibile creare i protocolli");
    }
}

/// Test di resilienza della rete mesh
#[test]
fn test_mesh_resilience() {
    // Creo una rete con master, repeater e sink
    let mut network = MeshNetwork::new();
    
    let master = Node::new("resilience-master".to_string(), NodeRole::Master);
    let repeater1 = Node::new("resilience-repeater1".to_string(), NodeRole::Repeater);
    let repeater2 = Node::new("resilience-repeater2".to_string(), NodeRole::Repeater);
    let sink = Node::new("resilience-sink".to_string(), NodeRole::Sink);
    
    // Aggiungo i nodi alla rete
    network.add_node(master);
    network.add_node(repeater1);
    network.add_node(repeater2);
    network.add_node(sink);
    
    // Creo un pacchetto da master a sink
    let packet = MeshPacket::new(
        "resilience-master".to_string(),
        "resilience-sink".to_string(),
        PacketType::Data,
        vec![1, 2, 3, 4]
    );
    
    // Verifico che il pacchetto venga inoltrato inizialmente
    assert!(network.forward_packet(&packet));
    
    // Simulo la disconnessione di un repeater
    network.remove_node("resilience-repeater1");
    
    // Verifico che il pacchetto venga ancora inoltrato
    // utilizzando un percorso alternativo
    assert!(network.forward_packet(&packet));
    
    // Simulo la disconnessione del secondo repeater
    network.remove_node("resilience-repeater2");
    
    // In questo caso, senza repeater, il pacchetto non dovrebbe
    // raggiungere la destinazione
    assert!(!network.forward_packet(&packet));
}
