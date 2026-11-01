#!/usr/bin/env python3
"""Build the public GitHub Pages study from stored reference-model evidence.

The narrative is self-contained: problem → evidence → architecture → robustness →
scope. Chronology of earlier model iterations is not the organizing frame.
"""
from pathlib import Path
import html
import json
import re
import runpy

root = Path(__file__).resolve().parents[1]
docs = root / 'docs'
data = json.loads((root / 'simulation/results/reference_architecture_validation.json').read_text())
esc = html.escape


def sanitize(s: str) -> str:
    return (
        s.replace('torsten_reference', 'reference')
        .replace('Torsten / Swissbit-style', 'Provided')
        .replace('Swissbit', 'vendor')
        .replace('Torsten', 'provided')
    )


for folder in ['sources', 'data', 'configs']:
    (docs / folder).mkdir(exist_ok=True)

for name in [
    'reference_architecture_validation.md',
    'reference_architecture_validation.json',
    'reference_architecture_validation.csv',
    'cross_guest_fault_interference.md',
    'cross_guest_fault_interference.json',
    'cross_guest_fault_interference.csv',
    'findings.md',
    'comparison_ucie2_vs_ucie3.md',
]:
    dest = 'sources' if name.endswith('.md') else 'data'
    (docs / dest / name.replace('cross_guest', 'cross_client')).write_text(
        sanitize((root / 'simulation/results' / name).read_text()).rstrip() + '\n'
    )

for src in (root / 'configs').glob('torsten_reference*.yaml'):
    (docs / 'configs' / src.name.replace('torsten_', '')).write_text(sanitize(src.read_text()))

for source in [
    'src/ucie/ucie_adapter.cpp',
    'simulation/experiments/reference_architecture_validation.py',
]:
    (docs / 'sources' / Path(source).name).write_text(sanitize((root / source).read_text()))

for name in ['ref2x_cross_client', 'ref2x_shared_quiet']:
    for ext in ['trace.csv', 'result.json']:
        (docs / 'data' / f'{name}_{ext}').write_text(
            sanitize((root / 'simulation/results/reference_architecture_runs' / name / ext).read_text())
        )


def badge(s: str) -> str:
    return f'<span class="tag">{s}</span>'


def table(headers, rows) -> str:
    head = ''.join(f'<th scope="col">{h}</th>' for h in headers)
    body = ''.join('<tr>' + ''.join(f'<td>{v}</td>' for v in r) + '</tr>' for r in rows)
    return f'<div class="table-wrap"><table><thead><tr>{head}</tr></thead><tbody>{body}</tbody></table></div>'


def section(sid: str, title: str, body: str, kind: str = 'RESULT') -> str:
    return f'<section id="{sid}"><div class="section-label">{badge(kind)}</div><h2>{title}</h2>{body}</section>'


def p(s: str) -> str:
    return f'<p>{s}</p>'


def ul(xs) -> str:
    return '<ul>' + ''.join(f'<li>{s}</li>' for s in xs) + '</ul>'


def pre(text: str, cls: str = '') -> str:
    klass = f' class="{cls}"' if cls else ''
    return f'<pre{klass}><code>{esc(text)}</code></pre>'


def flow(steps, highlight=None) -> str:
    items = []
    for step in steps:
        klass = ''
        if highlight and step == highlight:
            klass = ' class="danger"'
        items.append(f'<li{klass}>{step}</li>')
    return '<ol class="flow">' + ''.join(items) + '</ol>'


sections = []

# --- 2. The Problem ---
problem_body = (
    p('UCIe can detect and recover from communication failures. However, a higher-layer transaction may already have changed architectural state before its completion response becomes visible to the initiator.')
    + '<div class="pair">'
    + '<article class="panel"><h3>Nominal path</h3>'
    + flow(['AXI request', 'transport', 'target accepts', 'architectural commit', 'response'])
    + '</article>'
    + '<article class="panel danger-panel"><h3>Ambiguity window</h3>'
    + flow(['architectural commit', 'FAULT', 'response not observed'], highlight='FAULT')
    + '<p class="question">Did the operation execute?</p></article>'
    + '</div>'
    + p('The study classifies interrupted transactions as <code>COMPLETED</code>, <code>NOT_COMPLETED</code>, or <code class="unknown">UNKNOWN</code>. These are observer-dependent study labels, not UCIe-defined protocol states.')
    + '<div class="formula"><span>different architectural outcome</span><b>+</b><span>same implemented fate classification</span><b>=</b><strong>transaction-fate ambiguity</strong></div>'
)
sections.append(section('problem', 'The problem', problem_body, 'PROBLEM'))

