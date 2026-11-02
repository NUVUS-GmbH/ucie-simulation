# Quellenbasierte Protokollabgrenzung

Jede normative Aussage trägt Dokument, Revision und Abschnitt.  
Aussagen ohne zugängliche Quelle sind als **OFFEN / UNGEKLÄRT** markiert — nicht spekuliert.

## Quellkatalog (lokal vorhanden)

| Dokument | Datei | Revision / Issue | Verwendung |
| --- | --- | --- | --- |
| AMBA AXI Protocol | `docs/specs/amba5/IHI0022L_amba_axi_protocol_spec.pdf` | **IHI 0022 Issue L**, 27 Aug 2025 | AXI5 MM: Atomic, Poison, Trace, Responses |
| AMBA AXI Protocol (älter) | `IHI0022J`, `IHI0022H` | J / H | nur Fallback; Studie zitiert **L** |
| AMBA AXI-Stream | `docs/specs/amba5/IHI0051B_amba_axi_stream_protocol_spec.pdf` | **IHI 0051B** (ID040921), 2010/2021 | AXI4-Stream + AXI5-Stream Abgrenzung |
| AMBA AXI4-Stream | `docs/specs/amba5/IHI0051A_amba_axi4_stream_protocol_spec.pdf` | **IHI 0051A**, 03 Mar 2010 | historische Issue-A-Formulierung |
| UCIe 2.0 | `UCIe_Specification_rev2p0_ver1p0_final_2024Aug06_public_clean.pdf` | Rev 2.0 Ver 1.0, 6 Aug 2024 | Link/Adapter/CRC/Replay/LinkError |
| UCIe 3.0 | `UCIe_Specification_rev3p0_...public_clean [website requests].pdf` | Rev 3.0 | Vergleichsstudie; nicht Fokus dieser Extension |
| Projekt-Draft | `UCIe-S_AXI_Fehlerspezifikation.pdf` | Entwurf V0.1, 30.10.2026 | PROJECT REQUIREMENT |
| AoU 0.8 | — | — | **nicht im Workspace** → nur öffentlich zitierte FDI-Regel (§8.3.3) wie in README |

Arm-Lizenzhinweise: siehe `docs/specs/amba5/README.md`.

---

## (a) AXI4 / AXI Memory-Mapped

**Quelle für Responses (weiterhin Baseline):** IHI 0022 Issue L, Response-Kapitel (vgl. bestehende Studie zu OKAY/EXOKAY/SLVERR/DECERR; Write-Completion = B-Handshake; Reads: geforderte Beat-Anzahl inkl. RLAST auch bei Fehler).  
**SPECIFICATION FACT (L):** SLVERR bezeichnet eine unsuccessful transaction; **kein** architektonisches Commit-Bit.

**Bereits implementiert (abstrahiert):** AW/AR/W/R/B-ähnliche Handshake-Latenz, Burst-Längen, lokale SLVERR-Pfade, Watchdog — siehe `fault_handling_report.md`.

**Nicht AXI4-Stream.** Memory-Mapped-Kanäle sind ein anderes Interface als Stream.

---

## (b) AXI4-Stream als eigenes Interface

**Primärquelle:** IHI 0051B — spezifiziert gemeinsam AXI4-Stream und AXI5-Stream; kollektiver Begriff „AXI-Stream“ für gemeinsame Features (Preface / §1.1).

### Was Issue B gegenüber Issue A sagt

- Issue A definiert AXI4-Stream (IHI0051A; Inhalt auch in B als AXI4-Stream-Klasse).
- Issue B führt **AXI5-Stream** als Erweiterung ein (§1.1):
  - Wake-up (`TWAKEUP`, §2.3) — nur AXI5-Stream (`Wakeup_Signal`)
  - Interface-Parity (Kapitel 5) — nur AXI5-Stream (`Check_Type`)

### Kernregeln (gemeinsam / AXI4-Stream-Klasse)

| Thema | Abschnitt (IHI0051B) | Normative Aussage (Kurz) |
| --- | --- | --- |
| Transfer | §2.2 Handshake | Transfer = TVALID ∧ TREADY; Master darf nicht auf TREADY warten, bevor TVALID; TVALID bleibt bis Handshake |
| Nutzdaten | §2.1 / §2.4 | TDATA; Breite ganze Bytes |
| Byte-Qualifier | §2.5 | TKEEP: Stream-Zugehörigkeit (Null-Bytes entfernbar); TSTRB: Data- vs Position-Byte |
| Paketgrenze | §2.6 | TLAST markiert Paketgrenze; Start implizit (nach Reset bzw. nach vorherigem Paketende für TID/TDEST) |
| TID/TDEST | §2.7 | Stream-ID / Routing; innerhalb eines Pakets gleiche TID/TDEST |
| TUSER | §2.1 / User signaling | user-defined sideband |
| Continuous packets | §3.3 | Property `Continuous_Packets`: keine Interleaving; kein TSTRB; eingeschränkte Null-Bytes |

