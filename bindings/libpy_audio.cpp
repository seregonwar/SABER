// Binding C++ -> Python per il modulo audio del protocollo SABER
// Utilizza pybind11 per esporre le funzionalità C++ a Python

// Fix for PyBind11 includes
#ifdef _WIN32
#include "../external/pybind11/include/pybind11/pybind11.h"
#include "../external/pybind11/include/pybind11/functional.h"
#include "../external/pybind11/include/pybind11/stl.h"
#else
#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#endif

#include "../src/core_audio/buffer.hpp"
#include "../src/core_audio/audio_stream.hpp"  // Changed to include header file
#include "../src/core_audio/sync_engine.hpp"   // Changed to include header file instead of cpp

#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace py = pybind11; 

// Wrapper per il modulo audio verso Python
class AudioController {
public:
    AudioController() 
        : is_initialized_(false)
        , sample_rate_(48000)  // Default per musica come da specifiche PAPER.md
        , channels_(2)         // Default stereo
        , sync_engine_(nullptr)
    {}

    ~AudioController() {
        if (sync_engine_) {
            sync_engine_->stop();
        }
    }

    // Inizializza il controller audio
    bool initialize(uint32_t sample_rate = 48000, uint8_t channels = 2) {
        try {
            sample_rate_ = sample_rate;
            channels_ = channels;

            // Configuro il motore di sincronizzazione
            sync_engine_ = std::make_unique<saber::audio::SyncEngine>(
                sample_rate_, 
                channels_,
                20  // Buffer iniziale di 20ms
            );

            // Uso una lambda che cattura this per fornire il timestamp
            sync_engine_->initialize([this]() { return this->get_time_ms(); });

            is_initialized_ = true;
            return true;
        } catch (const std::exception& e) {
            py::print("Errore inizializzazione audio:", e.what());
            return false;
        }
    }

    // Avvia la riproduzione audio
    bool play_stream(const std::string& filename, uint32_t buffer_ms = 20) {
        if (!is_initialized_) {
            if (!initialize()) {
                return false;
            }
        }

        try {
            // In un'implementazione reale, qui caricheremmo il file audio
            // Per ora simulo il caricamento con un messaggio
            py::print("Caricamento file audio:", filename);

            // Avvio il motore di sincronizzazione con il buffer ottimale
            sync_engine_->start(buffer_ms);
            return true;
        } catch (const std::exception& e) {
            py::print("Errore avvio riproduzione:", e.what());
            return false;
        }
    }

    // Ferma la riproduzione
    bool stop_stream() {
        if (!sync_engine_) {
            return false;
        }

        try {
            sync_engine_->stop();
            return true;
        } catch (const std::exception& e) {
            py::print("Errore arresto riproduzione:", e.what());
            return false;
        }
    }

    // Imposta un callback per ottenere il timestamp sincronizzato
    void set_time_provider(const std::function<uint64_t()>& provider) {
        time_provider_ = provider;
    }

    // Ottiene la latenza corrente in millisecondi
    uint32_t get_current_latency() const {
        if (!sync_engine_) {
            return 0;
        }
        return sync_engine_->getCurrentLatency();
    }

    // Ottiene il livello di riempimento del buffer (0-100)
    uint8_t get_buffer_level() const {
        if (!sync_engine_) {
            return 0;
        }
        return sync_engine_->getBufferLevel();
    }

    // Verifica se il motore è attivo
    bool is_active() const {
        if (!sync_engine_) {
            return false;
        }
        return sync_engine_->isActive();
    }

    // Aggiorna lo stato di sincronizzazione
    void update_sync_state(bool is_synced, int64_t time_offset) {
        if (sync_engine_) {
            sync_engine_->updateSyncState(is_synced, time_offset);
        }
    }

    // Carica e riproduce un buffer audio con timestamp
    bool play_audio_buffer(const std::vector<float>& samples, uint64_t timestamp) {
        if (!sync_engine_) {
            return false;
        }

        size_t written = sync_engine_->writeAudioData(
            samples.data(), 
            samples.size() / channels_,  // Numero di frame
            timestamp
        );

        return written > 0;
    }

    // Configura la dimensione del buffer
    void set_buffer_size(uint32_t buffer_ms) {
        if (sync_engine_) {
            // Imposto il buffer ottimale per la sincronizzazione
            sync_engine_->start(buffer_ms);
        }
    }

private:
    // Ottiene il timestamp corrente in millisecondi
    uint64_t get_time_ms() const {
        // Se è stato configurato un provider esterno, lo uso
        if (time_provider_) {
            return time_provider_();
        }

        // Altrimenti uso il tempo del sistema locale
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
        return ms.time_since_epoch().count();
    }

    bool is_initialized_;
    uint32_t sample_rate_;
    uint8_t channels_;
    std::unique_ptr<saber::audio::SyncEngine> sync_engine_;
    std::function<uint64_t()> time_provider_;
};

// Modulo Python
PYBIND11_MODULE(libpy_audio, m) {
    m.doc() = "SABER Protocol - Audio Module";

    py::class_<AudioController>(m, "AudioController")
        .def(py::init<>())
        .def("initialize", &AudioController::initialize, 
            py::arg("sample_rate") = 48000, py::arg("channels") = 2,
            "Inizializza il controller audio")
        .def("play_stream", &AudioController::play_stream, 
            py::arg("filename"), py::arg("buffer_ms") = 20,
            "Avvia la riproduzione di un file audio")
        .def("stop_stream", &AudioController::stop_stream,
            "Ferma la riproduzione audio")
        .def("set_time_provider", &AudioController::set_time_provider,
            py::arg("provider"),
            "Imposta una funzione per ottenere il timestamp sincronizzato")
        .def("get_current_latency", &AudioController::get_current_latency,
            "Ottiene la latenza corrente in millisecondi")
        .def("get_buffer_level", &AudioController::get_buffer_level,
            "Ottiene il livello di riempimento del buffer (0-100)")
        .def("is_active", &AudioController::is_active,
            "Verifica se l'audio è in riproduzione")
        .def("update_sync_state", &AudioController::update_sync_state,
            py::arg("is_synced"), py::arg("time_offset"),
            "Aggiorna lo stato di sincronizzazione")
        .def("play_audio_buffer", &AudioController::play_audio_buffer,
            py::arg("samples"), py::arg("timestamp"),
            "Riproduce un buffer audio con timestamp")
        .def("set_buffer_size", &AudioController::set_buffer_size,
            py::arg("buffer_ms"),
            "Configura la dimensione del buffer in millisecondi");

    // Aggiungo costanti e versione
    m.attr("DEFAULT_SAMPLE_RATE_MUSIC") = 48000;
    m.attr("DEFAULT_SAMPLE_RATE_VOICE") = 16000;
    m.attr("DEFAULT_CHANNELS") = 2;
    m.attr("__version__") = "0.1.0";
}
