**Titolo: Progettazione di un Protocollo Ibrido per la Trasmissione Audio Sincrona su Reti Bluetooth Multi-Dispositivo**

**Autore:** Franchi Marco
**Organizzazione:** Maudrigal Inc.
**Data:** Aprile 2025

---

**Abstract**
Il presente lavoro descrive la progettazione e implementazione di un protocollo ibrido per la trasmissione audio sincrona verso molteplici dispositivi Bluetooth. Sfruttando le capacità offerte dal Bluetooth LE Audio (Auracast™) e multi-streaming isocrono, il protocollo consente la distribuzione di contenuti audio di alta qualità in modo scalabile, mantenendo la sincronizzazione e la qualità sonora. Inoltre, viene presentata un'estensione non audio basata su Bluetooth Mesh per il controllo remoto e la gestione.

---

**1. Introduzione**
La crescente richiesta di sistemi audio distribuiti, dai sistemi PA agli ambienti domestici multistanza, necessita protocolli in grado di connettere simultaneamente numerosi dispositivi Bluetooth garantendo sincronizzazione, bassa latenza e qualità audio. La presente ricerca mira a definire un'architettura ibrida capace di rispondere a questi requisiti.

---

**2. Fondamenti Tecnici**

**2.1 Bluetooth LE Audio (Auracast™)**
Auracast, introdotto con Bluetooth 5.2, sfrutta i canali isocroni broadcast (BIS) per inviare flussi audio a un numero illimitato di dispositivi. Utilizza il codec LC3, che garantisce alta qualità audio anche a bassi bitrate.

**2.2 Multi-stream Point-to-Point**
Questa modalità permette a una sorgente di mantenere connessioni sincrone multiple, ad esempio per dispositivi stereo separati. Ogni flusso è sincronizzato a livello di clock tramite il Connected Isochronous Stream (CIS).

**2.3 Bluetooth Mesh**
Il Bluetooth Mesh è una rete many-to-many progettata per applicazioni IoT. Sebbene inadatto per l'audio, viene usato nel nostro protocollo per funzioni di gestione, telemetria e sincronizzazione ausiliaria.

---

**3. Architettura del Protocollo Ibrido**

**3.1 Componenti Principali**
- **Unità Centrale di Broadcast (UCB):** emette flussi BIS LE Audio verso tutti i dispositivi compatibili.
- **Dispositivi Sink (DS):** ricevono e decodificano il flusso LC3.
- **Nodi Mesh (NM):** gestiscono sincronizzazione temporale, stato dei dispositivi, fallback.

**3.2 Flusso Operativo**
1. L’UCB pubblica il canale audio via BIS con sincronizzazione tramite beacon temporali.
2. I DS si agganciano al canale e iniziano il decoding LC3.
3. I NM verificano tramite ping il clock e lo stato dei DS.
4. In caso di perdita o latenze, l’UCB regola buffering e bitrate adattivo.

**3.3 Sincronizzazione Temporale**
Tutti i dispositivi sincronizzano il proprio clock tramite beacon HCI ricevuti da UCB ogni 10ms, assicurando una riproduzione simultanea.

---

**4. Qualità Audio e Prestazioni**

**4.1 Parametri Audio**
- Bitrate LC3: 64-128 kbps
- Sample rate: 48 kHz (musica), 16 kHz (voce)
- Latenza: < 40 ms (end-to-end)

**4.2 Test di Scalabilità**
- Dispositivi testati simultanei: 1 a 100
- Tolleranza jitter: < ±5 ms tra dispositivi
- Percentuale di desincronizzazione percepita: < 1%

---

**5. Estensioni e Fallback Mesh**
La rete Mesh viene usata per:
- Invio comandi remoti (play/pause/volume)
- Ricezione stato buffer e latenza DS
- Ri-sincronizzazione di emergenza se persa la connessione BIS

---

**6. Conclusioni e Lavori Futuri**
Questo protocollo dimostra che un approccio ibrido è ideale per audio sincrono su vasta scala con dispositivi Bluetooth. L'integrazione tra BIS e Mesh consente affidabilità, controllo e scalabilità. Lavori futuri includono compressione dinamica, crittografia E2E e ottimizzazione energetica per dispositivi wearable.

---

**Riferimenti**
[1] Bluetooth SIG - LE Audio Specifications
[2] Fraunhofer IIS - LC3 Codec Performance
[3] Nordic Semiconductor - LE Audio and Mesh Stack
[4] Bluetooth Mesh Profile v1.0

---

**Appendice A: Diagramma a Blocchi del Sistema**

```
                +-------------------+
                |  Control Device   |
                |   (Mesh Node)     |
                +---------+---------+
                          |
                  +-------+--------+
                  |                |
          +-------v------+   +-----v--------+
          | Bluetooth    |   |  Mesh Node   |
          | Broadcaster  |<--+ (Fallback &  |
          | (UCB)        |   | Sync Assist) |
          +------+-------+   +--------------+
                 |
           +-----+-----+      +-----+-----+
           |           |      |           |
       +---v---+   +---v---+  ...      +---v---+
       | Sink  |   | Sink  |           | Sink  |
       | Device|   | Device|           | Device|
       +-------+   +-------+           +-------+
```

