// Buffer di audio circolare thread-safe per il protocollo SABER
// Implementa il pattern RAII per gestione memoria sicura

#ifndef SABER_AUDIO_BUFFER_HPP
#define SABER_AUDIO_BUFFER_HPP

#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace saber {
namespace audio {

/**
 * Buffer circolare thread-safe per campioni audio
 * Ottimizzato per operazioni di lettura/scrittura audio in tempo reale
 */
template <typename T>
class RingBuffer {
public:
    /**
     * Costruttore
     * @param capacity Capacità del buffer in numero di elementi
     */
    explicit RingBuffer(size_t capacity)
        : buffer_(capacity)
        , capacity_(capacity)
        , write_pos_(0)
        , read_pos_(0)
        , size_(0)
    {
        if (capacity == 0) {
            throw std::invalid_argument("La capacità del buffer deve essere maggiore di zero");
        }
    }

    /**
     * Scrive dati nel buffer
     * @param data Puntatore ai dati da scrivere
     * @param count Numero di elementi da scrivere
     * @return Numero di elementi scritti
     */
    size_t write(const T* data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (count == 0) return 0;

        size_t available = capacity_ - size_;
        size_t to_write = std::min(count, available);
        
        if (to_write == 0) return 0;

        // Prima parte: dalla posizione di scrittura alla fine del buffer
        size_t first_part = std::min(to_write, capacity_ - write_pos_);
        std::memcpy(&buffer_[write_pos_], data, first_part * sizeof(T));
        
        // Seconda parte: dall'inizio del buffer
        if (first_part < to_write) {
            std::memcpy(&buffer_[0], data + first_part, (to_write - first_part) * sizeof(T));
        }
        
        // Aggiorno la posizione di scrittura
        write_pos_ = (write_pos_ + to_write) % capacity_;
        size_ += to_write;
        
        return to_write;
    }

    /**
     * Legge dati dal buffer
     * @param data Puntatore dove scrivere i dati letti
     * @param count Numero di elementi da leggere
     * @return Numero di elementi letti
     */
    size_t read(T* data, size_t count) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (count == 0 || size_ == 0) return 0;

        // Load the atomic value to a regular size_t before comparison
        size_t current_size = size_.load();
        size_t to_read = std::min(count, current_size);
        
        // Prima parte: dalla posizione di lettura alla fine del buffer
        size_t first_part = std::min(to_read, capacity_ - read_pos_);
        std::memcpy(data, &buffer_[read_pos_], first_part * sizeof(T));
        
        // Seconda parte: dall'inizio del buffer
        if (first_part < to_read) {
            std::memcpy(data + first_part, &buffer_[0], (to_read - first_part) * sizeof(T));
        }
        
        // Aggiorno la posizione di lettura
        read_pos_ = (read_pos_ + to_read) % capacity_;
        size_ -= to_read;
        
        return to_read;
    }

    /**
     * Legge dati dal buffer senza rimuoverli
     * @param data Puntatore dove scrivere i dati letti
     * @param count Numero di elementi da leggere
     * @return Numero di elementi letti
     */
    size_t peek(T* data, size_t count) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (count == 0 || size_ == 0) return 0;

        // Load the atomic value to a regular size_t before comparison
        size_t current_size = size_.load();
        size_t to_read = std::min(count, current_size);
        
        size_t read_pos = read_pos_; // Copia locale che non modificherà lo stato
        
        // Prima parte: dalla posizione di lettura alla fine del buffer
        size_t first_part = std::min(to_read, capacity_ - read_pos);
        std::memcpy(data, &buffer_[read_pos], first_part * sizeof(T));
        
        // Seconda parte: dall'inizio del buffer
        if (first_part < to_read) {
            std::memcpy(data + first_part, &buffer_[0], (to_read - first_part) * sizeof(T));
        }
        
        return to_read;
    }

    /**
     * Svuota il buffer
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        write_pos_ = 0;
        read_pos_ = 0;
        size_ = 0;
    }

    /**
     * Restituisce il numero di elementi nel buffer
     */
    size_t size() const {
        return size_.load();
    }

    /**
     * Restituisce la capacità del buffer
     */
    size_t capacity() const {
        return capacity_;
    }

    /**
     * Restituisce lo spazio disponibile nel buffer
     */
    size_t available() const {
        return capacity_ - size_.load();
    }

    /**
     * Controlla se il buffer è vuoto
     */
    bool empty() const {
        return size_.load() == 0;
    }

    /**
     * Controlla se il buffer è pieno
     */
    bool full() const {
        return size_.load() == capacity_;
    }

    /**
     * Restituisce la percentuale di riempimento (0-100)
     */
    uint8_t fill_percentage() const {
        return static_cast<uint8_t>((size_.load() * 100) / capacity_);
    }

private:
    std::vector<T> buffer_;             // Buffer dati sottostante
    const size_t capacity_;             // Capacità del buffer
    size_t write_pos_;                  // Posizione di scrittura
    size_t read_pos_;                   // Posizione di lettura
    std::atomic<size_t> size_;          // Numero di elementi nel buffer
    mutable std::mutex mutex_;          // Mutex per thread-safety
};

/**
 * Buffer audio specializzato per il protocollo SABER
 * Gestisce la sincronizzazione temporale dei campioni audio
 */
