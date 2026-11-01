#!/usr/bin/env python3
"""Build docs/de from the English pages. Reads English only; safe to re-run."""

from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "docs"
OUT = ROOT / "de"

AXI = (
    "Advanced eXtensible Interface. Das On-Chip-Protokoll zwischen Initiator und Target. "
    "Der Initiator erfährt nur aus der Completion-Antwort, ob eine Transaktion abgeschlossen wurde, "
    "nicht aus der UCIe-Link-Recovery."
)
NONID = (
    "Nicht sicher wiederholbar. Zweimal ausführen ist nicht dasselbe wie einmal, "
    "daher kann ein blindes Retry den Effekt ein zweites Mal committen."
)
MMIO = (
    "Memory-mapped I/O. Ein Lese- oder Schreibzugriff auf eine Geräteadresse, "
    "etwa ein Steuerregister, und nicht auf gewöhnlichen Speicher."
)
MMIOW = (
    "Memory-mapped-I/O-Schreibzugriff. Ein Schreibzugriff auf eine Geräteadresse "
    "und nicht auf gewöhnlichen Speicher."
)
INDET = (
    "Durch das, was der Initiator beobachten kann, nicht entschieden. "
    "Eine committete Transaktion und eine, die das Target nie erreicht hat, können gleich aussehen."
)

PAIRS = [
    (
        "Advanced eXtensible Interface. The on-chip protocol between the initiator and the target. The initiator learns whether a transaction finished only from the completion response, not from UCIe link recovery.",
        AXI,
    ),
    (
        "Not safe to repeat. Doing it twice is not the same as doing it once, so a blind retry can commit the effect a second time.",
        NONID,
    ),
    (
        "Memory-mapped I/O. A read or write to a device address, such as a control register, rather than to ordinary memory.",
        MMIO,
    ),
    (
        "Memory-mapped I/O write. A write to a device address rather than to ordinary memory.",
        MMIOW,
    ),
    (
        "Not settled by what the initiator can observe. A committed transaction and one that never reached the target can look the same.",
        INDET,
    ),
    (">non-idempotent</span>", ">nicht-idempotent</span>"),
    (">indeterminate</span>", ">unbestimmt</span>"),
    ("Skip to content", "Zum Inhalt"),
    ("UCIe 2.0 / 3.0 fault study", "UCIe 2.0 / 3.0 Fehlerstudie"),
    ('aria-label="Color theme"', 'aria-label="Farbschema"'),
    ('aria-label="On this page"', 'aria-label="Auf dieser Seite"'),
    ('aria-label="Site"', 'aria-label="Website"'),
    ('aria-label="Language"', 'aria-label="Sprache"'),
    (">Light</button>", ">Hell</button>"),
    (">Dark</button>", ">Dunkel</button>"),
    (">Summary<", ">Überblick<"),
    (">Method<", ">Methode<"),
    (">Results<", ">Ergebnisse<"),
    (">Limitations<", ">Grenzen<"),
    (">Review notes<", ">Prüfnotizen<"),
    (">Reproduce<", ">Reproduzieren<"),
    (">Scope<", ">Geltung<"),
    (">Counterexample<", ">Gegenbeispiel<"),
    (">Comparison<", ">Vergleich<"),
    (">Safety<", ">Sicherheit<"),
    (">Questions<", ">Fragen<"),
]


def add(en, de):
    PAIRS.append((en, de))


# --- index: head and hero ---
add(
    "<title>Link recovery alone does not determine AXI transaction fate</title>",
    "<title>Link-Recovery allein bestimmt den AXI-Transaktionsausgang nicht</title>",
)
add(
    'content="Under the modeled observation profiles, a committed and a non-committed non-idempotent transaction can be observationally equivalent after UCIe link recovery. A cross-layer observability result, not a claim that UCIe is incorrect."',
    'content="Unter den modellierten Beobachtungsprofilen können eine committete und eine nicht committete nicht-idempotente Transaktion nach der UCIe-Link-Recovery beobachtungsäquivalent sein. Ein Ergebnis zur schichtübergreifenden Beobachtbarkeit, keine Behauptung, UCIe sei fehlerhaft."',
)
add(
    "AXI/AoU-inspired transaction model · specification-derived UCIe recovery",
    "AXI/AoU-inspiriertes Transaktionsmodell · spezifikationsabgeleitete UCIe-Recovery",
)
add(
    ">Link recovery alone does not determine ",
    ">Link-Recovery allein bestimmt den ",
)
add(" transaction fate</h1>", " Transaktionsausgang nicht</h1>")
add(
    "Under the modeled observation profiles and specification-derived recovery behavior, there exist executions in which a committed and a non-committed ",
    "Unter den modellierten Beobachtungsprofilen und dem spezifikationsabgeleiteten Recovery-Verhalten gibt es Ausführungen, in denen eine committete und eine nicht committete ",
)
add(
    " transaction are observationally equivalent to the initiator. UCIe link recovery can leave higher-layer transaction fate ",
    " Transaktion für den Initiator beobachtungsäquivalent sind. Die UCIe-Link-Recovery kann den Transaktionsausgang der höheren Schicht ",
)
add(
    "The complete AoU 0.8 text was not available to the model-building workspace. AoU packing and credit behavior are explicit abstractions. The cited link-down activation rule is the normative AoU behavior used by the model. This is not a full AoU 0.8 conformance implementation.",
    "Der vollständige AoU-0.8-Text lag dem Arbeitsbereich zum Modellbau nicht vor. AoU-Packing und Credit-Verhalten sind ausdrückliche Abstraktionen. Die zitierte Link-down-Aktivierungsregel ist das normative AoU-Verhalten, das das Modell verwendet. Dies ist keine vollständige AoU-0.8-Konformitätsimplementierung.",
)
add("UNKNOWN reachable", "UNKNOWN erreichbar")
add("under modeled recovery scenarios", "unter modellierten Recovery-Szenarien")
add("Duplicate commit under blind retry", "Doppelter Commit bei blindem Retry")
add(
    "committed non-idempotent operation, in the model",
    "committete nicht-idempotente Operation, im Modell",
)
add("Eliminated in modeled UCIe 3.0 scenarios", "In den modellierten UCIe-3.0-Szenarien beseitigt")
add(
    "this transaction-fate ambiguity, not every 3.0 improvement",
    "diese Ambiguität des Transaktionsausgangs, nicht jede 3.0-Verbesserung",
)
add(">Yes</strong>", ">Ja</strong>")
add(">No</strong>", ">Nein</strong>")
add(
    "UCIe 3.0 adds useful link, management, sideband, recalibration, and emergency-handling capabilities. The study finds that these mechanisms do not, by themselves, provide architectural commit knowledge for the transaction-fate cases investigated here.",
    "UCIe 3.0 ergänzt nützliche Fähigkeiten für Link, Management, Sideband, Rekalibrierung und Notfallbehandlung. Die Studie stellt fest, dass diese Mechanismen für die hier untersuchten Fälle des Transaktionsausgangs allein kein Wissen über den architektonischen Commit liefern.",
)
add(
    'The three answers are reachability results for the modeled scenarios, from <a href="sources/comparison_ucie2_vs_ucie3.md">comparison_ucie2_vs_ucie3.md</a>. Reachability is the result used for the scientific conclusion. Rates further down are a fault-workload observation, seed 1, not an estimated field failure probability.',
    'Die drei Antworten sind Erreichbarkeitsergebnisse für die modellierten Szenarien, aus <a href="sources/comparison_ucie2_vs_ucie3.md">comparison_ucie2_vs_ucie3.md</a>. Die Erreichbarkeit trägt die wissenschaftliche Schlussfolgerung. Die Raten weiter unten sind eine Beobachtung unter der Fehlerlast, Seed 1, keine geschätzte Feldausfallwahrscheinlichkeit.',
)
add("<h2>What this study claims</h2>", "<h2>Was diese Studie behauptet</h2>")
add("<h2>What this study does not claim</h2>", "<h2>Was diese Studie nicht behauptet</h2>")
add("<li>Transaction-fate ambiguity is reachable in the executable model.</li>", "<li>Die Ambiguität des Transaktionsausgangs ist im ausführbaren Modell erreichbar.</li>")
add(
    "<li>Different architectural outcomes can be observationally equivalent under the modeled standardized interfaces.</li>",
    "<li>Unterschiedliche architektonische Ausgänge können unter den modellierten standardisierten Schnittstellen beobachtungsäquivalent sein.</li>",
)
add(
    "<li>Blind retry of a committed non-idempotent operation can create a duplicate effect.</li>",
    "<li>Ein blindes Retry einer committeten nicht-idempotenten Operation kann einen doppelten Effekt erzeugen.</li>",
)
add(
    "<li>The specific ambiguity remains reachable in the modeled UCIe 3.0 scenarios.</li>",
    "<li>Die konkrete Ambiguität bleibt in den modellierten UCIe-3.0-Szenarien erreichbar.</li>",
)
add("<li>UCIe is incorrect.</li>", "<li>UCIe sei fehlerhaft.</li>")
add("<li>Every UCIe implementation exhibits this behavior.</li>", "<li>Jede UCIe-Implementierung zeige dieses Verhalten.</li>")
add(
    "<li>UCIe is responsible for defining AXI architectural commit semantics.</li>",
    "<li>UCIe sei dafür zuständig, die Semantik des architektonischen AXI-Commits festzulegen.</li>",
)
add(
    "<li>UCIe 3.0 provides no automotive reliability improvements.</li>",
    "<li>UCIe 3.0 bringe keine Verbesserungen der automobilen Zuverlässigkeit.</li>",
)
add(
    "<li>The randomized UNKNOWN percentages are real-world failure probabilities.</li>",
    "<li>Die zufälligen UNKNOWN-Prozentsätze seien Ausfallwahrscheinlichkeiten im Feld.</li>",
)
add(
    "<li>The current model is a full AoU 0.8 conformance implementation.</li>",
    "<li>Das aktuelle Modell sei eine vollständige AoU-0.8-Konformitätsimplementierung.</li>",
)
add('<li><a href="#scope">What the study claims</a></li>', '<li><a href="#scope">Was die Studie behauptet</a></li>')
add('<li><a href="#question">Research question</a></li>', '<li><a href="#question">Forschungsfrage</a></li>')
add('<li><a href="#matters">Why this matters</a></li>', '<li><a href="#matters">Warum das zählt</a></li>')
add('<li><a href="#model">System model</a></li>', '<li><a href="#model">Systemmodell</a></li>')
add('<li><a href="#counterexample">Key counterexample</a></li>', '<li><a href="#counterexample">Zentrales Gegenbeispiel</a></li>')
add('<li><a href="#comparison">UCIe 2.0 vs UCIe 3.0</a></li>', '<li><a href="#comparison">UCIe 2.0 gegenüber UCIe 3.0</a></li>')
add('<li><a href="#method">Simulation method</a></li>', '<li><a href="#method">Simulationsmethode</a></li>')
add('<li><a href="#results">Measured results</a></li>', '<li><a href="#results">Gemessene Ergebnisse</a></li>')
add('<li><a href="#safety">Safety implications</a></li>', '<li><a href="#safety">Sicherheitsfolgen</a></li>')
add('<li><a href="#specification">Specification interpretation</a></li>', '<li><a href="#specification">Spezifikationsdeutung</a></li>')
add('<li><a href="#limitations">Limitations</a></li>', '<li><a href="#limitations">Grenzen</a></li>')
add('<li><a href="#reproduce">Reproducibility</a></li>', '<li><a href="#reproduce">Reproduzierbarkeit</a></li>')
add('<li><a href="#context">Context</a></li>', '<li><a href="#context">Einordnung</a></li>')
add('<li><a href="#questions">Open questions</a></li>', '<li><a href="#questions">Offene Fragen</a></li>')
add('<li><a href="#conclusion">Conclusion</a></li>', '<li><a href="#conclusion">Schluss</a></li>')

# --- question ---
add("02 · Research question", "02 · Forschungsfrage")
add(
    "After UCIe error recovery, can the ",
    "Kann der ",
)
add(
    " initiator always determine whether an outstanding ",
    " Initiator nach der UCIe-Fehlerrecovery stets bestimmen, ob eine offene ",
)
add(
    " transaction committed?</h2>",
    " Transaktion committet wurde?</h2>",
)
add(
    "For the initiator interface modeled here, the answer is no. Once the link is Active again, “the request was accepted” and “no completion arrived” is compatible with a committed operation and with an operation that never reached the target.",
    "Für die hier modellierte Initiator-Schnittstelle lautet die Antwort nein. Sobald der Link wieder Active ist, sind „die Anfrage wurde angenommen“ und „keine Completion ist eingetroffen“ sowohl mit einer committeten Operation als auch mit einer Operation verträglich, die das Target nie erreicht hat.",
)
add(
    "The simulator therefore classifies each interrupted transaction as one of three states. The classifier reads initiator-visible facts. It does not read the target\u2019s commit count. The count is ground truth, used afterwards to test whether a label was sound.",
    "Der Simulator klassifiziert jede unterbrochene Transaktion daher als einen von drei Zuständen. Der Klassifikator liest nur Fakten, die der Initiator sehen kann. Den Commit-Zähler des Targets liest er nicht. Dieser Zähler ist Grundwahrheit und dient danach nur der Prüfung, ob eine Kennzeichnung tragfähig war.",
)
add(
    "The initiator received an OKAY completion. A completion that was only generated, or only queued, does not count until the initiator has it.",
    "Der Initiator hat eine OKAY-Completion erhalten. Eine Completion, die nur erzeugt oder nur eingereiht wurde, zählt erst, wenn der Initiator sie hat.",
)
add(
    "The initiator has a positive reason to treat the effect as absent. Under FDI and ",
    "Der Initiator hat einen positiven Grund, den Effekt als ausgeblieben zu behandeln. Unter FDI und ",
)
add(
    " alone, that is a transaction the manager never handed to the endpoint. A NAK-only history supports this label only when it is latched before the retry buffers are flushed. UCIe does not require that latch.",
    " allein ist das eine Transaktion, die der Manager nie an den Endpoint übergeben hat. Eine reine NAK-Historie stützt diese Kennzeichnung nur, wenn sie vor dem Leeren der Retry-Puffer gelatcht wird. UCIe verlangt dieses Latch nicht.",
)
add(
    "UNKNOWN is the sound classification for the modeled observer when the available information proves neither completion nor non-execution. Both counterexample traces receive this label from FDI and ",
    "UNKNOWN ist die tragfähige Klassifikation für den modellierten Beobachter, wenn die verfügbare Information weder den Abschluss noch die Nichtausführung belegt. Beide Gegenbeispiel-Traces erhalten diese Kennzeichnung aus FDI und ",
)
add('aria-label="How the three labels are reached"', 'aria-label="Wie die drei Kennzeichnungen zustande kommen"')
add("<span>OKAY received</span>", "<span>OKAY empfangen</span>")
add("<span>Never handed off</span>", "<span>Nie übergeben</span>")
add("<span>Accepted, no proof</span>", "<span>Angenommen, kein Nachweis</span>")
add(
    "A real implementation may avoid exposing UNKNOWN by applying an additional system-level containment rule, reset policy, persistent transaction log, transaction identifier, duplicate-suppression mechanism, or device-specific recovery contract. Hiding the label does not necessarily mean the uncertainty disappeared. Resetting the device may contain the safety risk without reconstructing whether the original operation committed.",
    "Eine reale Implementierung kann UNKNOWN verbergen, indem sie eine zusätzliche Containment-Regel auf Systemebene, eine Reset-Policy, ein dauerhaftes Transaktionslog, einen Transaktionsbezeichner, eine Duplikatunterdrückung oder einen gerätespezifischen Recovery-Vertrag anwendet. Die Kennzeichnung zu verbergen heißt nicht, dass die Unsicherheit verschwunden ist. Ein Reset des Geräts kann das Sicherheitsrisiko eindämmen, ohne zu rekonstruieren, ob die ursprüngliche Operation committet wurde.",
)
add(
    "No standardized UCIe indication modeled in this study identifies the architectural commit state of the associated ",
    "Keine in dieser Studie modellierte standardisierte UCIe-Anzeige benennt den architektonischen Commit-Zustand der zugehörigen ",
)
add(
    " transaction. FDI reports link state on <code>pl_state_sts</code>. The cited AoU 0.8 §8.3.3 rule returns the activation FSM to DISABLED on link-down. Architectural commit is a target-device effect. It is not a UCIe link-layer field.",
    " Transaktion. FDI meldet den Linkzustand auf <code>pl_state_sts</code>. Die zitierte AoU-0.8-Regel §8.3.3 setzt die Aktivierungs-FSM bei Link-down auf DISABLED zurück. Der architektonische Commit ist ein Effekt des Target-Geräts. Er ist kein Feld der UCIe-Linkschicht.",
)

