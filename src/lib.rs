mod protocol;

pub use protocol::mesh::*;
pub use protocol::sync::*;
pub use protocol::crypto::*;

use pyo3::prelude::*;

pub const ROLE_MASTER: &str = "master";
pub const ROLE_REPEATER: &str = "repeater";
pub const ROLE_SINK: &str = "sink";

use pyo3::prelude::*;
use pyo3::wrap_pyfunction;

use protocol::mesh::{Node, NodeRole, MeshNetwork};
use protocol::sync::{SyncManager, AudioSync};
use protocol::crypto::MeshCrypto;

#[pymodule]
fn libpy_mesh(_py: Python, m: &PyModule) -> PyResult<()> {
    m.add_class::<Node>()?;
    m.add_class::<NodeRole>()?;
    m.add("ROLE_MASTER", ROLE_MASTER)?;
    m.add("ROLE_REPEATER", ROLE_REPEATER)?;
    m.add("ROLE_SINK", ROLE_SINK)?;
    Ok(())
}
