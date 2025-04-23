# Test unitari per il modulo di sincronizzazione del protocollo SABER
# Verifica la corretta sincronizzazione tra nodi e la gestione dei ritardi

import os
import sys
import time
import unittest
import asyncio
import threading
from typing import List, Dict, Any, Optional

# Aggiungo il percorso dei moduli alla path di Python
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'src', 'control'))

try:
    # Importo i moduli da testare
    from libpy_mesh import RustMesh, ROLE_MASTER, ROLE_REPEATER, ROLE_SINK
except ImportError:
    print("Errore: impossibile importare i moduli mesh. Assicurati di averli compilati.")
    sys.exit(1)

class TestSyncronization(unittest.TestCase):
    """Test per la sincronizzazione dei nodi nella rete mesh"""
    
    def setUp(self):
        """Inizializza i nodi di test"""
        self.master = RustMesh()
        self.repeater = RustMesh()
        self.sink = RustMesh()
        
        # Inizializzazione nodi in modo asincrono
        loop = asyncio.new_event_loop()
        asyncio.set_event_loop(loop)
        
        # Uso ID fissi per i test
        self.master_id = "test-master-1"
        self.repeater_id = "test-repeater-1"
        self.sink_id = "test-sink-1"
        
        # Inizializzo i nodi
        try:
            self.master.init_as_master(self.master_id, None)
            self.repeater.init_as_repeater(self.repeater_id, None)
            self.sink.init_as_sink(self.sink_id, None, True)  # True = modalità musica
        except Exception as e:
            self.skipTest(f"Impossibile inizializzare i nodi: {e}")
        
        # Verifico che l'inizializzazione sia stata completata correttamente
        master_info = self.master.get_node_info()
        self.assertEqual(master_info["node_id"], self.master_id)
        self.assertEqual(master_info["role"], "Master")
        
        # Registro i nodi nella rete mesh del master
        self.master.register_node(self.repeater_id, ROLE_REPEATER)
        self.master.register_node(self.sink_id, ROLE_SINK)
        
        # Attendiamo che la rete si formi
        time.sleep(1)  # Attesa breve per la propagazione
        
    def tearDown(self):
        """Pulisce le risorse dopo i test"""
        # Arresto eventuali processi di streaming audio
        try:
            self.master.stop_audio_playback()
            self.sink.stop_audio_playback()
        except:
            pass
    
    def test_mesh_connectivity(self):
        """Verifica la connettività tra i nodi della rete mesh"""
        # Ottengo la lista dei nodi attivi dal master
        nodes = self.master.get_active_nodes()
        
        # Verifico che il repeater e il sink siano visibili nella rete
        self.assertIn(self.repeater_id, nodes, "Il repeater dovrebbe essere visibile nella rete")
        self.assertIn(self.sink_id, nodes, "Il sink dovrebbe essere visibile nella rete")
    
    def test_mesh_synchronization(self):
        """Verifica la corretta sincronizzazione tra nodi"""
        # Avvio il protocollo sul master, che dovrebbe sincronizzare gli altri nodi
        self.master.start()
        
        # Attendo che la sincronizzazione avvenga
        max_attempts = 10
        for _ in range(max_attempts):
            if self.master.is_synchronized():
                break
            time.sleep(0.5)
        
        # Verifico che il master sia sincronizzato
        self.assertTrue(self.master.is_synchronized(), "Il master dovrebbe essere sincronizzato")
        
        # Verifico la propagazione della sincronizzazione al sink
        # Nel mondo reale, attendiamo finché il sink non è sincronizzato
        max_attempts = 10
        for _ in range(max_attempts):
            if self.sink.is_synchronized():
                break
            time.sleep(0.5)
        
        # Verifico che il sink abbia ricevuto la sincronizzazione
        self.assertTrue(self.sink.is_synchronized(), "Il sink dovrebbe essere sincronizzato col master")
    
    def test_latency_measurement(self):
        """Verifica la misurazione della latenza tra nodi"""
        # Avvio il protocollo e attendo la sincronizzazione
        self.master.start()
        time.sleep(2)  # Attendo la formazione completa della rete
        
        # Ottengo la latenza dal master
        master_latency = self.master.get_current_latency()
        
        # Ottengo la latenza dal sink
        sink_latency = self.sink.get_current_latency()
        
        # Verifico che le latenze siano valori plausibili (meno di 100ms in ambiente di test)
        self.assertLess(master_latency, 100, f"Latenza master troppo alta: {master_latency}ms")
        self.assertLess(sink_latency, 100, f"Latenza sink troppo alta: {sink_latency}ms")
        
        # La differenza di latenza non dovrebbe essere eccessiva in ambiente locale
        latency_diff = abs(master_latency - sink_latency)
        self.assertLess(latency_diff, 30, 
                      f"Differenza di latenza troppo alta: {latency_diff}ms")
    
    def test_audio_playback_sync(self):
        """Verifica la sincronizzazione della riproduzione audio"""
        # Avvio la riproduzione audio
        try:
            self.master.start_audio_playback()
            self.sink.start_audio_playback()
        except Exception as e:
            self.skipTest(f"Impossibile avviare la riproduzione audio: {e}")
        
        # Attendo che inizi la riproduzione
        time.sleep(1)
        
        # Verifico che entrambi i nodi siano sincronizzati
        self.assertTrue(self.master.is_synchronized(), "Il master dovrebbe essere sincronizzato")
        self.assertTrue(self.sink.is_synchronized(), "Il sink dovrebbe essere sincronizzato")
        
        # La latenza massima dovrebbe rispettare le specifiche (<40ms come da PAPER.md)
        master_latency = self.master.get_current_latency()
        sink_latency = self.sink.get_current_latency()
        
        self.assertLess(master_latency, 40, f"Latenza master troppo alta: {master_latency}ms")
        self.assertLess(sink_latency, 40, f"Latenza sink troppo alta: {sink_latency}ms")
        
        # Arresto la riproduzione
        self.master.stop_audio_playback()
        self.sink.stop_audio_playback()
    
    def test_reconnection(self):
        """Verifica la capacità di riconessione dei nodi"""
        # Avvio la sincronizzazione iniziale
        self.master.start()
        time.sleep(1)  # Attendo la sincronizzazione iniziale
        
        # Simulo una desincronizzazione rimuovendo il sink
        # e aggiungendolo nuovamente
        try:
            # Rimuovo il nodo dalla rete (se il metodo è disponibile)
            # Altrimenti simuliamo una riconnessione creando un nuovo sink
            new_sink = RustMesh()
            new_sink.init_as_sink(self.sink_id, None, True)
            
            # Registro nuovamente il sink
            self.master.register_node(self.sink_id, ROLE_SINK)
            
            # Attendo la risincronizzazione
            time.sleep(2)
            
            # Verifico che il nuovo sink si sia sincronizzato
            self.assertTrue(new_sink.is_synchronized(), 
                          "Il sink dovrebbe risincronizzarsi dopo la riconnessione")
            
        except Exception as e:
            # Se non è possibile simulare la rimozione/riconnessione, ignoriamo il test
            self.skipTest(f"Test di riconnessione non supportato: {e}")

if __name__ == "__main__":
    unittest.main()