class AudioBuffer {
public:
    /**
     * Costruttore
     * @param sample_rate Frequenza di campionamento in Hz
     * @param channels Numero di canali (1=mono, 2=stereo)
     * @param buffer_ms Dimensione del buffer in millisecondi
     */
    AudioBuffer(uint32_t sample_rate, uint8_t channels, uint32_t buffer_ms)
        : sample_rate_(sample_rate)
        , channels_(channels)
        , buffer_ms_(buffer_ms)
        , samples_per_ms_(sample_rate / 1000)
        , buffer_size_(samples_per_ms_ * buffer_ms * channels)
        , buffer_(std::make_unique<RingBuffer<float>>(buffer_size_))
        , timestamp_(0)
    {
        if (sample_rate == 0 || channels == 0 || buffer_ms == 0) {
            throw std::invalid_argument("Parametri audio non validi");
        }
    }

    /**
     * Scrive campioni audio nel buffer con timestamp associato
     * @param samples Puntatore ai campioni audio (formato float interleaved)
     * @param count Numero di campioni (per canale)
     * @param timestamp Timestamp in ms dei campioni
     * @return Numero di campioni scritti
     */
    size_t write_samples(const float* samples, size_t count, uint64_t timestamp) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Aggiorno il timestamp se necessario
        if (buffer_->empty()) {
            timestamp_ = timestamp;
        }
        
        // Scrivo nel buffer (moltiplico per il numero di canali perché i dati sono interleaved)
        size_t written = buffer_->write(samples, count * channels_);
        return written / channels_; // Restituisco il numero di frame (non di singoli campioni)
    }

    /**
     * Legge campioni audio dal buffer
     * @param samples Puntatore dove scrivere i campioni letti
     * @param count Numero di campioni da leggere (per canale)
     * @param current_time Timestamp attuale per sincronizzazione
     * @return Numero di campioni letti
     */
    size_t read_samples(float* samples, size_t count, uint64_t current_time) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (buffer_->empty()) return 0;
        
        // Calcolo la differenza di tempo
        int64_t time_diff = static_cast<int64_t>(current_time) - static_cast<int64_t>(timestamp_);
        
        // Se siamo in anticipo (abbiamo campioni che non dovrebbero ancora essere riprodotti)
        if (time_diff < 0) {
            // Riempio di zeri (silenzio)
            std::memset(samples, 0, count * channels_ * sizeof(float));
            return count;
        }
        
        // Se siamo in ritardo, saltiamo alcuni campioni per recuperare
        size_t samples_to_skip = std::min(
            static_cast<size_t>(time_diff * samples_per_ms_),
            buffer_->size() / channels_
        );
        
        if (samples_to_skip > 0) {
            std::vector<float> temp(samples_to_skip * channels_);
            buffer_->read(temp.data(), temp.size());
            
            // Aggiorno il timestamp dopo lo skip
            timestamp_ += samples_to_skip / samples_per_ms_;
        }
        
        // Leggo i campioni richiesti
        size_t read = buffer_->read(samples, count * channels_);
        
        // Aggiorno il timestamp
        timestamp_ += (read / channels_) / samples_per_ms_;
        
        return read / channels_; // Restituisco il numero di frame
    }

    /**
     * Ottiene il livello di riempimento del buffer in percentuale (0-100)
     */
    uint8_t get_fill_level() const {
        return buffer_->fill_percentage();
    }

    /**
     * Ottiene la latenza attuale del buffer in millisecondi
     */
    uint32_t get_latency_ms() const {
        return (buffer_->size() / channels_) / samples_per_ms_;
    }

    /**
     * Svuota il buffer audio
     */
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        buffer_->clear();
    }

    /**
     * Imposta la dimensione del buffer in millisecondi
     * @param buffer_ms Nuova dimensione in millisecondi
     */
    void set_buffer_size_ms(uint32_t buffer_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (buffer_ms == 0) {
            throw std::invalid_argument("Dimensione buffer non valida");
        }
        
        buffer_ms_ = buffer_ms;
        buffer_size_ = samples_per_ms_ * buffer_ms * channels_;
        
        // Creo un nuovo buffer con la nuova dimensione
        auto new_buffer = std::make_unique<RingBuffer<float>>(buffer_size_);
        
        // Trasferisco i dati dal vecchio al nuovo buffer
        if (!buffer_->empty()) {
            std::vector<float> temp(buffer_->size());
            size_t read = buffer_->read(temp.data(), temp.size());
            new_buffer->write(temp.data(), read);
        }
        
        buffer_ = std::move(new_buffer);
    }

private:
    uint32_t sample_rate_;              // Frequenza di campionamento in Hz
    uint8_t channels_;                  // Numero di canali
    uint32_t buffer_ms_;                // Dimensione del buffer in millisecondi
    uint32_t samples_per_ms_;           // Campioni per millisecondo
    size_t buffer_size_;                // Dimensione del buffer in campioni
    std::unique_ptr<RingBuffer<float>> buffer_;  // Buffer circolare
    uint64_t timestamp_;                // Timestamp del primo campione nel buffer
    std::mutex mutex_;                  // Mutex per thread-safety
};

} // namespace audio
} // namespace saber

#endif // SABER_AUDIO_BUFFER_HPP