# --- 3. Different ground truth, same classification ---
trace_a = [
    'AXI accepted',
    'request transported',
    'target accepted',
    '<strong class="commit">ARCHITECTURAL COMMIT</strong>',
    'response generated',
    '<span class="fault">fault / destructive recovery</span>',
    'no completion observed',
]
trace_b = [
    'AXI accepted',
    'request does not reach target',
    'no architectural commit',
    '<span class="fault">fault / recovery</span>',
    'no completion observed',
]
traces = (
    '<div class="pair">'
    + ''.join(
        f'<article class="trace"><h3>{title}</h3><ol>'
        + ''.join(f'<li>{step}</li>' for step in steps)
        + f'</ol><div class="verdict">Ground truth <code>commit_count = {count}</code></div></article>'
        for title, steps, count in [
            ('Trace A — committed, response lost', trace_a, 1),
            ('Trace B — never committed', trace_b, 0),
        ]
    )
    + '</div>'
    + '<div class="observer-box"><h3>Observer A / B / C</h3>'
    + pre('request accepted\ncompletion absent\nclassification: UNKNOWN', 'compact')
    + '</div>'
    + '<div class="equivalence"><strong>Different ground truth. Same fate classification.</strong>'
    + '<p>The two traces collapse to the same transaction-fate classification for the implemented observer profile.</p></div>'
)
sections.append(
    section(
        'traces',
        'Two traces with the same fate classification',
        p('After destructive UCIe recovery, the initiator may be unable to determine whether an outstanding non-idempotent AXI/MMIO operation executed.')
        + traces
        + p('The timed check compares fate labels and completion status. It does not compare complete observer-visible event histories, timing, status bits, retry counters, or error logs. Full observational equivalence has not been established by this check.')
        + p('These are schematic summaries of the reference-architecture runs. Blind software retry of a committed, indeterminate operation can produce a duplicate architectural effect; that is distinct from transport duplicate suppression.')
        + '<a href="data/reference_architecture_validation.json">Inspect the paired runs and blind-retry evidence ↗</a>',
    )
)

# --- 4. Why This Matters ---
why_body = (
    p('Consider a non-idempotent write that increments a shared counter:')
    + pre('counter = 7\n\nwrite increments counter')
    + '<div class="pair">'
    + '<article class="panel"><h3>If the operation committed</h3>'
    + pre('7 → 8')
    + p('but the response was lost, blind retry can produce:')
    + pre('8 → 9')
    + '</article>'
    + '<article class="panel danger-panel"><h3>Outcome</h3>'
    + pre('expected commits = 1\nactual commits = 2')
    + '</article>'
    + '</div>'
    + '<div class="callout">Blind retry can convert uncertainty into duplicate execution.</div>'
    + p('This applies to non-idempotent operations, including doorbells, DMA start, accelerator start, FIFO push, interrupt acknowledge, and actuator-related commands.')
)
sections.append(section('why', 'Why this matters', why_body, 'INTERPRETATION'))

# --- 5. Reference Architecture ---
svg = [
    '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 880 400" role="img" aria-labelledby="title desc">',
    '<title id="title">Provided UCIe D2D reference architecture</title>',
    '<desc id="desc">Two chiplets connect AXI Streaming through FDI, D2D controllers, RDI and PHY over mainband and sideband.</desc>',
    '<style>text{font:16px var(--font-sans);fill:var(--foreground)}rect{fill:var(--muted);stroke:var(--border)}path{stroke:var(--primary);stroke-width:2;fill:none}</style>',
]
for x, label in [(25, 'Chiplet A'), (505, 'Chiplet B')]:
    svg.append(f'<text x="{x + 175}" y="30" text-anchor="middle" font-weight="500">{label}</text>')
    for i, t in enumerate(
        ['AXI Streaming', 'Streaming / FDI', 'UCIe D2D Controller', 'RDI', 'UCIe PHY']
    ):
        y = 50 + i * 60
        svg.append(
            f'<rect x="{x}" y="{y}" width="350" height="42" rx="8"/><text x="{x + 175}" y="{y + 26}" text-anchor="middle">{t}</text>'
        )
        if i < 4:
            svg.append(f'<path d="M{x + 175} {y + 42}v18"/>')
svg.append(
    '<path d="M200 332v38h480v-38"/><rect x="320" y="348" width="240" height="42" rx="8"/>'
    '<text x="440" y="374" text-anchor="middle">Mainband + Sideband</text></svg>'
)
(docs / 'assets/reference-architecture.svg').write_text(''.join(svg))

