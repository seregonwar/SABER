## 🧠 1. ARCHITETTURA A BLOCCHI COMPLETA

```
                          ┌────────────────────────────┐
                          │      CONTROL UI (Python)   │
                          │  - Web UI / CLI            │
                          │  - Stato Mesh              │
                          │  - Test Controller         │
                          └────────────┬───────────────┘
                                       │
                      Python <=> Rust <=> C++
                                       │
   ┌────────────────────────┬──────────┴───────────────┬──────────────────────┐
   │                        │                          │                      │
┌───────┐             ┌─────────────┐            ┌─────────────┐        ┌─────────────┐
│ Node A│◄──────mesh──┤ Node B      │──────mesh──┤ Node C      │ ─mesh─►│ Node D      │
│ Master│             │ Repeater    │            │ Sink        │        │ Sink        │
└───────┘             └─────────────┘            └─────────────┘        └─────────────┘
    ▲                    ▲                             ▲                      ▲
    │ Audio Out          │ Audio Sync Beacon           │ Audio Sync           │
    └─────── C++ Layer ──┴─────────────────────────────┴──────────────────────┘
```

---

## 📁 2. STRUTTURA FILE

```
saber/
├── src/
│   ├── core_audio/           # C++
│   │   ├── buffer.hpp
│   │   ├── audio_stream.cpp
│   │   └── sync_engine.cpp
│   ├── protocol/
│   │   ├── main.rs           # Rust
│   │   ├── mesh.rs
│   │   ├── sync.rs
│   │   └── crypto.rs
│   └── control/
│       ├── ui.py             # Python
│       ├── test_runner.py
│       └── dashboard.py
├── bindings/
│   ├── libpy_audio.cpp       # pybind11
│   └── libpy_mesh.rs         # pyo3
├── tests/
│   ├── test_audio.py
│   ├── test_sync.py
│   └── test_mesh.rs
└── Cargo.toml / CMakeLists.txt / pyproject.toml
```

---

## 🧩 3. MOCKUP FILES

### 🔷 C++ – `audio_stream.cpp`

```cpp
#include "buffer.hpp"
#include <portaudio.h>

class AudioStream {
public:
    AudioStream() { initAudio(); }
    ~AudioStream() { Pa_Terminate(); }

    void initAudio() {
        Pa_Initialize();
        // Configurazione del device, buffer, callback
    }

    void startStream() {
        // Avvia lo stream con callback audio sincrono
    }

private:
    RingBuffer<float> audioBuffer;
};
```

---

### 🟠 Rust – `mesh.rs`

```rust
use btleplug::api::*;
use tokio::sync::mpsc;

pub struct Node {
    id: String,
    role: NodeRole,
}

pub enum NodeRole {
    Master,
    Repeater,
    Sink,
}

pub async fn start_mesh(node: Node) {
    let (tx, mut rx) = mpsc::channel(32);
    while let Some(packet) = rx.recv().await {
        handle_packet(packet);
    }
}

fn handle_packet(pkt: Vec<u8>) {
    // Decodifica + inoltro mesh
}
```

---

### 🟡 Python – `ui.py`

```python
import asyncio
from dashboard import MeshDashboard
from libpy_audio import AudioController
from libpy_mesh import RustMesh

controller = AudioController()
mesh = RustMesh()

async def main():
    await mesh.start()
    controller.play_stream("stream.wav")
    MeshDashboard().launch()

if __name__ == "__main__":
    asyncio.run(main())
```

---

## ✅ 4. TEST AUTOMATICO

### 📄 `test_sync.py`

```python
import unittest
from libpy_audio import AudioController

class TestAudioSync(unittest.TestCase):
    def test_latency(self):
        ctrl = AudioController()
        lat = ctrl.get_current_latency()
        self.assertLess(lat, 15)

if __name__ == "__main__":
    unittest.main()
```

### 📄 `test_mesh.rs`

```rust
#[test]
fn test_mesh_packet_forwarding() {
    let packet = vec![0x01, 0x02, 0x03];
    let routed = route_packet(packet.clone());
    assert_eq!(routed, packet);
}
```

---

## 🧠 5. GESTIONE MEMORIA IN C++

### Tecniche adottate:

- `std::unique_ptr` → proprietà esclusiva
- `std::shared_ptr` → uso concorrente (audio thread / mesh thread)
- `std::lock_guard` per mutex thread-safe
- Nessun `new`/`delete`, tutto gestito con RAII
- Nessun puntatore raw accessibile direttamente

Esempio:

```cpp
std::shared_ptr<AudioStream> stream = std::make_shared<AudioStream>();
```

---

## 📊 6. INTEGRAZIONE TRA I LINGUAGGI

| Linguaggio | Interfaccia |
|------------|-------------|
| C++ → Python | [pybind11](https://github.com/pybind/pybind11) |
| Rust → Python | [pyo3](https://github.com/PyO3/pyo3) |
| Python → C++/Rust | Tramite moduli nativi compilati (`.so` / `.dll`) |

---

## 🧪 7. COME TESTARE

1. **Python**:  
   ```bash
   python3 -m unittest discover tests/
   ```

2. **Rust**:  
   ```bash
   cargo test
   ```

3. **C++**:  
   Usa `ctest` con `CMakeLists.txt`:
   ```bash
   mkdir build && cd build
   cmake ..
   make && ctest
   ```
