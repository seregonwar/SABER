// Implementazione dello stream audio per SABER Protocol
// Gestisce la riproduzione e cattura audio sincronizzata

#include "audio_stream.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <cstring>
#ifdef _WIN32
#include "../../../external/portaudio/include/portaudio.h"
#else
#include <portaudio.h>
#endif

namespace saber {
namespace audio {

struct StreamCallbackData {
    AudioBuffer* buffer;
    std::function<uint64_t()> get_time_callback;
    std::atomic<bool> is_active{false};
    uint8_t channels; // Store channels directly in callback data
    
    StreamCallbackData(AudioBuffer* buf, std::function<uint64_t()> time_cb, uint8_t ch)
        : buffer(buf), get_time_callback(time_cb), channels(ch) {}
};

AudioStream::AudioStream(
    uint32_t sample_rate,
    uint8_t channels,
    uint32_t buffer_ms,
    std::function<uint64_t()> time_provider
)
    : sample_rate_(sample_rate)
    , channels_(channels)
    , buffer_(sample_rate, channels, buffer_ms)
    , time_provider_(time_provider)
    , callback_data_(std::make_unique<StreamCallbackData>(&buffer_, time_provider, channels))
    , stream_(nullptr)
    , is_initialized_(false)
{
    initAudio();
}

AudioStream::~AudioStream() {
    stopStream();
    
    if (stream_) {
        Pa_CloseStream(stream_);
        stream_ = nullptr;
    }
    
    if (is_initialized_) {
        Pa_Terminate();
    }
}

void AudioStream::initAudio() {
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        throw std::runtime_error(std::string("Errore inizializzazione PortAudio: ") + 
                                Pa_GetErrorText(err));
    }
    
    is_initialized_ = true;
    
    // Configuro i parametri dello stream
    PaStreamParameters outputParams;
    std::memset(&outputParams, 0, sizeof(outputParams));
    
    // Uso il dispositivo di output predefinito
    outputParams.device = Pa_GetDefaultOutputDevice();
    if (outputParams.device == paNoDevice) {
        throw std::runtime_error("Nessun dispositivo di output disponibile");
    }
    
    outputParams.channelCount = channels_;
    outputParams.sampleFormat = paFloat32;  // Usiamo float 32-bit
    outputParams.suggestedLatency = Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;
    
    // Apro lo stream audio
    err = Pa_OpenStream(
        &stream_,
        nullptr,          // No input
        &outputParams,
        sample_rate_,
        256,              // Frames per buffer (regoliamo per bassa latenza)
        paNoFlag,
        &AudioStream::paCallback,
        callback_data_.get()
    );
    
    if (err != paNoError) {
        throw std::runtime_error(std::string("Errore apertura stream audio: ") + 
                                Pa_GetErrorText(err));
    }
    
    std::cout << "Stream audio inizializzato: " << sample_rate_ << "Hz, " 
              << (int)channels_ << " canali" << std::endl;
}

void AudioStream::startStream() {
    if (!stream_) return;
    
    PaError err = Pa_StartStream(stream_);
    if (err != paNoError) {
        throw std::runtime_error(std::string("Errore avvio stream audio: ") + 
                                Pa_GetErrorText(err));
    }
    
    callback_data_->is_active = true;
    std::cout << "Stream audio avviato" << std::endl;
}

void AudioStream::stopStream() {
    if (!stream_) return;
    
    callback_data_->is_active = false;
    
    // Attendo un po' per svuotare i buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    PaError err = Pa_StopStream(stream_);
    if (err != paNoError) {
        std::cerr << "Errore arresto stream audio: " << Pa_GetErrorText(err) << std::endl;
    }
    
    std::cout << "Stream audio fermato" << std::endl;
}

size_t AudioStream::writeAudio(const float* data, size_t frames, uint64_t timestamp) {
    return buffer_.write_samples(data, frames, timestamp);
}

uint32_t AudioStream::getCurrentLatency() const {
    if (!stream_) return 0;
    
    // Combina la latenza del buffer software con quella hardware
    uint32_t sw_latency = buffer_.get_latency_ms();
    
    const PaStreamInfo* info = Pa_GetStreamInfo(stream_);
    uint32_t hw_latency = info ? static_cast<uint32_t>(info->outputLatency * 1000) : 0;
    
    return sw_latency + hw_latency;
}

void AudioStream::setBufferSize(uint32_t buffer_ms) {
    buffer_.set_buffer_size_ms(buffer_ms);
}

uint8_t AudioStream::getBufferLevel() const {
    return buffer_.get_fill_level();
}

int AudioStream::paCallback(
    const void* inputBuffer,
    void* outputBuffer,
    unsigned long framesPerBuffer,
    const PaStreamCallbackTimeInfo* timeInfo,
    PaStreamCallbackFlags statusFlags,
    void* userData
) {
    StreamCallbackData* data = static_cast<StreamCallbackData*>(userData);
    float* out = static_cast<float*>(outputBuffer);
    
    if (!data->is_active) {
        // Lo stream non è attivo, output silenzio
        std::memset(out, 0, framesPerBuffer * data->channels * sizeof(float));
        return paContinue;
    }
    
    // Ottiene il timestamp corrente dal sync provider
    uint64_t current_time = data->get_time_callback();
    
    // Legge i dati dal buffer con timestamp per sincronizzazione
    size_t read = data->buffer->read_samples(out, framesPerBuffer, current_time);
    
    // Se non ho letto abbastanza dati, riempio il resto con silenzio
    if (read < framesPerBuffer) {
        std::memset(
            out + (read * data->channels),
            0,
            (framesPerBuffer - read) * data->channels * sizeof(float)
        );
    }
    
    return paContinue;
}

} // namespace audio
} // namespace saber