params = [
    ('Link width', 'x16'),
    ('Protocol', 'Streaming'),
    ('FDI width', '512 bit'),
    ('Flit format', 'Format 6'),
    ('CRC', 'enabled'),
    ('Retry', 'enabled'),
    ('TX FIFO', '32 flits'),
    ('RX FIFO', '64 flits'),
    ('Retry buffer', '16 flits'),
    ('Credit return', 'early'),
    ('Credit threshold', '2 flits'),
    ('Sideband FIFO', '8 flits'),
    ('L1', 'enabled'),
    ('Error logging', 'enabled'),
]
sections.append(
    section(
        'architecture',
        'Reference architecture',
        p('The executable model is exercised against a provided UCIe D2D reference configuration before interpreting robustness.')
        + '<div class="architecture"><img src="assets/reference-architecture.svg" alt="Two-chiplet reference architecture: AXI Streaming, FDI, D2D controller, RDI, PHY, mainband and sideband">'
        + table(['Parameter', 'Value'], params)
        + '</div>'
        + '<p class="caption">Reference configuration used for model validation. These values are not universal UCIe requirements.</p>'
        + p('Streaming, Format 6, x16, and FDI-512 are configuration metadata. The model does not implement PHY bit timing, Format-6 byte packing, or FDI cycle accuracy. The diagram is a generic redraw; no original slide or third-party branding is reproduced.'),
        'REFERENCE',
    )
)

# --- 6. Robustness ---
robust_map = {
    'UNKNOWN after architectural commit': 'UNKNOWN after commit',
    'Duplicate commit after blind retry': 'Blind-retry duplicate commit',
    'Pre-target UNKNOWN (observer A)': 'Pre-target UNKNOWN',
    'ACK history reduces UNKNOWN (observer D)': 'Retained ACK/NAK disposition → pre-target NOT_COMPLETED (D assumption)',
    'OBSERVATIONAL_EQUIVALENCE (A/B/C)': 'Same fate labels and absent completion (A/B/C)',
    'Cross-client / cross-guest delay': 'Cross-client delay',
    'CREDIT_CONSERVATION under early return + recovery': 'Credit conservation',
}
robust_rows = []
for f in data['findings']:
    label = robust_map.get(f['name'])
    if not label:
        continue
    cls = 'positive' if f['classification'] == 'REPRODUCED' else ''
    robust_rows.append(
        [esc(label), f'<span class="{cls}">{f["classification"].replace("_", " ")}</span>']
    )
sections.append(
    section(
        'robustness',
        'Robustness of the finding',
        p('To test whether the ambiguity depended on simplified transport assumptions, the model was refined with realistic buffering, credits, multiple outstanding flits, and go-back-N replay.')
        + table(['Property', 'Reference architecture result'], robust_rows)
        + '<div class="callout">UNKNOWN with different commit outcomes remains reachable under the reference configuration and implemented classification policy.</div>'
        + '<p class="caption">Source: <a href="data/reference_architecture_validation.csv">validation CSV</a>. This is reference-model validation, not silicon validation.</p>',
    )
)

# --- 7. UCIe 2.x vs 3.0 ---
comp_body = (
    p('In the exercised UCIe 2.x and 3.0 reference scenarios, the implemented classifier returns UNKNOWN for the interrupted baseline operation. This does not establish that richer UCIe 3.0 diagnostics cannot resolve transaction fate.')
    + '<div class="pair">'
    + '<article class="panel"><h3>Timed reference-model coverage</h3>'
    + p('UCIe 2.x / 3.0: CRC/retry, Retrain and LinkError. The revision comparison exercises baseline fate, paired classifications, sideband, and diagnostic scenarios; the 3.0 extension is priority-sideband metadata.')
    + '</article>'
    + '<article class="panel"><h3>Broader state-model coverage</h3>'
    + p('Diagnostics, priority sideband, and emergency / management mechanisms are explored in a separate state-model survey. This is not evidence that every listed mechanism was tested in the timed reference model.')
    + '</article>'
    + '</div>'
    + '<div class="formula compact-formula"><span>Architectural AXI commit information?</span><strong>No standardized indication modeled</strong></div>'
    + '<div class="equivalence"><strong>UNKNOWN remains reachable</strong></div>'
    + table(
        ['Modeled aspect', 'UCIe 2.x', 'UCIe 3.0'],
        [
            ['CRC / retry, Retrain, LinkError', 'Modeled', 'Modeled'],
            ['Timed reference extension', 'Baseline', 'Priority-sideband metadata'],
            ['Architectural AXI commit indication', 'None modeled', 'None modeled'],
            ['Baseline observer A fate', 'UNKNOWN', 'UNKNOWN'],
        ],
    )
    + p('The reference comparison reports identical baseline fate for the studied property. It does not claim identical implementations or universal performance.')
    + '<p>See the <a href="sources/comparison_ucie2_vs_ucie3.md">state-model comparison report</a>; its workload percentages are not intrinsic protocol probabilities.</p>'
)
sections.append(section('comparison', 'UCIe 2.x vs UCIe 3.0', comp_body))

