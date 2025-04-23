#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Simulazione del modulo mesh/bluetooth per test del protocollo SABER
"""

import time
import random
import threading
import socket
import logging
from typing import Dict, List, Optional, Callable

# Costanti per i ruoli dei nodi
RUOLO_MASTRO = "master"
RUOLO_RIPETITORE = "repeater"
RUOLO_RICEVITORE = "sink"

# Configurazione del logger
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("simulazione-mesh")

class RustMesh:
    """Simulazione del modulo RustMesh per test del protocollo SABER"""

    def __init__(self):
        self.id_nodo = None
        self.ruolo = None
        self.indirizzo_bt = None
        self.sincronizzato = False
        self.nodi_attivi = []
        self.thread_sincronizzazione = None
        self.attivo = False
        self.latenza = 15  # Latenza simulata in ms
        self.scostamento_temporale = 0
        self.callback_audio = None
        self.modalità_musica = True

    def inizializza_come_mastro(self, id_nodo: Optional[str] = None, indirizzo_bt: Optional[str] = None) -> bool:
        """Inizializza un nodo mastro"""
        self.id_nodo = id_nodo if id_nodo else f"master-{random.randint(1000, 9999)}"
        self.indirizzo_bt = indirizzo_bt if indirizzo_bt else "00:11:22:33:44:55"
        self.ruolo = RUOLO_MASTRO
        self.sincronizzato = True  # Il nodo mastro è sempre sincronizzato

        logger.info(f"Nodo mastro inizializzato: {self.id_nodo}")

        self.nodi_attivi = [{"id_nodo": self.id_nodo, "ruolo": self.ruolo, "indirizzo": self.indirizzo_bt}]
        self._avvia_thread_sincronizzazione()
        return True

    def inizializza_come_ripetitore(self, id_nodo: Optional[str] = None, indirizzo_bt: Optional[str] = None) -> bool:
        """Inizializza un nodo ripetitore"""
        self.id_nodo = id_nodo if id_nodo else f"repeater-{random.randint(1000, 9999)}"
        self.indirizzo_bt = indirizzo_bt if indirizzo_bt else "11:22:33:44:55:66"
        self.ruolo = RUOLO_RIPETITORE

        logger.info(f"Nodo ripetitore inizializzato: {self.id_nodo}")

        self.nodi_attivi = [{"id_nodo": self.id_nodo, "ruolo": self.ruolo, "indirizzo": self.indirizzo_bt}]
        self._avvia_thread_sincronizzazione()
        return True

    def inizializza_come_ricevitore(self, id_nodo: Optional[str] = None, indirizzo_bt: Optional[str] = None, modalità_musica: bool = True) -> bool:
        """Inizializza un nodo ricevitore"""
        self.id_nodo = id_nodo if id_nodo else f"sink-{random.randint(1000, 9999)}"
        self.indirizzo_bt = indirizzo_bt if indirizzo_bt else "22:33:44:55:66:77"
        self.ruolo = RUOLO_RICEVITORE
        self.modalità_musica = modalità_musica

        logger.info(f"Nodo ricevitore inizializzato: {self.id_nodo}")

        self.nodi_attivi = [{"id_nodo": self.id_nodo, "ruolo": self.ruolo, "indirizzo": self.indirizzo_bt}]
        self._avvia_thread_sincronizzazione()
        return True

    def _avvia_thread_sincronizzazione(self):
        """Avvia un thread per simulare la sincronizzazione"""
        if not self.thread_sincronizzazione:
            self.attivo = True
            self.thread_sincronizzazione = threading.Thread(target=self._simulazione_sincronizzazione, daemon=True)
            self.thread_sincronizzazione.start()

    def _simulazione_sincronizzazione(self):
        """Simula la sincronizzazione tra nodi"""
        if self.ruolo == RUOLO_MASTRO:
            self.sincronizzato = True
            return

        # Per i ruoli RIPETITORE e RICEVITORE, simula un processo di sincronizzazione
        time.sleep(random.uniform(0.5, 2.0))
        self.sincronizzato = True
        self.scostamento_temporale = random.randint(-5, 5)

        # Ciclo principale per simulare variazioni di latenza
        while self.attivo:
            time.sleep(1.0)
            self.latenza = max(10, min(50, self.latenza + random.randint(-5, 5)))

            # Simula disconnessioni occasionali
            if random.random() < 0.05:
                self.sincronizzato = False
                logger.info(f"Nodo {self.id_nodo} temporaneamente desincronizzato")
                time.sleep(random.uniform(0.5, 2.0))
                self.sincronizzato = True
                logger.info(f"Nodo {self.id_nodo} resincronizzato")

    def ottieni_info_nodo(self) -> Dict:
        """Restituisce le informazioni sul nodo attuale"""
        return {
            "id_nodo": self.id_nodo,
            "ruolo": self.ruolo,
            "indirizzo": self.indirizzo_bt,
            "sincronizzato": self.sincronizzato,
            "scostamento_temporale_ms": self.scostamento_temporale
        }

    def ottieni_nodi_attivi(self) -> List[Dict]:
        """Restituisce la lista dei nodi attivi nella rete"""
        nodi = self.nodi_attivi.copy()

        if self.ruolo == RUOLO_MASTRO:
            nodi.append({"id_nodo": "sink-sim-1", "ruolo": RUOLO_RICEVITORE, "indirizzo": "33:44:55:66:77:88"})
            nodi.append({"id_nodo": "repeater-sim-1", "ruolo": RUOLO_RIPETITORE, "indirizzo": "44:55:66:77:88:99"})
        elif self.ruolo == RUOLO_RIPETITORE:
            nodi.append({"id_nodo": "master-sim", "ruolo": RUOLO_MASTRO, "indirizzo": "55:66:77:88:99:AA"})
            nodi.append({"id_nodo": "sink-sim-2", "ruolo": RUOLO_RICEVITORE, "indirizzo": "66:77:88:99:AA:BB"})
        elif self.ruolo == RUOLO_RICEVITORE:
            if random.random() < 0.7:
                nodi.append({"id_nodo": "master-sim", "ruolo": RUOLO_MASTRO, "indirizzo": "77:88:99:AA:BB:CC"})
            else:
                nodi.append({"id_nodo": "repeater-sim-1", "ruolo": RUOLO_RIPETITORE, "indirizzo": "88:99:AA:BB:CC:DD"})

        return nodi

    def registra_nodo(self, id_nodo: str, ruolo: str, indirizzo: Optional[str] = None) -> bool:
        """Registra un nodo remoto nella rete"""
        if not indirizzo:
            indirizzo = f"{random.randint(10, 99)}:{random.randint(10, 99)}:{random.randint(10, 99)}:{random.randint(10, 99)}:{random.randint(10, 99)}:{random.randint(10, 99)}"

        nuovo_nodo = {"id_nodo": id_nodo, "ruolo": ruolo, "indirizzo": indirizzo}

        for nodo in self.nodi_attivi:
            if nodo["id_nodo"] == id_nodo:
                return True  # Nodo già registrato

        self.nodi_attivi.append(nuovo_nodo)
        logger.info(f"Nuovo nodo registrato: {id_nodo} ({ruolo})")
        return True

    def è_sincronizzato(self) -> bool:
        """Controlla se il nodo è sincronizzato"""
        return self.sincronizzato

    def ottieni_latenza_corrente(self) -> int:
        """Restituisce la latenza attuale della rete in ms"""
        return self.latenza

    def avvia_riproduzione_audio(self) -> bool:
        """Avvia la riproduzione audio in modalità ricevitore"""
        if self.ruolo != RUOLO_RICEVITORE:
            logger.warning("Solo i nodi RICEVITORI possono avviare la riproduzione audio")
            return False

        logger.info("Avvio della riproduzione audio in modalità ricevitore")
        return True

    def ferma_riproduzione_audio(self) -> bool:
        """Ferma la riproduzione audio in modalità ricevitore"""
        if self.ruolo != RUOLO_RICEVITORE:
            return False

        logger.info("Arresto della riproduzione audio in modalità ricevitore")
        return True

    def imposta_callback_audio(self, callback: Callable) -> None:
        """Imposta un callback per ricevere dati audio"""
        self.callback_audio = callback

    def pulizia(self) -> None:
        """Libera le risorse utilizzate"""
        self.attivo = False
        if self.thread_sincronizzazione:
            self.thread_sincronizzazione.join(timeout=1.0)
        logger.info(f"Nodo {self.id_nodo} disconnesso")


# Per test veloci
if __name__ == "__main__":
    mesh = RustMesh()
    mesh.inizializza_come_mastro()
    print(mesh.ottieni_info_nodo())
    print(mesh.ottieni_nodi_attivi())
    time.sleep(2)
    print(f"Sincronizzato: {mesh.è_sincronizzato()}")
    mesh.pulizia()
