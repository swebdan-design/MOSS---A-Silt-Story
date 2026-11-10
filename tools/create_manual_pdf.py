#!/usr/bin/env python3
"""Typeset release/manual/MOSS_Manual.md as an A5 booklet PDF (ReportLab).
Handles the Markdown used in the manual: headings, paragraphs, **bold**,
*italic*, `code`, bullet and numbered lists, tables and images."""
import re, os
from pathlib import Path
from reportlab.lib.pagesizes import A5
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (BaseDocTemplate, Frame, PageTemplate, Paragraph, Spacer, Image,
                                Table, TableStyle, PageBreak, ListFlowable, ListItem, KeepTogether)

ROOT = Path(__file__).resolve().parents[1]
MD = ROOT / 'release/manual/MOSS_Manual.md'
PDF = ROOT / 'release/manual/MOSS_Manual.pdf'
FD = '/usr/share/fonts/truetype/dejavu/'
pdfmetrics.registerFont(TTFont('Body', FD + 'DejaVuSans.ttf'))
pdfmetrics.registerFont(TTFont('Body-Bold', FD + 'DejaVuSans-Bold.ttf'))
pdfmetrics.registerFont(TTFont('Body-Oblique', FD + ('DejaVuSans-Oblique.ttf' if os.path.exists(FD + 'DejaVuSans-Oblique.ttf') else 'DejaVuSerif.ttf')))
pdfmetrics.registerFont(TTFont('Mono', FD + 'DejaVuSansMono.ttf'))
from reportlab.pdfbase.pdfmetrics import registerFontFamily
registerFontFamily('Body', normal='Body', bold='Body-Bold', italic='Body-Oblique', boldItalic='Body-Bold')

NAVY = colors.HexColor('#101830'); SAGE = colors.HexColor('#3f7a58'); INK = colors.HexColor('#1c2a28')
MUTED = colors.HexColor('#5a6e6a'); CREAM = colors.HexColor('#f6f3e4'); GOLD = colors.HexColor('#b08a30')
body = ParagraphStyle('body', fontName='Body', fontSize=8.6, leading=12.4, textColor=INK, spaceAfter=5)
h1 = ParagraphStyle('h1', fontName='Body-Bold', fontSize=17, leading=21, textColor=NAVY, spaceAfter=4)
h2 = ParagraphStyle('h2', fontName='Body-Bold', fontSize=12.5, leading=16, textColor=NAVY, spaceBefore=4, spaceAfter=6)
h3 = ParagraphStyle('h3', fontName='Body-Bold', fontSize=10, leading=13, textColor=SAGE, spaceBefore=4, spaceAfter=4)
cell = ParagraphStyle('cell', parent=body, fontSize=7.4, leading=9.6, spaceAfter=0)
cellb = ParagraphStyle('cellb', parent=cell, fontName='Body-Bold')


def inline(t):
    t = t.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')
    t = re.sub(r'\*\*(.+?)\*\*', r'<b>\1</b>', t)
    t = re.sub(r'(?<![\w*])\*(?!\s)(.+?)(?<!\s)\*(?![\w*])', r'<i>\1</i>', t)
    t = re.sub(r'`(.+?)`', r'<font face="Mono">\1</font>', t)
    return t


def image(path, maxw, maxh):
    from PIL import Image as PI
    w, h = PI.open(path).size
    s = min(maxw / w, maxh / h)
    return Image(str(path), w * s, h * s)


