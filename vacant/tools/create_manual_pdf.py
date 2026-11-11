#!/usr/bin/env python3
"""Render docs/VACANT_Manual.md to PDF with ReportLab.

  create_manual_pdf.py [out.pdf]   (default build/release/VACANT_Manual.pdf)

Handles the Markdown the manual uses: headings, paragraphs, bullet lists,
tables, rules, **bold**, *italic*, `code`. Adds the album cover as a title page
and a page of screenshots captured from the ROM (build/shots/, if present).
"""
import re
import sys
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A5
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.platypus import (Image, PageBreak, Paragraph, SimpleDocTemplate, Spacer, Table,
                                TableStyle, HRFlowable, KeepTogether)

ROOT = Path(__file__).resolve().parents[1]
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build/release/VACANT_Manual.pdf'
INK = colors.HexColor('#1b1830')
ACCENT = colors.HexColor('#2a8c94')
DIM = colors.HexColor('#5a5878')

S = {
    'title': ParagraphStyle('t', fontName='Helvetica-Bold', fontSize=26, leading=30, alignment=TA_CENTER,
                            textColor=INK, spaceAfter=4),
    'sub': ParagraphStyle('s', fontName='Helvetica', fontSize=11, leading=14, alignment=TA_CENTER, textColor=DIM),
    'h2': ParagraphStyle('h2', fontName='Helvetica-Bold', fontSize=13.5, leading=17, textColor=INK,
                         spaceBefore=10, spaceAfter=5),
    'h3': ParagraphStyle('h3', fontName='Helvetica-Bold', fontSize=10.5, leading=13, textColor=ACCENT,
                         spaceBefore=6, spaceAfter=3),
    'p': ParagraphStyle('p', fontName='Helvetica', fontSize=9, leading=12.6, textColor=INK, spaceAfter=5),
    'li': ParagraphStyle('li', fontName='Helvetica', fontSize=9, leading=12.2, textColor=INK, leftIndent=10,
                         bulletIndent=2, spaceAfter=2),
    'cell': ParagraphStyle('c', fontName='Helvetica', fontSize=7.6, leading=9.6, textColor=INK),
    'cellb': ParagraphStyle('cb', fontName='Helvetica-Bold', fontSize=7.6, leading=9.6, textColor=colors.white),
    'cap': ParagraphStyle('cap', fontName='Helvetica-Oblique', fontSize=7.5, leading=10, textColor=DIM,
                          alignment=TA_CENTER),
}


def inline(t):
    t = t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    t = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', t)
    t = re.sub(r'(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])', r'<i>\1</i>', t)
    t = re.sub(r'`(.+?)`', r'<font face="Courier">\1</font>', t)
    return t


def table(rows):
    data = []
    for i, r in enumerate(rows):
        data.append([Paragraph(inline(c), S['cellb'] if i == 0 else S['cell']) for c in r])
    w = A5[0] - 30 * mm
    n = len(rows[0])
    t = Table(data, colWidths=[w / n] * n, repeatRows=1)
    t.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, 0), INK),
        ('ROWBACKGROUNDS', (0, 1), (-1, -1), [colors.white, colors.HexColor('#eeedf5')]),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LINEBELOW', (0, 0), (-1, -1), 0.25, colors.HexColor('#c8c6d8')),
        ('TOPPADDING', (0, 0), (-1, -1), 2.5), ('BOTTOMPADDING', (0, 0), (-1, -1), 2.5),
    ]))
    return t


def parse(md):
    story, para, items, rows = [], [], [], []

    def flush():
        nonlocal para, items, rows
        if para:
            story.append(Paragraph(inline(' '.join(para)), S['p']))
            para = []
        if items:
            for it in items:
                story.append(Paragraph(inline(it), S['li'], bulletText='•'))
            story.append(Spacer(1, 3))
            items = []
        if rows:
            story.append(table(rows))
            story.append(Spacer(1, 6))
            rows = []

    lines = md.splitlines()
    for ln in lines[1:]:            # the first heading is drawn on the title page
        s = ln.rstrip()
        if s.startswith('## ') or s.startswith('### '):
            flush()
            if s.startswith('## ') and re.match(r'## \d+\.', s):
                story.append(Paragraph(inline(s[3:]), S['h2']))
            elif s.startswith('## '):
                continue            # subtitle lines handled on the title page
            else:
                continue
        elif s.startswith('**Page') and s.endswith('**'):
            flush()
            story.append(Paragraph(inline(s.strip('*')), S['h3']))
        elif s.startswith('|'):
            if para or items:
                flush()
            cells = [c.strip() for c in s.strip('|').split('|')]
            if all(re.fullmatch(r'-+', c.replace(' ', '')) for c in cells):
                continue
            rows.append(cells)
        elif s.startswith('- '):
            if para or rows:
                flush()
            items.append(s[2:])
        elif s == '---':
            flush()
            story.append(HRFlowable(width='100%', thickness=0.4, color=colors.HexColor('#c8c6d8'),
                                    spaceBefore=4, spaceAfter=6))
        elif not s:
            flush()
        elif s.startswith('#'):
            continue
        else:
            if items and not s.startswith('- '):
                items[-1] += ' ' + s.strip()
            else:
                para.append(s.strip())
    flush()
    return story


def page_deco(c, doc):
    c.saveState()
    c.setFont('Helvetica', 7)
    c.setFillColor(DIM)
    c.drawString(15 * mm, 9 * mm, 'VACANT  ·  by Synth Centric')
    c.drawRightString(A5[0] - 15 * mm, 9 * mm, str(doc.page))
    c.restoreState()


def main():
    md = (ROOT / 'docs/VACANT_Manual.md').read_text()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(str(OUT), pagesize=A5, leftMargin=15 * mm, rightMargin=15 * mm,
                            topMargin=14 * mm, bottomMargin=16 * mm, title='VACANT - Manual',
                            author='Synth Centric', subject='An Endless City After Dark')
    story = []
    cover = ROOT / 'artwork/VACANT_Album_Cover.png'
    story.append(Spacer(1, 6 * mm))
    story.append(Paragraph('VACANT', S['title']))
    story.append(Paragraph('An Endless City After Dark', S['sub']))
    story.append(Paragraph('by Synth Centric', S['sub']))
    story.append(Spacer(1, 6 * mm))
    if cover.exists():
        side = A5[0] - 40 * mm
        story.append(Image(str(cover), width=side, height=side))
    story.append(Spacer(1, 4 * mm))
    story.append(Paragraph('A generative city and ambient album for the Game Boy Color', S['cap']))
    story.append(PageBreak())
    story += parse(md)
    sheet = ROOT / 'build/shots/contact_sheet.png'
    if sheet.exists():
        from PIL import Image as PI
        w, h = PI.open(sheet).size
        width = A5[0] - 30 * mm
        story.append(PageBreak())
        story.append(Paragraph('Screens', S['h2']))
        story.append(KeepTogether([
            Image(str(sheet), width=width, height=width * h / w),
            Spacer(1, 3 * mm),
            Paragraph('Captured from the running ROM in an emulator (PyBoy, Game Boy Color mode): title, '
                      'Empty Streets in light rain, Industrial Quarter, Residential Towers in fog, Lower City in '
                      'heavy rain, Forgotten Places, a lightning flash, a train on the elevated railway, the '
                      'synthesiser.', S['cap'])]))
    doc.build(story, onFirstPage=lambda c, d: None, onLaterPages=page_deco)
    print(f'create_manual_pdf: {OUT}')


if __name__ == '__main__':
    main()