# --- matters ---
add("03 · Why this matters", "03 · Warum das zählt")
add(
    "<h2>A missing completion is not evidence that the command had no effect.</h2>",
    "<h2>Eine fehlende Completion ist kein Beleg, dass der Befehl wirkungslos war.</h2>",
)
add(
    "If a command increments a hardware counter, starts DMA, triggers an actuator command, or rings a doorbell, repeating it may not be safe. The model treats those operations, and an accelerator start, as ",
    "Wenn ein Befehl einen Hardwarezähler erhöht, DMA startet, einen Aktor auslöst oder eine Doorbell setzt, kann das Wiederholen unsicher sein. Das Modell behandelt diese Operationen und einen Beschleunigerstart als ",
)
add(
    ". The printed counterexample is the counter: the committed trace changes it from 7 to 8, then loses the response.",
    ". Das gedruckte Gegenbeispiel ist der Zähler: der committete Trace ändert ihn von 7 auf 8 und verliert danach die Antwort.",
)
add(
    "A link-scoped recovery event may affect transaction-fate knowledge for multiple clients sharing that physical transport. In the UCIe 3.0 run <code>ucie3_two_guests_shared_link</code>, two initiators each finish with <code>architectural_commit_count = 1</code> and classification UNKNOWN, and the modeled link-scoped signals match. The two-guest scenario motivates a separate freedom-from-interference study. It does not by itself establish an ISO 26262 freedom-from-interference violation.",
    "Ein Recovery-Ereignis mit Link-Geltung kann das Wissen über den Transaktionsausgang für mehrere Clients beeinträchtigen, die denselben physischen Transport teilen. Im UCIe-3.0-Lauf <code>ucie3_two_guests_shared_link</code> enden zwei Initiatoren jeweils mit <code>architectural_commit_count = 1</code> und der Klassifikation UNKNOWN, und die modellierten linkbezogenen Signale stimmen überein. Das Zwei-Gast-Szenario motiviert eine eigene Untersuchung der Rückwirkungsfreiheit. Es begründet für sich keine Verletzung der Rückwirkungsfreiheit nach ISO 26262.",
)
add(
    "UCIe provides mechanisms that can support automotive reliability, availability, diagnostics, and safety architectures. It does not itself define ISO 26262 safety goals or the complete application-level reaction to every communication fault. The result identifies one decision the surrounding system must make explicitly: how an interrupted ",
    "UCIe stellt Mechanismen bereit, die automobile Zuverlässigkeit, Verfügbarkeit, Diagnose und Sicherheitsarchitekturen stützen können. Es definiert selbst weder ISO-26262-Sicherheitsziele noch die vollständige Reaktion der Anwendung auf jeden Kommunikationsfehler. Das Ergebnis benennt eine Entscheidung, die das umgebende System ausdrücklich treffen muss: wie eine unterbrochene ",
)
add(
    " operation is contained when its architectural effect cannot be reconstructed from the available communication status.",
    " Operation eingegrenzt wird, wenn sich ihr architektonischer Effekt aus dem verfügbaren Kommunikationsstatus nicht rekonstruieren lässt.",
)

# --- model ---
add("04 · System model", "04 · Systemmodell")
add(">One ", ">Ein ")
add(
    " manager, one AoU endpoint pair, one UCIe link, one target.</h2>",
    " Manager, ein AoU-Endpoint-Paar, ein UCIe-Link, ein Target.</h2>",
)
add(
    "The simulator is an ",
    "Der Simulator ist ein ",
)
add(
    "/AoU-inspired transaction model over specification-derived UCIe recovery behavior. Time is a sequence of protocol events. It is not a PHY and not a cycle-accurate UCIe implementation. The fault is injected on the channel between the adapters. Architectural commit happens at the target, after both the request and the write data have been delivered. That commit point is a device-model assumption. AoU and UCIe do not define it. The completion then has to travel back.",
    "/AoU-inspiriertes Transaktionsmodell über spezifikationsabgeleitetem UCIe-Recovery-Verhalten. Die Zeit ist eine Folge von Protokollereignissen. Es ist keine PHY und keine zyklusgenaue UCIe-Implementierung. Der Fehler wird auf dem Kanal zwischen den Adaptern injiziert. Der architektonische Commit geschieht am Target, nachdem Anfrage und Schreibdaten zugestellt wurden. Dieser Commit-Punkt ist eine Annahme des Gerätemodells. AoU und UCIe legen ihn nicht fest. Die Completion muss danach zurücklaufen.",
)
add(
    "The complete AoU 0.8 text was not available to the model-building workspace. AoU packing and credit behavior are explicit abstractions; the cited link-down activation rule is the normative AoU behavior used by the model. If the complete AoU 0.8 specification is later added to the repository, that statement should be revised only after the implementation is cross-checked against it.",
    "Der vollständige AoU-0.8-Text lag dem Arbeitsbereich zum Modellbau nicht vor. AoU-Packing und Credit-Verhalten sind ausdrückliche Abstraktionen; die zitierte Link-down-Aktivierungsregel ist das normative AoU-Verhalten, das das Modell verwendet. Wird die vollständige AoU-0.8-Spezifikation später ins Repository aufgenommen, sollte diese Aussage erst nach einem Abgleich der Implementierung mit ihr überarbeitet werden.",
)
add(
    "The model is intentionally event-accurate rather than cycle-accurate because the primary property is an observability and reachability property: whether two different architectural outcomes can produce the same observer-visible state. Cycle-accurate modeling would be needed for quantitative real-time claims such as worst-case recovery latency, deadline violations, arbitration effects, or a precise ambiguity duration in nanoseconds.",
    "Das Modell ist absichtlich ereignisgenau und nicht zyklusgenau, weil die zentrale Eigenschaft eine Frage der Beobachtbarkeit und Erreichbarkeit ist: ob zwei verschiedene architektonische Ausgänge denselben für den Beobachter sichtbaren Zustand erzeugen können. Zyklusgenaue Modellierung wäre nötig für quantitative Echtzeitaussagen wie Worst-Case-Recovery-Latenz, Deadline-Verletzungen, Arbitrierungseffekte oder eine Ambiguitätsdauer in Nanosekunden.",
)
add(">Request path</li>", ">Anfragepfad</li>")
add(">Commit point</li>", ">Commit-Punkt</li>")
add(">Response path</li>", ">Antwortpfad</li>")
add(">Fault location</li>", ">Fehlerort</li>")
add(">Recovery path</li>", ">Recovery-Pfad</li>")
add('<span class="tag">Request</span>', '<span class="tag">Anfrage</span>')
add('<span class="tag">Fault</span>', '<span class="tag">Fehler</span>')
add('<span class="tag">Commit</span>', '<span class="tag">Commit</span>')
add(">Fault-injection channel</span>", ">Fehlerinjektionskanal</span>")
add(
    "The completion travels back from the target to the initiator. Link recovery runs between the two adapters: Retrain, LinkError, reset, return to Active. Editable diagram: ",
    "Die Completion läuft vom Target zum Initiator zurück. Die Link-Recovery läuft zwischen den beiden Adaptern: Retrain, LinkError, Reset, Rückkehr nach Active. Editierbares Diagramm: ",
)
add(
    "<title id=\"archTitle\">System model from AXI initiator to AXI target</title>",
    "<title id=\"archTitle\">Systemmodell vom AXI-Initiator zum AXI-Target</title>",
)
add(
    "<desc id=\"archDesc\">The request travels from the AXI initiator through AoU endpoint A, UCIe adapter A, the fault-injection channel, UCIe adapter B, and AoU endpoint B to the AXI target. Architectural commit is at the target. The completion response travels back to the initiator. Link recovery runs between the two adapters.</desc>",
    "<desc id=\"archDesc\">Die Anfrage läuft vom AXI-Initiator über AoU-Endpoint A, UCIe-Adapter A, den Fehlerinjektionskanal, UCIe-Adapter B und AoU-Endpoint B zum AXI-Target. Der architektonische Commit liegt am Target. Die Completion-Antwort läuft zum Initiator zurück. Die Link-Recovery läuft zwischen den beiden Adaptern.</desc>",
)
add(">Fault-injection</text>", ">Fehlerinjektion</text>")
add(">channel</text>", ">Kanal</text>")
add(
    "Recovery path · Retrain, LinkError, reset, return to Active",
    "Recovery-Pfad · Retrain, LinkError, Reset, Rückkehr nach Active",
)
add(
    "Response path · completion travels back toward the initiator",
    "Antwortpfad · die Completion läuft zum Initiator zurück",
)
add(
    "Commit is a target-local architectural effect. It is not the UCIe acknowledgement, and it is not the ",
    "Der Commit ist ein lokaler architektonischer Effekt am Target. Er ist weder die UCIe-Quittung noch die ",
)
add(
    " completion.</text>",
    " Completion.</text>",
)
add(
    "A lost completion can leave architectural_commit_count = 1 while completion_received = false.",
    "Eine verlorene Completion kann architectural_commit_count = 1 lassen, während completion_received = false ist.",
)
add(
    "Stack used by the study. Packing is one message per flit, and one credit stands for one message. Both are labeled abstractions. Editable diagram: ",
    "Der von der Studie verwendete Stapel. Das Packing ist eine Nachricht je Flit, und ein Credit steht für eine Nachricht. Beides ist als Abstraktion gekennzeichnet. Editierbares Diagramm: ",
)

# --- counterexample ---
add("05 · Key counterexample", "05 · Zentrales Gegenbeispiel")
add("<h2>Same observation. Different reality.</h2>", "<h2>Dieselbe Beobachtung. Andere Wirklichkeit.</h2>")
add(
    "Two executions are observationally equivalent for an observer when their architectural ground truth differs, but every fact available to that observer is identical.",
    "Zwei Ausführungen sind für einen Beobachter beobachtungsäquivalent, wenn ihre architektonische Grundwahrheit verschieden ist, aber jede ihm verfügbare Tatsache übereinstimmt.",
)
add(
    "These are two different executions of a ",
    "Das sind zwei verschiedene Ausführungen eines ",
)
add(
    " increment at <code>0x1000</code>, policy A (discard outstanding transactions, no ",
    "-Inkrements bei <code>0x1000</code>, Policy A (offene Transaktionen verwerfen, keine ",
)
add(
    " completion). In T17 the target commits and the write response is then lost. In T18 every copy of the request fails CRC before the target accepts it. After LinkError and return to Active, the FDI/",
    "-Completion). In T17 committet das Target, und die Schreibantwort geht danach verloren. In T18 scheitert jede Kopie der Anfrage an der CRC, bevor das Target sie annimmt. Nach LinkError und der Rückkehr nach Active ist die FDI/",
)
add(" view is the same.", "-Sicht dieselbe.")
add(">Committed write, response lost</h3>", ">Committeter Schreibzugriff, Antwort verloren</h3>")
add(">Write never reaches the target</h3>", ">Schreibzugriff erreicht das Target nie</h3>")
add(" write accepted</li>", " Schreibzugriff angenommen</li>")
add(" write accepted locally</li>", " Schreibzugriff lokal angenommen</li>")
add("<li>Request reaches the target</li>", "<li>Anfrage erreicht das Target</li>")
add("<li>Target commits</li>", "<li>Target committet</li>")
add("<li>Architectural state changes: counter 7 to 8</li>", "<li>Architektonischer Zustand ändert sich: Zähler 7 auf 8</li>")
add("<li>Response generated</li>", "<li>Antwort erzeugt</li>")
add("<li>Link failure: response CRC fails on replay</li>", "<li>Linkfehler: die CRC der Antwort scheitert beim Replay</li>")
add("<li>Response lost during cleanup</li>", "<li>Antwort geht beim Aufräumen verloren</li>")
add("<li>Recovery: link returns to Active</li>", "<li>Recovery: der Link kehrt nach Active zurück</li>")
add("<li>Initiator receives no completion</li>", "<li>Der Initiator erhält keine Completion</li>")
add("<li>UCIe request repeatedly corrupted</li>", "<li>UCIe-Anfrage wiederholt verfälscht</li>")
add("<li>Request never reaches the target</li>", "<li>Anfrage erreicht das Target nie</li>")
add("<dt>Ground truth</dt>", "<dt>Grundwahrheit</dt>")
add("<dd>COMMITTED</dd>", "<dd>COMMITTED</dd>")
add("<dd>NOT COMMITTED</dd>", "<dd>NOT COMMITTED</dd>")
add("<dt>Commit count</dt>", "<dt>Commit-Zähler</dt>")
add("<dt>Classification</dt>", "<dt>Klassifikation</dt>")
add(">Same observation. Different reality.</p>", ">Dieselbe Beobachtung. Andere Wirklichkeit.</p>")
add(
    "Trace A has <code>architectural_commit_count = 1</code>. Trace B has <code>architectural_commit_count = 0</code>. Both have <code>axi_accepted = true</code> and <code>completion_received = false</code>, and the same relevant link and recovery indications. The modeled observer cannot distinguish the two executions. UCIe 3.0 profile 3, which may also read software error state and standardized management facts, still matches on this pair.",
    "Trace A hat <code>architectural_commit_count = 1</code>. Trace B hat <code>architectural_commit_count = 0</code>. Beide haben <code>axi_accepted = true</code> und <code>completion_received = false</code> sowie dieselben relevanten Link- und Recovery-Anzeigen. Der modellierte Beobachter kann die beiden Ausführungen nicht unterscheiden. Das UCIe-3.0-Profil 3, das zusätzlich Software-Fehlerzustand und standardisierte Management-Fakten lesen darf, stimmt auf diesem Paar ebenfalls überein.",
)
add(
    '<p style="margin-top: var(--space-24)">The recovery path printed for both traces is:</p>',
    '<p style="margin-top: var(--space-24)">Der für beide Traces gedruckte Recovery-Pfad ist:</p>',
)
add('aria-label="Link state path in the printed counterexamples"', 'aria-label="Linkzustandspfad in den gedruckten Gegenbeispielen"')
add("<li>Recovery</li>", "<li>Recovery</li>")
add(
    "AoU remains enabled across Retrain. FDI link-down is not signaled for Retrain. LinkError returns the activation FSM to DISABLED under the cited AoU 0.8 §8.3.3 rule, flushes retry buffers, and discards the outstanding message. This is the path of these two traces. Other runs end in a transparent retry or a completed response. Counts are in the results.",
    "AoU bleibt über Retrain hinweg aktiviert. FDI-Link-down wird für Retrain nicht signalisiert. LinkError setzt die Aktivierungs-FSM nach der zitierten AoU-0.8-Regel §8.3.3 auf DISABLED, leert die Retry-Puffer und verwirft die offene Nachricht. Das ist der Pfad dieser beiden Traces. Andere Läufe enden in einem transparenten Retry oder einer abgeschlossenen Antwort. Die Zähler stehen bei den Ergebnissen.",
)
add(
    "If the initiating adapter latches retry disposition before the flush, the traces separate. T18 was only NAK\u2019d, and UCIe does not deliver a CRC-invalid flit, so that assumed latch classifies it NOT_COMPLETED. T17 was acknowledged and then lost its response, so it stays UNKNOWN. The latch is an assumption. UCIe flushes retry buffers on this path and does not require the NAK/ACK history to be kept, tied to an ",
    "Wenn der initiierende Adapter die Retry-Disposition vor dem Leeren latcht, trennen sich die Traces. T18 wurde nur mit NAK beantwortet, und UCIe stellt einen CRC-ungültigen Flit nicht zu, daher klassifiziert dieses angenommene Latch ihn als NOT_COMPLETED. T17 wurde quittiert und verlor danach die Antwort, daher bleibt es UNKNOWN. Das Latch ist eine Annahme. UCIe leert auf diesem Pfad die Retry-Puffer und verlangt nicht, dass die NAK/ACK-Historie erhalten, an eine ",
)
add(
    " id, and shown to software. The cited AoU 0.8 text does not require it either. Safe automatic retry of T17 is no, with or without the latch.",
    "-Id gebunden und der Software gezeigt wird. Der zitierte AoU-0.8-Text verlangt das ebenfalls nicht. Ein sicheres automatisches Retry von T17 ist nein, mit oder ohne Latch.",
)
add(
    'The same observational equivalence is reported for priority sideband plus a mainband loss, and for endpoint reset. In each pair the commit counts differ and profiles 1, 2, and 3 still match. <a href="results.html#pairs">Signal sets</a>.',
    'Dieselbe Beobachtungsäquivalenz wird für Priority-Sideband plus einen Mainband-Verlust und für Endpoint-Reset berichtet. In jedem Paar unterscheiden sich die Commit-Zähler, und die Profile 1, 2 und 3 stimmen weiterhin überein. <a href="results.html#pairs">Signalsätze</a>.',
)
add(
    "Emergency Shutdown is not another CRC or replay case. The UCIe 3.0 scenarios show that the same transaction-fate reasoning can arise when communication is destructively interrupted by endpoint or power recovery. Emergency Shutdown has a different system purpose: rapid containment of a severe condition. The study asks a separate post-event question: what can software infer about transactions that were in flight when containment occurred? It does not treat Emergency Shutdown as defective because it does not preserve transaction completion.",
    "Emergency Shutdown ist kein weiterer CRC- oder Replay-Fall. Die UCIe-3.0-Szenarien zeigen, dass dieselbe Überlegung zum Transaktionsausgang entstehen kann, wenn die Kommunikation durch Endpoint- oder Power-Recovery zerstörend unterbrochen wird. Emergency Shutdown hat einen anderen Systemzweck: die rasche Eingrenzung eines schweren Zustands. Die Studie stellt eine eigene Frage nach dem Ereignis: Was kann Software über Transaktionen schließen, die zum Zeitpunkt der Eingrenzung unterwegs waren? Sie behandelt Emergency Shutdown nicht als mangelhaft, weil es die Transaktions-Completion nicht erhält.",
)
add(
    'Condensed from the generated traces. Line-by-line text: <a href="sources/findings.md">findings.md</a> and <a href="sources/findings_ucie3.md">findings_ucie3.md</a>. Diagram file: <a href="assets/counterexample.svg">counterexample.svg</a>.',
    'Verdichtet aus den erzeugten Traces. Zeilengetreuer Text, auf Englisch: <a href="sources/findings.md">findings.md</a> und <a href="sources/findings_ucie3.md">findings_ucie3.md</a>. Diagrammdatei: <a href="assets/counterexample.svg">counterexample.svg</a>.',
)