**Wichtiger Spec-Satz (§1.1):** Die Spezifikation beschreibt **wie** Daten transferiert werden, **nicht** die Bedeutung der Daten.  
→ Architektonische „Wirkung“ ist **Anwendungs-/Senkenmodell**, kein AXI-Stream-Commit-Bit.

**Appendix A:** AXI-Stream ≠ AXI Write-Data-Channel (kein Adresskanal, keine BRESP).

---

## (c) Streaming-Transport über UCIe

**Nicht** dasselbe wie (b).

| Aussage | Quelle / Status |
| --- | --- |
| UCIe CRC, Replay (≥375 Flit-Zeiten), LinkError 16 ms, Sideband 8 ms, Flush, keine Quiescence-Garantie | UCIe 2.0 §§3.7, 3.8, 10.3.3.x, 10.3.4 — im Modell genutzt (`simulation/model/ucie.py`, timed C++) |
| Adapter/FDI exposiert Link-State, kein AXI-Commit-Bit | UCIe + bestehende Studie |
| `reference.protocol: streaming` in Torsten-Configs | **REFERENCE-SLIDE** Metadata (`configs/torsten_reference_ucie2x.yaml`) — **kein** Nachweis, dass der Slide AXI4-Stream meint |
| Packing „eine Message pro Flit“, Credits pro Message | **MODEL ABSTRACTION** (AoU-Text fehlt) |
| Exakte normative Abbildung AXI4-Stream ↔ UCIe-Flit-Payload | **UNGEKLÄRT** ohne AoU-/Bridge-Databook |

**Arbeitsdefinition für die Studie:**  
„Streaming-Transport über UCIe“ = bestehender Flit-/Retry-/Credit-Pfad, der **beliebige** Protokollnachrichten (heute MM-Bridge-Messages; später Stream-Segmente) trägt. Das Stream-**Interface** sitzt initiator-/targetseitig **vor/nach** der Bridge.

---

## (d) AXI5 — konkret ausgewählte optionale Fähigkeiten

**Quelle:** IHI 0022 Issue L (27 Aug 2025).  
Issue J entfernte AXI3/AXI4-Text zugunsten AXI5-Klassen; für optionale Features zitieren wir **L**.

### Geprüft gegen Issue L

| Feature | Abschnitt | Spec-Charakter | Studienentscheidung |
| --- | --- | --- | --- |
| **Atomic transactions** | A6.4 (Overview … Support A6.4.10); Signaling A6.4.6 `AWATOP` | Optional; vier Formen AtomicStore/Load/Swap/Compare; Store ohne Read-Data | **Teilumfang:** nur **AtomicStore + ADD** (Profil AXI5-MM-P1) |
| Exclusive accesses | A6.3 | Separat von Atomic transactions | **nicht untersucht** in P1 (eigene Semantik) |
| **Poison** | A16.1; Signale WPOISON/RPOISON; Property `Poison` | Optional; 1 Bit / 64-bit granule; **unabhängig** von Error-Response | **Beobachtung + Policy:** Transport/Markierung; **kein Undo** post-commit |
| **Trace signals** | A12.3; AWTRACE/WTRACE/…; Property `Trace_Signals` | Optional; Verwendung IMPLEMENTATION DEFINED | **nicht in Fate-Classifier**; höchstens Observability-Export |
| AXI5-Stream Wake-up/Parity | IHI0051B §§2.3, 5 | Stream-Erweiterung, nicht MM | **nicht** im AXI5-MM-P1; AXIS-P1 bleibt AXI4-Stream-Klasse |

### Trennung Spec / Target / Bridge

| Schicht | AtomicStore(ADD) | Poison | Trace |
| --- | --- | --- | --- |
| **Standardisierte Semantik** | A6.4: Operation am Speicherort; Store → eine Write-Response ohne Read-Data | A16.1: Daten als korrupt markiert; darf mit/ohne SLVERR auftreten | A12.3: keine feste Trace-Semantik in der Spec |
| **Optionales Feature** | Property/Support laut A6.4.10 | Property `Poison` | Property `Trace_Signals` |
| **Target-Verhalten** | Commit der Atomic-Wirkung am Target-Modell | Target darf vergiftete Daten ablehnen oder markiert speichern — **Bridge-/Target-Policy** | IMPLEMENTATION DEFINED |
| **Bridge-Policy** | Wann Atomic über UCIe als eine logische Tx gilt; Fehler → lokales SLVERR? | Poison in USER vs WPOISON; Draft ERR-01 offen | Ob Trace über FDI/Diagnostik sichtbar wird |

**Keine Behauptung:** „vollständige AXI5-Unterstützung“.

---

## Explizite Nicht-Austauschbarkeit

```
AXI4-MM ──Bridge──► AoU/UCIe flit streaming transport ──► remote AXI4-MM
AXI4-Stream ──Bridge──► (same UCIe transport) ──► remote AXI4-Stream
AXI5-MM options ── on the MM path only (Atomic/Poison/…)
AXI5-Stream options (TWAKEUP, parity) ── on the Stream path only; out of AXIS-P1
```

Vermischen dieser Zeilen in Claims oder Experimentnamen ist ein Methodenfehler.