def build():
    lines = MD.read_text().splitlines()
    W, H = A5
    inner = W - 30 * mm
    story = []
    # cover
    story.append(Spacer(1, 6 * mm))
    story.append(image(ROOT / 'release/artwork/MOSS_Cover_1000x1000.png', inner, inner))
    story.append(Spacer(1, 6 * mm))
    story.append(Paragraph('USER BOOKLET', ParagraphStyle('c', parent=h3, alignment=1, textColor=MUTED)))
    story.append(Paragraph('An interactive musical adventure for the original Game Boy',
                           ParagraphStyle('c2', parent=body, alignment=1)))
    story.append(PageBreak())
    i = 0
    para = []
    def flush():
        nonlocal para
        if para:
            story.append(Paragraph(inline(' '.join(para)), body)); para = []
    first_h1 = True
    while i < len(lines):
        ln = lines[i]
        if not ln.strip():
            flush(); i += 1; continue
        if ln.startswith('# '):
            flush()
            if first_h1: first_h1 = False; i += 1; continue
            story.append(Paragraph(inline(ln[2:]), h1)); i += 1; continue
        if ln.startswith('## '):
            flush()
            if story and not isinstance(story[-1], PageBreak) and re.match(r'## \d+\.', ln):
                story.append(PageBreak())
            m = re.match(r'## (\d+)\. (.*)', ln)
            if m:
                story.append(Paragraph(f'<font color="#b08a30">{int(m.group(1)):02d} /</font> {inline(m.group(2))}', h2))
            else:
                story.append(Paragraph(inline(ln[3:]), h2))
            i += 1; continue
        if ln.startswith('### '):
            flush(); story.append(Paragraph(inline(ln[4:]), h3)); i += 1; continue
        m = re.match(r'!\[[^\]]*\]\(([^)]+)\)', ln.strip())
        if m:
            flush()
            p = MD.parent / m.group(1)
            if 'title_art' in m.group(1) and len(story) < 6:
                i += 1; continue
            story.append(image(p, inner, 70 * mm if 'world_map' not in m.group(1) else 120 * mm))
            story.append(Spacer(1, 3 * mm)); i += 1; continue
        if ln.startswith('|'):
            flush()
            rows = []
            while i < len(lines) and lines[i].startswith('|'):
                cells = [c.strip() for c in lines[i].strip().strip('|').split('|')]
                if not all(set(c) <= set('-: ') for c in cells): rows.append(cells)
                i += 1
            ncol = max(len(r) for r in rows)
            data = [[Paragraph(inline(c), cellb if r == 0 else cell) for c in row + [''] * (ncol - len(row))]
                    for r, row in enumerate(rows)]
            t = Table(data, colWidths=[inner / ncol] * ncol, repeatRows=1)
            t.setStyle(TableStyle([('BACKGROUND', (0, 0), (-1, 0), colors.HexColor('#e4ebdf')),
                                   ('LINEBELOW', (0, 0), (-1, -1), 0.3, colors.HexColor('#b8c4b8')),
                                   ('VALIGN', (0, 0), (-1, -1), 'TOP'),
                                   ('LEFTPADDING', (0, 0), (-1, -1), 3), ('RIGHTPADDING', (0, 0), (-1, -1), 3)]))
            story.append(t); story.append(Spacer(1, 4 * mm)); continue
        if re.match(r'\s*(- |\d+\. )', ln):
            flush()
            items, numbered = [], bool(re.match(r'\s*\d+\. ', ln))
            while i < len(lines) and re.match(r'\s*(- |\d+\. )', lines[i]):
                text = re.sub(r'\s*(- |\d+\. )', '', lines[i], count=1)
                i += 1
                while i < len(lines) and lines[i].startswith('  ') and lines[i].strip():
                    text += ' ' + lines[i].strip(); i += 1
                items.append(ListItem(Paragraph(inline(text), body), leftIndent=10))
            story.append(ListFlowable(items, bulletType='1' if numbered else 'bullet', start='1' if numbered else None,
                                      bulletFontName='Body', bulletFontSize=8, leftIndent=12))
            continue
        para.append(ln.strip()); i += 1
    flush()

    def page(c, doc):
        c.saveState()
        c.setFillColor(CREAM); c.rect(0, 0, W, H, stroke=0, fill=1)
        if doc.page > 1:
            c.setFont('Body', 7); c.setFillColor(MUTED)
            c.drawString(15 * mm, 9 * mm, 'MOSS / A SILT Story / Synth Centric')
            c.drawRightString(W - 15 * mm, 9 * mm, f'{doc.page:02d}')
        c.restoreState()

    doc = BaseDocTemplate(str(PDF), pagesize=A5, title='MOSS - A SILT Story - User Booklet',
                          author='Synth Centric', subject='Game Boy interactive musical adventure: user booklet',
                          leftMargin=15 * mm, rightMargin=15 * mm, topMargin=15 * mm, bottomMargin=16 * mm)
    frame = Frame(doc.leftMargin, doc.bottomMargin, doc.width, doc.height, id='f')
    doc.addPageTemplates([PageTemplate(id='p', frames=[frame], onPage=page)])
    doc.build(story)
    print('wrote', PDF)


if __name__ == '__main__':
    build()