# --- comparison ---
add("06 · UCIe 2.0 vs UCIe 3.0", "06 · UCIe 2.0 gegenüber UCIe 3.0")
add(
    "<h2>For this transaction-fate property, the tested UCIe 3.0 mechanisms did not remove the ambiguity.</h2>",
    "<h2>Für diese Eigenschaft des Transaktionsausgangs haben die geprüften UCIe-3.0-Mechanismen die Ambiguität nicht beseitigt.</h2>",
)
add(
    "UCIe 3.0 improves failure handling at the link level. The architectural commit point remains a target-device effect, outside the UCIe status modeled here. The comparison replays one systematic suite and one seeded random suite on both models, then exercises mechanisms that Revision 3.0 adds. A UCIe 2.0 rule is used for the 3.0 model only where the 3.0 text still states it.",
    "UCIe 3.0 verbessert die Fehlerbehandlung auf Linkebene. Der architektonische Commit-Punkt bleibt ein Effekt des Target-Geräts, außerhalb des hier modellierten UCIe-Status. Der Vergleich spielt eine systematische Suite und eine gesäte Zufallssuite auf beiden Modellen nach und übt dann Mechanismen, die Revision 3.0 ergänzt. Eine UCIe-2.0-Regel wird im 3.0-Modell nur verwendet, wo der 3.0-Text sie noch ausspricht.",
)
add(
    "For the specific property studied here, reconstructing the architectural fate of an interrupted ",
    "Für die hier untersuchte Eigenschaft, den architektonischen Ausgang einer unterbrochenen ",
)
add(
    " transaction from the modeled standardized observations, the tested UCIe 3.0 mechanisms did not eliminate the ambiguity found in the UCIe 2.0 model. That is not a statement that UCIe 3.0 is no better than UCIe 2.0. Equality of the UNKNOWN counts, 171 and 171, applies only to this workload and this transaction-fate property. It is not a general comparison of UCIe 2.0 and 3.0 reliability.",
    " Transaktion aus den modellierten standardisierten Beobachtungen zu rekonstruieren, haben die geprüften UCIe-3.0-Mechanismen die im UCIe-2.0-Modell gefundene Ambiguität nicht beseitigt. Das ist keine Aussage, UCIe 3.0 sei nicht besser als UCIe 2.0. Die Gleichheit der UNKNOWN-Zähler, 171 und 171, gilt nur für diese Last und diese Eigenschaft des Transaktionsausgangs. Sie ist kein allgemeiner Vergleich der Zuverlässigkeit von UCIe 2.0 und 3.0.",
)
add(
    'Capability comparison. “Retained” means UCIe 3.0 still states the rule. Sources: comparison report and the mechanism table in findings_ucie3.md. Diagram file: <a href="assets/comparison.svg">comparison.svg</a>.',
    'Vergleich der Fähigkeiten. „Beibehalten“ heißt, dass UCIe 3.0 die Regel weiterhin ausspricht. Quellen: Vergleichsbericht und die Mechanismentabelle in findings_ucie3.md, beide auf Englisch. Diagrammdatei: <a href="assets/comparison.svg">comparison.svg</a>.',
)
add("<th scope=\"col\">Capability</th>", "<th scope=\"col\">Fähigkeit</th>")
add("<td>Yes, retained. In-flight packets may still be dropped</td>", "<td>Ja, beibehalten. Flits unterwegs können weiterhin verworfen werden</td>")
add("<td>Yes, retained</td>", "<td>Ja, beibehalten</td>")
add(
    "FDI link state. Modeled error registers name CRC, Retrain, and LinkError, not an ",
    "FDI-Linkzustand. Die modellierten Fehlerregister nennen CRC, Retrain und LinkError, nicht eine ",
)
add(
    " transaction</td>",
    "-Transaktion</td>",
)
add(
    "Those registers remain. Runtime parity, runtime recalibration, the Link Health Monitor, and management transport are also visible. In every equivalence pair the committed and uncommitted traces still match",
    "Diese Register bleiben. Runtime-Parität, Runtime-Rekalibrierung, der Link Health Monitor und der Management-Transport sind zusätzlich sichtbar. In jedem Äquivalenzpaar stimmen der committete und der nicht committete Trace weiterhin überein",
)
add(
    "<td>Baseline</td>",
    "<td>Baseline</td>",
)
add(
    "Performant mode removes a 32 UI gap. Optional priority sideband. Not a fate milestone in this model",
    "Der Performant-Modus beseitigt eine Lücke von 32 UI. Optionales Priority-Sideband. In diesem Modell kein Meilenstein des Transaktionsausgangs",
)
add(
    "<td>Not the mechanisms added in the 3.0 text</td>",
    "<td>Nicht die im 3.0-Text ergänzten Mechanismen</td>",
)
add(
    "Optional Fast Throttle. Optional fatal Emergency Shutdown. Neither defines a log of outstanding mainband transactions",
    "Optionales Fast Throttle. Optionales fatales Emergency Shutdown. Keines von beiden definiert ein Log offener Mainband-Transaktionen",
)
add(">Standardized ", ">Standardisierte ")
add(" commit indication</th>", "-Commit-Anzeige</th>")
add(
    "Not among the modeled status. Link state does not name the transaction\u2019s architectural commit.",
    "Nicht unter dem modellierten Status. Der Linkzustand benennt den architektonischen Commit der Transaktion nicht.",
)
add(
    "Added status still describes link, transport, electrical, or emergency state. The equivalence pairs still match.",
    "Ergänzter Status beschreibt weiterhin Link, Transport, elektrischen Zustand oder Notfall. Die Äquivalenzpaare stimmen weiterhin überein.",
)
add("<td>Yes. Equivalent-workload count 171 and 171</td>", "<td>Ja. Anzahl auf der äquivalenten Last 171 und 171</td>")
add("<th scope=\"row\">Blind retry always safe</th>", "<th scope=\"row\">Blindes Retry stets sicher</th>")
add(
    "<td>No. Policy D on the committed loss reaches architectural_commit_count = 2</td>",
    "<td>Nein. Policy D auf dem Verlust nach dem Commit erreicht architectural_commit_count = 2</td>",
)
add(">Yes<", ">Ja<")
add(">No<", ">Nein<")
add(
    "In the modeled architecture, transport-level acknowledgement and target architectural commit are distinct events. On the UCIe 3.0 systematic runs, 9 transactions had a transport acknowledgement with <code>architectural_commit_count</code> 0, and 33 transactions had a commit without an initiator completion. Retaining transport ACK/NAK history can improve classification before target acceptance, but it cannot by itself prove the architectural commit state after the request has progressed beyond the transport boundary. UNKNOWN before target acceptance falls from 48.8% (21/43) to 11.6% (5/43) when retry disposition is assumed to be retained. That latch is an assumed implementation enhancement, not a standardized ",
    "In der modellierten Architektur sind die Quittung auf Transportebene und der architektonische Commit am Target verschiedene Ereignisse. In den systematischen UCIe-3.0-Läufen hatten 9 Transaktionen eine Transportquittung bei <code>architectural_commit_count</code> 0, und 33 Transaktionen einen Commit ohne Completion beim Initiator. Das Behalten der Transport-ACK/NAK-Historie kann die Klassifikation vor der Target-Annahme verbessern, belegt aber allein nicht den architektonischen Commit, nachdem die Anfrage die Transportgrenze überschritten hat. UNKNOWN vor der Target-Annahme fällt von 48,8 % (21/43) auf 11,6 % (5/43), wenn angenommen wird, dass die Retry-Disposition erhalten bleibt. Dieses Latch ist eine angenommene Implementierungsverbesserung, kein standardisierter ",
)
add(
    " transaction-fate mechanism.",
    " Mechanismus für den Transaktionsausgang.",
)
add(
    "Fast Throttle, runtime recalibration, and runtime link-testing parity are specified not to discard a valid flit by themselves. Those runs complete the write: classification COMPLETED, commit count 1, completion received. They are not new cases in which a lost completion becomes safe to replay.",
    "Fast Throttle, Runtime-Rekalibrierung und Runtime-Linktest-Parität sind so spezifiziert, dass sie einen gültigen Flit nicht von sich aus verwerfen. Diese Läufe schließen den Schreibzugriff ab: Klassifikation COMPLETED, Commit-Zähler 1, Completion empfangen. Sie sind keine neuen Fälle, in denen eine verlorene Completion sicher wiederholt werden dürfte.",
)

# --- method on index ---
add("07 · Simulation method", "07 · Simulationsmethode")
add(
    "<h2>Ground truth and the initiator\u2019s view are recorded separately.</h2>",
    "<h2>Grundwahrheit und die Sicht des Initiators werden getrennt aufgezeichnet.</h2>",
)
add(
    "Each run is a state machine over ",
    "Jeder Lauf ist ein Zustandsautomat über ",
)
add(
    ", AoU, and UCIe phases: local acceptance, packing, transmission, remote reception, target acceptance, architectural commit, response creation, and response delivery. Faults are armed at a chosen phase. The UCIe paths exercised are CRC replay, the retry threshold, Retrain, LinkReset, and LinkError, plus the UCIe 3.0 mechanisms listed above.",
    ", AoU- und UCIe-Phasen: lokale Annahme, Packing, Übertragung, entfernter Empfang, Target-Annahme, architektonischer Commit, Antworterzeugung und Antwortzustellung. Fehler werden in einer gewählten Phase scharf gemacht. Die geübten UCIe-Pfade sind CRC-Replay, die Retry-Schwelle, Retrain, LinkReset und LinkError, dazu die oben genannten UCIe-3.0-Mechanismen.",
)
add(
    "The <span class=\"term\" title=\"Not safe to repeat. Doing it twice is not the same as doing it once, so a blind retry can commit the effect a second time.\">non-idempotent</span> <abbr class=\"term\" title=\"Memory-mapped I/O. A read or write to a device address, such as a control register, rather than to ordinary memory.\">MMIO</abbr> model commits at the target. The initiator-visible record stores whether ",
    "Das <span class=\"term\" title=\"Nicht sicher wiederholbar. Zweimal ausführen ist nicht dasselbe wie einmal, daher kann ein blindes Retry den Effekt ein zweites Mal committen.\">nicht-idempotent</span> <abbr class=\"term\" title=\"Memory-mapped I/O. Ein Lese- oder Schreibzugriff auf eine Geräteadresse, etwa ein Steuerregister, und nicht auf gewöhnlichen Speicher.\">MMIO</abbr>-Modell committet am Target. Der für den Initiator sichtbare Datensatz speichert, ob ",
)
add(
    " was accepted and whether OKAY was delivered. Systematic scenarios cover the phase crossings. A second suite draws 200 randomized injections from seed 1. Both suites are replayed on the UCIe 3.0 rule set.",
    " angenommen wurde und ob OKAY zugestellt wurde. Systematische Szenarien decken die Phasenübergänge ab. Eine zweite Suite zieht 200 zufällige Injektionen aus Seed 1. Beide Suites werden auf dem UCIe-3.0-Regelsatz nachgespielt.",
)
add('aria-label="Example of ground truth against initiator state"', 'aria-label="Beispiel: Grundwahrheit gegenüber dem Initiatorzustand"')
add("<span>Ground truth</span>", "<span>Grundwahrheit</span>")
add("<span>Result</span>", "<span>Ergebnis</span>")
add(
    "That split is T17. The classifier never consults the commit count. A later check uses the count only to detect a false safe retry or a duplicate commit.",
    "Diese Trennung ist T17. Der Klassifikator liest den Commit-Zähler nie. Eine spätere Prüfung verwendet den Zähler nur, um ein falsch als sicher eingestuftes Retry oder einen doppelten Commit zu erkennen.",
)
add(
    "Outstanding-transaction fate after cleanup is a named policy, because the specification text used here does not define the ",
    "Der Ausgang einer offenen Transaktion nach dem Aufräumen ist eine benannte Policy, weil der hier verwendete Spezifikationstext die ",
)
add(
    ' completion for an outstanding transaction. Five policies are compared. Observation profiles restrict what the classifier may read: FDI and ',
    "-Completion für eine offene Transaktion nicht festlegt. Fünf Policies werden verglichen. Beobachtungsprofile beschränken, was der Klassifikator lesen darf: FDI und ",
)
add(
    '; software error registers; standardized management; and, as an explicit assumption, latched retry disposition. <a href="methodology.html">Method, policies, and what was taken from the specifications</a>.',
    "; Software-Fehlerregister; standardisiertes Management; und, als ausdrückliche Annahme, gelatchte Retry-Disposition. <a href=\"methodology.html\">Methode, Policies und was aus den Spezifikationen übernommen wurde</a>.",
)

# --- results on index ---
add("08 · Measured results", "08 · Gemessene Ergebnisse")
add(
    "<h2>UCIe 2.0 baseline, then the same workload on UCIe 3.0.</h2>",
    "<h2>UCIe-2.0-Baseline, danach dieselbe Last auf UCIe 3.0.</h2>",
)
add(
    "Counts below are the generated UCIe 2.0 report: 89 systematic simulations and 200 randomized simulations at seed 1. Classification counts are transaction labels, not a partition of the 289 simulations.",
    "Die Zähler unten sind der erzeugte UCIe-2.0-Bericht: 89 systematische Simulationen und 200 zufällige Simulationen bei Seed 1. Die Klassifikationszähler sind Transaktionskennzeichnungen, keine Zerlegung der 289 Simulationen.",
)
add("<h3>Workload</h3>", "<h3>Last</h3>")
add("<h3>Faults and recovery</h3>", "<h3>Fehler und Recovery</h3>")
add(" class</h3>", "-Klasse</h3>")
add("<h3>Duplicate effects</h3>", "<h3>Doppelte Effekte</h3>")
add("<span>Simulations</span>", "<span>Simulationen</span>")
add("<span>Systematic</span>", "<span>Systematisch</span>")
add("<span>Randomized</span>", "<span>Zufällig</span>")
add("<span>Fault injections</span>", "<span>Fehlerinjektionen</span>")
add("<span>Transparent retries</span>", "<span>Transparente Retries</span>")
add("<span>Duplicate commit</span>", "<span>Doppelter Commit</span>")
add("<span>False safe retry</span>", "<span>Falsch sicheres Retry</span>")
add(
    "Baseline invariant lines. Policy D\u2019s separate violation count is not added here.",
    "Invariantenzeilen der Baseline. Der getrennte Verletzungszähler von Policy D wird hier nicht addiert.",
)
add(
    "The equivalent workload on the UCIe 3.0 model reports the same 289 simulations, the same 171 UNKNOWN transactions, and 204 recovery traces explored on each side. Duplicate commit and false safe retry remain reachable. The report also records 17 liveness failures, 10 credit-invariant violations, and 0 receiver overflows on the UCIe 2.0 baseline, and 17 liveness failures on the UCIe 3.0 equivalent workload. Those liveness and credit totals are simulator outcomes. This study does not currently attribute them to normative UCIe defects. The model includes an injected credit fault. The generated report records the 10 credit-invariant violations and does not split them by cause.",
    "Die äquivalente Last auf dem UCIe-3.0-Modell berichtet dieselben 289 Simulationen, dieselben 171 UNKNOWN-Transaktionen und 204 untersuchte Recovery-Traces auf jeder Seite. Doppelter Commit und falsch sicheres Retry bleiben erreichbar. Der Bericht verzeichnet außerdem 17 Lebendigkeitsfehler, 10 Credit-Invariantenverletzungen und 0 Empfängerüberläufe auf der UCIe-2.0-Baseline sowie 17 Lebendigkeitsfehler auf der äquivalenten UCIe-3.0-Last. Diese Lebendigkeits- und Credit-Summen sind Ergebnisse des Simulators. Die Studie schreibt sie derzeit keinen normativen UCIe-Mängeln zu. Das Modell enthält einen injizierten Credit-Fehler. Der erzeugte Bericht verzeichnet die 10 Credit-Invariantenverletzungen und teilt sie nicht nach Ursache auf.",
)
add("<h3 style=\"margin-top: var(--space-32)\">UNKNOWN rate by fault position</h3>", "<h3 style=\"margin-top: var(--space-32)\">UNKNOWN-Rate nach Fehlerposition</h3>")
add(
    "Fault-workload observation, seed 1 — not an estimated field failure probability. Randomized suite only, watched transaction, FDI/",
    "Beobachtung unter der Fehlerlast, Seed 1 — keine geschätzte Feldausfallwahrscheinlichkeit. Nur die Zufallssuite, beobachtete Transaktion, FDI/",
)
add(
    ". A non-zero COMPLETED count means replay or a non-destructive fault still delivered the response. The comparison reports these same five rates for UCIe 3.0. They are not used as the scientific conclusion. Reachability is.",
    ". Ein COMPLETED-Zähler ungleich null heißt, dass Replay oder ein nicht zerstörender Fehler die Antwort dennoch zugestellt hat. Der Vergleich berichtet dieselben fünf Raten für UCIe 3.0. Sie tragen nicht die wissenschaftliche Schlussfolgerung. Die Erreichbarkeit tut das.",
)
add(
    'aria-label="UNKNOWN rate by fault position. Before target acceptance 48.8 percent, 21 of 43. After target acceptance 52.1 percent, 25 of 48. After architectural commit 69.4 percent, 25 of 36. After response generation 54.5 percent, 18 of 33. After response delivery 0 percent, 0 of 40."',
    'aria-label="UNKNOWN-Rate nach Fehlerposition. Vor der Target-Annahme 48,8 Prozent, 21 von 43. Nach der Target-Annahme 52,1 Prozent, 25 von 48. Nach dem architektonischen Commit 69,4 Prozent, 25 von 36. Nach der Antwortgenerierung 54,5 Prozent, 18 von 33. Nach der Antwortzustellung 0 Prozent, 0 von 40."',
)
add(">Before target acceptance<", ">Vor der Target-Annahme<")
add(">After target acceptance<", ">Nach der Target-Annahme<")
add(">After architectural commit<", ">Nach dem architektonischen Commit<")
add(">After response generation<", ">Nach der Antwortgenerierung<")
add(">After response delivery<", ">Nach der Antwortzustellung<")
add(
    "Fault-workload observation, seed 1 — not an estimated field failure probability. Bar scale is 0 to 100 percent. Editable chart: ",
    "Beobachtung unter der Fehlerlast, Seed 1 — keine geschätzte Feldausfallwahrscheinlichkeit. Die Balken skala ist 0 bis 100 Prozent. Editierbares Diagramm: ",
)
add(
    "Same randomized, watched-transaction sample. UNKNOWN rate is UNKNOWN divided by n.",
    "Dieselbe zufällige Stichprobe der beobachteten Transaktion. Die UNKNOWN-Rate ist UNKNOWN geteilt durch n.",
)
add("<th scope=\"col\">Fault position</th>", "<th scope=\"col\">Fehlerposition</th>")
add("<th scope=\"col\" class=\"num\">UNKNOWN rate</th>", "<th scope=\"col\" class=\"num\">UNKNOWN-Rate</th>")
add(
    "The logical-event ambiguity window, counted only after architectural commit, is the same on both revisions: 137 closed windows and 78 open windows. An open window means the initiator returned to an operational link without a completion and without a standardized commit bit. The window is not eliminated. <a href=\"results.html\">Full tables, policies, and equivalence pairs</a>.",
    "Das Ambiguitätsfenster in logischen Ereignissen, gezählt erst nach dem architektonischen Commit, ist in beiden Revisionen gleich: 137 geschlossene Fenster und 78 offene Fenster. Ein offenes Fenster heißt, dass der Initiator zu einem betriebsfähigen Link zurückgekehrt ist, ohne Completion und ohne standardisiertes Commit-Bit. Das Fenster ist nicht beseitigt. <a href=\"results.html\">Vollständige Tabellen, Policies und Äquivalenzpaare</a>.",
)

