// Binding Rust -> Python per il modulo mesh del protocollo SABER
// Utilizza PyO3 per esporre le funzionalità Rust a Python

use pyo3::prelude::*;
use pyo3::wrap_pyfunction;
use pyo3::types::{PyDict, PyList};

use std::sync::Arc;
use std::collections::HashMap;

// Importa i moduli dalla parte Rust del protocollo
use saber::mesh::{Node, NodeRole, MeshNetwork, MeshPacket};
use saber::main::{SaberProtocol, SaberConfig, start_master, start_repeater, start_sink};

/// Wrapper Python per il protocollo SABER
#[pyclass]
struct RustMesh {
    /// Istanza del protocollo
    protocol: Option<SaberProtocol>,
    /// ID del nodo
    node_id: String,
    /// Ruolo del nodo
    role: String,
}

#[pymethods]
impl RustMesh {
    /// Costruttore
    #[new]
    fn new() -> Self {
        RustMesh {
            protocol: None,
            node_id: String::new(),
            role: String::new(),
        }
    }

    /// Inizializza il protocollo come nodo Master (UCB)
    #[pyo3(text_signature = "($self, node_id=None, bt_address=None)")]
    fn init_as_master(&mut self, node_id: Option<String>, bt_address: Option<String>) -> PyResult<bool> {
        match start_master(node_id.clone(), bt_address) {
            Ok(protocol) => {
                self.node_id = protocol.config.node_id.clone();
                self.role = String::from("Master");
                self.protocol = Some(protocol);
                Ok(true)
            },
            Err(e) => {
                Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore inizializzazione master: {}", e)))
            }
        }
    }

    /// Inizializza il protocollo come nodo Repeater
    #[pyo3(text_signature = "($self, node_id=None, bt_address=None)")]
    fn init_as_repeater(&mut self, node_id: Option<String>, bt_address: Option<String>) -> PyResult<bool> {
        match start_repeater(node_id.clone(), bt_address) {
            Ok(protocol) => {
                self.node_id = protocol.config.node_id.clone();
                self.role = String::from("Repeater");
                self.protocol = Some(protocol);
                Ok(true)
            },
            Err(e) => {
                Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore inizializzazione repeater: {}", e)))
            }
        }
    }

    /// Inizializza il protocollo come nodo Sink
    #[pyo3(text_signature = "($self, node_id=None, bt_address=None, is_music=True)")]
    fn init_as_sink(&mut self, node_id: Option<String>, bt_address: Option<String>, is_music: bool) -> PyResult<bool> {
        match start_sink(node_id.clone(), bt_address, is_music) {
            Ok(protocol) => {
                self.node_id = protocol.config.node_id.clone();
                self.role = String::from("Sink");
                self.protocol = Some(protocol);
                Ok(true)
            },
            Err(e) => {
                Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore inizializzazione sink: {}", e)))
            }
        }
    }

    /// Avvia il protocollo
    #[pyo3(text_signature = "($self)")]
    fn start(&self) -> PyResult<bool> {
        if let Some(protocol) = &self.protocol {
            Ok(protocol.is_synchronized())
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Verifica se il nodo è sincronizzato
    #[pyo3(text_signature = "($self)")]
    fn is_synchronized(&self) -> PyResult<bool> {
        if let Some(protocol) = &self.protocol {
            Ok(protocol.is_synchronized())
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Ottiene la latenza corrente
    #[pyo3(text_signature = "($self)")]
    fn get_current_latency(&self) -> PyResult<u32> {
        if let Some(protocol) = &self.protocol {
            Ok(protocol.get_current_latency())
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Avvia la riproduzione audio
    #[pyo3(text_signature = "($self)")]
    fn start_audio_playback(&mut self) -> PyResult<bool> {
        if let Some(protocol) = &mut self.protocol {
            match protocol.start_audio_playback() {
                Ok(_) => Ok(true),
                Err(e) => Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore avvio riproduzione: {}", e)))
            }
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Ferma la riproduzione audio
    #[pyo3(text_signature = "($self)")]
    fn stop_audio_playback(&mut self) -> PyResult<bool> {
        if let Some(protocol) = &mut self.protocol {
            match protocol.stop_audio_playback() {
                Ok(_) => Ok(true),
                Err(e) => Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore arresto riproduzione: {}", e)))
            }
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Registra un nuovo nodo nella rete
    #[pyo3(text_signature = "($self, node_id, role, address=None)")]
    fn register_node(&self, node_id: String, role: String, address: Option<String>) -> PyResult<bool> {
        if let Some(protocol) = &self.protocol {
            let node_role = match role.to_lowercase().as_str() {
                "master" => NodeRole::Master,
                "repeater" => NodeRole::Repeater,
                "sink" => NodeRole::Sink,
                _ => return Err(PyErr::new::<pyo3::exceptions::PyValueError, _>("Ruolo non valido"))
            };

            match protocol.register_node(node_id, node_role, address) {
                Ok(_) => Ok(true),
                Err(e) => Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore registrazione nodo: {}", e)))
            }
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Ottiene tutti i nodi attivi
    #[pyo3(text_signature = "($self)")]
    fn get_active_nodes(&self, py: Python) -> PyResult<PyObject> {
        if let Some(protocol) = &self.protocol {
            match protocol.get_active_nodes() {
                Ok(nodes) => {
                    let py_list = PyList::empty(py);
                    for node_id in nodes {
                        py_list.append(node_id)?;
                    }
                    Ok(py_list.into())
                },
                Err(e) => Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>(format!("Errore ottenimento nodi: {}", e)))
            }
        } else {
            Err(PyErr::new::<pyo3::exceptions::PyRuntimeError, _>("Protocollo non inizializzato"))
        }
    }

    /// Ottiene informazioni sul nodo locale
    #[pyo3(text_signature = "($self)")]
    fn get_node_info(&self, py: Python) -> PyResult<PyObject> {
        let dict = PyDict::new(py);
        dict.set_item("node_id", &self.node_id)?;
        dict.set_item("role", &self.role)?;

        if let Some(protocol) = &self.protocol {
            dict.set_item("is_synchronized", protocol.is_synchronized())?;
            dict.set_item("latency", protocol.get_current_latency())?;
        } else {
            dict.set_item("is_synchronized", false)?;
            dict.set_item("latency", 0)?;
        }

        Ok(dict.into())
    }
}

/// Modulo Python SABER per il protocollo mesh
#[pymodule]
fn libpy_mesh(_py: Python, m: &PyModule) -> PyResult<()> {
    m.add_class::<RustMesh>()?;
    
    // Aggiungo costanti per i ruoli dei nodi
    m.add("ROLE_MASTER", "master")?;
    m.add("ROLE_REPEATER", "repeater")?;
    m.add("ROLE_SINK", "sink")?;
    
    // Aggiungo la versione
    m.add("__version__", "0.1.0")?;
    
    Ok(())
}
