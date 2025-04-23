#include "sync_engine.hpp"
#include "buffer.hpp"
#include "audio_stream.hpp"
#include <thread>
#include <mutex>
#include <iostream>
#include <vector>
#include <stdexcept>

namespace saber {
namespace audio {

SyncEngine::SyncEngine(uint32_t sample_rate, uint8_t channels, uint32_t initial_buffer_ms)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , time_offset_(0)
    , is_active_(false)
    , is_synchronized_(false)
    , audio_stream_(nullptr)
{
    // Inizializzo un timestamp di partenza
    start_time_ = std::chrono::steady_clock::now();
}

SyncEngine::~SyncEngine() {
    stop();
}

void SyncEngine::initialize(std::function<uint64_t()> time_provider) {
    time_provider_ = time_provider;

    // Creo lo stream audio passando la funzione che fornisce il timestamp locale sincronizzato
    audio_stream_ = std::make_unique<AudioStream>(
        sample_rate_,
        channels_,
        20, // Buffer iniziale di 20ms
        [this]() { return getLocalSyncTime(); }
    );

    std::cout << "SyncEngine inizializzato: " << sample_rate_ << "Hz, " 
              << (int)channels_ << " canali" << std::endl;
}

void SyncEngine::start(uint32_t optimal_buffer_ms) {
    if (!audio_stream_) {
        throw std::runtime_error("SyncEngine non inizializzato");
    }

    // Imposto la dimensione ottimale del buffer in base alla latenza di rete
    audio_stream_->setBufferSize(optimal_buffer_ms);

    // Attendo che il buffer si riempia parzialmente prima di iniziare
    std::this_thread::sleep_for(std::chrono::milliseconds(optimal_buffer_ms / 2));

    // Avvio lo stream audio
    audio_stream_->startStream();
    is_active_ = true;

    std::cout << "SyncEngine avviato con buffer di " << optimal_buffer_ms << "ms" << std::endl;
}

void SyncEngine::stop() {
    if (audio_stream_ && is_active_) {
        audio_stream_->stopStream();
        is_active_ = false;
        std::cout << "SyncEngine fermato" << std::endl;
    }
}

void SyncEngine::updateSyncState(bool is_synced, int64_t time_offset) {
    is_synchronized_ = is_synced;
    time_offset_ = time_offset;

    if (is_synced) {
        std::cout << "SyncEngine sincronizzato con offset di " << time_offset << "ms" << std::endl;
    } else {
        std::cout << "SyncEngine non sincronizzato" << std::endl;
    }
}

size_t SyncEngine::writeAudioData(const float* data, size_t frames, uint64_t source_timestamp) {
    if (!audio_stream_) {
        return 0;
    }

    return audio_stream_->writeAudio(data, frames, source_timestamp);
}

uint32_t SyncEngine::getCurrentLatency() const {
    if (!audio_stream_) {
        return 0;
    }

    return audio_stream_->getCurrentLatency();
}

uint8_t SyncEngine::getBufferLevel() const {
    if (!audio_stream_) {
        return 0;
    }

    return audio_stream_->getBufferLevel();
}

bool SyncEngine::isActive() const {
    return is_active_;
}

bool SyncEngine::isSynchronized() const {
    return is_synchronized_;
}

uint64_t SyncEngine::getLocalSyncTime() const {
    // Se disponibile, uso il timestamp fornito dal protocollo
    if (time_provider_) {
        return time_provider_();
    }

    // Altrimenti calcolo un timestamp locale basato sull'offset
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time_).count();
    
    // Applico l'offset di sincronizzazione
    return static_cast<uint64_t>(elapsed + time_offset_);
}

} // namespace audio
} // namespace saber