# --- safety ---
add("09 · Safety implications", "09 · Sicherheitsfolgen")
add(
    "<h2>The automotive system has to define a policy for a command whose effect cannot be reconstructed.</h2>",
    "<h2>Das automobile System muss eine Policy für einen Befehl festlegen, dessen Effekt sich nicht rekonstruieren lässt.</h2>",
)
add(
    "UCIe provides mechanisms that can support automotive reliability, availability, diagnostics, and safety architectures. It does not itself define ISO 26262 safety goals or the complete application-level reaction to every communication fault. The result identifies one decision that the automotive system architecture must make explicitly: how an interrupted ",
    "UCIe stellt Mechanismen bereit, die automobile Zuverlässigkeit, Verfügbarkeit, Diagnose und Sicherheitsarchitekturen stützen können. Es definiert selbst weder ISO-26262-Sicherheitsziele noch die vollständige Reaktion der Anwendung auf jeden Kommunikationsfehler. Das Ergebnis benennt eine Entscheidung, die die automobile Systemarchitektur ausdrücklich treffen muss: wie eine unterbrochene ",
)
add(
    "Policy D retries the same logical transaction after the link is up. On the post-commit loss, the initiator is then told COMPLETED, <code>completion_received</code> is true, and <code>architectural_commit_count</code> is 2. The first commit of that scenario moved the counter from 7 to 8. The retry is a second architectural effect of the same increment. That is a simulation result for this policy, not a required UCIe behavior.",
    "Policy D wiederholt dieselbe logische Transaktion, nachdem der Link oben ist. Beim Verlust nach dem Commit wird dem Initiator danach COMPLETED gemeldet, <code>completion_received</code> ist true, und <code>architectural_commit_count</code> ist 2. Der erste Commit dieses Szenarios hat den Zähler von 7 auf 8 gesetzt. Das Retry ist ein zweiter architektonischer Effekt desselben Inkrements. Das ist ein Simulationsergebnis für diese Policy, kein gefordertes UCIe-Verhalten.",
)
add("<li><strong>No response</strong></li>", "<li><strong>Keine Antwort</strong></li>")
add("<li><strong>Naive interpretation</strong><br>Transaction failed</li>", "<li><strong>Naive Deutung</strong><br>Transaktion fehlgeschlagen</li>")
add("<li><strong>Automatic retry</strong></li>", "<li><strong>Automatisches Retry</strong></li>")
add("<li><strong>Original transaction had already committed</strong></li>", "<li><strong>Die ursprüngliche Transaktion hatte bereits committet</strong></li>")
add(
    "<li><strong>Duplicate effect</strong><br>Counter 7 → 8, then a second commit</li>",
    "<li><strong>Doppelter Effekt</strong><br>Zähler 7 → 8, danach ein zweiter Commit</li>",
)
add(
    "Exactly-once semantics therefore cannot be assumed from link recovery alone. They need an additional contract: transaction sequencing, persistent commit tracking, or duplicate suppression. UNKNOWN is the sound classification for the modeled observer when the available information proves neither completion nor non-execution. It is not a state that must always be passed to an application or a guest. A real system may instead:",
    "Exactly-once-Semantik lässt sich daher nicht aus der Link-Recovery allein annehmen. Sie braucht einen zusätzlichen Vertrag: Transaktionsreihenfolge, dauerhafte Commit-Verfolgung oder Duplikatunterdrückung. UNKNOWN ist die tragfähige Klassifikation für den modellierten Beobachter, wenn die verfügbare Information weder den Abschluss noch die Nichtausführung belegt. Es ist kein Zustand, der stets an eine Anwendung oder einen Gast durchgereicht werden muss. Ein reales System kann stattdessen:",
)
add(
    "transition the operation or device into an ",
    "die Operation oder das Gerät in einen ",
)
add(
    " state and escalate to the safety manager,</li>",
    " Zustand überführen und an den Safety-Manager eskalieren,</li>",
)
add(
    "<li>reset or reinitialize a sufficiently large fault-containment region,</li>",
    "<li>einen hinreichend großen Fehlercontainment-Bereich zurücksetzen oder neu initialisieren,</li>",
)
add(
    "<li>use an end-to-end exactly-once or duplicate-suppression mechanism.</li>",
    "<li>einen Ende-zu-Ende-Mechanismus für Exactly-once oder Duplikatunterdrückung verwenden.</li>",
)
add(
    "Any of those can be a valid system-level response. Containment is not the same thing as reconstructing whether the original operation committed. Automatic retry of an interrupted ",
    "Jede dieser Antworten kann auf Systemebene gültig sein. Eingrenzung ist nicht dasselbe wie die Rekonstruktion, ob die ursprüngliche Operation committet wurde. Ein automatisches Retry einer unterbrochenen ",
)
add(
    " operation is unsafe in this model without end-to-end commit knowledge or duplicate suppression.",
    " Operation ist in diesem Modell ohne Ende-zu-Ende-Wissen über den Commit oder ohne Duplikatunterdrückung unsicher.",
)
add(
    "A link-scoped recovery event may affect transaction-fate knowledge for multiple clients sharing that physical transport. The same CRC, Retrain, and LinkError status appears, in the equivalence pairs, for a command that committed and for a command that did not. A LinkError invalidates a known success. It does not justify recording the command as known not to have executed. The two-guest run motivates a separate freedom-from-interference study. It does not by itself establish an ISO 26262 freedom-from-interference violation. Determining whether physical-link recovery can produce cross-guest temporal, spatial, or transaction-state interference in a virtual ECU architecture is future work.",
    "Ein Recovery-Ereignis mit Link-Geltung kann das Wissen über den Transaktionsausgang für mehrere Clients beeinträchtigen, die denselben physischen Transport teilen. Derselbe CRC-, Retrain- und LinkError-Status erscheint in den Äquivalenzpaaren für einen Befehl, der committet hat, und für einen, der es nicht hat. Ein LinkError entwertet einen bekannten Erfolg. Er rechtfertigt nicht, den Befehl als bekanntermaßen nicht ausgeführt zu verzeichnen. Der Zwei-Gast-Lauf motiviert eine eigene Untersuchung der Rückwirkungsfreiheit. Er begründet für sich keine Verletzung der Rückwirkungsfreiheit nach ISO 26262. Ob die Recovery des physischen Links in einer virtuellen-ECU-Architektur zeitliche, räumliche oder den Transaktionszustand betreffende Interferenz zwischen Gästen erzeugen kann, ist künftige Arbeit.",
)

# --- specification ---
add("10 · Specification interpretation", "10 · Spezifikationsdeutung")
add("<h2>Link recovery and transaction recovery are different events.</h2>", "<h2>Link-Recovery und Transaktions-Recovery sind verschiedene Ereignisse.</h2>")
add("<h3>What the specifications provide</h3>", "<h3>Was die Spezifikationen bereitstellen</h3>")
add("<h3>What they do not provide in the modeled scenarios</h3>", "<h3>Was sie in den modellierten Szenarien nicht bereitstellen</h3>")
add("<li>Link fault detection, including CRC and replay.</li>", "<li>Erkennung von Linkfehlern, einschließlich CRC und Replay.</li>")
add("<li>Retrain when errors cross an implementation-specific threshold.</li>", "<li>Retrain, wenn Fehler eine implementierungsspezifische Schwelle überschreiten.</li>")
add(
    "<li>LinkError, including a visible state, partner propagation, and cleanup in which in-flight packets may be dropped.</li>",
    "<li>LinkError, einschließlich eines sichtbaren Zustands, der Weitergabe an den Partner und eines Aufräumens, bei dem Flits unterwegs verworfen werden können.</li>",
)
add(
    "<li>Recovery coordination back to Active, including the sequence-number handshake.</li>",
    "<li>Recovery-Koordination zurück nach Active, einschließlich des Sequenznummern-Handshakes.</li>",
)
add(
    "<li>Error propagation on FDI. UCIe 3.0 adds further software-visible and management-visible status. Those fields describe the link, the electrical margin, or an emergency or power indication.</li>",
    "<li>Fehlerweitergabe auf FDI. UCIe 3.0 ergänzt weiteren für Software und Management sichtbaren Status. Diese Felder beschreiben den Link, die elektrische Reserve oder eine Notfall- oder Power-Anzeige.</li>",
)
add(
    " transaction\u2019s architectural commit state. That state belongs to the target device.</li>",
    "-Transaktion. Dieser Zustand gehört zum Target-Gerät.</li>",
)
add(
    "A standardized indication, in the UCIe status modeled here, of the associated ",
    "Eine standardisierte Anzeige, im hier modellierten UCIe-Status, für den architektonischen Commit-Zustand der zugehörigen ",
)
add(
    "<li>End-to-end exactly-once semantics. Those are an upper-layer recovery responsibility when the system requires them.</li>",
    "<li>Ende-zu-Ende-Exactly-once-Semantik. Die ist eine Recovery-Verantwortung der höheren Schicht, wenn das System sie verlangt.</li>",
)
add("<li>Mandatory transaction commit logging.</li>", "<li>Verpflichtendes Logging des Transaktions-Commits.</li>")
add("<li>Duplicate suppression across recovery.</li>", "<li>Duplikatunterdrückung über die Recovery hinweg.</li>")
add(
    "<li>A deterministic classification of every interrupted transaction from link status alone.</li>",
    "<li>Eine deterministische Klassifikation jeder unterbrochenen Transaktion allein aus dem Linkstatus.</li>",
)
add(
    "This study does not claim that UCIe is incorrect. It identifies a cross-layer recovery gap between link-level recovery semantics and transaction-level architectural knowledge.",
    "Diese Studie behauptet nicht, UCIe sei fehlerhaft. Sie benennt eine schichtübergreifende Recovery-Lücke zwischen der Recovery-Semantik der Linkschicht und dem architektonischen Wissen auf Transaktionsebene.",
)
add(
    "UCIe 3.0 §10.3.4, as used by the model, still says the adapter and PHY do not guarantee quiescence of protocol transactions. The investigated UCIe status and management information describes link, transport, electrical, or emergency state. It does not provide the modeled initiator with a standardized ",
    "UCIe 3.0 §10.3.4 sagt, wie vom Modell verwendet, weiterhin, dass Adapter und PHY die Ruhe der Protokolltransaktionen nicht garantieren. Der untersuchte UCIe-Status und die Managementinformation beschreiben Link, Transport, elektrischen Zustand oder Notfall. Sie geben dem modellierten Initiator keine standardisierte ",
)
add(
    " architectural-commit indication.",
    " Anzeige des architektonischen Commits.",
)
add(
    "The model identifies an integration ambiguity whose outcome depends on retry-state handling across Retrain. Preserving retry state ends the unacknowledged-response case as COMPLETED with commit count 1. Reinitializing it, which is what §3.8 describes for the IDLE-handshake retry variables, ends as UNKNOWN with commit count 1 and a deadlock flag. The study does not treat that difference as a confirmed normative defect. Separately, a write response already queued in the initiating AoU and not yet accepted on ",
    "Das Modell benennt eine Integrationsambiguität, deren Ausgang von der Behandlung des Retry-Zustands über Retrain hinweg abhängt. Den Retry-Zustand zu erhalten beendet den Fall der unquittierten Antwort als COMPLETED mit Commit-Zähler 1. Ihn neu zu initialisieren, wie §3.8 es für die Retry-Variablen des IDLE-Handshakes beschreibt, endet als UNKNOWN mit Commit-Zähler 1 und einem Deadlock-Flag. Die Studie behandelt diesen Unterschied nicht als bestätigten normativen Mangel. Getrennt davon ist eine Schreibantwort, die im initiierenden AoU bereits eingereiht und auf ",
)
add(
    " is UNKNOWN under policy A and COMPLETED under policy B, both with commit count 1. Behavior of that queued response is not determined by the modeled UCIe interface. The report calls it an integration choice, not a detected coding bug.",
    " noch nicht angenommen ist, unter Policy A UNKNOWN und unter Policy B COMPLETED, beide mit Commit-Zähler 1. Das Verhalten dieser eingereihten Antwort legt die modellierte UCIe-Schnittstelle nicht fest. Der Bericht nennt das eine Integrationsentscheidung, keinen festgestellten Codierfehler.",
)

