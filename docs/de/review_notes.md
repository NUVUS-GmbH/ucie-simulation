# Prüfnotizen

Sprache: Deutsch · [English](../review_notes.md)

Notizen für eine technische Leserin oder einen technischen Leser der GitHub-Pages-Seite in `docs/`. Die Seite stellt die Ausgabe des Simulators dar. Sie ist kein neues Experiment. Die englische Fassung dieser Notizen bleibt die Prüfvorlage.

Kanonische Berichte, auf Englisch:

- `simulation/results/findings.md`
- `simulation/results/findings_ucie3.md`
- `simulation/results/comparison_ucie2_vs_ucie3.md`

`docs/sources/` enthält Kopien dieser drei Dateien, damit die veröffentlichte Seite darauf verlinken kann. `docs/data/findings.json` wiederholt die Zahlen in strukturierter Form. Wird der Simulator erneut ausgeführt, müssen diese Kopien aktualisiert werden. Die Seiten rechnen die Raten nicht neu.

## Wie eine Behauptung eingeordnet wird

Die öffentlichen Seiten drucken diese Kennzeichnungen nicht an jeden Satz. Diese Liste ist die Prüfung, die bei der Überarbeitung verwendet wurde.

- SPECIFICATION FACT: eine Regel, die die Berichte aus UCIe 2.0 oder 3.0 zitieren, oder das öffentlich zitierte AoU-Verhalten §8.3.3 bei Link-down. Der vollständige AoU-0.8-Text lag nicht im Arbeitsbereich.
- SIMULATION RESULT: ein Zähler, ein Trace oder ein Erreichbarkeitsergebnis, kopiert aus den erzeugten Berichten.
- MODEL ASSUMPTION: Commit-Punkt des Geräts, Packing, Credits, Replay-Zähler-Parameter, gelatchte Retry-Disposition, benannte Policies A–E.
- INTERPRETATION: was diese Ergebnisse für die Beobachtbarkeit des Transaktionsausgangs bedeuten. Keine Behauptung, UCIe sei fehlerhaft.
- AUTOMOTIVE IMPLICATION: eine Entscheidung auf Systemebene, die das Ergebnis ausdrücklich macht. Keine ISO-26262-Metrik und kein Urteil zur Rückwirkungsfreiheit.
- FUTURE RESEARCH QUESTION: einschließlich jedes Punkts zu UCIe 3D. In den Zählern von 2.0/3.0 steckt kein 3D-Ergebnis.

## Gemachte Behauptungen

