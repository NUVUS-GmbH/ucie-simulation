#!/usr/bin/env python3
"""Translate the generated study using a reviewed, complete text catalogue.

Run through build_study.py. New English text must be added to translations/de.json;
unknown text fails the build rather than silently producing a mixed-language page.
Machine identifiers and unchanged technical terms have explicit catalogue entries.
"""
from html import escape
from html.parser import HTMLParser
from pathlib import Path
import json
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1] / 'docs'
CATALOGUE = Path(__file__).parent / 'translations/de.json'


class GermanPage(HTMLParser):
    def __init__(self, catalogue):
        super().__init__(convert_charrefs=False)
        self.catalogue = catalogue
        self.parts = []
        self.skip = 0
        self.missing = set()

    def translate(self, value):
        key = value.strip()
        if not key or not any(c.isalpha() for c in key):
            return value
        if key not in self.catalogue:
            self.missing.add(key)
            return value
        left = value[:len(value) - len(value.lstrip())]
        right = value[len(value.rstrip()):]
        return left + self.catalogue[key] + right

    def handle_decl(self, decl):
        self.parts.append('<!' + decl + '>')

    def handle_starttag(self, tag, attrs):
        values = dict(attrs)
        out = []
        for name, value in attrs:
            if value is None:
                out.append(name)
                continue
            if tag == 'html' and name == 'lang':
                value = 'de'
            elif name in ('aria-label', 'alt') or (tag == 'meta' and values.get('name') == 'description' and name == 'content'):
                value = self.translate(value)
            elif name == 'href' and values.get('class') == 'brand':
                value = 'index.html?lang=de'
            elif name in ('src', 'href'):
                u = urlsplit(value)
                if u.path and not u.scheme and not u.netloc and not value.startswith('/'):
                    value = '../' + value
            if name == 'aria-current' and values.get('data-lang-link'):
                continue
            out.append(f'{name}="{escape(value, quote=True)}"')
        if values.get('data-lang-link') == 'de':
            out.append('aria-current="page"')
        self.parts.append('<' + tag + (' ' if out else '') + ' '.join(out) + '>')
        if tag in ('script', 'style'):
            self.skip += 1

    def handle_endtag(self, tag):
        self.parts.append('</' + tag + '>')
        if tag in ('script', 'style'):
            self.skip -= 1

    def handle_data(self, data):
        self.parts.append(data if self.skip else escape(self.translate(data), quote=False))

    def handle_entityref(self, name):
        self.parts.append('&' + name + ';')

    def handle_charref(self, name):
        self.parts.append('&#' + name + ';')

    def handle_comment(self, data):
        self.parts.append('<!--' + data + '-->')


def main():
    parser = GermanPage(json.loads(CATALOGUE.read_text()))
    parser.feed((ROOT / 'index.html').read_text())
    if parser.missing:
        raise ValueError('Missing German translations:\n' + '\n'.join(sorted(parser.missing)))
    (ROOT / 'de').mkdir(exist_ok=True)
    (ROOT / 'de/index.html').write_text(''.join(parser.parts))
    for name, anchor in [('results', 'robustness'), ('methodology', 'architecture'), ('limitations', 'limitations')]:
        url = 'index.html#' + anchor
        (ROOT / 'de' / (name + '.html')).write_text(
            f'<!doctype html><html lang="de"><meta charset="utf-8">'
            f'<meta name="viewport" content="width=device-width, initial-scale=1">'
            f'<meta http-equiv="refresh" content="0;url={url}">'
            f'<title>Aktualisierte UCIe-Studie</title>'
            f'<p><a href="{url}">Aktualisierte Studie lesen</a>.</p></html>'
        )


if __name__ == '__main__':
    main()
