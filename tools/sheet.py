"""Combine screenshots into one contact sheet: sheet.py out.png a.png b.png ..."""
import sys
from PIL import Image
fs = sys.argv[2:]
cols = min(4, len(fs))
rows = (len(fs) + cols - 1) // cols
W = Image.new('RGB', (cols * 166, rows * 150), 'white')
for i, f in enumerate(fs):
    W.paste(Image.open(f).convert('RGB'), ((i % cols) * 166, (i // cols) * 150))
W.resize((W.width * 2, W.height * 2), Image.NEAREST).save(sys.argv[1])
