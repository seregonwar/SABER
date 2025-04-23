import os
import sys
import time
import unittest

# Print Python's current search paths for debugging
print(f"Python version: {sys.version}")
print(f"Current directory: {os.getcwd()}")

# Add project path to Python's path
build_path = os.path.join(os.path.dirname(__file__), '..', 'build', 'Debug')
sys.path.insert(0, os.path.dirname(__file__)) # Add tests directory
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..')) # Add root directory
sys.path.insert(0, build_path) # Add build directory

print(f"Added to path: {build_path}")
print(f"Files in build directory: {os.listdir(build_path)}")
print(f"Python path: {sys.path}")

try:
    # Import the audio module we've just compiled
    # The module exports AudioController, not AudioStream
    from libpy_audio import AudioController
    print("Successfully imported AudioController from libpy_audio")
except ImportError as e:
    print(f"Error importing libpy_audio module: {e}")
    sys.exit(1)

class TestAudioDriver(unittest.TestCase):
    """Tests for the SABER audio driver functionality"""
    
    def setUp(self):
        """Initialize audio driver for testing"""
        # Create audio controller with default settings
        self.audio = AudioController()
    
    def tearDown(self):
        """Clean up after tests"""
        # Stop audio streaming if running
        try:
            self.audio.stop_stream()
        except:
            pass
    
    def test_audio_initialization(self):
        """Test that the audio driver initializes correctly"""
        # Initialize audio system
        try:
            success = self.audio.initialize(sample_rate=48000, channels=2)
            initialized = True
        except Exception as e:
            initialized = False
            success = False
            print(f"Error initializing audio: {e}")
        
        # Check that initialization was successful
        self.assertTrue(initialized, "Audio driver should initialize without exceptions")
        self.assertTrue(success, "Audio initialization should return success")
    
    def test_buffer_operations(self):
        """Test audio buffer operations"""
        # Initialize audio first
        self.audio.initialize(sample_rate=48000, channels=2)
        
        # Create a test buffer with 1 second of audio (48000 samples)
        # Using a simple sine wave pattern would be ideal, but for test purposes
        # we'll just use a buffer filled with zeros
        buffer_size = 48000 * 2  # 1 second stereo (2 channels)
        test_buffer = [0.0] * buffer_size
        
        # Write to the buffer and check return value
        # Using play_audio_buffer method from AudioController
        success = self.audio.play_audio_buffer(test_buffer, int(time.time() * 1000))
        
        # Check that writing succeeded
        self.assertTrue(success, "Should be able to write frames to audio buffer")
        
        # Check buffer level
        buffer_level = self.audio.get_buffer_level()
        print(f"Buffer level: {buffer_level}%")
        self.assertGreaterEqual(buffer_level, 0, "Buffer level should be a non-negative value")
        self.assertLessEqual(buffer_level, 100, "Buffer level should be <= 100%")
    
    def test_stream_operations(self):
        """Test starting and stopping the audio stream"""
        # Initialize audio
        self.audio.initialize(sample_rate=48000, channels=2)
        
        # Start stream by playing a test file
        try:
            # Using a dummy filename since we're in test mode
            success = self.audio.play_stream("test_file.wav")
            started = True
        except Exception as e:
            started = False
            success = False
            print(f"Error starting audio stream: {e}")
        
        # Check that the stream started
        self.assertTrue(started, "Audio stream should start without exceptions")
        self.assertTrue(success, "Audio stream should start successfully")
        
        # Get current latency
        latency = self.audio.get_current_latency()
        print(f"Current audio latency: {latency}ms")
        
        # Check if the stream is active
        active = self.audio.is_active()
        self.assertTrue(active, "Audio stream should be active after starting")
        
        # Sleep briefly to let audio system process
        time.sleep(0.5)
        
        # Stop stream
        try:
            success = self.audio.stop_stream()
            stopped = True
        except Exception as e:
            stopped = False
            success = False
            print(f"Error stopping audio stream: {e}")
        
        # Check that the stream stopped
        self.assertTrue(stopped, "Audio stream should stop without exceptions")
        self.assertTrue(success, "Audio stream should stop successfully")
    
    def test_buffer_size_change(self):
        """Test changing the buffer size"""
        # Initialize audio
        self.audio.initialize(sample_rate=48000, channels=2)
        
        # Initial buffer size
        initial_size = 20  # ms
        
        # New buffer size
        new_size = 40  # ms
        
        # Change buffer size
        try:
            self.audio.set_buffer_size(new_size)
            changed = True
        except Exception as e:
            changed = False
            print(f"Error changing buffer size: {e}")
        
        # Check that buffer size changed successfully
        self.assertTrue(changed, "Should be able to change buffer size without exceptions")

if __name__ == "__main__":
    unittest.main()