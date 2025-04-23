import sys
import os
import asyncio
from PyQt5 import QtWidgets, QtCore
import subprocess
import re

PYTHON_EXE = sys.executable
UI_SCRIPT = os.path.abspath("src/control/ui.py")

class DeviceScanThread(QtCore.QThread):
    devices_found = QtCore.pyqtSignal(list)

    def run(self):
        devices = []
        cmd = [
            "powershell",
            "-Command",
            "Get-PnpDevice -Class Bluetooth | Where-Object { $_.Status -eq 'OK' } | Select-Object FriendlyName, InstanceId"
        ]
        result = subprocess.run(cmd, capture_output=True, text=True)
        for line in result.stdout.splitlines():
            match = re.match(r"^(?P<name>.+?)\s{2,}(?P<id>.+BTHENUM.+)$", line.strip())
            if match:
                name = match.group('name').strip()
                instance_id = match.group('id').strip()
                mac_match = re.search(r'([0-9A-Fa-f]{12})', instance_id.replace('_', ''))
                if mac_match:
                    mac = ':'.join(mac_match.group(1)[i:i+2] for i in range(0, 12, 2)).upper()
                    devices.append({'name': name, 'address': mac})
        self.devices_found.emit(devices)

class SaberOrchestratorWindow(QtWidgets.QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("SABER Orchestrator (PyQt5)")
        self.setGeometry(100, 100, 600, 400)
        self.devices = []
        self.sink_procs = []
        self.master_proc = None
        self.wav_file = None
        self.setup_ui()

    def setup_ui(self):
        layout = QtWidgets.QVBoxLayout(self)

        self.scan_btn = QtWidgets.QPushButton("Scansiona dispositivi Bluetooth")
        self.scan_btn.clicked.connect(self.scan_devices)
        layout.addWidget(self.scan_btn)

        self.device_list = QtWidgets.QListWidget()
        self.device_list.setSelectionMode(QtWidgets.QAbstractItemView.MultiSelection)
        layout.addWidget(self.device_list)

        self.wav_btn = QtWidgets.QPushButton("Scegli file WAV")
        self.wav_btn.clicked.connect(self.choose_wav)
        layout.addWidget(self.wav_btn)

        self.wav_label = QtWidgets.QLabel("Nessun file selezionato")
        layout.addWidget(self.wav_label)

        self.start_btn = QtWidgets.QPushButton("Avvia Test")
        self.start_btn.clicked.connect(self.start_test)
        self.start_btn.setEnabled(False)
        layout.addWidget(self.start_btn)

        self.stop_btn = QtWidgets.QPushButton("Ferma Tutto")
        self.stop_btn.clicked.connect(self.stop_test)
        self.stop_btn.setEnabled(False)
        layout.addWidget(self.stop_btn)

        self.status_box = QtWidgets.QTextEdit()
        self.status_box.setReadOnly(True)
        layout.addWidget(self.status_box)

        self.device_list.itemSelectionChanged.connect(self.update_start_btn)

    def scan_devices(self):
        self.status_box.append("Scansione dispositivi Bluetooth in corso...")
        self.device_list.clear()
        self.scan_btn.setEnabled(False)
        self.scan_thread = DeviceScanThread()
        self.scan_thread.devices_found.connect(self.populate_devices)
        self.scan_thread.finished.connect(lambda: self.scan_btn.setEnabled(True))
        self.scan_thread.start()

    def populate_devices(self, devices):
        self.devices = devices
        self.device_list.clear()
        for idx, d in enumerate(devices):
            name = d['name'] or "Sconosciuto"
            self.device_list.addItem(f"[{idx}] {name} ({d['address']})")
        self.status_box.append(f"Trovati {len(devices)} dispositivi connessi.")
        self.update_start_btn()

    def choose_wav(self):
        file, _ = QtWidgets.QFileDialog.getOpenFileName(self, "Scegli file WAV", "", "WAV files (*.wav)")
        if file:
            self.wav_file = file
            self.wav_label.setText(os.path.basename(file))
            self.status_box.append(f"File WAV selezionato: {file}")
            self.update_start_btn()

    def update_start_btn(self):
        if self.wav_file and len(self.device_list.selectedItems()) == 2:
            self.start_btn.setEnabled(True)
        else:
            self.start_btn.setEnabled(False)

    def start_test(self):
        sel = self.device_list.selectedIndexes()
        if len(sel) != 2 or not self.wav_file:
            QtWidgets.QMessageBox.critical(self, "Errore", "Seleziona due dispositivi e un file WAV.")
            return
        addr1 = self.devices[sel[0].row()]['address']
        addr2 = self.devices[sel[1].row()]['address']
        self.sink_procs = [
            subprocess.Popen([PYTHON_EXE, UI_SCRIPT, "--role", "sink", "--bt", addr1, "--id", "sink-1"]),
            subprocess.Popen([PYTHON_EXE, UI_SCRIPT, "--role", "sink", "--bt", addr2, "--id", "sink-2"])
        ]
        self.status_box.append("Avvio Master...")
        self.master_proc = subprocess.Popen([
            PYTHON_EXE, UI_SCRIPT, "--role", "master", "--wav", self.wav_file, "--id", "master-1"
        ])
        self.start_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self.status_box.append("Test in corso! Premi 'Ferma Tutto' per terminare.")

    def stop_test(self):
        self.status_box.append("Terminazione processi...")
        for proc in self.sink_procs + ([self.master_proc] if self.master_proc else []):
            if proc and proc.poll() is None:
                proc.terminate()
        self.sink_procs = []
        self.master_proc = None
        self.stop_btn.setEnabled(False)
        self.start_btn.setEnabled(True)
        self.status_box.append("Tutti i processi terminati.")

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    window = SaberOrchestratorWindow()
    window.show()
    sys.exit(app.exec_())
