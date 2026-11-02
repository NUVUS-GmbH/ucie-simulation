# Übergabe Phase 2 und Phase 3

## Phase 2 — Implementierung (noch nicht begonnen)

### Reihenfolge (empfohlen)

1. **Regression-Harnisch unverändert lassen** (`fh_*`, `fault_handling_campaign`).
2. **AXIS-P1 Datenstrukturen:** Transfer/Paket-State, TKEEP/TLAST/TID/TDEST, zwei Senken.
3. **AXIS Bridge-Policy** (Flit-Segment ↔ Transfer) als klar benannte Abstraction; AoU-PDF weiter als OFFEN.
4. **AXIS Experiment-Runner** + CTest-Namen `ax_*`.
5. **AXI5-MM-P1:** `AWATOP` AtomicStore(ADD), Poison-Flags, Trace-Observability-only.
6. **AXI5 Experimente** `a5_*`.
7. Coverage-Matrix Statuszellen aktualisieren.
8. Website **nicht** anfassen, bis Phase-3-Bericht reviewed ist.

### Harte Constraints (aus Phase 1 / Baseline)

| Constraint | Wo verankert |
| --- | --- |
| `classify` / Retry ohne Ground Truth | `src/common/classify.cpp`, `axi_master.cpp` |
| missing ACK ≠ NOT_COMPLETED | classify + `fh_diag_ack_loss_after_delivery` |
| `diag_never_handed = !handed_to_ucie` | `world.cpp` |
| timed ≠ event-sequence Observer | `fault_handling_campaign.py` |
| Instant cancel = MODEL ASSUMPTION | config + exp 15/16 |
| Kein Poison-Undo | `axi_target.cpp` Kommentar / Invariante |
| Keine erfundenen Register | Draft §9 / Coverage |

### Nicht implementieren in Phase 2

- AXI5-Stream TWAKEUP / Stream-Parity
- AtomicLoad/Swap/Compare, Exclusive
- Vollständige Interconnect-Upsize/Downsize
- Vendor-CSR-Map
- Wire-Cancel-Protokoll (höchstens als neues, klar gelabeltes Experiment „planned protocol“, nicht als Default-Sicherheit)

### Done-Kriterien Phase 2

- [x] Unit-Tests für AXIS handshake + Continuous_Packets-Verletzung
- [x] Unit-Tests AtomicStore ADD + Poison no-undo + Trace ignored by classify
- [x] Smoke `ax_clean_packet_imm` / `ax_clean_packet_pkt` grün; volle AXIS-Kampagne 24+7
- [x] `a5_atomic_store_add_ok` + AXI5-MM-P1 Kampagne 20+7
- [x] Bestehende 19+12 weiter grün (nach AXI5-Infra erneut verifiziert)
- [x] Kein Commit/Push ohne explizite User-Anweisung; Website unberührt

---

## Phase 3 — Messung und Bericht

### Reihenfolge

1. Volle AXIS- und AXI5-Matrizen fahren (automotive + Referenz-Subset).
2. Gegenbeispiele aus `04_research_questions.md` aktiv suchen (nicht nur Happy Path).
3. Bericht analog `fault_handling_report.md` mit Evidenzstufen.
4. Claims gegen Coverage-Matrix und Spec-Zitate prüfen.
5. Optional: Draft V0.1 ERR-01 (Poison) mit AXI5-Pfad abgleichen — ohne Databook-Bestätigung als PROJECT/MODEL belassen.

### Berichtspflichtige Aussagen

- Teilwirkungen SINK-IMM vs SINK-PKT
- Ob TLAST lokal jemals als Remote-Commit missbraucht wurde (muss widerlegt/begrenzt sein)
- AtomicStore-Doppel-ADD unter BlindRetry
- Poison/Error-Unabhängigkeit
- Trace nicht in Fate
- Cancel weiterhin Annahme, sofern nicht neues Wire-Modell eingeführt

### Done-Kriterien Phase 3

- [x] Alle Matrix-IDs mit PASS/FAIL + Bedingungstabelle
- [x] Mindestens ein gemessener Gegenbeispiel-Block je Profil
- [x] Explizite Liste zurückgezogener/eingeschränkter Formulierungen
- [ ] Übergabe an Website-Update als **separater** Schritt

Phase-3-Lieferung: `simulation/results/phase3_abschlussbericht.md`,
`simulation/results/phase3_antwort_torsten.md` (nicht versendet).

---

## Offene Punkte — nur diese erfordern ggf. User-Input

Fachlich belastbare Defaults wurden als MODEL PROFILE gesetzt.  
**Rückfrage nur nötig**, wenn eines der folgenden anders sein soll:

1. Stream-Datenbreite ≠ 128 bit oder TID/TDEST ≠ 4/4.
2. Default-Senke für „primäre“ Kampagne (IMM vs PKT) — Plan: **beide gleichberechtigt**.
3. Poison-Default-Target: Reject-ohne-Commit vs Accept-markiert — Plan: **Reject** als Default, Accept als Variante.
4. Ob App-ACK in Phase 2 überhaupt modelliert wird — Plan: **Schnittstelle vorbereiten, Experimente `_appack` optional nachziehen**.

Ohne Antwort gelten die Profile in `03_study_profiles.md`.

---

## Schnellpfad Reproduktion Baseline (vor Phase-2-Start)

```bash
cmake -S . -B build && cmake --build build -j
ctest --test-dir build --output-on-failure -R 'core|fh_|baseline_unknown|precommit|blind_retry|observational'
python3 -m simulation.experiments.fault_handling_campaign
```

Erwartung: `n_auto=19`, `n_ref=12`, beide `*_all_pass: true`.
