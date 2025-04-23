use pyo3::prelude::*;
use pyo3::pyclass;
use pyo3::pymethods;

// Qui NON serve pymodule, viene gestito da src/lib.rs

// Assicurati che tutte le struct/classi che vuoi esporre abbiano #[pyclass] e #[pymethods]
// Esempio per Node (da ripetere per le altre):
//
// #[pyclass]
// pub struct Node { ... }
//
// #[pymethods]
// impl Node {
//     #[new]
//     pub fn new(...) -> Self { ... }
//     // altri metodi...
// }

// Ripeti questa struttura per NodeRole, MeshNetwork, SyncManager, AudioSync, MeshCrypto
