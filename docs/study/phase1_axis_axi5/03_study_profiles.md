# Untersuchungsprofile (benannt)

Offene Interface-Details werden hier als **MODEL PROFILE** festgelegt, damit Phase 2 implementierbar ist, ohne Spec-Fakten zu erfinden.

---

## Profil AXIS-P1-Continuous

### Spec-Anbindung

- **Interface-Klasse:** AXI4-Stream (nicht AXI5-Stream).
- **Dokument:** Arm IHI 0051B (und inhaltlich konsistent IHI 0051A für die AXI4-Stream-Basis).
- **Property:** `Continuous_Packets = True` (IHI0051B §3.3).
- **Nicht enthalten:** `TWAKEUP` / `Wakeup_Signal` (AXI5-Stream only, §2.3); Parity/`Check_Type` (Kapitel 5, AXI5-Stream only).

### Signale und Breiten (MODEL PROFILE)

| Signal | Status | Festlegung |
| --- | --- | --- |
| ACLK, ARESETn | erforderlich | vorhanden |
| TVALID, TREADY | erforderlich | Handshake §2.2 |
| TDATA | ja | **128 bit** (16 Byte) — MODEL PROFILE |
| TKEEP | ja | 16 bit; bei `TLAST=0` alle 1 (Continuous_Packets) |
| TSTRB | **abwesend** | Continuous_Packets §3.3 |
| TLAST | ja | Paketgrenze |
| TID | ja | **4 bit**; konstant innerhalb eines Pakets |
| TDEST | ja | **4 bit**; konstant innerhalb eines Pakets |
| TUSER | optional schmal | **8 bit**; Studie nutzt Bit0 nur falls App-ACK-Markierung getestet wird — sonst 0 |
| TWAKEUP / *CHK | nein | außerhalb Profil |

**Unterstützte Kombinationen:**

- Ein Transmitter → eine von zwei Senken (siehe unten); kein Multi-Master-Interleaving (`Continuous_Packets`).
- Paketlängen: 1 … N Transfers, N konfigurierbar (Default-Kampagne: 1, 4, 16 Beats).
- TID/TDEST-Paare: Studie startet mit **einem** aktiven Paar `(0,0)`; zweites Paar nur in expliziten Routing-Negativtests.
- Null-Bytes nur am Paketende auf dem TLAST-Beat oberhalb der Datenbytes (§3.3).

**Einschränkungen (ausdrücklich):**

- Keine Position-Bytes.
- Kein Upsizing/Downsizing-Interconnect-Modell (Spec erwähnt Möglichkeit; **nicht untersucht**).
- Keine Behauptung, TUSER trage standardisierte Diagnose.

### Was ist eine „Übertragung“ / „Abschluss“ / „Wirkung“?

| Begriff | Definition im Profil |
| --- | --- |
| **Transfer** | Ein TVALID∧TREADY-Handshake (IHI0051B §2.2 / §1.1.2) |
| **Paket** | Folge von Transfers bis einschließlich TLAST=1 für ein TID/TDEST (§2.6) |
| **Protokollabschluss (Interface)** | Letzter Handshake des Pakets mit TLAST (keine BRESP) |
| **Architektonische Wirkung** | Senkenmodell-abhängig (unten) — **nicht** durch AXI-Stream spezifiziert |

### Zwei Senkenmodelle (verpflichtend)

| Senken-ID | Verhalten | Commit-Zeitpunkt (Ground Truth) |
| --- | --- | --- |
| **SINK-IMM** | Jeder akzeptierte Transfer mit Datenbytes erzeugt sofort eine architektonische Wirkung (z. B. Byte-Append / Counter += gültige Bytes) | pro Transfer nach Accept |
| **SINK-PKT** | Puffer bis TLAST; Wirkung erst nach vollständigem Paketempfang | einmalig nach TLAST-Accept |

Beide Senken sind **Target-Modelle**, keine Spec-Norm.

### Anwendungsbestätigungen