# --- 8. Diagnostics ---
obs = []
for key, label, gt in [
    ('baseline_unknown', 'Commit + response lost', 'COMMITTED'),
    ('precommit', 'Request lost pre-target', 'NOT_COMMITTED'),
]:
    t = data['runs'][key]['transactions'][0]
    cells = [label, gt]
    for x in 'abcd':
        fate = t[f'fate_{x}']
        cells.append(f'<code class="{"unknown" if fate == "UNKNOWN" else "positive"}">{fate}</code>')
    obs.append(cells)
sections.append(
    section(
        'observers',
        'Diagnostics and observer knowledge',
        p('Classification depends on both observer visibility and which information the implemented classifier uses.')
        + table(
            ['Scenario', 'Ground truth', 'AXI only', 'AXI + FDI', 'UCIe status', 'Extra metadata'],
            obs,
        )
        + '<div class="callout">The current classifier does not exploit all modeled diagnostic information. Therefore these experiments do not establish that richer diagnostics cannot resolve transaction fate.</div>'
        + p('Under the currently implemented classification policy, adding the modeled diagnostic state does not change the fate classification. AXI-only profile A does not consume diagnostic logs. Profiles B/C record additional FDI/UCIe state, but those fields are not passed into the classification decision; their agreement with A is enforced by the implementation.')
        + p(
            badge('ASSUMPTION')
            + ' Profile D retains ACK/NAK history sufficient to distinguish the pre-target loss in this experiment. This is extra observer knowledge, not a standardized architectural commit indication.'
        )
        + p(
            badge('INTERPRETATION')
            + ' Profile D uses selected retained disposition metadata and separates the tested pre-target loss. Whether additional diagnostic information can determine other outcomes, including post-commit fate, remains an open question.'
        ),
    )
)

# --- 9. Timed SystemC ---
timed_body = (
    p('Transaction fate and timing are separate properties.')
    + '<div class="formula"><span>Functionally correct</span><b>+</b><span>exactly once</span><b>+</b><span>completed</span><b>≠</b><strong>deadline met</strong></div>'
    + '<div class="ladder"><span>Deductive argument</span><b>→</b><span>Executable state model</span><b>→</b><span>Timed SystemC</span><b>→</b><strong>Reference architecture</strong></div>'
    + p('Each step adds implementation detail while retaining the central counterexample. The model is timed and microarchitecture-aware, not transistor-level and not validated against a specific commercial UCIe IP.')
    + table(
        ['Reference experiment', 'Measured result', 'Provenance'],
        [
            ['Sustained traffic TX / RX peak', '11 / 0 flits (capacities 32 / 64)', badge('RESULT')],
            ['Credit counters, throughput run', '120 consumed / 120 returned', badge('RESULT')],
            ['Diagnostics', '8 error-log entries; observer A remains UNKNOWN', badge('RESULT')],
            ['L1', '1 event; no evidence here of additional ambiguity from L1 alone', badge('RESULT')],
            ['LinkError minimum residency', '16 ms', badge('SPEC')],
            ['Retrain / LinkReset / reactivation', '5 µs / 10 µs / 2 µs', badge('CONFIG')],
            ['AoU credit depth', '32', badge('ASSUMPTION')],
            ['Serialization cycles', '4', badge('CONFIG')],
        ],
    )
    + p('The zero RX occupancy is the reported sampled peak; it does not establish that a real receiver needs no buffering. Timing values are labeled by source. Do not treat configured delays as normative UCIe timings.')
)
sections.append(section('timed', 'Timed SystemC findings', timed_body))

