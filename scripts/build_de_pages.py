#!/usr/bin/env python3
"""Keep legacy German URLs pointed at the current English reference study.

The prior phrase-replacement translation targets the superseded study.
A reviewed translation is needed before restoring localized content.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / 'docs' / 'de'


def main():
    for name, anchor in [
        ('index', 'content'),
        ('results', 'robustness'),
        ('methodology', 'architecture'),
        ('limitations', 'limitations'),
    ]:
        url = '../index.html#' + anchor
        (ROOT / (name + '.html')).write_text(
            f'<!doctype html><html lang="de"><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">'
            f'<meta http-equiv="refresh" content="0;url={url}">'
            f'<title>Aktualisierte UCIe-Studie</title>'
            f'<p>Die aktuelle Referenzmodell-Validierung ist auf Englisch verfügbar. '
            f'<a href="{url}">Aktualisierte Studie lesen</a>.</p></html>'
        )


if __name__ == '__main__':
    main()