# --- limitations, reproduce, context, questions, conclusion, sources ---
add("11 · Limitations", "11 · Grenzen")
add("<h2>What this study can and cannot be used to claim.</h2>", "<h2>Wofür diese Studie als Beleg taugt und wofür nicht.</h2>")
add(
    "<li>The simulation is an abstract protocol model, not a silicon implementation. It is intentionally event-accurate. Cycle-accurate modeling would be required before any claim about recovery latency or deadline miss.</li>",
    "<li>Die Simulation ist ein abstraktes Protokollmodell, keine Siliziumimplementierung. Sie ist absichtlich ereignisgenau. Vor jeder Aussage über Recovery-Latenz oder eine verfehlte Deadline wäre zyklusgenaue Modellierung nötig.</li>",
)
add(
    "<li>The complete AoU 0.8 text was not available to the model-building workspace. This is not a full AoU 0.8 conformance implementation. AoU packing and credit behavior are explicit abstractions. The cited §8.3.3 link-down rule is the normative AoU behavior used by the model.</li>",
    "<li>Der vollständige AoU-0.8-Text lag dem Arbeitsbereich zum Modellbau nicht vor. Dies ist keine vollständige AoU-0.8-Konformitätsimplementierung. AoU-Packing und Credit-Verhalten sind ausdrückliche Abstraktionen. Die zitierte Link-down-Regel §8.3.3 ist das normative AoU-Verhalten, das das Modell verwendet.</li>",
)
add(
    " completion.</li>",
    "-Completion.</li>",
)
add(
    "<li>Some behavior is an integration choice or a named policy, because the modeled UCIe interface does not determine the ",
    "<li>Manches Verhalten ist eine Integrationsentscheidung oder eine benannte Policy, weil die modellierte UCIe-Schnittstelle die ",
)
add(
    "<li>Randomized percentages are a fault-workload observation at seed 1. They are not field failure probabilities.</li>",
    "<li>Zufällige Prozentsätze sind eine Beobachtung unter der Fehlerlast bei Seed 1. Sie sind keine Feldausfallwahrscheinlichkeiten.</li>",
)
add("<li>Reachability is the scientific conclusion. The rates are not.</li>", "<li>Die Erreichbarkeit ist die wissenschaftliche Schlussfolgerung. Die Raten sind es nicht.</li>")
add(
    "<li>The study does not prove that every UCIe implementation exhibits this behavior.</li>",
    "<li>Die Studie beweist nicht, dass jede UCIe-Implementierung dieses Verhalten zeigt.</li>",
)
add(
    "<li>The ambiguity is reachable under the modeled, specification-derived recovery behavior and the stated device assumptions.</li>",
    "<li>Die Ambiguität ist unter dem modellierten, spezifikationsabgeleiteten Recovery-Verhalten und den genannten Geräteannahmen erreichbar.</li>",
)
add(
    "<li>UCIe link behavior and target architectural-commit semantics are modeled at different layers. Commit is what the target device does. Observation is what the initiator can read. The surrounding system owns the recovery contract above the link.</li>",
    "<li>UCIe-Linkverhalten und die Semantik des architektonischen Commits am Target sind auf verschiedenen Schichten modelliert. Der Commit ist das, was das Target-Gerät tut. Die Beobachtung ist das, was der Initiator lesen kann. Das umgebende System besitzt den Recovery-Vertrag oberhalb des Links.</li>",
)
add(
    '<p><a href="limitations.html">Modeling limits, parameters, and the device-commit assumption</a>.</p>',
    '<p><a href="limitations.html">Modellgrenzen, Parameter und die Annahme zum Geräte-Commit</a>.</p>',
)
add("12 · Reproducibility", "12 · Reproduzierbarkeit")
add("<h2>The reports are generated. The counts are not hand-written.</h2>", "<h2>Die Berichte werden erzeugt. Die Zähler sind nicht von Hand geschrieben.</h2>")
add("<p>From the repository root:</p>", "<p>Vom Wurzelverzeichnis des Repository:</p>")
add(
    "The second command writes <code>simulation/results/findings.md</code>, <code>simulation/results/findings_ucie3.md</code>, and <code>simulation/results/comparison_ucie2_vs_ucie3.md</code>. Copies published with this site are linked below so the pages do not depend on a server.",
    "Der zweite Befehl schreibt <code>simulation/results/findings.md</code>, <code>simulation/results/findings_ucie3.md</code> und <code>simulation/results/comparison_ucie2_vs_ucie3.md</code>. Die mit dieser Seite veröffentlichten Kopien sind unten verlinkt, damit die Seiten keinen Server brauchen. Die Berichte bleiben englisch.",
)
add('<a class="button button-secondary" href="results.html">Detailed results</a>', '<a class="button button-secondary" href="results.html">Ausführliche Ergebnisse</a>')
add("13 · Context", "13 · Einordnung")
add(
    "<h2>The general phenomenon is known. The contribution is the boundary this model executes.</h2>",
    "<h2>Das allgemeine Phänomen ist bekannt. Der Beitrag ist die Grenze, die dieses Modell ausführt.</h2>",
)
add(
    "The general request-executed / response-lost ambiguity is a known distributed-systems and exactly-once-processing problem. The contribution of this study is not the discovery of that general phenomenon.",
    "Die allgemeine Ambiguität aus ausgeführter Anfrage und verlorener Antwort ist ein bekanntes Problem verteilter Systeme und der Exactly-once-Verarbeitung. Der Beitrag dieser Studie ist nicht die Entdeckung dieses allgemeinen Phänomens.",
)
add(
    "To our knowledge, the work established by this repository is narrower. It applies that phenomenon to the ",
    "Nach unserem Kenntnisstand ist die durch dieses Repository belegte Arbeit enger. Sie wendet das Phänomen auf die Grenze ",
)
add(
    "/AoU–UCIe boundary, constructs executable fault traces, separates architectural ground truth from standardized observer-visible state, demonstrates duplicate execution under blind replay, compares equivalent UCIe 2.0 and UCIe 3.0 recovery scenarios, and frames the consequence for automotive virtualization. A proper literature review would be required before any stronger novelty claim.",
    "/AoU–UCIe an, konstruiert ausführbare Fehlertraces, trennt architektonische Grundwahrheit vom standardisierten, für den Beobachter sichtbaren Zustand, zeigt doppelte Ausführung bei blindem Replay, vergleicht äquivalente Recovery-Szenarien von UCIe 2.0 und UCIe 3.0 und rahmt die Folge für die automobile Virtualisierung. Vor jeder stärkeren Neuheitsbehauptung wäre eine eigentliche Literaturdurchsicht nötig.",
)
add("14 · Open questions", "14 · Offene Fragen")
add("<h2>Questions this study does not answer.</h2>", "<h2>Fragen, die diese Studie nicht beantwortet.</h2>")
add(
    "These are future research questions. UCIe 3D is not part of the UCIe 2.0 and UCIe 3.0 simulation. No 3D result is mixed into the counts above.",
    "Das sind künftige Forschungsfragen. UCIe 3D ist nicht Teil der Simulation von UCIe 2.0 und UCIe 3.0. Kein 3D-Ergebnis ist in die Zähler oben gemischt.",
)
add(
    "<li>Which layer should own transaction-fate recovery: AoU, the device protocol, the hypervisor, the safety manager, or the application?</li>",
    "<li>Welche Schicht soll die Recovery des Transaktionsausgangs besitzen: AoU, das Geräteprotokoll, der Hypervisor, der Safety-Manager oder die Anwendung?</li>",
)
add(
    " classes may be safely retried after destructive recovery?</li>",
    "-Klassen dürfen nach einer zerstörenden Recovery sicher wiederholt werden?</li>",
)
add(
    "<li>Which ",
    "<li>Welche ",
)
add(
    "<li>Can persistent transaction IDs or duplicate suppression provide exactly-once behavior across LinkError or reset?</li>",
    "<li>Können dauerhafte Transaktions-IDs oder Duplikatunterdrückung Exactly-once-Verhalten über LinkError oder Reset hinweg herstellen?</li>",
)
add(
    "<li>How should multiple virtual ECUs sharing one physical UCIe link preserve freedom from interference during recovery?</li>",
    "<li>Wie sollen mehrere virtuelle ECUs, die einen physischen UCIe-Link teilen, während der Recovery die Rückwirkungsfreiheit wahren?</li>",
)
add(
    "<li>What are the worst-case fault-detection and recovery latencies for safety-critical traffic? The present model is event-accurate and does not answer that in nanoseconds.</li>",
    "<li>Wie groß sind die Worst-Case-Latenzen für Fehlererkennung und Recovery bei sicherheitskritischem Verkehr? Das vorliegende Modell ist ereignisgenau und beantwortet das nicht in Nanosekunden.</li>",
)
add(
    "<li>How do UCIe 3.0 Priority Sideband, Fast Throttle, Emergency Shutdown, and runtime recalibration interact with automotive safety-state transitions?</li>",
    "<li>Wie wirken UCIe-3.0-Priority-Sideband, Fast Throttle, Emergency Shutdown und Runtime-Rekalibrierung mit automobilen Übergängen des Sicherheitszustands zusammen?</li>",
)
add(
    "<li>Does UCIe 3D require a different end-to-end integrity and transaction-recovery model when adapter-level CRC and replay are absent or optional for the selected architecture?</li>",
    "<li>Verlangt UCIe 3D ein anderes Modell für Ende-zu-Ende-Integrität und Transaktions-Recovery, wenn CRC und Replay auf Adapterebene für die gewählte Architektur fehlen oder optional sind?</li>",
)
add("15 · Conclusion", "15 · Schluss")
add(
    "<h2>Restoring the link and reconstructing the command are different problems.</h2>",
    "<h2>Den Link wiederherzustellen und den Befehl zu rekonstruieren sind verschiedene Probleme.</h2>",
)
add(
    "Under the modeled recovery scenarios, UCIe 2.0 and UCIe 3.0 can restore link operation without necessarily providing the modeled ",
    "Unter den modellierten Recovery-Szenarien können UCIe 2.0 und UCIe 3.0 den Linkbetrieb wiederherstellen, ohne dem modellierten ",
)
add(
    " initiator enough standardized information to reconstruct the architectural fate of every outstanding ",
    " Initiator zwingend genug standardisierte Information zu geben, um den architektonischen Ausgang jeder offenen ",
)
add(" operation.</p>", " Operation zu rekonstruieren.</p>")
add(
    "This is not a claim that UCIe is incorrect. It identifies a cross-layer integration responsibility: a safety-critical system that requires deterministic transaction recovery must provide an additional contract above or around link recovery.",
    "Das ist keine Behauptung, UCIe sei fehlerhaft. Es benennt eine schichtübergreifende Integrationsverantwortung: Ein sicherheitskritisches System, das deterministische Transaktions-Recovery verlangt, muss einen zusätzlichen Vertrag oberhalb oder um die Link-Recovery herum bereitstellen.",
)
add(
    "For automotive virtualization, the relevant safety question is therefore not only whether the physical link recovered, but whether the system has a defined and safe policy for commands whose execution state remains ",
    "Für die automobile Virtualisierung lautet die relevante Sicherheitsfrage daher nicht nur, ob der physische Link wiederhergestellt ist, sondern ob das System eine festgelegte und sichere Policy für Befehle hat, deren Ausführungszustand ",
)
add(
    "UCIe provides robust mechanisms for detecting, reporting, and recovering from communication faults. The study shows that restoring communication and reconstructing the architectural fate of an interrupted higher-layer operation are different problems. A safety-critical system must define the latter explicitly.",
    "UCIe stellt robuste Mechanismen bereit, um Kommunikationsfehler zu erkennen, zu melden und die Kommunikation wiederherzustellen. Die Studie zeigt, dass die Wiederherstellung der Kommunikation und die Rekonstruktion des architektonischen Ausgangs einer unterbrochenen Operation der höheren Schicht verschiedene Probleme sind. Ein sicherheitskritisches System muss Letzteres ausdrücklich festlegen.",
)
add("16 · Sources", "16 · Quellen")
add("<h2>Reports, model, and tests</h2>", "<h2>Berichte, Modell und Tests</h2>")
add(
    "<p>Published with the site:</p>",
    "<p>Mit der Seite veröffentlicht. Die Berichte bleiben die englischen Originale:</p>",
)
add(
    '<li><a href="sources/findings.md">UCIe 2.0 findings</a> · copy of <code>simulation/results/findings.md</code></li>',
    '<li><a href="sources/findings.md">UCIe-2.0-Bericht</a> · Kopie von <code>simulation/results/findings.md</code></li>',
)
add(
    '<li><a href="sources/findings_ucie3.md">UCIe 3.0 findings</a> · copy of <code>simulation/results/findings_ucie3.md</code></li>',
    '<li><a href="sources/findings_ucie3.md">UCIe-3.0-Bericht</a> · Kopie von <code>simulation/results/findings_ucie3.md</code></li>',
)
add(
    '<li><a href="sources/comparison_ucie2_vs_ucie3.md">Comparison</a> · copy of <code>simulation/results/comparison_ucie2_vs_ucie3.md</code></li>',
    '<li><a href="sources/comparison_ucie2_vs_ucie3.md">Vergleich</a> · Kopie von <code>simulation/results/comparison_ucie2_vs_ucie3.md</code></li>',
)
add('<li><a href="data/findings.json">Machine-readable figures</a></li>', '<li><a href="data/findings.json">Maschinenlesbare Zahlen</a></li>')
add(
    '<li><a href="review_notes.md">Review notes</a> · claims, assumptions, metrics, conservative wording</li>',
    '<li><a href="review_notes.md">Prüfnotizen</a> · Behauptungen, Annahmen, Metriken, zurückhaltende Formulierung</li>',
)
add(
    "<p>Repository tree, at the root of the project rather than on the Pages artifact:</p>",
    "<p>Repository-Baum, in der Projektwurzel und nicht im Pages-Artefakt:</p>",
)
add(
    " injector and scenarios</li>",
    " Injektor und Szenarien</li>",
)
add(
    " systematic, randomized, and comparison runners</li>",
    " systematische, zufällige und Vergleichsläufe</li>",
)
add(
    "<li><code>README.md</code> run instructions and the specification rules the model names</li>",
    "<li><code>README.md</code> Laufanweisungen und die Spezifikationsregeln, die das Modell benennt</li>",
)
add(
    "Specification editions named by the reports: UCIe Specification Revision 2.0, Version 1.0 (6 August 2024), and Revision 3.0, Version 1.0 (5 August 2025). Rules actually implemented are cited from <code>simulation/model/ucie.py</code> and <code>simulation/model/spec30.py</code>.",
    "Von den Berichten benannte Spezifikationsausgaben: UCIe Specification Revision 2.0, Version 1.0 (6. August 2024) und Revision 3.0, Version 1.0 (5. August 2025). Tatsächlich implementierte Regeln werden aus <code>simulation/model/ucie.py</code> und <code>simulation/model/spec30.py</code> zitiert.",
)
add(
    "NUVUS · UCIe transaction-fate study. Figures come from the simulator reports. This site does not add measurements.",
    "NUVUS · UCIe-Studie zum Transaktionsausgang. Die Zahlen stammen aus den Simulatorberichten. Diese Seite fügt keine Messwerte hinzu.",
)