# --- 10. Cross-client timing ---
bars = (
    '<div class="bar-group"><h3>Reference configuration <span class="delta">+44 ns</span></h3>'
    '<div class="bar-row"><span>Quiet</span><div class="bar" style="width:31.363636363636363%">46 ns</div></div>'
    '<div class="bar-row"><span>A faulted</span><div class="bar faultbar" style="width:61.36363636363637%">90 ns</div></div>'
    '</div>'
)
events = [
    ('2004', 'Client A', 'Request TX; fault armed'),
    ('2012', 'Client B', 'Request TX begins'),
    ('2013 / 2017', 'Shared replay', 'CRC → go-back-N replay of 3 flits'),
    ('2019', 'Client B', 'Ahead-of-sequence flit discarded'),
    ('2025 / 2029', 'Shared replay', 'Further CRC / go-back-N replay'),
    ('2031', 'Client B', 'Ahead-of-sequence flit discarded again'),
    ('2043', 'Client B', 'Request reaches remote endpoint'),
    ('2065', 'Client B', 'Write-data TX begins'),
    ('2074 / 2078', 'Client B', 'Target acceptance / architectural commit'),
    ('2090', 'Client B', 'Completion (90 ns after creation)'),
    ('2104', 'Link', 'Retrain begins after Client B completion'),
]
sections.append(
    section(
        'timing',
        'Cross-client timing effects',
        p('Independent traffic sources sharing the same UCIe transport resources can influence each other\'s timing. Client A carries the injected faults; Client B is otherwise fault-free. No hypervisor or guest isolation model is established here.')
        + '<figure class="panel">'
        + bars
        + '<figcaption>Common scale · 0–110 ns · quiet versus Client-A-faulted workload</figcaption></figure>'
        + pre('cross-client added delay ≈ 44 ns')
        + p('The effect remained observable with multiple outstanding flits and realistic buffering. Client B commits once and completes.')
        + p(
            badge('CONFIG')
            + ' Experimental timing budget = <strong>55.2 ns</strong> (46 × 1.20); faulted latency exceeds it. This is an experimental budget, not an automotive-standard deadline.'
        )
        + p(
            badge('INTERPRETATION')
            + ' This is architecture- and policy-dependent. It is not a universal UCIe property or, by itself, an ISO 26262 freedom-from-interference violation.'
        )
        + '<div class="callout">Shared resource: adapter replay scheduling and the go-back-N sequence domain.</div>'
        + '<figure class="timing-diagram"><img src="assets/replay-timing.svg" alt="Time-scaled trace from 2000 to 2110 ns. Client B transmits at 2012, is sequence-discarded at 2019 and 2031, reaches the receiver at 2043, commits at 2078, and completes at 2090 before Retrain at 2104."><figcaption>Absolute simulation time. Shaded interval spans sequence waiting and delivery; it is not a pure-wait measurement.</figcaption></figure>'
        + '<details><summary>Exact trace events and timestamps</summary><div class="timeline">'
        + ''.join(f'<div><time>{t} ns</time><strong>{who}</strong><span>{what}</span></div>' for t, who, what in events)
        + '</div></details>'
        + p('Client B transmits while several flits are outstanding; its request is discarded twice while the receiver awaits the expected sequence. Replay is scheduled ahead of new payload in the adapter. Retrain at 2104 ns happens after B completes and cannot explain that completion delay.')
        + '<p><a href="data/ref2x_cross_client_trace.csv">Full event CSV</a> · <a href="sources/ucie_adapter.cpp">Adapter scheduling code ↗</a></p>',
    )
)

# --- 11. Model-dependent ---
sections.append(
    section(
        'model-dependent',
        'What was model-dependent',
        p('Not every measured simulator outcome is a protocol property. Some results depend on transport simplifications.')
        + '<div class="pair">'
        + '<article class="panel muted"><h3>Simplified transport model</h3>'
        + '<p>Single in-flight payload<br>Simplified buffers<br>Abstract credits</p>'
        + '<div class="big">1 <small>flit peak</small></div></article>'
        + '<article class="panel"><h3>Reference architecture</h3>'
        + '<p>Multiple outstanding flits · TX 32 / RX 64<br>Retry 16 · early credits / threshold 2<br>Sideband 8 · L1 · error logging · go-back-N replay</p>'
        + '<div class="big">3 <small>flits peak / capacity 16</small></div></article>'
        + '</div>'
        + pre('retry-buffer peak = 1   →   not robust\nretry-buffer peak = 3   →   reference architecture')
        + p('The single-entry occupancy behavior depended on the simplified one-flit transport model. The measured peak of three under the reference configuration is workload-specific, not a new capacity limit.')
        + '<div class="callout">Model-dependent behavior — not a failed protocol claim.</div>',
    )
)

# --- 12. Mitigations ---
sections.append(
    section(
        'mitigations',
        'Mitigation experiments',
        p('These experiments test architectural sensitivity. They are not UCIe requirements.')
        + table(
            ['Modeled architecture option', 'Client B latency (exploratory)'],
            [
                ['Baseline round-robin', '106 ns'],
                ['Safety-priority arbitration', '39 ns'],
                ['Per-client transmission slots', '71 ns'],
                ['Replay budget yield', '59 ns'],
                ['Reserved bandwidth', '71 ns'],
                ['Virtual channels / separated queues', '71 ns'],
            ],
        )
        + p('Changed sequence spaces, arbitration, and resource allocation are experimental policies. Latency improvements can trade off fairness. These options have not been revalidated as a full mitigation campaign against the reference configuration; they do not establish guarantees.')
        + '<a href="sources/cross_client_fault_interference.md">Exploratory campaign report ↗</a>',
        'POLICY / EXPLORATORY RESULT',
    )
)

