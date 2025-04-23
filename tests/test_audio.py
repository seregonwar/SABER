# Test unitari per il modulo audio del protocollo SABER
# Verifica la corretta sincronizzazione audio e la gestione dei buffer

import os
import sys
import time
import unittest
import threading
import numpy as np
from typing import List, Tuple

# Aggiungo il percorso dei moduli alla path di Python
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src', 'control'))

try:
    # Importo i moduli da testare
    from libpy_audio import AudioController, DEFAULT_SAMPLE_RATE_MUSIC
except ImportError:
    print("Errore: impossibile importare i moduli audio. Assicurati di averli compilati.")
    sys.exit(1)

class TestAudioSync(unittest.TestCase):
    """Test per la sincronizzazione audio"""
    
    def setUp(self):
        """Inizializza i controller audio per i test"""
        self.audio1 = AudioController()
        self.audio2 = AudioController()
        
        # Inizializzo i controller con gli stessi parametri
        self.audio1.initialize(DEFAULT_SAMPLE_RATE_MUSIC, 2)  # Stereo
        self.audio2.initialize(DEFAULT_SAMPLE_RATE_MUSIC, 2)
        
        # Definisco un timestamp di sincronizzazione comune
        self.start_time = int(time.time() * 1000)  # ms
        
        # Configuro i provider di timestamp
        # Uso il timestamp di riferimento più un offset per simulare la sincronizzazione
        self.audio1.set_time_provider(lambda: int(time.time() * 1000) - self.start_time)
        self.audio2.set_time_provider(lambda: int(time.time() * 1000) - self.start_time)
    
    def tearDown(self):
        """Pulisce le risorse dopo i test"""
        self.audio1.stop_stream()
        self.audio2.stop_stream()
    
    def generate_test_samples(self, duration_ms: int = 1000) -> Tuple[List[float], int]:
        """Genera campioni audio di test (onda sinusoidale)"""
        sample_rate = DEFAULT_SAMPLE_RATE_MUSIC
        frequency = 440  # Hz (La)
        channels = 2
        
        # Calcolo il numero di campioni
        num_samples = int(sample_rate * duration_ms / 1000)
        
        # Genero un'onda sinusoidale
        t = np.linspace(0, duration_ms / 1000, num_samples)
        data = np.sin(2 * np.pi * frequency * t).astype(np.float32)
        
        # Converto in formato interleaved stereo
        stereo_data = np.zeros(num_samples * channels, dtype=np.float32)
        for i in range(num_samples):
            stereo_data[i*channels] = data[i]      # Canale sinistro
            stereo_data[i*channels+1] = data[i]    # Canale destro
        
        return stereo_data.tolist(), sample_rate
    
    def test_buffer_level(self):
        """Verifica che il livello del buffer sia corretto"""
        # Genero campioni audio di test
        samples, sample_rate = self.generate_test_samples(500)
        
        # Timestamp corrente
        current_time = int(time.time() * 1000)
        
        # Invio campioni al buffer audio
        self.audio1.play_audio_buffer(samples, current_time)
        
        # Verifico che il buffer non sia vuoto
        buffer_level = self.audio1.get_buffer_level()
        self.assertGreater(buffer_level, 0, "Il buffer dovrebbe contenere dati")
    
    def test_latency_calculation(self):
        """Verifica che la latenza venga calcolata correttamente"""
        # Imposto un buffer di dimensione nota
        buffer_ms = 30
        self.audio1.set_buffer_size(buffer_ms)
        
        # La latenza dovrebbe essere almeno pari alla dimensione del buffer
        latency = self.audio1.get_current_latency()
        
        # Tollero una differenza di 5ms per l'overhead di sistema
        self.assertGreaterEqual(latency, buffer_ms - 5, 
                             f"La latenza ({latency}ms) dovrebbe essere almeno pari al buffer ({buffer_ms}ms)")
        
    def test_synchronized_playback(self):
        """Verifica che la riproduzione sia sincronizzata tra due controller"""
        # Genero campioni audio di test
        samples, sample_rate = self.generate_test_samples(1000)
        
        # Imposto lo stesso buffer per entrambi i controller
        buffer_ms = 20
        self.audio1.set_buffer_size(buffer_ms)
        self.audio2.set_buffer_size(buffer_ms)
        
        # Timestamp comune per la sincronizzazione
        current_time = int(time.time() * 1000)
        sync_time = current_time + 100  # Avvio fra 100ms
        
        # Invio gli stessi campioni a entrambi i controller con lo stesso timestamp
        self.audio1.play_audio_buffer(samples, sync_time)
        self.audio2.play_audio_buffer(samples, sync_time)
        
        # Avvio la riproduzione
        test_file = os.path.join(os.path.dirname(__file__), "test_audio.wav")
        if os.path.exists(test_file):
            self.audio1.play_stream(test_file, buffer_ms)
            self.audio2.play_stream(test_file, buffer_ms)
        
        # Attendo che inizi la riproduzione
        time.sleep(0.2)
        
        # Verifico che entrambi siano attivi
        self.assertTrue(self.audio1.is_active(), "Il controller 1 dovrebbe essere attivo")
        self.assertTrue(self.audio2.is_active(), "Il controller 2 dovrebbe essere attivo")
        
        # Ottengo le latenze
        latency1 = self.audio1.get_current_latency()
        latency2 = self.audio2.get_current_latency()
        
        # La differenza di latenza non dovrebbe superare la soglia di jitter (±5ms)
        latency_diff = abs(latency1 - latency2)
        max_jitter = 5  # ms
        
        self.assertLessEqual(latency_diff, max_jitter, 
                          f"La differenza di latenza ({latency_diff}ms) supera la soglia di jitter ({max_jitter}ms)")
        
        # Fermo la riproduzione
        self.audio1.stop_stream()
        self.audio2.stop_stream()
    
    def test_buffer_adjustment(self):
        """Verifica che la dimensione del buffer possa essere regolata dinamicamente"""
        # Imposto diversi valori di buffer e verifico che siano applicati correttamente
        test_values = [10, 25, 40]
        
        for buffer_ms in test_values:
            self.audio1.set_buffer_size(buffer_ms)
            
            # La latenza dovrebbe essere almeno pari al buffer impostato
            time.sleep(0.1)  # Attendo che il buffer venga aggiornato
            latency = self.audio1.get_current_latency()
            
            # Con una tolleranza di 5ms per l'overhead
            self.assertGreaterEqual(latency, buffer_ms - 5, 
                                 f"La latenza ({latency}ms) dovrebbe essere almeno {buffer_ms}ms")

if __name__ == "__main__":
    unittest.main()