# --- methodology page ---
add(
    "<title>Simulation method — UCIe transaction-fate study</title>",
    "<title>Simulationsmethode — UCIe-Studie zum Transaktionsausgang</title>",
)
add(
    'content="Discrete-event model, observation profiles, and the policies used to classify AXI transaction fate after UCIe recovery."',
    'content="Ereignismodell, Beobachtungsprofile und die Policies, mit denen der AXI-Transaktionsausgang nach der UCIe-Recovery klassifiziert wird."',
)
add(
    '<h1 class="page-title">How the model separates architectural truth from what the initiator can see</h1>',
    '<h1 class="page-title">Wie das Modell architektonische Wahrheit von dem trennt, was der Initiator sehen kann</h1>',
)
add(
    "/AoU-inspired transaction model over specification-derived UCIe recovery behavior. It records the target\u2019s commit count and, separately, the completion the initiator actually received.",
    "/AoU-inspiriertes Transaktionsmodell über spezifikationsabgeleitetem UCIe-Recovery-Verhalten. Es zeichnet den Commit-Zähler des Targets auf und, getrennt davon, die Completion, die der Initiator tatsächlich erhalten hat.",
)
add("<h2>Event model</h2>", "<h2>Ereignismodell</h2>")
add(
    "Time is a sequence of protocol events. The 375-flit replay timeout, the 128-flit sequence-number handshake bound, the 8 ms sideband timeout, and the 16 ms LinkError residency are single events in the model. They are not wall-clock delays. The CRC LFSR in the specification is not executed. One-, two-, and three-bit flips are treated as detected because UCIe §3.7 guarantees detection of up to three bits. The replay-count limit defaults to 3. Section 3.8 points at the PCIe rules for speeds at or below 32 GT/s and does not restate that integer, so the model keeps it as a parameter.",
    "Die Zeit ist eine Folge von Protokollereignissen. Das Replay-Timeout von 375 Flits, die Handshake-Grenze der Sequenznummer von 128 Flits, das Sideband-Timeout von 8 ms und die LinkError-Verweildauer von 16 ms sind im Modell einzelne Ereignisse. Sie sind keine Wanduhr-Verzögerungen. Das CRC-LFSR der Spezifikation wird nicht ausgeführt. Ein-, Zwei- und Drei-Bit-Kipper gelten als erkannt, weil UCIe §3.7 die Erkennung von bis zu drei Bits garantiert. Die Replay-Zählergrenze steht standardmäßig auf 3. Abschnitt 3.8 verweist für Geschwindigkeiten bis 32 GT/s auf die PCIe-Regeln und wiederholt diese Ganzzahl nicht, daher hält das Modell sie als Parameter.",
)
add("<p>A transaction moves through logical phases. A fault may be armed at one of them:</p>", "<p>Eine Transaktion durchläuft logische Phasen. Ein Fehler kann in einer davon scharf gemacht werden:</p>")
add(
    ".</li>",
    ".</li>",
)
add(
    "Created, then accepted locally on ",
    "Erzeugt, dann lokal angenommen auf ",
)
add("<li>AoU request packed and the request flit transmitted.</li>", "<li>AoU-Anfrage gepackt und der Anfrage-Flit übertragen.</li>")
add("<li>Request received by the remote endpoint.</li>", "<li>Anfrage vom entfernten Endpoint empfangen.</li>")
add(
    "<li>Write data accepted locally, packed, transmitted, and received remotely. Reads skip these phases.</li>",
    "<li>Schreibdaten lokal angenommen, gepackt, übertragen und entfernt empfangen. Lesezugriffe überspringen diese Phasen.</li>",
)
add("<li>Target accepts the operation.</li>", "<li>Das Target nimmt die Operation an.</li>")
add("<li>Target commits. This is the architectural effect.</li>", "<li>Das Target committet. Das ist der architektonische Effekt.</li>")
add("<li>Write response created, packed, transmitted, and received.</li>", "<li>Schreibantwort erzeugt, gepackt, übertragen und empfangen.</li>")
add("<li>Transaction completed at the initiator.</li>", "<li>Transaktion beim Initiator abgeschlossen.</li>")
add(
    "The device model commits a write only after both the request and the write data have been delivered. It does not commit on the address alone. Early write response is off unless a scenario turns it on. Both choices are device-model assumptions. AoU and UCIe do not define the device\u2019s commit point.",
    "Das Gerätemodell committet einen Schreibzugriff erst, nachdem Anfrage und Schreibdaten zugestellt wurden. Es committet nicht auf der Adresse allein. Eine frühe Schreibantwort ist aus, sofern ein Szenario sie nicht einschaltet. Beide Entscheidungen sind Annahmen des Gerätemodells. AoU und UCIe legen den Commit-Punkt des Geräts nicht fest.",
)
add("<h2>Link paths</h2>", "<h2>Linkpfade</h2>")
add(
    "UCIe behavior is taken from Revision 2.0, Version 1.0 (6 August 2024), and, for the second model, from Revision 3.0, Version 1.0 (5 August 2025). A 2.0 rule is used in the 3.0 model only where the 3.0 text still states it. The implemented rules are named in <code>simulation/model/ucie.py</code> and <code>simulation/model/spec30.py</code>.",
    "Das UCIe-Verhalten stammt aus Revision 2.0, Version 1.0 (6. August 2024) und, für das zweite Modell, aus Revision 3.0, Version 1.0 (5. August 2025). Eine 2.0-Regel wird im 3.0-Modell nur verwendet, wo der 3.0-Text sie noch ausspricht. Die implementierten Regeln sind in <code>simulation/model/ucie.py</code> und <code>simulation/model/spec30.py</code> benannt.",
)
add(
    "The paths include CRC failure and replay instead of delivery of an invalid flit, the retry threshold leading to Retrain, LinkReset, and LinkError. LinkError cleanup flushes retry buffers and pipelines. The cited AoU 0.8 §8.3.3 rule returns the activation FSM to DISABLED on FDI link-down. Retrain is not that event, and the model leaves AoU enabled across Retrain. After reset, the link returns to Active, the sequence-number handshake completes, AoU activation completes, and credits are initialized to depth 4 in each direction.",
    "Die Pfade umfassen CRC-Fehler und Replay statt der Zustellung eines ungültigen Flits, die Retry-Schwelle hin zu Retrain, LinkReset und LinkError. Das Aufräumen bei LinkError leert Retry-Puffer und Pipelines. Die zitierte AoU-0.8-Regel §8.3.3 setzt die Aktivierungs-FSM bei FDI-Link-down auf DISABLED. Retrain ist dieses Ereignis nicht, und das Modell lässt AoU über Retrain hinweg aktiviert. Nach dem Reset kehrt der Link nach Active zurück, der Sequenznummern-Handshake schließt ab, die AoU-Aktivierung schließt ab, und Credits werden in jeder Richtung auf Tiefe 4 initialisiert.",
)
add(
    "UCIe 3.0 mechanisms are modeled only to the extent the 3.0 text defines them: sideband performant mode, priority sideband opcode, Fast Throttle, Emergency Shutdown, runtime recalibration, and runtime parity. The priority-packet payload is left to a future ECN, so the model records opcode only and does not invent a transaction encoding. None of those mechanisms, as modeled, supplies a standardized indication of the associated ",
    "UCIe-3.0-Mechanismen sind nur so weit modelliert, wie der 3.0-Text sie festlegt: Sideband-Performant-Modus, Priority-Sideband-Opcode, Fast Throttle, Emergency Shutdown, Runtime-Rekalibrierung und Runtime-Parität. Die Nutzlast des Priority-Pakets bleibt einem künftigen ECN überlassen, daher zeichnet das Modell nur den Opcode auf und erfindet keine Transaktionskodierung. Keiner dieser Mechanismen liefert, wie modelliert, eine standardisierte Anzeige des architektonischen Commit-Zustands der zugehörigen ",
)
add(
    " transaction\u2019s architectural commit state. The classifier refuses a commit bit the specification text used here does not define. Emergency Shutdown is modeled as rapid containment of a severe condition, not as a transaction-completion mechanism.",
    "-Transaktion. Der Klassifikator lehnt ein Commit-Bit ab, das der hier verwendete Spezifikationstext nicht definiert. Emergency Shutdown ist als rasche Eingrenzung eines schweren Zustands modelliert, nicht als Mechanismus zum Abschluss einer Transaktion.",
)
add("<h2>Two records</h2>", "<h2>Zwei Aufzeichnungen</h2>")
add(
    "Ground truth is the number of times the target applied the operation. Initiator state is whether ",
    "Die Grundwahrheit ist die Anzahl, wie oft das Target die Operation angewendet hat. Der Initiatorzustand ist, ob ",
)
add(
    " accepted the transaction and whether an OKAY completion was delivered. The label is computed from the initiator state and from the observation profile. The commit count is consulted only by the later invariant checks.",
    " die Transaktion angenommen hat und ob eine OKAY-Completion zugestellt wurde. Die Kennzeichnung wird aus dem Initiatorzustand und dem Beobachtungsprofil berechnet. Der Commit-Zähler wird nur von den späteren Invariantenprüfungen gelesen.",
)
add(
    "COMPLETED requires OKAY at the initiator. NOT_COMPLETED, under FDI and ",
    "COMPLETED verlangt OKAY beim Initiator. NOT_COMPLETED ist unter FDI und ",
)
add(
    " alone, is a transaction the manager has not handed to the endpoint. After handoff, a missing completion is UNKNOWN. An explicit UNKNOWN response from policy C is UNKNOWN. It is not OKAY, and it is not permission to retry.",
    " allein eine Transaktion, die der Manager nicht an den Endpoint übergeben hat. Nach der Übergabe ist eine fehlende Completion UNKNOWN. Eine ausdrückliche UNKNOWN-Antwort aus Policy C ist UNKNOWN. Sie ist nicht OKAY und keine Erlaubnis zum Retry.",
)
add("<h2>Policies</h2>", "<h2>Policies</h2>")
add(
    "UCIe does not say which ",
    "UCIe sagt nicht, welche ",
)
add(
    " completion an outstanding transaction receives after LinkError. The model does not invent one silent answer. <code>Policy</code> in <code>simulation/model/types.py</code> is the choice.",
    "-Completion eine offene Transaktion nach LinkError erhält. Das Modell erfindet keine stille Antwort. <code>Policy</code> in <code>simulation/model/types.py</code> ist die Entscheidung.",
)
add(
    "Named policies. Definitions follow the repository README.",
    "Benannte Policies. Die Definitionen folgen der README des Repository.",
)
add("<th scope=\"col\">Policy</th>", "<th scope=\"col\">Policy</th>")
add("<th scope=\"col\">Meaning</th>", "<th scope=\"col\">Bedeutung</th>")
add(
    "Discard outstanding AoU transactions. No ",
    "Offene AoU-Transaktionen verwerfen. Keine ",
)
add(" completion.</td>", "-Completion.</td>")
add(
    "<td>If a completion already sat in the initiating AoU, deliver it after cleanup.</td>",
    "<td>Wenn eine Completion bereits im initiierenden AoU lag, nach dem Aufräumen zustellen.</td>",
)
add(
    "<td>Return an explicit UNKNOWN to the software model. Not OKAY, and not permission to retry.</td>",
    "<td>Ein ausdrückliches UNKNOWN an das Softwaremodell zurückgeben. Nicht OKAY und keine Erlaubnis zum Retry.</td>",
)
add(
    "<td>Automatically retry the same logical transaction after the link is up.</td>",
    "<td>Dieselbe logische Transaktion automatisch wiederholen, nachdem der Link oben ist.</td>",
)
add(
    "<td>Do not retry automatically. Same initiator-visible result as A in this model.</td>",
    "<td>Nicht automatisch wiederholen. In diesem Modell dasselbe für den Initiator sichtbare Ergebnis wie A.</td>",
)
add("<h2>What the classifier may read</h2>", "<h2>Was der Klassifikator lesen darf</h2>")
add(
    "Observation profiles. Retry disposition is an assumption, not a standardized profile.",
    "Beobachtungsprofile. Die Retry-Disposition ist eine Annahme, kein standardisiertes Profil.",
)
add("<th scope=\"col\">Profile</th>", "<th scope=\"col\">Profil</th>")
add("<th scope=\"col\">Facts available</th>", "<th scope=\"col\">Verfügbare Fakten</th>")
add("<th scope=\"row\">2 · Software error</th>", "<th scope=\"row\">2 · Softwarefehler</th>")
add("<th scope=\"row\">3 · Management</th>", "<th scope=\"row\">3 · Management</th>")
add("<th scope=\"row\">Retry disposition</th>", "<th scope=\"row\">Retry-Disposition</th>")
add(
    " was accepted, OKAY was or was not received, and a destructive link event occurred. This is the recovery interface the model treats as defined for the protocol layer: <code>pl_state_sts</code>, plus whatever ",
    " angenommen wurde, OKAY empfangen wurde oder nicht, und ein zerstörendes Linkereignis eintrat. Das ist die Recovery-Schnittstelle, die das Modell für die Protokollschicht als festgelegt behandelt: <code>pl_state_sts</code>, plus die ",
)
add(" response was already delivered.</td>", "-Antwort, die bereits zugestellt wurde.</td>")
add(
    "Profile 1, plus link and D2D error registers. Those registers name CRC, Retrain, and LinkError conditions. They do not name an ",
    "Profil 1, plus Link- und D2D-Fehlerregister. Diese Register nennen CRC-, Retrain- und LinkError-Zustände. Sie nennen keine ",
)
add(" transaction.</td>", "-Transaktion.</td>")
add(
    "Profile 2, plus management-transport status, Link Health Monitor, and, on UCIe 3.0, Fast Throttle, Emergency Shutdown, and priority-sideband opcode. None of the structures this model reads carries a standardized ",
    "Profil 2, plus Status des Management-Transports, Link Health Monitor und, auf UCIe 3.0, Fast Throttle, Emergency Shutdown und Priority-Sideband-Opcode. Keine der Strukturen, die dieses Modell liest, trägt eine standardisierte ",
)
add(" architectural-commit indication.</td>", " Anzeige des architektonischen Commits.</td>")
add(
    "ACK/NAK history latched before the retry buffer is flushed. Sections 3.8 and 10.3.3.5 do not require that history to survive, or to be tied to an ",
    "ACK/NAK-Historie, gelatcht bevor der Retry-Puffer geleert wird. Die Abschnitte 3.8 und 10.3.3.5 verlangen nicht, dass diese Historie erhalten bleibt oder an eine ",
)
add(" id. Using it is an assumption.</td>", "-Id gebunden wird. Sie zu verwenden ist eine Annahme.</td>")
add(
    "The classifier is not allowed to let profiles 2 and 3 invent a different FDI/",
    "Der Klassifikator darf den Profilen 2 und 3 nicht erlauben, eine andere FDI/",
)
add(
    " label. A true commit bit would mean the model had invented a field the specification text used here does not define. On the reported equivalence pairs, all three standardized profiles match. That match is a simulation result for those pairs, not a proof that every added UCIe 3.0 register is irrelevant to reliability.",
    "-Kennzeichnung zu erfinden. Ein wahres Commit-Bit hieße, das Modell hätte ein Feld erfunden, das der hier verwendete Spezifikationstext nicht definiert. Auf den berichteten Äquivalenzpaaren stimmen alle drei standardisierten Profile überein. Diese Übereinstimmung ist ein Simulationsergebnis für diese Paare, kein Beweis, dass jedes ergänzte UCIe-3.0-Register für die Zuverlässigkeit belanglos ist.",
)
add("<h2>Fault injection</h2>", "<h2>Fehlerinjektion</h2>")
add(
    "The systematic suite walks named scenarios, including a fault at each relevant phase of the watched transaction. The randomized suite draws 200 injections with seed 1. Faults include bit corruption, repeated corruption of a replay, retry timeout and threshold, forced Retrain, LinkReset, and LinkError, dropped request or response, and the UCIe 3.0-only indications. The position tables in the results use the phase at which the watched transaction\u2019s fault fired, grouped into five buckets from “before target acceptance” through “after response delivery”.",
    "Die systematische Suite durchläuft benannte Szenarien, einschließlich eines Fehlers in jeder relevanten Phase der beobachteten Transaktion. Die Zufallssuite zieht 200 Injektionen mit Seed 1. Fehler umfassen Bitverfälschung, wiederholte Verfälschung eines Replays, Retry-Timeout und -Schwelle, erzwungenes Retrain, LinkReset und LinkError, verworfene Anfrage oder Antwort und die nur in UCIe 3.0 vorhandenen Anzeigen. Die Positionstabellen in den Ergebnissen nutzen die Phase, in der der Fehler der beobachteten Transaktion ausgelöst hat, gruppiert in fünf Eimer von „vor der Target-Annahme“ bis „nach der Antwortzustellung“.",
)
add(
    "AoU packing is one message per flit. Credits are one per message, not per granule. The complete AoU 0.8 text was not available to the model-building workspace, so this is not a full AoU 0.8 conformance implementation. Those packing and credit choices are labeled abstractions in <code>simulation/model/aou.py</code>. The cited §8.3.3 link-down rule is the normative AoU behavior the model uses.",
    "Das AoU-Packing ist eine Nachricht je Flit. Credits sind einer je Nachricht, nicht je Granule. Der vollständige AoU-0.8-Text lag dem Arbeitsbereich zum Modellbau nicht vor, daher ist dies keine vollständige AoU-0.8-Konformitätsimplementierung. Diese Packing- und Credit-Entscheidungen sind in <code>simulation/model/aou.py</code> als Abstraktionen gekennzeichnet. Die zitierte Link-down-Regel §8.3.3 ist das normative AoU-Verhalten, das das Modell verwendet.",
)
add(
    '<p><a href="index.html#method">Back to the summary</a> · <a href="results.html">Measured results</a> · <a href="limitations.html">Limitations</a></p>',
    '<p><a href="index.html#method">Zurück zum Überblick</a> · <a href="results.html">Gemessene Ergebnisse</a> · <a href="limitations.html">Grenzen</a></p>',
)
add(
    "NUVUS · UCIe transaction-fate study. Method statements follow the repository README and the model comments cited above.",
    "NUVUS · UCIe-Studie zum Transaktionsausgang. Die Methodenaussagen folgen der README des Repository und den oben zitierten Modellkommentaren.",
)