# --- 13. Automotive ---
sections.append(
    section(
        'automotive',
        'Automotive interpretation',
        p('Automotive relevance arises when safety-critical commands cross a shared chiplet transport path.')
        + p('A system-level safety architecture must decide how to handle an operation whose architectural effect cannot be reconstructed after recovery.')
        + ul(
            [
                'Reset a fault-containment region',
                'Mark the operation as indeterminate',
                'Use persistent transaction IDs',
                'Use duplicate suppression',
                'Use device-specific recovery contracts',
            ]
        )
        + p('This study does not claim an ISO 26262 violation. Cross-client timing coupling is architecture- and policy-dependent evidence for system architects, not a normative freedom-from-interference finding.'),
        'INTERPRETATION',
    )
)

# --- 14 / 15. Claims / Non-claims ---
claims = [
    'A committed operation with lost completion and a non-committed operation both receive UNKNOWN under the implemented observer/classifier.',
    'These outcomes remain reachable with reference-configured FIFOs, retry buffering, credits and multiple outstanding flits, under the implemented classification policy.',
    'Blind retry of an indeterminate non-idempotent operation can duplicate its effect.',
    'The current classifier leaves modeled diagnostic information unused; the experiments do not establish the limits of richer diagnostic classifiers.',
    'Some timing coupling between independent traffic sources remains under the reference configuration.',
    'Some simulator outcomes are implementation-dependent.',
]
non = [
    'UCIe is broken.',
    'UCIe violates its specification.',
    'Every UCIe implementation behaves this way.',
    'The reference configuration fully specifies a commercial implementation.',
    'The timed SystemC model is cycle-accurate to real silicon.',
    'The measured timing numbers are universal.',
    'Cross-client interference automatically implies an ISO 26262 freedom-from-interference violation.',
    'UCIe is responsible for defining AXI target commit semantics.',
]
sections.append(
    section(
        'claims',
        'What this study claims',
        '<p><strong>Strongly supported:</strong> the measured commit counts, UNKNOWN classifications, and blind-retry duplicate execution. <strong>Supported with qualification:</strong> reachability persists under the reference configuration and current classifier.</p>'
        + f'<article class="claim-box">{ul(claims)}</article>'
        + '<div class="callout"><strong>Not yet established:</strong> full observational equivalence of the traces, or that richer UCIe diagnostics cannot resolve transaction fate.</div>',
        'INTERPRETATION',
    )
)
sections.append(
    section(
        'non-claims',
        'What this study does not claim',
        f'<article class="claim-box muted">{ul(non)}</article>',
        'INTERPRETATION',
    )
)

# --- 16. Limitations ---
sections.append(
    section(
        'limitations',
        'Limitations',
        ul(
            [
                'Abstract target model: architectural commit is modeled, not silicon-verified.',
                'Incomplete knowledge of commercial UCIe controller microarchitecture.',
                'Configurable recovery policies (for example, outstanding AXI discard after LinkError).',
                'Timed SystemC, not transistor-level or cycle-accurate to commercial IP.',
                'No silicon validation; no RTL or commercial IP confirmation.',
                'Random workload rates are not field probabilities.',
                'The reference architecture is not a complete implementation specification.',
                'The AoU model remains an abstraction where the full specification behavior is not modeled.',
                'Format-6 optional bytes, electrical training, and FDI cycle accuracy are out of scope.',
                'Diagnostic fields are study abstractions, not standardized or vendor register maps.',
                'Finite scenarios establish reachability, not exhaustive absence of other outcomes.',
                'The timed equivalence flag checks fate labels and completion status, not complete observer-visible histories.',
                'Profiles B/C do not use all recorded diagnostic state for classification; a stronger diagnostic classifier has not been evaluated.',
            ]
        ),
        'ASSUMPTION / SCOPE',
    )
)

