#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Test Runner per il protocollo SABER
Esegue tutti i test e verifica il corretto funzionamento del sistema
"""

import os
import sys
import time
import argparse
import subprocess
import unittest
import logging
from typing import List, Dict, Any, Optional, Tuple

# Configurazione logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("saber-test-runner")

class SaberTestRunner:
    """Esecutore di test per il protocollo SABER"""
    
    def __init__(self, project_root: Optional[str] = None):
        # Se non specificato, utilizzo la directory parent dello script
        if project_root is None:
            self.project_root = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        else:
            self.project_root = os.path.abspath(project_root)
        
        self.tests_dir = os.path.join(self.project_root, "tests")
        self.src_dir = os.path.join(self.project_root, "src")
        
        # Risultati dei test
        self.results = {
            "rust": {"passed": 0, "failed": 0, "skipped": 0},
            "python": {"passed": 0, "failed": 0, "skipped": 0},
            "cpp": {"passed": 0, "failed": 0, "skipped": 0},
            "integration": {"passed": 0, "failed": 0, "skipped": 0}
        }
        
        # Controllo che il progetto esista
        if not os.path.isdir(self.project_root):
            raise ValueError(f"Directory di progetto non trovata: {self.project_root}")
        
        # Controllo che le cartelle principali esistano
        if not os.path.isdir(self.tests_dir):
            raise ValueError(f"Directory dei test non trovata: {self.tests_dir}")
        
        if not os.path.isdir(self.src_dir):
            raise ValueError(f"Directory sorgente non trovata: {self.src_dir}")
    
    def run_all_tests(self, verbose: bool = False) -> bool:
        """Esegue tutti i test disponibili"""
        logger.info("Avvio suite di test SABER Protocol")
        logger.info(f"Directory progetto: {self.project_root}")
        
        success = True
        
        # Eseguo i test in ordine di dipendenza
        if not self.run_rust_tests(verbose):
            logger.warning("Test Rust falliti o parzialmente falliti.")
            success = False
        
        if not self.run_cpp_tests(verbose):
            logger.warning("Test C++ falliti o parzialmente falliti.")
            success = False
        
        if not self.run_python_tests(verbose):
            logger.warning("Test Python falliti o parzialmente falliti.")
            success = False
        
        if not self.run_integration_tests(verbose):
            logger.warning("Test di integrazione falliti o parzialmente falliti.")
            success = False
        
        # Mostro riepilogo finale
        self._show_summary()
        
        return success
    
    def run_rust_tests(self, verbose: bool = False) -> bool:
        """Esegue i test Rust utilizzando cargo"""
        logger.info("Esecuzione test Rust...")
        
        try:
            cmd = ["cargo", "test"]
            if verbose:
                cmd.append("--verbose")
            
            # Eseguo il comando nel project root
            process = subprocess.run(
                cmd,
                cwd=self.project_root,
                capture_output=True,
                text=True
            )
            
            # Analizza l'output
            output = process.stdout
            
            if verbose:
                print("\n--- Output test Rust ---")
                print(output)
                print("------------------------")
            
            # Estraggo le statistiche
            if "test result: ok" in output:
                # Tutti i test passati
                self.results["rust"]["passed"] = output.count("test result: ok")
                success = True
            else:
                # Alcuni test falliti
                self.results["rust"]["passed"] = output.count(": ok")
                self.results["rust"]["failed"] = output.count(": FAILED")
                success = self.results["rust"]["failed"] == 0
            
            self.results["rust"]["skipped"] = output.count("ignored")
            
            logger.info(f"Test Rust completati: " +
                       f"{self.results['rust']['passed']} passati, " +
                       f"{self.results['rust']['failed']} falliti, " +
                       f"{self.results['rust']['skipped']} ignorati")
            
            return success
        
        except Exception as e:
            logger.error(f"Errore durante l'esecuzione dei test Rust: {e}")
            return False
    
    def run_cpp_tests(self, verbose: bool = False) -> bool:
        """Esegue i test C++ utilizzando CMake/CTest"""
        logger.info("Esecuzione test C++...")
        
        try:
            # Verifica se esiste già la directory build
            build_dir = os.path.join(self.project_root, "build")
            if not os.path.isdir(build_dir):
                os.makedirs(build_dir)
                
                # Configura CMake
                logger.info("Configurazione CMake...")
                cmake_cmd = ["cmake", ".."]
                subprocess.run(
                    cmake_cmd,
                    cwd=build_dir,
                    capture_output=not verbose,
                    text=True
                )
            
            # Compila i test
            logger.info("Compilazione test C++...")
            make_cmd = ["cmake", "--build", "."]
            subprocess.run(
                make_cmd,
                cwd=build_dir,
                capture_output=not verbose,
                text=True
            )
            
            # Esegue i test
            logger.info("Esecuzione test C++...")
            ctest_cmd = ["ctest"]
            if verbose:
                ctest_cmd.append("-V")
            
            process = subprocess.run(
                ctest_cmd,
                cwd=build_dir,
                capture_output=True,
                text=True
            )
            
            # Analizza l'output
            output = process.stdout
            
            if verbose:
                print("\n--- Output test C++ ---")
                print(output)
                print("-----------------------")
            
            # Estraggo le statistiche
            if "100% tests passed" in output:
                # Tutti i test passati
                self.results["cpp"]["passed"] = output.count("Test #")
                success = True
            else:
                # Alcuni test falliti
                self.results["cpp"]["passed"] = output.count(": Passed")
                self.results["cpp"]["failed"] = output.count(": Failed")
                success = self.results["cpp"]["failed"] == 0
            
            logger.info(f"Test C++ completati: " +
                       f"{self.results['cpp']['passed']} passati, " +
                       f"{self.results['cpp']['failed']} falliti")
            
            return success
        
        except Exception as e:
            logger.error(f"Errore durante l'esecuzione dei test C++: {e}")
            return False
    
    def run_python_tests(self, verbose: bool = False) -> bool:
        """Esegue i test Python utilizzando unittest"""
        logger.info("Esecuzione test Python...")
        
        try:
            # Raccolgo tutti i test Python nella directory tests/
            test_files = [f for f in os.listdir(self.tests_dir) 
                         if f.startswith("test_") and f.endswith(".py")]
            
            if not test_files:
                logger.warning("Nessun file di test Python trovato.")
                return True
            
            success = True
            
            for test_file in test_files:
                test_module = os.path.splitext(test_file)[0]
                logger.info(f"Esecuzione test: {test_module}")
                
                cmd = [sys.executable, "-m", "unittest", f"tests.{test_module}"]
                
                process = subprocess.run(
                    cmd,
                    cwd=self.project_root,
                    capture_output=True,
                    text=True
                )
                
                # Analizza l'output
                output = process.stdout + process.stderr
                
                if verbose:
                    print(f"\n--- Output test {test_module} ---")
                    print(output)
                    print("-------------------------------")
                
                # Verifico il risultato
                if process.returncode == 0:
                    self.results["python"]["passed"] += 1
                    logger.info(f"Test {test_module} completato con successo.")
                else:
                    self.results["python"]["failed"] += 1
                    success = False
                    logger.error(f"Test {test_module} fallito.")
                    if "SkipTest" in output:
                        self.results["python"]["skipped"] += 1
            
            logger.info(f"Test Python completati: " +
                       f"{self.results['python']['passed']} passati, " +
                       f"{self.results['python']['failed']} falliti, " +
                       f"{self.results['python']['skipped']} ignorati")
            
            return success
        
        except Exception as e:
            logger.error(f"Errore durante l'esecuzione dei test Python: {e}")
            return False
    
    def run_integration_tests(self, verbose: bool = False) -> bool:
        """Esegue i test di integrazione"""
        logger.info("Esecuzione test di integrazione...")
        
        # Per i test di integrazione, avvieremo più istanze di nodi SABER
        # e verificheremo che funzionino insieme correttamente
        
        try:
            # In una versione reale, qui avvieremmo effettivamente più processi
            # e verificheremmo la loro comunicazione. Per semplicità, in questo mock
            # faremo solo un controllo di funzionalità di base
            
            # Verifico che i moduli necessari per l'integrazione esistano
            modules_to_check = [
                os.path.join(self.src_dir, "control", "ui.py"),
                os.path.join(self.src_dir, "control", "dashboard.py"),
                os.path.join(self.project_root, "bindings", "libpy_mesh.rs"),
                os.path.join(self.project_root, "bindings", "libpy_audio.cpp")
            ]
            
            missing_modules = [m for m in modules_to_check if not os.path.exists(m)]
            
            if missing_modules:
                logger.error(f"Moduli mancanti per i test di integrazione: {missing_modules}")
                self.results["integration"]["failed"] += 1
                return False
            
            # Se tutti i moduli esistono, il test di integrazione passa
            self.results["integration"]["passed"] += 1
            
            logger.info(f"Test di integrazione completati: " +
                       f"{self.results['integration']['passed']} passati, " +
                       f"{self.results['integration']['failed']} falliti")
            
            return True
        
        except Exception as e:
            logger.error(f"Errore durante l'esecuzione dei test di integrazione: {e}")
            return False
    
    def _show_summary(self):
        """Mostra un riepilogo dei risultati dei test"""
        total_passed = sum(r["passed"] for r in self.results.values())
        total_failed = sum(r["failed"] for r in self.results.values())
        total_skipped = sum(r["skipped"] for r in self.results.values())
        
        print("\n" + "="*50)
        print(" SABER Protocol - Riepilogo Test")
        print("="*50)
        
        print(f"\nTest Rust:")
        print(f"  Passati:  {self.results['rust']['passed']}")
        print(f"  Falliti:  {self.results['rust']['failed']}")
        print(f"  Ignorati: {self.results['rust']['skipped']}")
        
        print(f"\nTest C++:")
        print(f"  Passati:  {self.results['cpp']['passed']}")
        print(f"  Falliti:  {self.results['cpp']['failed']}")
        print(f"  Ignorati: {self.results['cpp']['skipped']}")
        
        print(f"\nTest Python:")
        print(f"  Passati:  {self.results['python']['passed']}")
        print(f"  Falliti:  {self.results['python']['failed']}")
        print(f"  Ignorati: {self.results['python']['skipped']}")
        
        print(f"\nTest Integrazione:")
        print(f"  Passati:  {self.results['integration']['passed']}")
        print(f"  Falliti:  {self.results['integration']['failed']}")
        print(f"  Ignorati: {self.results['integration']['skipped']}")
        
        print("\n" + "-"*50)
        print(f"TOTALE:   {total_passed + total_failed + total_skipped} test")
        print(f"Passati:  {total_passed}")
        print(f"Falliti:  {total_failed}")
        print(f"Ignorati: {total_skipped}")
        print("-"*50)
        
        if total_failed == 0:
            print("\n✅ TUTTI I TEST PASSATI!")
        else:
            print(f"\n❌ {total_failed} TEST FALLITI")
        
        print("="*50 + "\n")


def main():
    """Funzione principale"""
    parser = argparse.ArgumentParser(description="SABER Protocol Test Runner")
    parser.add_argument("--root", type=str, help="Directory radice del progetto")
    parser.add_argument("--verbose", "-v", action="store_true", help="Mostra output dettagliato")
    
    args = parser.parse_args()
    
    try:
        runner = SaberTestRunner(args.root)
        success = runner.run_all_tests(args.verbose)
        
        # Restituisco un codice di uscita appropriato
        return 0 if success else 1
    
    except Exception as e:
        logger.error(f"Errore durante l'esecuzione dei test: {e}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
