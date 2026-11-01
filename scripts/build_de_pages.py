#!/usr/bin/env python3
"""Keep legacy German URLs pointed at the current English reference study.

The prior phrase-replacement translation targets the superseded study.
A reviewed translation is needed before restoring localized content.
"""
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1] / 'docs' / 'de'
def main():
    for name, anchor in [('index', 'content'), ('results', 'validation'), ('methodology', 'architecture'), ('limitations', 'limitations')]:
        url = '../index.html#' + anchor
        (ROOT / (name + '.html')).write_text(f'<!doctype html><html lang="de"><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><meta http-equiv="refresh" content="0;url={url}"><title>Aktualisierte UCIe-Studie</title><p>Die aktuelle Referenzmodell-Validierung ist auf Englisch verfügbar. <a href="{url}">Aktualisierte Studie lesen</a>.</p></html>')
if __name__ == '__main__':
    main()
