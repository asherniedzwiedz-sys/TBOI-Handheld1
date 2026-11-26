#!/usr/bin/env python3
"""Embed a ROM into web/player.html to make a single-file browser player.

Usage: mkweb.py web/player.html build/undercroft.nes out.html
The page loads the jsnes emulator from a CDN, so it needs an internet
connection the first time it opens.
"""
import base64
import sys

HEAD = ('<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n'
        '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
        '</head>\n<body style="margin:0">\n')
TAIL = '\n</body>\n</html>\n'


def main(template, rom, out):
    page = open(template).read()
    b64 = base64.b64encode(open(rom, 'rb').read()).decode()
    if '__ROM__' not in page:
        sys.exit('template has no __ROM__ placeholder')
    open(out, 'w').write(HEAD + page.replace('__ROM__', b64) + TAIL)
    print(f'web player: {out}')


if __name__ == '__main__':
    main(*sys.argv[1:4])
