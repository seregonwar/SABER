use pyo3::prelude::*;

/// Un esempio di funzione esportata in Python
#[pyfunction]
fn hello_from_rust() -> &'static str {
    "Ciao dal backend Rust!"
}

#[pymodule]
fn libpy_mesh(_py: Python, m: &PyModule) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(hello_from_rust, m)?)?;
    Ok(())
}
