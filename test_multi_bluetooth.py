import subprocess
import time
import sys
import os
import signal

# Sostituisci questi valori con i MAC address reali dei tuoi dispositivi Bluetooth
BT_ADDR_1 = "XX:XX:XX:XX:XX:01"
BT_ADDR_2 = "XX:XX:XX:XX:XX:02"

PYTHON_EXE = sys.executable  # Usa lo stesso interprete Python corrente
UI_SCRIPT = os.path.abspath("src/control/ui.py")

def start_sink(bt_addr, node_id):
    return subprocess.Popen(
        [PYTHON_EXE, UI_SCRIPT, "--role", "sink", "--bt", bt_addr, "--id", node_id],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )

def start_master():
    return subprocess.Popen(
        [PYTHON_EXE, UI_SCRIPT, "--role", "master", "--id", "master-1"],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True
    )

def read_status(proc):
    """Legge le ultime righe di output del processo."""
    try:
        lines = []
        while True:
            line = proc.stdout.readline()
            if not line:
                break
            lines.append(line.strip())
        return lines
    except Exception:
        return []

def main():
    print("Avvio test automatico multi-Bluetooth...")
    sink1 = start_sink(BT_ADDR_1, "sink-1")
    sink2 = start_sink(BT_ADDR_2, "sink-2")
    time.sleep(3)  # Attendi che i sink si inizializzino
    master = start_master()
    time.sleep(5)  # Attendi che la rete si stabilizzi

    try:
        # Monitora lo stato per 30 secondi
        for t in range(30):
            print(f"[{t+1}/30] Controllo stato...")
            out1 = read_status(sink1)
            out2 = read_status(sink2)
            outm = read_status(master)

            # Cerca segni di sincronizzazione e connessione
            for out, name in zip([out1, out2], ["Sink1", "Sink2"]):
                if any("Sincronizzato: Sì" in l or "Synchronized: Yes" in l for l in out):
                    print(f"{name}: OK - sincronizzato")
                else:
                    print(f"{name}: NON sincronizzato")

            if any("Sincronizzato: Sì" in l or "Synchronized: Yes" in l for l in outm):
                print("Master: OK - sincronizzato")
            else:
                print("Master: NON sincronizzato")

            time.sleep(1)

    finally:
        print("Termino i processi...")
        for proc in [sink1, sink2, master]:
            if proc.poll() is None:
                if os.name == "nt":
                    proc.send_signal(signal.CTRL_BREAK_EVENT)
                else:
                    proc.terminate()
                proc.wait(timeout=3)

if __name__ == "__main__":
    main()