- SIMULATION RESULT. Unter den modellierten Beobachtungsprofilen gibt es Ausführungen, in denen eine committete und eine nicht committete nicht-idempotente Transaktion für den Initiator nach zerstörender Link-Recovery beobachtungsäquivalent sind.
- INTERPRETATION. Die UCIe-Link-Recovery allein bestimmt nicht den architektonischen Ausgang jeder offenen Transaktion der höheren Schicht. Die Studie sagt nicht, UCIe habe AXI-Transaktions-Recovery versprochen.
- SIMULATION RESULT. `UNKNOWN` ist für UCIe 2.0 und für UCIe 3.0 auf der äquivalenten Last erreichbar. UNKNOWN 171 und 171. Die Gleichheit dieser Zähler gilt nur für diese Last und diese Eigenschaft.
- INTERPRETATION. UNKNOWN ist die tragfähige Kennzeichnung für den modellierten Beobachter, wenn die Fakten weder den Abschluss noch die Nichtausführung belegen. Ein System kann diese Kennzeichnung mit Eingrenzung, Reset oder einem zusätzlichen Vertrag verbergen. Das Verbergen rekonstruiert den Effekt nicht.
- SIMULATION RESULT. T17 (Commit-Zähler 1, Antwort verloren) und T18 (Commit-Zähler 0, Anfrage nie angenommen) stimmen unter FDI/AXI überein. Im UCIe-3.0-Replay stimmen auch die Profile 1, 2 und 3 überein. Die gemeinsame Profil-3-Signalkette ist aus `findings_ucie3.md` zitiert.
- SIMULATION RESULT. Dieselbe Äquivalenz wird für Emergency Shutdown, Priority-Sideband plus Mainband-Verlust und Endpoint-Reset berichtet.
- INTERPRETATION. Emergency Shutdown ist die Eingrenzung eines schweren Zustands. Die Frage nach dem Ereignis ist, was Software schließen kann. Der Mechanismus wird nicht als mangelhaft bezeichnet, weil er die Completion nicht erhält.
- SIMULATION RESULT. Für diese Eigenschaft des Transaktionsausgangs haben die geprüften UCIe-3.0-Mechanismen die Ambiguität nicht beseitigt, den UNKNOWN-Zähler nicht gesenkt und das Ambiguitätsfenster in logischen Ereignissen nicht verkürzt.
- INTERPRETATION. Diese Gleichheit ist nicht „UCIe 3.0 ist nicht besser als UCIe 2.0“. Revision 3.0 ergänzt weiterhin Fähigkeiten für Link, Management, Sideband, Rekalibrierung und Notfallbehandlung.
- SIMULATION RESULT. Policy D auf dem Verlust nach dem Commit erreicht `architectural_commit_count = 2`.
- SIMULATION RESULT. In diesem Modell sind Transportquittung und architektonischer Commit verschieden: 9 systematische UCIe-3.0-Transaktionen hatten ein Transport-ACK bei Commit-Zähler 0, und 33 einen Commit ohne Completion beim Initiator.
- SIMULATION RESULT und MODEL ASSUMPTION. Gelatchte Retry-Disposition ändert UNKNOWN vor der Target-Annahme von 48,8 % (21/43) auf 11,6 % (5/43). Das Latch ist eine Annahme, kein standardisierter AXI-Mechanismus für den Transaktionsausgang. Spätere Eimer bleiben unverändert.
- SIMULATION RESULT und AUTOMOTIVE IMPLICATION. `ucie3_two_guests_shared_link` lässt beide beobachteten Transaktionen UNKNOWN mit Commit-Zähler 1 und übereinstimmenden linkbezogenen Signalen. Die Seite sagt, das motiviere eine Untersuchung der Rückwirkungsfreiheit. Sie stellt keine ISO-26262-Verletzung fest.
- SIMULATION RESULT. Fast Throttle, Runtime-Rekalibrierung und Runtime-Parität enden in den Läufen, die den Flit behalten, als COMPLETED mit Commit-Zähler 1.
- INTERPRETATION. Die Behandlung des Retry-Zustands bei Retrain ist eine Integrationsambiguität. Die Seite nennt das keinen normativen Mangel. Der Fall der eingereihten Antwort unter Policy A/B bleibt eine Integrationsentscheidung, wie der Bericht sagt.
- Der schichtübergreifende Satz bleibt: Die Studie behauptet nicht, UCIe sei fehlerhaft. Sie benennt eine schichtübergreifende Integrationsverantwortung.

## Annahmen, die auf der Seite stehen

- Policy A ist die Standardlesart des gedruckten Gegenbeispiels. Die Policies B, C, D und E werden als benannte Alternativen gezeigt.
- FDI/AXI ist die standardisierte Sicht des Initiators. Gelatchte Retry-Disposition ist überall als Annahme gekennzeichnet, wo ihre Rate von 11,6 % vor der Annahme erscheint.
- Das Gerät committet einen Schreibzugriff erst, nachdem Anfrage und Schreibdaten eingetroffen sind. Eine frühe Schreibantwort bleibt aus, sofern ein Szenario sie nicht einschaltet.
- AoU-Verhalten jenseits der zitierten Link-down-Regel §8.3.3 ist eine Abstraktion: eine Nachricht je Flit, ein Credit je Nachricht, Credit-Tiefe 4 in den gedruckten Traces.
- Die Replay-Zählergrenze von 3 ist ein Parameter. Das CRC-LFSR wird nicht ausgeführt. Schwellen in Millisekunden und Flits sind einzelne Ereignisse.
- Das UCIe-3.0-Priority-Sideband ist nur der Opcode. Es wurde keine Transaktionskodierung ergänzt.
- Die Seiten berichten kein ASIL, keine ISO-26262-Metrik und keinen Test auf Fahrzeugebene. „Level-4“ wurde aus dem Überblick entfernt, damit es nicht als eine solche Metrik gelesen wird. Das Zwei-Gast-Szenario wird nicht als Verletzung der Rückwirkungsfreiheit dargestellt.
- UCIe 3D erscheint nur als offene Frage. Es ist nicht im ausgeführten Modell.

## Verwendete Metriken

UCIe-2.0-Baseline, alles aus `findings.md`:

- 289 Simulationen, 89 systematisch, 200 zufällig
- 288 Fehlerinjektionen, 33 transparente Retries, 76 Retrain, 11 LinkReset, 158 LinkError
- FDI/AXI-Kennzeichnungen: 158 COMPLETED, 10 NOT_COMPLETED, 171 UNKNOWN
- 1 Verletzung durch doppelten Commit, 1 Verletzung durch falsch sicheres Retry, 17 Lebendigkeitsfehler, 10 Credit-Invariantenverletzungen, 0 Empfängerüberläufe

