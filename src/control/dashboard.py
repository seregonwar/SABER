#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Dashboard per visualizzare lo stato della rete SABER
Simulazione per test
"""

import time
import threading
import logging
from typing import Dict, Optional

# Configurazione del logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("dashboard")

class MeshDashboard:
    """Dashboard per il monitoraggio dello stato della rete SABER"""

    def __init__(self, ui_controller):
        # Controller che fornisce i dati della rete
        self.controller = ui_controller
        self.running = False
        self.update_thread: Optional[threading.Thread] = None
        self.last_status: Dict = {}

    def start(self):
        """Avvia la dashboard"""
        self.running = True
        self.update_thread = threading.Thread(target=self._update_loop, daemon=True)
        self.update_thread.start()
        logger.info("Dashboard avviata")

    def stop(self):
        """Ferma la dashboard"""
        self.running = False
        if self.update_thread:
            self.update_thread.join(timeout=1.0)
        logger.info("Dashboard fermata")

    def _update_loop(self):
        """Loop principale di aggiornamento"""
        while self.running:
            try:
                # Ottiene lo stato corrente della rete
                status = self.controller.get_mesh_status()

                # Mostra lo stato nel terminale
                self._print_status(status)

                # Salva lo stato per futuri confronti
                self.last_status = status

                # Attende prima di aggiornare nuovamente
                time.sleep(1.0)

            except Exception as e:
                logger.error(f"Errore nell'aggiornamento della dashboard: {e}")
                time.sleep(2.0)

    def _print_status(self, status: Dict):
        """Stampa lo stato della rete in formato leggibile"""
        if not status or 'error' in status:
            logger.warning(f"Impossibile ottenere lo stato: {status.get('error', 'Errore sconosciuto')}")
            return

        # Verifica se lo stato è significativamente cambiato
        if self.last_status and self._is_similar_status(status, self.last_status):
            return

        # Stampa dettagli di stato
        print("\n===== STATO RETE SABER =====")
        print(f"ID Nodo: {status.get('node_id', 'Sconosciuto')}")
        print(f"Ruolo: {status.get('role', 'Sconosciuto')}")
        print(f"Sincronizzato: {'Sì' if status.get('is_synchronized', False) else 'No'}")
        print(f"Latenza: {status.get('latency_ms', 0)}ms")
        print(f"Livello Buffer: {status.get('buffer_level', 0)}%")
        print(f"Attivo: {'Sì' if status.get('is_active', False) else 'No'}")

        # Lista dei nodi connessi
        print("\nNodi Connessi:")
        for node in status.get('active_nodes', []):
            print(f"  - {node.get('node_id', 'Sconosciuto')} ({node.get('role', 'Sconosciuto')})")
        print("============================\n")

    def _is_similar_status(self, current: Dict, previous: Dict) -> bool:
        """Verifica se lo stato corrente è simile al precedente"""
        if current.get('is_synchronized') != previous.get('is_synchronized'):
            return False

        if abs(current.get('latency_ms', 0) - previous.get('latency_ms', 0)) > 5:
            return False

        if abs(current.get('buffer_level', 0) - previous.get('buffer_level', 0)) > 10:
            return False

        if len(current.get('active_nodes', [])) != len(previous.get('active_nodes', [])):
            return False

        return True

    @property
    def is_running(self) -> bool:
        """Restituisce True se la dashboard è in esecuzione"""
        return self.running
