// audio_stream.hpp
// Definizione della classe AudioStream per SABER Protocol

#ifndef SABER_AUDIO_STREAM_HPP
#define SABER_AUDIO_STREAM_HPP

#include "buffer.hpp"
#include <functional>
#include <memory>
#include <atomic>

// Forward declaration for portaudio
typedef void PaStream;
struct PaStreamCallbackTimeInfo;
typedef unsigned long PaStreamCallbackFlags;

namespace saber {
namespace audio {

// Forward declaration for internal usage
struct StreamCallbackData;

/**
 * Class for handling audio streaming with PortAudio
 * Provides synchronized audio playback capabilities for the SABER protocol
 */
class AudioStream {
public:
    /**
     * Constructor
     * @param sample_rate Sample rate in Hz
     * @param channels Number of audio channels
     * @param buffer_ms Buffer size in milliseconds
     * @param time_provider Function to provide synchronized timestamps
     */
    AudioStream(
        uint32_t sample_rate = 48000,
        uint8_t channels = 2,
        uint32_t buffer_ms = 100,
        std::function<uint64_t()> time_provider = []() { return 0; }
    );
    
    /**
     * Destructor - Ensures proper PortAudio cleanup
     */
    ~AudioStream();
    
    /**
     * Initialize the audio system using PortAudio
     */
    void initAudio();
    
    /**
     * Start the audio stream
     */
    void startStream();
    
    /**
     * Stop the audio stream
     */
    void stopStream();
    
    /**
     * Write audio data to the buffer with timestamp
     * @param data Pointer to audio data (float samples)
     * @param frames Number of frames to write
     * @param timestamp The timestamp associated with the first sample
     * @return Number of frames actually written
     */
    size_t writeAudio(const float* data, size_t frames, uint64_t timestamp);
    
    /**
     * Get the current end-to-end latency in milliseconds
     * @return Latency in milliseconds
     */
    uint32_t getCurrentLatency() const;
    
    /**
     * Change the buffer size
     * @param buffer_ms New buffer size in milliseconds
     */
    void setBufferSize(uint32_t buffer_ms);
    
    /**
     * Get the current buffer fill level (0-100%)
     * @return Fill level as percentage (0-100)
     */
    uint8_t getBufferLevel() const;

private:
    /**
     * PortAudio callback function
     */
    static int paCallback(
        const void* inputBuffer,
        void* outputBuffer,
        unsigned long framesPerBuffer,
        const PaStreamCallbackTimeInfo* timeInfo,
        PaStreamCallbackFlags statusFlags,
        void* userData
    );
    
    uint32_t sample_rate_;
    uint8_t channels_;
    AudioBuffer buffer_;  // Using AudioBuffer from buffer.hpp
    std::function<uint64_t()> time_provider_;
    std::unique_ptr<StreamCallbackData> callback_data_;
    PaStream* stream_;
    bool is_initialized_;
};

} // namespace audio
} // namespace saber

#endif // SABER_AUDIO_STREAM_HPP