Positionstabelle, zufällige beobachtete Transaktion, FDI/AXI: 48,8 % (21/43), 52,1 % (25/48), 69,4 % (25/36), 54,5 % (18/33), 0,0 % (0/40). Die Tabelle mit gelatchter Disposition steht nur auf der Ergebnisseite, einschließlich 11,6 % (5/43) vor der Annahme.

Vergleich der äquivalenten Last: dieselben Positionsraten, 171 und 171 UNKNOWN, 17 und 17 Lebendigkeitsfehler, 204 und 204 Recovery-Traces und die Zeilen des Ambiguitätsfensters (137 / 78 geschlossen und offen, Mittelwerte 3,8 und 5,4, Fenster beseitigt: Nein).

Verlust nach dem Commit unter Policy D: Commit-Zähler 2, Completion empfangen, Klassifikation COMPLETED, Verletzungen 3. Die Seite addiert diese 3 Verletzungen nicht zur Baseline-Summe von 1 Verletzung durch doppelten Commit. Sie werden als getrennte Zeilen berichtet, passend zu den Berichten.

Verwendete Zählerwerte: T17 ändert den Zähler von 7 auf 8. Die Darstellung des doppelten Effekts sagt „7 → 8, danach ein zweiter Commit“. Sie druckt keinen gemessenen Zählerwert von 9. Die Berichte drucken diesen dritten Wert nicht.

## Zurückhaltend gehaltene Formulierung

- Die Seite sagt, UCIe 3.0 ergänze nützliche Fähigkeiten für Link, Management, Sideband, Rekalibrierung und Notfallbehandlung. Sie sagt nicht, diese Ergänzungen schlössen den Transaktionsausgang, und sie sagt nicht, Revision 3.0 sei nicht besser als Revision 2.0.
- Überschrift des Überblicks: „Link-Recovery allein bestimmt den AXI-Transaktionsausgang nicht.“ Die ältere Formulierung „garantiert keine AXI-Transaktions-Recovery“ wurde zurückgezogen, damit die Seite nicht nahelegt, UCIe verspreche diese Recovery.
- „Zusätzlicher Status“ ist eingeschränkt: die Felder stimmen auf beiden Seiten jedes Äquivalenzpaares weiterhin überein. Die öffentlichen Seiten verwenden die interne Kennzeichnung „class F“ nicht mehr. Sie sagen, der modellierte Status benenne den architektonischen Commit-Zustand der AXI-Transaktion nicht.
- Emergency Shutdown und Fast Throttle werden als optional beschrieben, Emergency Shutdown als fatal, gemäß `findings_ucie3.md`. Die Spalte UCIe 2.0 sagt, diese Mechanismen seien nicht die im 3.0-Text ergänzten. Sie behauptet nicht, Revision 2.0 habe keinerlei Power-Management.
- Raten sind als „unter der gewählten gesäten Fehlerlast beobachtet“ und „keine dem Protokoll innewohnenden Wahrscheinlichkeiten“ gekennzeichnet.
- Lebendigkeitsfehler (17) und Credit-Invariantenverletzungen (10) bleiben in den Tabellen. Sie stehen nicht in den Leitkarten des Überblicks. Die Seite nennt sie Ergebnisse des Simulators und schreibt sie keinem normativen UCIe-Mangel zu. Das Modell injiziert einen Credit-Fehler. Der Bericht teilt die 10 Treffer nicht nach Ursache auf, daher tut die Seite das ebenfalls nicht.
- Vermieden: „critical flaw“, „broken specification“, „catastrophic“ und „UCIe bug“.
- Der geforderte Satz steht im Überblick und auf der Seite Grenzen: Die Studie behauptet nicht, UCIe sei fehlerhaft; sie benennt eine schichtübergreifende Recovery-Lücke.

## Darstellungsentscheidungen

- Die Schritte des Gegenbeispiels auf der Überblicksseite sind eine Verdichtung von T17 und T18 in der Berichtsreihenfolge. Der zeilengetreue Trace bleibt in den Markdown-Berichten.
- Die Balkenbreiten nutzen die veröffentlichten Prozentsätze auf einer Skala von 0–100 %. Sie sind nicht auf den größten Balken neu normiert.
- Diagramme sind eingebettetes SVG, damit sie den hellen und dunklen NUVUS-Tokens folgen. Die Dateien in `docs/assets/` sind die editierbaren Kopien. Ein eigenständiges SVG lädt `tokens.css` aus demselben Verzeichnis und öffnet daher im hellen Thema.
- Farbe, Schrift, Abstand, Radius und das Wortzeichen stammen von der Marke NUVUS. Die Schriften sind die mit dieser Marke gelieferten Dateien unter der SIL Open Font License.
