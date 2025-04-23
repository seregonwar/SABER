#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Interfaccia utente per SABER Protocol
Gestisce il controllo dei nodi e la configurazione
"""

import os
import sys
import time
import argparse
import asyncio
import logging
from typing import Dict, List, Optional, Tuple, Union

# Importiamo i nostri moduli compilati da Rust e C++
try:
    from libpy_mesh import RustMesh, ROLE_MASTER, ROLE_REPEATER, ROLE_SINK
    from libpy_audio import AudioController, DEFAULT_SAMPLE_RATE_MUSIC
    from dashboard import MeshDashboard
except ImportError as e:
    print(f"Errore importazione moduli: {e}")
    print("Assicurati di aver compilato correttamente i binding Rust e C++")
    sys.exit(1)

# Configurazione logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("saber-ui")

class SaberUI:
    """Interfaccia utente principale per il protocollo SABER"""
    
    def __init__(self):
        self.mesh = RustMesh()
        self.audio = AudioController()
        self.dashboard = None
        self.node_id = None
        self.node_role = None
        self.connected_nodes = []
        self.is_initialized = False
        self.is_running = False
    
    async def initialize(self, role: str, node_id: Optional[str] = None, 
                         bt_address: Optional[str] = None, is_music: bool = True) -> bool:
        """Inizializza il nodo con il ruolo specificato"""
        logger.info(f"Inizializzazione nodo SABER con ruolo {role}")
        
        try:
            if role.lower() == ROLE_MASTER:
                success = self.mesh.init_as_master(node_id, bt_address)
                if success:
                    self.node_role = ROLE_MASTER
            elif role.lower() == ROLE_REPEATER:
                success = self.mesh.init_as_repeater(node_id, bt_address)
                if success:
                    self.node_role = ROLE_REPEATER
            elif role.lower() == ROLE_SINK:
                success = self.mesh.init_as_sink(node_id, bt_address, is_music)
                if success:
                    self.node_role = ROLE_SINK
            else:
                logger.error(f"Ruolo non valido: {role}")
                return False
            
            # Ottengo l'ID assegnato al nodo
            node_info = self.mesh.get_node_info()
            self.node_id = node_info["node_id"]
            
            # Inizializzo l'audio controller
            sample_rate = DEFAULT_SAMPLE_RATE_MUSIC if is_music else 16000
            audio_success = self.audio.initialize(sample_rate, 2)  # Stereo
            
            # Collego il timestamp provider dalla mesh al controller audio
            self.audio.set_time_provider(lambda: int(time.time() * 1000))
            
            if success and audio_success:
                self.is_initialized = True
                logger.info(f"Nodo inizializzato: ID={self.node_id}, Ruolo={self.node_role}")
                
                # Attendo che la mesh si sincronizzi
                for _ in range(10):  # Timeout di 5 secondi
                    if self.mesh.is_synchronized():
                        break
                    await asyncio.sleep(0.5)
                
                return True
            else:
                logger.error("Errore inizializzazione")
                return False
                
        except Exception as e:
            logger.error(f"Errore durante l'inizializzazione: {e}")
            return False
    
    async def start(self) -> bool:
        """Avvia il protocollo e la riproduzione audio"""
        if not self.is_initialized:
            logger.error("Impossibile avviare: nodo non inizializzato")
            return False
        
        try:
            # Avvio la riproduzione audio
            if self.node_role in [ROLE_MASTER, ROLE_SINK]:
                logger.info("Avvio riproduzione audio...")
                if self.node_role == ROLE_MASTER:
                    # Solo il master riproduce un file audio
                    audio_file = os.path.expanduser("~/Music/test_audio.wav")
                    self.audio.play_stream(audio_file)
                else:
                    # I sink ricevono lo stream dalla mesh
                    self.mesh.start_audio_playback()
            
            # Aggiorno lo stato
            self.is_running = True
            
            # Avvio il dashboard se richiesto
            if self.dashboard:
                logger.info("Avvio dashboard...")
                self.dashboard.start()
            
            return True
        
        except Exception as e:
            logger.error(f"Errore durante l'avvio: {e}")
            return False
    
    def stop(self) -> bool:
        """Ferma il protocollo e la riproduzione"""
        if not self.is_running:
            return True
        
        try:
            # Fermo la riproduzione audio
            self.audio.stop_stream()
            
            # Fermo la ricezione audio
            if self.node_role == ROLE_SINK:
                self.mesh.stop_audio_playback()
            
            # Aggiorno lo stato
            self.is_running = False
            
            # Arresto il dashboard se attivo
            if self.dashboard and self.dashboard.is_running:
                self.dashboard.stop()
            
            logger.info("Protocollo SABER arrestato")
            return True
        
        except Exception as e:
            logger.error(f"Errore durante l'arresto: {e}")
            return False
    
    async def get_mesh_status(self) -> Dict:
        """Ottiene lo stato della rete mesh"""
        try:
            # Ottengo le informazioni sul nodo locale
            local_info = self.mesh.get_node_info()
            
            # Ottengo la lista di nodi attivi
            active_nodes = self.mesh.get_active_nodes()
            
            # Ottengo le informazioni di latenza e buffer
            latency = self.mesh.get_current_latency()
            buffer_level = self.audio.get_buffer_level()
            is_active = self.audio.is_active()
            
            # Creo un dizionario con lo stato completo
            status = {
                "node_id": self.node_id,
                "role": self.node_role,
                "is_synchronized": local_info["is_synchronized"],
                "latency_ms": latency,
                "buffer_level": buffer_level,
                "is_active": is_active,
                "active_nodes": active_nodes
            }
            
            return status
        
        except Exception as e:
            logger.error(f"Errore recupero stato mesh: {e}")
            return {"error": str(e)}
    
    def enable_dashboard(self, enable: bool = True) -> None:
        """Abilita o disabilita la dashboard grafica"""
        if enable and not self.dashboard:
            self.dashboard = MeshDashboard(self)
        elif not enable:
            self.dashboard = None
    
    def register_remote_node(self, node_id: str, role: str, address: Optional[str] = None) -> bool:
        """Registra un nodo remoto nella rete mesh"""
        try:
            success = self.mesh.register_node(node_id, role, address)
            if success:
                logger.info(f"Nodo registrato: {node_id} ({role})")
            return success
        except Exception as e:
            logger.error(f"Errore registrazione nodo: {e}")
            return False
    
    def get_audio_latency(self) -> int:
        """Ottiene la latenza audio corrente in millisecondi"""
        return self.audio.get_current_latency()
    
    def adjust_buffer_size(self, buffer_ms: int) -> None:
        """Regola la dimensione del buffer audio"""
        self.audio.set_buffer_size(buffer_ms)
        logger.info(f"Buffer audio regolato a {buffer_ms}ms")


async def main():
    """Funzione principale"""
    parser = argparse.ArgumentParser(description="SABER Protocol UI")
    parser.add_argument("--role", type=str, required=True, choices=["master", "repeater", "sink"], 
                        help="Ruolo del nodo nella rete mesh")
    parser.add_argument("--id", type=str, help="ID del nodo (opzionale)")
    parser.add_argument("--bt", type=str, help="Indirizzo Bluetooth (opzionale)")
    parser.add_argument("--voice", action="store_true", help="Modalità voce (16kHz) anziché musica (48kHz)")
    parser.add_argument("--dashboard", action="store_true", help="Avvia la dashboard grafica")
    
    args = parser.parse_args()
    
    # Creo e inizializzo l'interfaccia
    ui = SaberUI()
    
    # Abilito la dashboard se richiesto
    if args.dashboard:
        ui.enable_dashboard()
    
    # Inizializzo il nodo col ruolo specificato
    success = await ui.initialize(args.role, args.id, args.bt, not args.voice)
    if not success:
        logger.error("Inizializzazione fallita")
        return 1
    
    # Avvio il protocollo
    success = await ui.start()
    if not success:
        logger.error("Avvio fallito")
        return 1
    
    # Stampo il status iniziale
    status = await ui.get_mesh_status()
    logger.info(f"Stato SABER: {status}")
    
    try:
        # Loop principale che aggiorna periodicamente lo stato
        while True:
            await asyncio.sleep(1)
            status = await ui.get_mesh_status()
            
            # Stampo solo se ci sono cambiamenti significativi
            if status.get("is_synchronized") is False or status.get("latency_ms", 0) > 30:
                logger.info(f"Stato SABER: {status}")
    
    except KeyboardInterrupt:
        logger.info("Interruzione richiesta dall'utente")
    finally:
        # Arresto pulito
        ui.stop()
    
    return 0


if __name__ == "__main__":
    sys.exit(asyncio.run(main()))