# --- 17. Reproducibility ---
downloads = [
    ('sources/reference_architecture_validation.md', 'Reference report'),
    ('data/reference_architecture_validation.json', 'Complete JSON'),
    ('data/reference_architecture_validation.csv', 'Validation CSV'),
    ('configs/reference_ucie2x.yaml', 'configs/reference_ucie2x.yaml'),
    ('configs/reference_ucie3.yaml', 'configs/reference_ucie3.yaml'),
    ('data/ref2x_cross_client_trace.csv', 'Faulted event trace'),
    ('data/ref2x_shared_quiet_trace.csv', 'Quiet event trace'),
    ('sources/findings.md', 'Transaction-fate case report'),
    ('sources/comparison_ucie2_vs_ucie3.md', 'Revision comparison report'),
    ('sources/reference_architecture_validation.py', 'Campaign code (sanitized export)'),
    ('sources/ucie_adapter.cpp', 'Timed adapter code'),
]
sections.append(
    section(
        'reproducibility',
        'Reproducibility',
        p('From the repository root, with SystemC, CMake, pkg-config, and Python available:')
        + '<pre><code>cmake -S . -B build\ncmake --build build -j\npython -m simulation.experiments.reference_architecture_validation</code></pre>'
        + p('The campaign loads the repository’s internal reference configurations. The neutral public exports below preserve their parameter values; naming and identifying comments are sanitized. Stored evidence is presented here without rerunning or changing the simulation.')
        + p('Evidence downloads retain their original English text and machine-readable field names.')
        + '<div class="downloads">'
        + ''.join(f'<a href="{url}">{label} ↗</a>' for url, label in downloads)
        + '</div>'
        + '<p class="caption">Stored reports and JSON retain historical terminology, including <code>observational_equivalence_abc</code>. In the timed checker this flag means matching fate labels and completion status; it is not a full-history equivalence test. Diagnostic claims in those reports must likewise be read subject to the classifier limitations above. Measurements have not been changed.</p>'
        + '<details><summary>Evidence notes and terminology</summary><p>Downloads preserve legacy machine keys such as <code>guest</code>; these denote traffic clients here, not modeled hypervisor guests. The exported reference report contains a generic caveat that coupling “may disappear” under multi-outstanding buffering. Its measured reference run instead reports 44 ns added delay and a deadline miss; this page follows those measurements. Whether other implementations eliminate the effect remains open.</p></details>',
        'REPRODUCIBILITY',
    )
)

# --- 18. Open questions ---
sections.append(
    section(
        'questions',
        'Open questions',
        ul(
            [
                'Which remaining behaviors depend on implementation-specific UCIe controller microarchitecture?',
                'What state survives Retrain, LinkReset, and LinkError in a real controller?',
                'Can implementation-specific diagnostics eliminate any remaining UNKNOWN cases?',
                'What end-to-end mechanism is sufficient to provide exactly-once behavior?',
                'Which findings reproduce in RTL or commercial IP?',
                'How should these behaviors be represented in an automotive virtual platform?',
            ]
        ),
        'OPEN QUESTION',
    )
)

# --- 19. Conclusion ---
sections.append(
    section(
        'conclusion',
        'Conclusion',
        p('The study demonstrates that recovering a UCIe link and reconstructing the architectural fate of an interrupted higher-layer operation are different problems. In the reference-architecture model, a committed non-idempotent operation whose response is lost and an operation that never reached the target both receive UNKNOWN under the implemented observer/classifier. Blind retry of the committed case can create duplicate architectural effects. Full observer-history equivalence and the ability of richer diagnostics to resolve fate remain untested stronger claims.')
        + p('The reference configuration shows that this ambiguity is not dependent on a one-flit transport simplification. At the same time, model refinement exposed other behaviors as implementation-dependent. The next step is validation against a concrete UCIe controller implementation.'),
        'INTERPRETATION',
    )
)

nav = [
    ('problem', 'Problem'),
    ('traces', 'Traces'),
    ('architecture', 'Architecture'),
    ('robustness', 'Robustness'),
    ('claims', 'Claims'),
    ('reproducibility', 'Evidence'),
]

summary_cards = [
    ('Transaction fate', 'Yes', 'UNKNOWN reachable', 'positive'),
    ('Blind retry', 'Reachable', 'Duplicate architectural effect', 'positive'),
    ('UCIe 3.0', 'UNKNOWN', 'Baseline result under the implemented classifier', ''),
    (
        'Reference architecture',
        'Yes',
        'Ambiguity remains with realistic FIFOs, credits, retry buffer, and multiple outstanding flits',
        'positive',
    ),
]

hero = (
    '<div class="hero">'
    '<div class="eyebrow">UCIe / AoU FAULT-INJECTION STUDY</div>'
    '<h1>An Interrupted AXI Transaction Can Have an Indeterminate Architectural Outcome</h1>'
    '<p class="lead">In the modeled UCIe recovery scenarios, a committed operation with lost completion and an operation that never reached the target both receive UNKNOWN under the implemented observer/classifier.</p>'
    + p('The ground-truth outcomes differ; the implemented fate classifications match. This does not establish equality of the complete observer-visible histories or that richer diagnostics cannot determine fate. The study uses three fate classifications: <code>COMPLETED</code>, <code>NOT_COMPLETED</code>, and <code class="unknown">UNKNOWN</code>. These are study labels, not UCIe-defined protocol states.')
    + '<div class="summary">'
    + ''.join(
        f'<div><span>{label}</span><small>{sub}</small><strong class="{cls}">{value}</strong></div>'
        for label, value, sub, cls in summary_cards
    )
    + '</div>'
    + '<p class="takeaway">This study separates link recovery from transaction recovery.</p>'
    + '<details class="legend"><summary>How to read the evidence labels</summary>'
    '<p>SPEC: specification-derived fact · REFERENCE: supplied architecture parameter · CONFIG: simulation configuration · POLICY: implementation choice · ASSUMPTION: model abstraction · RESULT: measured simulation outcome · INTERPRETATION: inference from results · OPEN QUESTION: unresolved research.</p>'
    '</details></div>'
)

