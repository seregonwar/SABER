import asyncio
import subprocess
import sys
import os
from bleak import BleakScanner
import tkinter as tk
from tkinter import filedialog, messagebox
from threading import Thread

PYTHON_EXE = sys.executable
UI_SCRIPT = os.path.abspath("src/control/ui.py")

class SaberOrchestratorGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("SABER Orchestrator")
        self.devices = []
        self.selected_devices = []
        self.wav_file = None
        self.sink_procs = []
        self.master_proc = None
        self.setup_ui()

    def setup_ui(self):
        frm = tk.Frame(self.root)
        frm.pack(padx=10, pady=10)
        
        self.scan_btn = tk.Button(frm, text="Scansiona dispositivi Bluetooth", command=self.scan_devices)
        self.scan_btn.grid(row=0, column=0, columnspan=2, pady=5)
        
        self.device_listbox = tk.Listbox(frm, selectmode=tk.MULTIPLE, width=50)
        self.device_listbox.grid(row=1, column=0, columnspan=2, pady=5)
        
        self.wav_btn = tk.Button(frm, text="Scegli file WAV", command=self.choose_wav)
        self.wav_btn.grid(row=2, column=0, pady=5)
        self.wav_label = tk.Label(frm, text="Nessun file selezionato")
        self.wav_label.grid(row=2, column=1, pady=5)
        
        self.start_btn = tk.Button(frm, text="Avvia Test", command=self.start_test, state=tk.DISABLED)
        self.start_btn.grid(row=3, column=0, pady=10)
        self.stop_btn = tk.Button(frm, text="Ferma Tutto", command=self.stop_test, state=tk.DISABLED)
        self.stop_btn.grid(row=3, column=1, pady=10)
        
        self.status_text = tk.Text(frm, height=10, width=60, state=tk.DISABLED)
        self.status_text.grid(row=4, column=0, columnspan=2, pady=5)

    def scan_devices(self):
        self.status("Scansione dispositivi Bluetooth...")
        self.device_listbox.delete(0, tk.END)
        def scan():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            devices = loop.run_until_complete(BleakScanner.discover(timeout=5.0))
            self.devices = devices
            for idx, d in enumerate(devices):
                name = d.name or "Sconosciuto"
                self.device_listbox.insert(tk.END, f"[{idx}] {name} ({d.address})")
            self.status(f"Trovati {len(devices)} dispositivi.")
        Thread(target=scan, daemon=True).start()

    def choose_wav(self):
        file = filedialog.askopenfilename(filetypes=[("WAV files", "*.wav")])
        if file:
            self.wav_file = file
            self.wav_label.config(text=os.path.basename(file))
            self.status(f"File WAV selezionato: {file}")
            self.update_start_btn()

    def update_start_btn(self):
        if self.wav_file and len(self.device_listbox.curselection()) == 2:
            self.start_btn.config(state=tk.NORMAL)
        else:
            self.start_btn.config(state=tk.DISABLED)

    def start_test(self):
        sel = self.device_listbox.curselection()
        if len(sel) != 2 or not self.wav_file:
            messagebox.showerror("Errore", "Seleziona due dispositivi e un file WAV.")
            return
        addr1 = self.devices[sel[0]].address
        addr2 = self.devices[sel[1]].address
        self.status(f"Avvio Sink 1 ({addr1}) e Sink 2 ({addr2})...")
        self.sink_procs = [
            subprocess.Popen([PYTHON_EXE, UI_SCRIPT, "--role", "sink", "--bt", addr1, "--id", "sink-1"]),
            subprocess.Popen([PYTHON_EXE, UI_SCRIPT, "--role", "sink", "--bt", addr2, "--id", "sink-2"])
        ]
        self.status("Avvio Master...")
        self.master_proc = subprocess.Popen([
            PYTHON_EXE, UI_SCRIPT, "--role", "master", "--wav", self.wav_file, "--id", "master-1"
        ])
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self.status("Test in corso! Premi 'Ferma Tutto' per terminare.")

    def stop_test(self):
        self.status("Terminazione processi...")
        for proc in self.sink_procs + ([self.master_proc] if self.master_proc else []):
            if proc and proc.poll() is None:
                proc.terminate()
        self.sink_procs = []
        self.master_proc = None
        self.stop_btn.config(state=tk.DISABLED)
        self.start_btn.config(state=tk.NORMAL)
        self.status("Tutti i processi terminati.")

    def status(self, msg):
        self.status_text.config(state=tk.NORMAL)
        self.status_text.insert(tk.END, msg + "\n")
        self.status_text.see(tk.END)
        self.status_text.config(state=tk.DISABLED)

if __name__ == "__main__":
    root = tk.Tk()
    app = SaberOrchestratorGUI(root)
    root.mainloop()
