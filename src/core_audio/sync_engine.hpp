#pragma once

#include "buffer.hpp"
#include "audio_stream.hpp"
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>

namespace saber {
namespace audio {

/**
 * Engine di sincronizzazione audio
 * Gestisce la comunicazione tra il livello di protocollo e l'audio engine
 */
class SyncEngine {
public:
    /**
     * Costruttore
     * @param sample_rate Frequenza di campionamento in Hz
     * @param channels Numero di canali (1=mono, 2=stereo)
     * @param initial_buffer_ms Buffer iniziale in millisecondi
     */
    SyncEngine(uint32_t sample_rate, uint8_t channels, uint32_t initial_buffer_ms = 20);

    /**
     * Distruttore
     */
    ~SyncEngine();

    /**
     * Inizializza l'engine
     * @param time_provider Funzione che fornisce il timestamp globale sincronizzato
     */
    void initialize(std::function<uint64_t()> time_provider);

    /**
     * Avvia la riproduzione sincronizzata
     * @param optimal_buffer_ms Buffer ottimale in ms basato sulla latenza di rete
     */
    void start(uint32_t optimal_buffer_ms = 20);

    /**
     * Ferma la riproduzione
     */
    void stop();

    /**
     * Aggiorna lo stato di sincronizzazione
     * @param is_synced Flag che indica se il nodo è sincronizzato
     * @param time_offset Offset temporale da applicare in millisecondi
     */
    void updateSyncState(bool is_synced, int64_t time_offset);

    /**
     * Scrive dati audio nel buffer
     * @param data Campioni audio (float interleaved)
     * @param frames Numero di frame
     * @param source_timestamp Timestamp dei campioni (dal master)
     * @return Numero di frame scritti
     */
    size_t writeAudioData(const float* data, size_t frames, uint64_t source_timestamp);

    /**
     * Ottiene la latenza corrente in millisecondi
     */
    uint32_t getCurrentLatency() const;

    /**
     * Ottiene il livello di riempimento del buffer (0-100)
     */
    uint8_t getBufferLevel() const;

    /**
     * Verifica se il motore è attivo
     */
    bool isActive() const;

    /**
     * Verifica se il motore è sincronizzato
     */
    bool isSynchronized() const;

private:
    /**
     * Ottiene il timestamp corrente sincronizzato per l'audio locale
     * Questo timestamp tiene conto dell'offset di sincronizzazione
     */
    uint64_t getLocalSyncTime() const;

    uint32_t sample_rate_;                      // Frequenza di campionamento
    uint8_t channels_;                          // Numero di canali
    std::chrono::steady_clock::time_point start_time_;  // Timestamp di partenza
    int64_t time_offset_;                       // Offset di sincronizzazione in ms
    std::atomic<bool> is_active_;               // Flag attività
    std::atomic<bool> is_synchronized_;         // Flag sincronizzazione
    std::function<uint64_t()> time_provider_;   // Provider timestamp sincronizzato
    std::unique_ptr<AudioStream> audio_stream_; // Stream audio
};

} // namespace audio
} // namespace saber