# Forschungsfragen und Nachweise

Evidenzstufen (wie im bestehenden Fault-Handling-Report):

- **directly measured** — Kampagnen-JSON/Traces
- **under explicit model assumptions** — folgt nur unter genannten Annahmen
- **not yet established** — Spec/Databook/weitere Arbeit nötig

---

## A. Profil AXIS-P1-Continuous

### A1. Was ist Übertragung, Protokollabschluss, architektonische Wirkung?

| Frage | Geplante Antwortstruktur | Nachweis |
| --- | --- | --- |
| Transfer? | TVALID∧TREADY (IHI0051B §2.2) | Spec-Fakt + Modellereignis `AXIS_TRANSFER` |
| Paketabschluss? | Letzter Transfer mit TLAST für TID/TDEST (§2.6) | Spec-Fakt + `AXIS_PACKET_DONE` |
| Wirkung? | SINK-IMM: pro Transfer; SINK-PKT: nach TLAST | MODEL PROFILE; Ground Truth nur Auswertung |

### A2. Sind Teilwirkungen möglich?

| Senke | Teilwirkung bei Abbruch mitten im Paket? |
| --- | --- |
| SINK-IMM | **Ja** — bereits akzeptierte Transfers können committed sein |
| SINK-PKT | **Nein** bzgl. Senken-Commit — aber Bytes können in Senken-/Bridge-Puffern liegen (nicht architektonisch sichtbar) |

**Gegenbeispiel-Kandidat:** LinkError nach k von n Transfers: IMM commit=k, PKT commit=0, Initiator sieht in beiden Fällen kein Stream-„OKAY“.

### A3. Was kann der Initiator wann beobachten?

| Zeitpunkt | Sichtbar (Profil A analog) | Nicht sichtbar |
| --- | --- | --- |
| Vor erstem Handshake | lokal TVALID gestellt | Remote-Accept |
| Nach Transfer-i | lokale Handshake-Zähler | Senken-Commit (ohne App-ACK) |
| Nach TLAST lokal akzeptiert | Paketende lokal | Remote-Paket-Commit (SINK-PKT) |
| Nach LinkError | FDI/Status/Diagnostik wie MM-Studie | Commit-Bit |

### A4. Welche Evidenz schließt bisherige **und** zukünftige Ausführung aus?

| Evidenz | Bisherige Wirkung | Zukünftige Wirkung | Bemerkung |
| --- | --- | --- | --- |
| Nie an UCIe übergeben (`diag_never_handed`) | unter Annahmen: keine Remote-Wirkung | nur wenn Remote nicht aus anderem Pfad speisen kann | analog MM; ACK fehlt **nicht** |
| App-ACK „complete“ | App-definiert | nicht automatisch | separates Interface |
| Instant cancel | — | unter MODEL ASSUMPTION | Exp analog 15/16 |
| „Paket lokal mit TLAST beendet“ | beweist **nicht** Remote-SINK-PKT-Commit | — | |

### A5. Wann ist Retry sicher?

**Unter Annahmen sicher nur wenn:**

1. Classifier/Bridge: Paket **nie** handed **oder** Senke SINK-PKT und nachweislich kein TLAST remote akzeptiert **und**
2. zukünftige Ausführung der alten Instanz unmöglich (Cancel mit wirksamer Latenz / Dup-Suppress / Token) **und**
3. bei SINK-IMM: keine Teilcommits der alten Instanz **oder** Retry ist semantisch append-tolerant (meist **nicht** — Default: Retry unsicher nach Teilaccept).

**Gegenbeispiele:**

- IMM + Abbruch nach Transfer 2/4 + Blind-Retry → Doppelbytes.
- PKT + TLAST remote akzeptiert + lokaler Timeout ohne App-ACK + Retry → Doppelpaket.
- ACK-loss nach Delivery (Stream-Segment) → analog `fh_diag_ack_loss_after_delivery`.

### A6. Widerlegbare Aussagen (Hypothesen)

1. „Stream hat kein SLVERR, also weniger Ambiguität“ — **vermutlich falsch**: Ambiguität verschiebt sich auf Wirkung/Timeout.
2. „TLAST lokal ⇒ Remote-Paketwirkung“ — widerlegbar mit Response-/Delivery-Loss vor SINK-PKT-Commit.
3. „Continuous_Packets macht Fate trivial“ — widerlegbar durch UCIe-Loss mitten im Paket + SINK-IMM.

---

## B. Profil AXI5-MM-P1

### B1. Übertragung / Abschluss / Wirkung

| Begriff | AtomicStore(ADD) |
| --- | --- |
| Übertragung | AW + W (+ optional Poison) über Bridge/UCIe |
| Protokollabschluss | Write-Response-Handshake (B); **kein** R-Kanal (A6.4.1) |
| Wirkung | Atomic Update am Target (`counter += x`) = Ground-Truth-Commit |

### B2. Teilwirkungen?

- AtomicStore ist spezifiziert als Update der geschriebenen Location (A6.4).  
- Im Modell: **eine** Commit-Aktion — keine Half-Atomic.  
- **Aber:** lokale SLVERR nach Remote-Commit bleibt möglich (Hypothese der bestehenden Studie, Exp 3-Familie).

### B3. Initiatorbeobachtung

Wie MM-Baseline, plus optional:

- Poison am Request (wenn Property an),
- Trace-Bits (Observability only),
- weiterhin **kein** Commit-Bit in UCIe-Status.

### B4. Evidenz gegen Ausführung

| Evidenz | Aussage |
| --- | --- |
| `diag_never_handed` + local SLVERR | NOT_COMPLETED (wie heute) |
| missing ACK | **nicht** NOT_COMPLETED |
| Poison am Request, SINK-REJECT | Target-Commit 0 **wenn** Request ankam und Policy greift — Initiator sieht das nur bei Response/Diagnostik |
| EXOKAY | **nicht anwendbar** (Atomic ≠ Exclusive) |

### B5. Sicheres Retry

Zusätzlich zu MM-Regeln:

- Nach AtomicStore mit unklarem Fate: Blind-Retry ⇒ Doppel-ADD (**Gegenbeispiel**).
- Poison-Reject-ohne-Commit: Retry kann sicher sein, **wenn** NOT_COMPLETED **und** Cancel/Dup-Suppress — sonst Race.
- Trace ändert Retry-Sicherheit **nicht**.

### B6. Widerlegbare Aussagen

1. „Atomic macht Fate bestimmbarer“ — widerlegbar: SLVERR + commit=1.
2. „Poison ersetzt SLVERR als Commit-Orakel“ — Spec A16.1: Poison und Error-Response unabhängig; widerlegbar durch Poison∧OKAY und !Poison∧SLVERR.
3. „Trace beweist Ausführung“ — Spec: Verwendung IMPLEMENTATION DEFINED; Classifier darf Trace nicht als Commit lesen.

---

## C. Querschnitt UCIe-Transport

Unverändert aus Baseline, gilt für beide Profile:

- UCIe definiert kein AXI-/Stream-Commit-Bit.
- Replay kann Delivery verzögern, ohne Initiator-Sicht auf Wirkung.
- Destructive recovery kann outstanding Protokollzustand droppen (§10.3.3.7 / §10.3.4).