# --- results page ---
add(
    "<title>Measured results — UCIe 2.0 vs UCIe 3.0</title>",
    "<title>Gemessene Ergebnisse — UCIe 2.0 gegenüber UCIe 3.0</title>",
)
add(
    'content="Equivalent-workload counts, UNKNOWN rates, ambiguity windows, and observational-equivalence pairs from the UCIe fault-injection study."',
    'content="Zähler der äquivalenten Last, UNKNOWN-Raten, Ambiguitätsfenster und Paare der Beobachtungsäquivalenz aus der UCIe-Fehlerinjektionsstudie."',
)
add('<p class="kicker">Results</p>', '<p class="kicker">Ergebnisse</p>')
add('<h1 class="page-title">Measured results</h1>', '<h1 class="page-title">Gemessene Ergebnisse</h1>')
add(
    "Every count on this page is taken from the generated reports. Reachability is the scientific conclusion. A percentage below is a fault-workload observation, seed 1 — not an estimated field failure probability, and not a general comparison of UCIe 2.0 and UCIe 3.0 reliability.",
    "Jeder Zähler auf dieser Seite stammt aus den erzeugten Berichten. Die Erreichbarkeit ist die wissenschaftliche Schlussfolgerung. Ein Prozentsatz unten ist eine Beobachtung unter der Fehlerlast, Seed 1 — keine geschätzte Feldausfallwahrscheinlichkeit und kein allgemeiner Vergleich der Zuverlässigkeit von UCIe 2.0 und UCIe 3.0.",
)
add("<h2>Fault position, FDI / ", "<h2>Fehlerposition, FDI / ")
add("<h2>Equivalent workload</h2>", "<h2>Äquivalente Last</h2>")
add(
    "One systematic suite and one random suite, seed 1, 200 injections, replayed on both models. Source: comparison_ucie2_vs_ucie3.md.",
    "Eine systematische Suite und eine Zufallssuite, Seed 1, 200 Injektionen, auf beiden Modellen nachgespielt. Quelle, auf Englisch: comparison_ucie2_vs_ucie3.md.",
)
add("<th scope=\"col\">Metric</th>", "<th scope=\"col\">Metrik</th>")
add("<th scope=\"row\">Simulations</th>", "<th scope=\"row\">Simulationen</th>")
add("<th scope=\"row\">Systematic + randomized</th>", "<th scope=\"row\">Systematisch + zufällig</th>")
add("<th scope=\"row\">UNKNOWN transactions</th>", "<th scope=\"row\">UNKNOWN-Transaktionen</th>")
add("<th scope=\"row\">Duplicate commit reachable</th>", "<th scope=\"row\">Doppelter Commit erreichbar</th>")
add("<th scope=\"row\">False-safe-retry reachable</th>", "<th scope=\"row\">Falsch sicheres Retry erreichbar</th>")
add("<th scope=\"row\">Liveness failures</th>", "<th scope=\"row\">Lebendigkeitsfehler</th>")
add("<th scope=\"row\">Recovery traces explored</th>", "<th scope=\"row\">Untersuchte Recovery-Traces</th>")
add("<th scope=\"row\">UNKNOWN before target acceptance</th>", "<th scope=\"row\">UNKNOWN vor der Target-Annahme</th>")
add("<th scope=\"row\">UNKNOWN after target acceptance</th>", "<th scope=\"row\">UNKNOWN nach der Target-Annahme</th>")
add("<th scope=\"row\">UNKNOWN after architectural commit</th>", "<th scope=\"row\">UNKNOWN nach dem architektonischen Commit</th>")
add("<th scope=\"row\">UNKNOWN after response generation</th>", "<th scope=\"row\">UNKNOWN nach der Antwortgenerierung</th>")
add("<th scope=\"row\">UNKNOWN after response delivery</th>", "<th scope=\"row\">UNKNOWN nach der Antwortzustellung</th>")
add(
    "For the specific property studied here — reconstructing the architectural fate of an interrupted non-idempotent transaction from the modeled standardized observations — the tested UCIe 3.0 mechanisms did not eliminate the ambiguity found in the UCIe 2.0 model. Zero scenario names leave the UNKNOWN set, and zero names enter it. The logical-event ambiguity window is not shorter. Equality of these counts, including UNKNOWN 171 and 171, applies only to this workload and this transaction-fate property. It is not a statement that UCIe 3.0 is no better than UCIe 2.0, and it is not a general comparison of reliability.",
    "Für die hier untersuchte Eigenschaft — den architektonischen Ausgang einer unterbrochenen nicht-idempotenten Transaktion aus den modellierten standardisierten Beobachtungen zu rekonstruieren — haben die geprüften UCIe-3.0-Mechanismen die im UCIe-2.0-Modell gefundene Ambiguität nicht beseitigt. Kein Szenarioname verlässt die UNKNOWN-Menge, und keiner tritt ein. Das Ambiguitätsfenster in logischen Ereignissen ist nicht kürzer. Die Gleichheit dieser Zähler, einschließlich UNKNOWN 171 und 171, gilt nur für diese Last und diese Eigenschaft des Transaktionsausgangs. Sie ist keine Aussage, UCIe 3.0 sei nicht besser als UCIe 2.0, und kein allgemeiner Vergleich der Zuverlässigkeit.",
)
add(
    "UCIe 3.0 adds useful link, management, sideband, recalibration, and emergency-handling capabilities. Sideband performant mode removes a 32 UI sideband gap. That gap is not a fate milestone in this model, and the specification does not define it as one. The added mechanisms do not, by themselves, provide architectural commit knowledge for the transaction-fate cases investigated here.",
    "UCIe 3.0 ergänzt nützliche Fähigkeiten für Link, Management, Sideband, Rekalibrierung und Notfallbehandlung. Der Sideband-Performant-Modus beseitigt eine Sideband-Lücke von 32 UI. Diese Lücke ist in diesem Modell kein Meilenstein des Transaktionsausgangs, und die Spezifikation definiert sie nicht als einen. Die ergänzten Mechanismen liefern für die hier untersuchten Fälle des Transaktionsausgangs allein kein Wissen über den architektonischen Commit.",
)
add("<h2>UCIe 2.0 baseline totals</h2>", "<h2>Summen der UCIe-2.0-Baseline</h2>")
add(
    'From <a href="sources/findings.md">findings.md</a>. FDI/',
    'Aus <a href="sources/findings.md">findings.md</a>, auf Englisch. FDI/',
)
add(
    " classification counts cover systematic and randomized transactions. They are not a split of the 289 simulations. The reported liveness and credit invariant totals are simulator outcomes. This study does not currently attribute them to normative UCIe defects. The model includes an injected credit fault. The generated report records 10 credit-invariant violations and does not split them by cause, so they are not investigated further on this page.",
    "-Klassifikationszähler umfassen systematische und zufällige Transaktionen. Sie sind keine Zerlegung der 289 Simulationen. Die berichteten Summen zu Lebendigkeit und Credit-Invariante sind Ergebnisse des Simulators. Die Studie schreibt sie derzeit keinen normativen UCIe-Mängeln zu. Das Modell enthält einen injizierten Credit-Fehler. Der erzeugte Bericht verzeichnet 10 Credit-Invariantenverletzungen und teilt sie nicht nach Ursache auf, daher werden sie auf dieser Seite nicht weiter untersucht.",
)
add("<th scope=\"col\">Counter</th>", "<th scope=\"col\">Zähler</th>")
add("<th scope=\"col\" class=\"num\">Value</th>", "<th scope=\"col\" class=\"num\">Wert</th>")
add("<th scope=\"row\">Systematic</th>", "<th scope=\"row\">Systematisch</th>")
add("<th scope=\"row\">Randomized, seed 1</th>", "<th scope=\"row\">Zufällig, Seed 1</th>")
add("<th scope=\"row\">Fault injections</th>", "<th scope=\"row\">Fehlerinjektionen</th>")
add("<th scope=\"row\">Successful transparent retries</th>", "<th scope=\"row\">Erfolgreiche transparente Retries</th>")
add("<th scope=\"row\">Retrain events</th>", "<th scope=\"row\">Retrain-Ereignisse</th>")
add("<th scope=\"row\">LinkReset events</th>", "<th scope=\"row\">LinkReset-Ereignisse</th>")
add("<th scope=\"row\">LinkError events</th>", "<th scope=\"row\">LinkError-Ereignisse</th>")
add("<th scope=\"row\">Duplicate-commit violations</th>", "<th scope=\"row\">Verletzungen durch doppelten Commit</th>")
add("<th scope=\"row\">False-safe-retry violations</th>", "<th scope=\"row\">Verletzungen durch falsch sicheres Retry</th>")
add("<th scope=\"row\">Deadlocks or liveness failures</th>", "<th scope=\"row\">Deadlocks oder Lebendigkeitsfehler</th>")
add("<th scope=\"row\">Credit invariant violations</th>", "<th scope=\"row\">Credit-Invariantenverletzungen</th>")
add("<th scope=\"row\">Receiver-overflow violations</th>", "<th scope=\"row\">Empfängerüberlauf-Verletzungen</th>")
add(
    "Fault-workload observation, seed 1 — not an estimated field failure probability. Randomized simulation, watched transaction only. <em>n</em> is the number of injected faults in that bucket.",
    "Beobachtung unter der Fehlerlast, Seed 1 — keine geschätzte Feldausfallwahrscheinlichkeit. Zufallssimulation, nur die beobachtete Transaktion. <em>n</em> ist die Zahl der injizierten Fehler in diesem Eimer.",
)
add("<h3>Same runs, if retry disposition is latched</h3>", "<h3>Dieselben Läufe, wenn die Retry-Disposition gelatcht ist</h3>")
add(
    "This table is the report\u2019s second classification. Retaining transport ACK/NAK history can improve classification before target acceptance: UNKNOWN in that bucket falls from 48.8% (21/43) to 11.6% (5/43). It cannot by itself prove the architectural commit state after the request has progressed beyond the transport boundary. The later buckets do not change. This latch is an assumed implementation enhancement, not a standardized AXI transaction-fate mechanism.",
    "Diese Tabelle ist die zweite Klassifikation des Berichts. Das Behalten der Transport-ACK/NAK-Historie kann die Klassifikation vor der Target-Annahme verbessern: UNKNOWN in diesem Eimer fällt von 48,8 % (21/43) auf 11,6 % (5/43). Sie belegt allein nicht den architektonischen Commit, nachdem die Anfrage die Transportgrenze überschritten hat. Die späteren Eimer ändern sich nicht. Dieses Latch ist eine angenommene Implementierungsverbesserung, kein standardisierter AXI-Mechanismus für den Transaktionsausgang.",
)
add(
    "Assumption: ACK/NAK history survives the flush and is visible to the classifier.",
    "Annahme: Die ACK/NAK-Historie übersteht das Leeren und ist für den Klassifikator sichtbar.",
)
add("<h2>Ambiguity window</h2>", "<h2>Ambiguitätsfenster</h2>")
add(
    "Logical protocol events, equivalent workload, watched transaction, counted only after architectural commit. UCIe 3.0 notification lines are not counted, so a longer trace printout is not treated as a longer window. The window is eliminated only when no open window remains.",
    "Logische Protokollereignisse, äquivalente Last, beobachtete Transaktion, gezählt erst nach dem architektonischen Commit. UCIe-3.0-Benachrichtigungszeilen werden nicht gezählt, daher gilt ein längerer Trace-Ausdruck nicht als längeres Fenster. Das Fenster gilt nur als beseitigt, wenn kein offenes Fenster übrig ist.",
)
add("<th scope=\"col\">Window metric</th>", "<th scope=\"col\">Fenstermetrik</th>")
add("<th scope=\"row\">Closed windows (completion arrived)</th>", "<th scope=\"row\">Geschlossene Fenster (Completion eingetroffen)</th>")
add("<th scope=\"row\">Open windows (fate unresolved)</th>", "<th scope=\"row\">Offene Fenster (Ausgang ungeklärt)</th>")
add("<th scope=\"row\">Minimum closed window</th>", "<th scope=\"row\">Kürzestes geschlossenes Fenster</th>")
add("<th scope=\"row\">Maximum closed window</th>", "<th scope=\"row\">Längstes geschlossenes Fenster</th>")
add("<th scope=\"row\">Average closed window</th>", "<th scope=\"row\">Mittleres geschlossenes Fenster</th>")
add("<th scope=\"row\">Minimum open window</th>", "<th scope=\"row\">Kürzestes offenes Fenster</th>")
add("<th scope=\"row\">Maximum open window</th>", "<th scope=\"row\">Längstes offenes Fenster</th>")
add("<th scope=\"row\">Average open window</th>", "<th scope=\"row\">Mittleres offenes Fenster</th>")
add("<th scope=\"row\">Window eliminated</th>", "<th scope=\"row\">Fenster beseitigt</th>")
add(
    "On the printed UCIe 3.0 replay of T17, the open window length is 9 logical events from architectural commit. T18 has no such window: the commit count is 0, so the report marks that window not applicable.",
    "Im gedruckten UCIe-3.0-Replay von T17 ist das offene Fenster 9 logische Ereignisse ab dem architektonischen Commit lang. T18 hat kein solches Fenster: der Commit-Zähler ist 0, daher markiert der Bericht dieses Fenster als nicht anwendbar.",
)
add("<h2>Observational equivalence</h2>", "<h2>Beobachtungsäquivalenz</h2>")
add(
    "Two executions are observationally equivalent for an observer when their architectural ground truth differs, but every fact available to that observer is identical. A pair below meets that test: the commit counts differ, and all three observation profiles still match. The comparison reports four pairs. The modeled observer cannot distinguish the two executions in a pair.",
    "Zwei Ausführungen sind für einen Beobachter beobachtungsäquivalent, wenn ihre architektonische Grundwahrheit verschieden ist, aber jede ihm verfügbare Tatsache übereinstimmt. Ein Paar unten besteht diese Prüfung: die Commit-Zähler unterscheiden sich, und alle drei Beobachtungsprofile stimmen weiterhin überein. Der Vergleich berichtet vier Paare. Der modellierte Beobachter kann die beiden Ausführungen eines Paares nicht unterscheiden.",
)
add(
    "Profile 3 views. axi_commit_bit is false because the specification does not define one. Source: findings_ucie3.md.",
    "Sichten von Profil 3. axi_commit_bit ist false, weil die Spezifikation keines definiert. Quelle, auf Englisch: findings_ucie3.md.",
)
add("<th scope=\"col\">Pair</th>", "<th scope=\"col\">Paar</th>")
add("<th scope=\"col\">Commit counts</th>", "<th scope=\"col\">Commit-Zähler</th>")
add("<th scope=\"col\">Shared profile-3 signals</th>", "<th scope=\"col\">Gemeinsame Profil-3-Signale</th>")
add("<th scope=\"row\">Equivalent LinkError loss</th>", "<th scope=\"row\">Äquivalenter LinkError-Verlust</th>")
add("<th scope=\"row\">Priority sideband plus mainband loss</th>", "<th scope=\"row\">Priority-Sideband plus Mainband-Verlust</th>")
add("<th scope=\"row\">Endpoint reset</th>", "<th scope=\"row\">Endpoint-Reset</th>")
add(">1 and 0<", ">1 und 0<")
add(
    "Each committed and uncommitted view also has <code>axi_accepted=true</code>, <code>completion_received=false</code>, and <code>axi_commit_bit=false</code>. Emergency Shutdown is not another CRC or replay case. It has a different system purpose: rapid containment of a severe condition. The pair shows that the same transaction-fate question can arise when communication is destructively interrupted by endpoint or power recovery. The study asks what software can infer about transactions that were in flight. It does not treat Emergency Shutdown as defective because it does not preserve transaction completion.",
    "Jede committete und nicht committete Sicht hat außerdem <code>axi_accepted=true</code>, <code>completion_received=false</code> und <code>axi_commit_bit=false</code>. Emergency Shutdown ist kein weiterer CRC- oder Replay-Fall. Es hat einen anderen Systemzweck: die rasche Eingrenzung eines schweren Zustands. Das Paar zeigt, dass dieselbe Frage zum Transaktionsausgang entstehen kann, wenn die Kommunikation durch Endpoint- oder Power-Recovery zerstörend unterbrochen wird. Die Studie fragt, was Software über Transaktionen schließen kann, die unterwegs waren. Sie behandelt Emergency Shutdown nicht als mangelhaft, weil es die Transaktions-Completion nicht erhält.",
)
add(
    "<h3>UCIe 3.0-only scenarios whose watched transaction is UNKNOWN</h3>",
    "<h3>Nur-UCIe-3.0-Szenarien, deren beobachtete Transaktion UNKNOWN ist</h3>",
)
add(
    "In <code>ucie3_two_guests_shared_link</code> the profile-3 views are identical. A link-scoped recovery event may affect transaction-fate knowledge for multiple clients sharing that physical transport. This scenario motivates a separate freedom-from-interference study. It does not by itself establish an ISO 26262 freedom-from-interference violation.",
    "In <code>ucie3_two_guests_shared_link</code> sind die Profil-3-Sichten identisch. Ein Recovery-Ereignis mit Link-Geltung kann das Wissen über den Transaktionsausgang für mehrere Clients beeinträchtigen, die denselben physischen Transport teilen. Dieses Szenario motiviert eine eigene Untersuchung der Rückwirkungsfreiheit. Es begründet für sich keine Verletzung der Rückwirkungsfreiheit nach ISO 26262.",
)
add("<h2>Transport acknowledgement and retry</h2>", "<h2>Transportquittung und Retry</h2>")
add(
    "In the modeled architecture, transport-level acknowledgement and target architectural commit are distinct events. On the UCIe 3.0 systematic runs, 9 transactions had a transport ACK with architectural_commit_count 0, and 33 transactions had a commit without an initiator completion. That distinction is a result of this model. It is not a universal claim about every protocol.",
    "In der modellierten Architektur sind die Quittung auf Transportebene und der architektonische Commit am Target verschiedene Ereignisse. In den systematischen UCIe-3.0-Läufen hatten 9 Transaktionen ein Transport-ACK bei architectural_commit_count 0, und 33 Transaktionen einen Commit ohne Completion beim Initiator. Diese Unterscheidung ist ein Ergebnis dieses Modells. Sie ist keine universelle Aussage über jedes Protokoll.",
)
add("<p>Automatic retry of the committed UCIe 3.0 loss, policy D:</p>", "<p>Automatisches Retry des committeten UCIe-3.0-Verlusts, Policy D:</p>")
add("<span>Commit count</span>", "<span>Commit-Zähler</span>")
add("<span>Completion</span>", "<span>Abschluss</span>")
add("<span>Classification</span>", "<span>Klassifikation</span>")
add("<span>Violations</span>", "<span>Verletzungen</span>")
add(
    "The UCIe 2.0 policy comparison reports the same shape on the post-commit loss: initiator COMPLETED, commit 2, completion received, violations 3. On the pre-acceptance loss, policy D reports initiator COMPLETED, commit 1, violations 0. The effect there had not yet happened, so the retry\u2019s single commit matches one execution.",
    "Der Policy-Vergleich von UCIe 2.0 berichtet dieselbe Form beim Verlust nach dem Commit: Initiator COMPLETED, Commit 2, Completion empfangen, Verletzungen 3. Beim Verlust vor der Annahme berichtet Policy D Initiator COMPLETED, Commit 1, Verletzungen 0. Der Effekt war dort noch nicht eingetreten, daher entspricht der eine Commit des Retries einer Ausführung.",
)
add("<h3>Mechanisms that do not drop the mainband transaction</h3>", "<h3>Mechanismen, die die Mainband-Transaktion nicht verwerfen</h3>")
add(
    "These indications are specified not to discard a valid flit by themselves.",
    "Diese Anzeigen sind so spezifiziert, dass sie einen gültigen Flit nicht von sich aus verwerfen.",
)
add("<th scope=\"col\">Run</th>", "<th scope=\"col\">Lauf</th>")
add("<th scope=\"col\">Class</th>", "<th scope=\"col\">Klasse</th>")
add("<th scope=\"col\" class=\"num\">Commit count</th>", "<th scope=\"col\" class=\"num\">Commit-Zähler</th>")
add("<th scope=\"col\">Completion</th>", "<th scope=\"col\">Abschluss</th>")
add("<h2>Policy comparison and integration choices</h2>", "<h2>Policy-Vergleich und Integrationsentscheidungen</h2>")
add(
    " observation, from the UCIe 2.0 report. “Violations 0” means the invariant checker recorded none for that paired run.",
    "-Beobachtung, aus dem UCIe-2.0-Bericht. „Verletzungen 0“ heißt, dass der Invariantenprüfer für diesen gepaarten Lauf keine verzeichnet hat.",
)
add("<th scope=\"col\">Post-commit loss</th>", "<th scope=\"col\">Verlust nach dem Commit</th>")
add("<th scope=\"col\">Pre-acceptance loss</th>", "<th scope=\"col\">Verlust vor der Annahme</th>")
add("<p>Policy C differs from policy A by making UNKNOWN explicit. The fate label is still UNKNOWN. Policy D differs by retrying.</p>", "<p>Policy C unterscheidet sich von Policy A, indem sie UNKNOWN ausdrücklich macht. Die Kennzeichnung des Ausgangs bleibt UNKNOWN. Policy D unterscheidet sich durch das Retry.</p>")
add(
    "Retrain while the response is still unacknowledged: preserve-retry ends as COMPLETED, commit 1, deadlock false. Reinitialize-retry ends as UNKNOWN, commit 1, deadlock true. The model identifies an integration ambiguity whose outcome depends on retry-state handling. UCIe 2.0 §3.8 reinitializes the IDLE-handshake retry variables on Retrain, and §10.3.3.5 states a flush for LinkReset, not for Retrain. The study does not treat that difference as a confirmed normative defect.",
    "Retrain, während die Antwort noch unquittiert ist: Preserve-Retry endet als COMPLETED, Commit 1, Deadlock false. Reinitialize-Retry endet als UNKNOWN, Commit 1, Deadlock true. Das Modell benennt eine Integrationsambiguität, deren Ausgang von der Behandlung des Retry-Zustands abhängt. UCIe 2.0 §3.8 initialisiert die Retry-Variablen des IDLE-Handshakes bei Retrain neu, und §10.3.3.5 nennt ein Leeren für LinkReset, nicht für Retrain. Die Studie behandelt diesen Unterschied nicht als bestätigten normativen Mangel.",
)
add(
    "A write response sitting in the initiating AoU, not yet accepted on ",
    "Eine Schreibantwort, die im initiierenden AoU liegt und auf ",
)
add(
    ", then LinkError: policy A is UNKNOWN with commit 1, and policy B is COMPLETED with commit 1. UCIe does not say whether a response already queued above the adapter survives LinkError cleanup. The report calls that an integration choice, not a detected coding bug.",
    " noch nicht angenommen ist, dann LinkError: Policy A ist UNKNOWN mit Commit 1, und Policy B ist COMPLETED mit Commit 1. UCIe sagt nicht, ob eine bereits oberhalb des Adapters eingereihte Antwort das Aufräumen bei LinkError übersteht. Der Bericht nennt das eine Integrationsentscheidung, keinen festgestellten Codierfehler.",
)
add(
    '<p><a href="sources/findings.md">UCIe 2.0 report</a> · <a href="sources/findings_ucie3.md">UCIe 3.0 report</a> · <a href="sources/comparison_ucie2_vs_ucie3.md">Comparison</a> · <a href="data/findings.json">JSON</a></p>',
    '<p><a href="sources/findings.md">UCIe-2.0-Bericht</a> · <a href="sources/findings_ucie3.md">UCIe-3.0-Bericht</a> · <a href="sources/comparison_ucie2_vs_ucie3.md">Vergleich</a> · <a href="data/findings.json">JSON</a>. Die Berichte bleiben englisch.</p>',
)
add(
    "NUVUS · UCIe transaction-fate study. Counts are copied from the simulator reports.",
    "NUVUS · UCIe-Studie zum Transaktionsausgang. Die Zähler sind aus den Simulatorberichten kopiert.",
)