page = (
    '<!doctype html><html lang="en"><head><meta charset="utf-8">'
    '<meta name="viewport" content="width=device-width, initial-scale=1">'
    '<title>An Interrupted AXI Transaction Can Have an Indeterminate Architectural Outcome</title>'
    '<meta name="description" content="Technical study of UCIe/AoU recovery and AXI transaction-fate ambiguity: evidence, reference architecture, robustness, and limits.">'
    '<link rel="icon" href="assets/favicon.png">'
    '<script src="assets/language.js"></script>'
    '<link rel="alternate" hreflang="en" href="index.html">'
    '<link rel="alternate" hreflang="de" href="de/index.html">'
    '<link rel="alternate" hreflang="x-default" href="index.html">'
    '<script src="assets/theme-init.js"></script>'
    '<link rel="stylesheet" href="assets/tokens.css">'
    '<link rel="stylesheet" href="assets/study.css">'
    '<script src="assets/theme.js" defer></script></head><body>'
    '<a class="skip" href="#content">Skip to content</a>'
    '<header><a class="brand" href="index.html"><span class="wordmark" role="img" aria-label="NUVUS"></span>'
    '<span class="brand-caption">/ UCIe · AoU research</span></a>'
    '<nav aria-label="Study sections">'
    + ''.join(f'<a href="#{sid}">{label}</a>' for sid, label in nav)
    + '</nav>'
    '<nav class="language" aria-label="Language"><a href="index.html?lang=en" data-lang-link="en" lang="en" hreflang="en" aria-current="page">EN</a><a href="de/index.html?lang=de" data-lang-link="de" lang="de" hreflang="de">DE</a></nav>'
    '<div class="theme" role="group" aria-label="Colour theme">'
    '<button type="button" data-theme-set="light" aria-pressed="false">Light</button>'
    '<button type="button" data-theme-set="dark" aria-pressed="false">Dark</button>'
    '<button type="button" data-theme-set="system" aria-pressed="true">System</button>'
    '</div></header>'
    '<main id="content">'
    + hero
    + ''.join(sections)
    + '</main>'
    '<footer>UCIe / AoU fault-injection study · Reference-model evidence, not commercial-IP validation.'
    '<a href="#content">Back to top ↑</a></footer></body></html>'
)

# Inline SVGs inherit the document's semantic brand tokens in both themes.
for asset, css_class in [
    ('reference-architecture.svg', 'architecture-diagram'),
    ('replay-timing.svg', 'replay-diagram'),
]:
    svg_text = (docs / 'assets' / asset).read_text().replace('<svg ', f'<svg class="{css_class}" ', 1)
    svg_text = re.sub(
        r'(<style>)(.*?)(</style>)',
        lambda m: m[1]
        + re.sub(r'([^{}]+)\{', lambda rule: '.' + css_class + ' ' + rule[1] + '{', m[2])
        + m[3],
        svg_text,
    )
    page = re.sub(
        r'<img src="assets/' + re.escape(asset) + r'"[^>]*>',
        lambda match, svg_text=svg_text: svg_text,
        page,
    )

page = re.sub(
    r'>(<(?:(?:/?)(?:section|article|div|table|thead|tbody|tr|h[123]|p|header|main|footer|details|summary|figure|ul|ol|li|nav|head|body))\b)',
    r'>\n\1',
    page,
)
(docs / 'index.html').write_text(page)

# Supersede old navigation endpoints so stale claims cannot remain on alternate pages.
redirect_map = {
    'index': 'content',
    'results': 'robustness',
    'methodology': 'architecture',
    'limitations': 'limitations',
}
for prefix in ['', 'de/']:
    for name, target in redirect_map.items():
        if not prefix and name == 'index':
            continue
        url = ('../' if prefix else '') + 'index.html#' + target
        lang = 'de' if prefix else 'en'
        title = 'Aktualisierte UCIe-Studie' if prefix else 'Updated UCIe study'
        body = (
            'Die aktuelle Referenzmodell-Validierung ist auf Englisch verfügbar. '
            if prefix
            else 'The current reference-model study is available in English. '
        )
        link = 'Aktualisierte Studie lesen' if prefix else 'Read the updated study'
        (docs / prefix / (name + '.html')).write_text(
            f'<!doctype html><html lang="{lang}"><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">'
            f'<meta http-equiv="refresh" content="0;url={url}"><title>{title}</title>'
            f'<p>{body}<a href="{url}">{link}</a>.</p></html>'
        )

runpy.run_path(str(root / 'scripts/build_de_pages.py'), run_name='__main__')