- AXI4-Stream liefert **keine** standardisierte End-to-End-ACK.
- Optionaler Rückkanal (zweite Stream-Richtung oder MM-Status) ist **separates Interface**.
- Profil-Default: **kein** App-ACK. Experimente mit App-ACK tragen Suffix `_appack` und behandeln ACK-Verlust getrennt vom Basisinterface.

### Abbildung auf UCIe (Bridge-Policy, nicht Spec)

**MODEL PROFILE Bridge-AXIS-P1:**

- Ein Stream-Paket → eine oder mehrere UCIe-Payload-Messages (Default: **ein Flit-Segment pro Transfer**, Paketende-Flag = TLAST).
- Bridge-Token/Generation analog MM (weiterhin MODEL ASSUMPTION).
- Lokales Stream-„Close“ nach LinkError: **kein** BRESP; stattdessen sichtbare Bridge-/Diagnose-Events + optionaler App-Timeout — Details Phase 2, angelehnt an ERR-05-Idee ohne AXI-Response-Kanal.

---

## Profil AXI5-MM-P1

### Spec-Anbindung

- **Dokument:** IHI 0022 Issue L.
- **Basis:** bestehende AXI-MM-über-UCIe-Brücke der Studie.
- **Ausgewählt:**
  1. **AtomicStore** mit Operator **ADD** (A6.4.1, A6.4.2, Signaling A6.4.6 `AWATOP`).
  2. **Poison**-Signalisierung WPOISON/RPOISON (A16.1) als beobachtbares Datenattribut.
  3. **Trace**-Signale (A12.3): nur optionaler Trace-Export; **kein** Classifier-Input.

### Explizit ausgeschlossen in P1

- AtomicLoad / AtomicSwap / AtomicCompare
- Exclusive accesses (A6.3)
- MTE, MPAM, CHI, Credited transport gating, Parity A16.2
- Behauptung „AXI5 complete“
- Poison als nachträgliches Undo eines Commits (widerspricht bestehender Studie / Target-Kommentar)

### AtomicStore(ADD) — Semantik vs. Policy

| Ebene | Festlegung |
| --- | --- |
| Spec | Manager sendet Adresse + Daten + Op; Subordinate speichert Ergebnis; **eine Write-Response, keine Read-Data** (A6.4.1) |
| Target-Modell | `counter := counter + operand` atomar bzgl. Modellzeit; Ground-Truth-Commit analog heutiger Writes |
| Bridge | AtomicStore ist **eine** logische outstanding Tx; Early-Write-Response bleibt default **aus** |
| Fehler | Lokales SLVERR nach LinkError/WDT wie ERR-05/WDT — **beweist keinen** Atomic-Nicht-Commit |

### Poison — Semantik vs. Policy

| Ebene | Festlegung |
| --- | --- |
| Spec | Markiert Daten als zuvor korrupt; unabhängig von Error-Response (A16.1) |
| Bridge-Policy P1 | Poison-Bit wird mit Write-Daten transportiert und am Target als `poisoned` markiert |
| Wirkung | **SINK-REJECT-POISON** (Default): vergiftete AtomicStore/Write **committen nicht**; Response kann trotzdem OKAY oder SLVERR sein — **zwei Varianten** als Experimente, nicht als Spec-Zwang |
| Undo | Verboten: nach Normal-Commit kein Rückgängigmachen durch späteres Poison |

### Trace

- Property kann im Modell „True“ gesetzt werden.
- Observer dürfen Trace-Assert in Disposition/Debug sehen.
- `classify()` und Retry-Logik **lesen Trace nicht**.

---

## Gemeinsame Observer-/Classifier-Regeln (erweitert, unverändert im Geist)

1. Ground Truth nie in Observer A–C oder Retry-Entscheidung.
2. Fehlendes UCIe-ACK ≠ NOT_COMPLETED.
3. `diag_never_handed` nur aus `!handed_to_ucie` (initiatorseitig).
4. Observer-Vergleiche: **timed** und **event-sequence** getrennt ausweisen.
5. Instant Shared-State-Cancel bleibt MODEL ASSUMPTION; Stream-Profile erben das, bis ein Wire-Cancel spezifiziert ist.