# --- limitations page ---
add(
    "<title>Limitations — UCIe transaction-fate study</title>",
    "<title>Grenzen — UCIe-Studie zum Transaktionsausgang</title>",
)
add(
    'content="Scope limits of the UCIe 2.0 and UCIe 3.0 fault-injection model. Reachability under the stated assumptions, not a claim about every implementation."',
    'content="Geltungsgrenzen des Fehlerinjektionsmodells für UCIe 2.0 und UCIe 3.0. Erreichbarkeit unter den genannten Annahmen, keine Aussage über jede Implementierung."',
)
add('<p class="kicker">Limitations</p>', '<p class="kicker">Grenzen</p>')
add('<h1 class="page-title">Scope of the result</h1>', '<h1 class="page-title">Geltung des Ergebnisses</h1>')
add(
    "The study shows that transaction-fate ambiguity is reachable under the modeled observation profiles and specification-derived recovery behavior. It does not measure a silicon link. A percentage on the results page is a fault-workload observation, seed 1 — not an estimated field failure probability.",
    "Die Studie zeigt, dass die Ambiguität des Transaktionsausgangs unter den modellierten Beobachtungsprofilen und dem spezifikationsabgeleiteten Recovery-Verhalten erreichbar ist. Sie misst keinen Silizium-Link. Ein Prozentsatz auf der Ergebnisseite ist eine Beobachtung unter der Fehlerlast, Seed 1 — keine geschätzte Feldausfallwahrscheinlichkeit.",
)
add("<h2>Model and specifications</h2>", "<h2>Modell und Spezifikationen</h2>")
add(
    "<li>The simulation is an abstract protocol model, not a silicon implementation. It is not a PHY. It is intentionally event-accurate rather than cycle-accurate, because the primary property is whether two architectural outcomes can produce the same observer-visible state. Cycle-accurate modeling would be needed for worst-case recovery latency, deadline violations, arbitration effects, or a precise ambiguity duration in nanoseconds.</li>",
    "<li>Die Simulation ist ein abstraktes Protokollmodell, keine Siliziumimplementierung. Sie ist keine PHY. Sie ist absichtlich ereignisgenau und nicht zyklusgenau, weil die zentrale Eigenschaft ist, ob zwei architektonische Ausgänge denselben für den Beobachter sichtbaren Zustand erzeugen können. Zyklusgenaue Modellierung wäre nötig für Worst-Case-Recovery-Latenz, Deadline-Verletzungen, Arbitrierungseffekte oder eine Ambiguitätsdauer in Nanosekunden.</li>",
)
add(
    "<li>The complete AoU 0.8 text was not available to the model-building workspace. This is not a full AoU 0.8 conformance implementation. AoU packing and credit behavior are explicit abstractions. The only AoU rule treated as normative is the publicly cited §8.3.3 behavior: FDI link-down returns the activation FSM to DISABLED and drops an in-flight Activate or Deactivate handshake. Retrain does not. If the complete AoU 0.8 specification is later added, this statement should be revised only after the implementation is cross-checked against it.</li>",
    "<li>Der vollständige AoU-0.8-Text lag dem Arbeitsbereich zum Modellbau nicht vor. Dies ist keine vollständige AoU-0.8-Konformitätsimplementierung. AoU-Packing und Credit-Verhalten sind ausdrückliche Abstraktionen. Die einzige als normativ behandelte AoU-Regel ist das öffentlich zitierte Verhalten von §8.3.3: FDI-Link-down setzt die Aktivierungs-FSM auf DISABLED und verwirft einen unterwegs befindlichen Activate- oder Deactivate-Handshake. Retrain tut das nicht. Wird die vollständige AoU-0.8-Spezifikation später ergänzt, sollte diese Aussage erst nach einem Abgleich der Implementierung mit ihr überarbeitet werden.</li>",
)
add(
    " fate after LinkError cleanup is a policy. The threshold algorithm for Retrain is implementation specific. The replay-count integer is a parameter, because §3.8 points at the PCIe recommendation and does not restate the integer.</li>",
    "-Ausgang nach dem Aufräumen bei LinkError ist eine Policy. Der Schwellenalgorithmus für Retrain ist implementierungsspezifisch. Die Replay-Zähler-Ganzzahl ist ein Parameter, weil §3.8 auf die PCIe-Empfehlung verweist und die Ganzzahl nicht wiederholt.</li>",
)
add(
    "<li>Some behavior is implementation-defined and modeled as explicit policy. Outstanding ",
    "<li>Manches Verhalten ist implementierungsdefiniert und als ausdrückliche Policy modelliert. Der ",
)
add(
    "<li>Packing is one message per flit. Credits are one per message, not per granule. Initial credit depth in the printed traces is 4. The on-wire credit encoding is not modeled.</li>",
    "<li>Das Packing ist eine Nachricht je Flit. Credits sind einer je Nachricht, nicht je Granule. Die anfängliche Credit-Tiefe in den gedruckten Traces ist 4. Die Credit-Kodierung auf der Leitung ist nicht modelliert.</li>",
)
add(
    "<li>The CRC LFSR is not executed. Detection of one-, two-, and three-bit corruption follows the §3.7 guarantee. The millisecond and flit-count thresholds are single events, not wall-clock delays.</li>",
    "<li>Das CRC-LFSR wird nicht ausgeführt. Die Erkennung von Ein-, Zwei- und Drei-Bit-Verfälschung folgt der Garantie von §3.7. Die Schwellen in Millisekunden und Flit-Zahl sind einzelne Ereignisse, keine Wanduhr-Verzögerungen.</li>",
)
add(
    " architectural commit semantics are modeled at different abstraction levels. The device commits a write only after the request and the write data have been delivered. That is a device-model assumption. Early write response is off unless a scenario turns it on.</li>",
    "-Semantik des architektonischen Commits sind auf verschiedenen Abstraktionsebenen modelliert. Das Gerät committet einen Schreibzugriff erst, nachdem Anfrage und Schreibdaten zugestellt wurden. Das ist eine Annahme des Gerätemodells. Eine frühe Schreibantwort ist aus, sofern ein Szenario sie nicht einschaltet.</li>",
)
add(
    "<li>UCIe link behavior and ",
    "<li>UCIe-Linkverhalten und ",
)
add("<h2>What the numbers mean</h2>", "<h2>Was die Zahlen bedeuten</h2>")
add(
    "<li>Randomized percentages depend on the fault workload and seed. The published random suite uses seed 1 and 200 injections. A different seed or a different mix of fault positions would move the rates.</li>",
    "<li>Zufällige Prozentsätze hängen von der Fehlerlast und vom Seed ab. Die veröffentlichte Zufallssuite nutzt Seed 1 und 200 Injektionen. Ein anderer Seed oder eine andere Mischung der Fehlerpositionen würde die Raten verschieben.</li>",
)
add(
    "<li>Reachability is the scientific conclusion. UNKNOWN is reachable in both revisions on the equivalent workload. That claim does not depend on treating 48.8% or 69.4% as a property of UCIe, or as a field failure probability.</li>",
    "<li>Die Erreichbarkeit ist die wissenschaftliche Schlussfolgerung. UNKNOWN ist in beiden Revisionen auf der äquivalenten Last erreichbar. Diese Aussage hängt nicht davon ab, 48,8 % oder 69,4 % als Eigenschaft von UCIe oder als Feldausfallwahrscheinlichkeit zu behandeln.</li>",
)
add(
    "<li>The reported liveness and credit invariant totals are simulator outcomes. This study does not currently attribute them to normative UCIe defects. The model includes an injected credit fault. The generated report records 10 credit-invariant violations and does not split them by cause.</li>",
    "<li>Die berichteten Summen zu Lebendigkeit und Credit-Invariante sind Ergebnisse des Simulators. Die Studie schreibt sie derzeit keinen normativen UCIe-Mängeln zu. Das Modell enthält einen injizierten Credit-Fehler. Der erzeugte Bericht verzeichnet 10 Credit-Invariantenverletzungen und teilt sie nicht nach Ursache auf.</li>",
)
add(
    "<li>The study does not prove that every implementation will exhibit the same outcome. An implementation can latch more local history than UCIe requires, preserve a response that policy A discards, or add an end-to-end commit log. Those are outside the standardized facts this classifier is allowed to read. Hiding UNKNOWN with a reset or a containment rule does not, by itself, reconstruct whether the original operation committed.</li>",
    "<li>Die Studie beweist nicht, dass jede Implementierung denselben Ausgang zeigt. Eine Implementierung kann mehr lokale Historie latchen, als UCIe verlangt, eine Antwort erhalten, die Policy A verwirft, oder ein Ende-zu-Ende-Commit-Log ergänzen. Das liegt außerhalb der standardisierten Fakten, die dieser Klassifikator lesen darf. UNKNOWN mit einem Reset oder einer Containment-Regel zu verbergen rekonstruiert für sich nicht, ob die ursprüngliche Operation committet wurde.</li>",
)
add(
    "<li>The study shows that the ambiguity is reachable under the modeled, specification-consistent assumptions. Where the text is silent, the model uses a named policy rather than an invented completion.</li>",
    "<li>Die Studie zeigt, dass die Ambiguität unter den modellierten, spezifikationsverträglichen Annahmen erreichbar ist. Wo der Text schweigt, verwendet das Modell eine benannte Policy statt einer erfundenen Completion.</li>",
)
add("<h2>UCIe 3.0 mechanisms</h2>", "<h2>UCIe-3.0-Mechanismen</h2>")
add(
    "<li>Priority sideband payload details are left to a future ECN. The model records opcode only. It does not invent a transaction encoding inside a priority packet.</li>",
    "<li>Einzelheiten der Priority-Sideband-Nutzlast bleiben einem künftigen ECN überlassen. Das Modell zeichnet nur den Opcode auf. Es erfindet keine Transaktionskodierung in einem Priority-Paket.</li>",
)
add(
    "<li>Fast Throttle and Emergency Shutdown are optional. Emergency Shutdown is fatal and can be left only by reset. It has a different system purpose: rapid containment of a severe condition. The study asks what software can infer about transactions that were in flight. It does not treat Emergency Shutdown as defective because it does not preserve transaction completion. The specification, as used here, does not define a log of outstanding mainband transactions, so the model does not add one.</li>",
    "<li>Fast Throttle und Emergency Shutdown sind optional. Emergency Shutdown ist fatal und kann nur durch Reset verlassen werden. Es hat einen anderen Systemzweck: die rasche Eingrenzung eines schweren Zustands. Die Studie fragt, was Software über Transaktionen schließen kann, die unterwegs waren. Sie behandelt Emergency Shutdown nicht als mangelhaft, weil es die Transaktions-Completion nicht erhält. Die Spezifikation, wie hier verwendet, definiert kein Log offener Mainband-Transaktionen, daher fügt das Modell keines hinzu.</li>",
)
add(
    "<li>Sideband performant mode removes a 32 UI gap. This model counts logical protocol events, not UI. The shorter gap is not turned into a fate event.</li>",
    "<li>Der Sideband-Performant-Modus beseitigt eine Lücke von 32 UI. Dieses Modell zählt logische Protokollereignisse, nicht UI. Die kürzere Lücke wird nicht zu einem Ereignis des Transaktionsausgangs gemacht.</li>",
)
add(
    "<li>Higher line rates, 48 GT/s and 64 GT/s, do not add an end-to-end transaction acknowledgement in the text the model uses. The model does not treat a higher rate as a change to transaction fate.</li>",
    "<li>Höhere Leitungsraten, 48 GT/s und 64 GT/s, ergänzen im vom Modell verwendeten Text keine Ende-zu-Ende-Transaktionsquittung. Das Modell behandelt eine höhere Rate nicht als Änderung des Transaktionsausgangs.</li>",
)
add(
    '<p><a href="index.html#limitations">Back to the summary</a> · <a href="methodology.html">Method</a> · <a href="review_notes.md">Review notes</a></p>',
    '<p><a href="index.html#limitations">Zurück zum Überblick</a> · <a href="methodology.html">Methode</a> · <a href="review_notes.md">Prüfnotizen</a></p>',
)
add(
    "NUVUS · UCIe transaction-fate study.",
    "NUVUS · UCIe-Studie zum Transaktionsausgang.",
)


def localize(text, filename):
    text = text.replace('lang="en" data-page=', 'lang="de" data-page=', 1)
    text = text.replace('href="assets/', 'href="../assets/')
    text = text.replace('src="assets/', 'src="../assets/')
    text = text.replace('href="sources/', 'href="../sources/')
    text = text.replace('href="data/', 'href="../data/')
    text = text.replace('href="de/', 'href="')
    text = text.replace(
        f'<link rel="alternate" hreflang="en" href="{filename}">',
        f'<link rel="alternate" hreflang="en" href="../{filename}">',
    )
    text = text.replace(
        f'<link rel="alternate" hreflang="x-default" href="{filename}">',
        f'<link rel="alternate" hreflang="x-default" href="../{filename}">',
    )
    text = text.replace(
        f'<a href="{filename}?lang=en" hreflang="en" lang="en" aria-current="page">EN</a>',
        f'<a href="../{filename}?lang=en" hreflang="en" lang="en">EN</a>',
    )
    text = text.replace(
        f'<a href="{filename}?lang=de" hreflang="de" lang="de">DE</a>',
        f'<a href="{filename}?lang=de" hreflang="de" lang="de" aria-current="page">DE</a>',
    )
    return text


def main():
    blob = ""
    pages = ["index.html", "methodology.html", "results.html", "limitations.html"]
    sources = {}
    for name in pages:
        sources[name] = (ROOT / name).read_text()
        blob += sources[name]
    missing = [en for en, _de in PAIRS if en not in blob]
    if missing:
        print(f"MISSING {len(missing)}")
        for en in missing:
            print("---", en[:180].replace("\n", " "))
        raise SystemExit(1)
    ordered = sorted(PAIRS, key=lambda pair: len(pair[0]), reverse=True)
    OUT.mkdir(exist_ok=True)
    for name, src in sources.items():
        text = src
        for en, de in ordered:
            text = text.replace(en, de)
        text = localize(text, name)
        text = text.replace(
            "deren Ausführungszustand <span class=\"term\" title=\"" + INDET + "\">unbestimmt</span>.",
            "deren Ausführungszustand <span class=\"term\" title=\"" + INDET + "\">unbestimmt</span> bleibt.",
        )
        (OUT / name).write_text(text)
        print("wrote", name, "chars", len(text))


if __name__ == "__main__":
    main()
