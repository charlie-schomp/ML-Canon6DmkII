"""Render TEST-fastcard as a fillable PDF form (checkboxes + text fields). python make_fastcard_pdf.py"""
from reportlab.lib.pagesizes import letter
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.colors import HexColor, white
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

FONTS = "C:/Windows/Fonts/"
pdfmetrics.registerFont(TTFont("Body", FONTS + "arial.ttf"))
pdfmetrics.registerFont(TTFont("Body-Bold", FONTS + "arialbd.ttf"))
pdfmetrics.registerFont(TTFont("Body-Italic", FONTS + "ariali.ttf"))
pdfmetrics.registerFont(TTFont("Mono", FONTS + "consola.ttf"))
from reportlab.lib.fonts import addMapping
addMapping("Body", 0, 0, "Body"); addMapping("Body", 1, 0, "Body-Bold"); addMapping("Body", 0, 1, "Body-Italic")

INK = HexColor("#16191e"); MUTED = HexColor("#5b6470"); LINE = HexColor("#cfd5dc")
REC = HexColor("#c4232b"); FIELD = HexColor("#f2f5f8"); SHADE = HexColor("#eef1f4")
W, H = letter; M = 54; COLW = W - 2 * M

body = ParagraphStyle("b", fontName="Body", fontSize=9.5, leading=13, textColor=INK)
small = ParagraphStyle("s", parent=body, fontSize=8.5, leading=11.5, textColor=MUTED)
h1 = ParagraphStyle("h1", parent=body, fontName="Body-Bold", fontSize=20, leading=24)
h2 = ParagraphStyle("h2", parent=body, fontName="Body-Bold", fontSize=12.5, leading=16)


def code(t):
    return '<font name="Mono" size="8.5">%s</font>' % t


class Sheet:
    def __init__(self, path):
        self.c = canvas.Canvas(path, pagesize=letter)
        self.c.setTitle("6D2 Fast-Card Test (v76)")
        self.c.setAuthor("6D Mark II Magic Lantern port")
        self.c.setSubject("Fillable test sheet for the fast-SD-card tester")
        self.page = 1; self.n = 0
        self.top()

    def top(self):
        self.y = H - M
        self.c.setFont("Body", 7.5); self.c.setFillColor(MUTED)
        self.c.drawString(M, 30, "6D Mark II Magic Lantern v76 - fast-card test")
        self.c.drawRightString(W - M, 30, "page %d" % self.page)

    def need(self, h):
        if self.y - h < 50:
            self.c.showPage(); self.page += 1; self.top()

    def para(self, text, style=body, indent=0, gap=4, width=None):
        p = Paragraph(text, style)
        w, h = p.wrap((width or COLW) - indent, H)
        self.need(h + gap)
        p.drawOn(self.c, M + indent, self.y - h)
        self.y -= h + gap
        return h

    def section(self, num, title):
        self.need(40)
        self.y -= 8
        self.c.setFillColor(REC); self.c.setFont("Mono", 9)
        self.c.drawString(M, self.y - 12, num)
        self.c.setFillColor(INK)
        p = Paragraph(title, h2); w, h = p.wrap(COLW - 26, 40); p.drawOn(self.c, M + 26, self.y - h)
        self.y -= h + 3
        self.c.setStrokeColor(LINE); self.c.setLineWidth(0.6); self.c.line(M, self.y, W - M, self.y)
        self.y -= 7

    def name(self, base):
        self.n += 1
        return "%s_%d" % (base, self.n)

    def check(self, text, base="check", field_ph=None, field_w=150):
        """checkbox + text, optionally a text field at the right for a value"""
        tw = COLW - 20 - (field_w + 10 if field_ph else 0)
        p = Paragraph(text, body); w, h = p.wrap(tw, H)
        h = max(h, 14); self.need(h + 6)
        self.c.acroForm.checkbox(name=self.name(base), x=M, y=self.y - 12, size=11, buttonStyle="check",
                                 borderColor=MUTED, fillColor=white, textColor=REC, borderWidth=0.8,
                                 tooltip=text[:60])
        p.drawOn(self.c, M + 18, self.y - p.wrap(tw, H)[1])
        if field_ph:
            self.field(field_ph, M + COLW - field_w, self.y - 14, field_w, 15)
        self.y -= h + 6

    def field(self, tip, x, y, w, h=15, multiline=False, base="field"):
        self.c.acroForm.textfield(name=self.name(base), tooltip=tip, x=x, y=y, width=w, height=h,
                                  borderColor=LINE, fillColor=FIELD, textColor=INK, borderWidth=0.6,
                                  fontName="Helvetica", fontSize=9 if not multiline else 9,
                                  fieldFlags="multiline" if multiline else "")

    def labelled_fields(self, items):
        """a row of label-over-field pairs: [(label, width), ...]"""
        self.need(34)
        x = M
        for label, w in items:
            self.c.setFont("Body", 8); self.c.setFillColor(MUTED); self.c.drawString(x, self.y - 9, label)
            self.field(label, x, self.y - 28, w, 16)
            x += w + 10
        self.y -= 36


s = Sheet("TEST-fastcard.pdf")
c = s.c

# ---------------------------------------------------------------- header
c.setFillColor(REC); c.setFont("Mono", 8)
c.drawString(M, s.y - 8, "RAW   EOS 6D MARK II   FIRMWARE 1.1.1   MAGIC LANTERN v76   ~1 HOUR")
s.y -= 16
s.para("6D2 Fast-Card Test", h1, gap=6)
s.para("Thanks for testing. The goal is to find which raw video formats a fast SD card can record "
       "<b>continuously</b> on the 6D Mark II with this build, and whether long takes are stable. "
       "Tick the boxes as you go and fill in the results; this form saves in any PDF reader that can fill forms "
       "(Acrobat Reader, Preview, Edge, Chrome). Nothing here touches Canon settings or factory data.")
s.para("<b>Firmware 1.1.1 only.</b> If the camera ever locks up, pull the battery and keep any "
       + code("CRASH*.LOG") + " / " + code("ASSERT*.LOG") + " from the card root.", gap=2)

s.section("00", "What you need")
s.check("A 6D Mark II on firmware 1.1.1, and the fast card (details below).", "need")
s.check("A lens with an <b>AF/MF switch</b>, set to <b>MF</b>. With the lens on AF, Canon refuses the 5x zoom "
        "that the bit-depth switch and crop mode use.", "need")
s.check("About <b>70 GB free</b> for the ladder. The stability takes come after a fresh format.", "need")
s.check("A computer with Python 3, for " + code("cardcheck.py") + " (it's in the package).", "need")
s.labelled_fields([("Card brand and model", 210), ("Size (GB)", 70), ("Speed class", 90), ("Camera serial (optional)", 104)])

s.section("01", "Install")
s.check("<b>ML already on this camera:</b> copy " + code("autoexec.bin") + " and the " + code("ML/") +
        " folder from the zip to the card root, replacing the old ones.", "install", field_ph=None)
s.check("<b>Or, first time on this camera:</b> format the card in the camera, copy " + code("autoexec.bin") + ", "
        + code("ML/") + " and " + code("ML-SETUP.FIR") + " (all in the zip) to the card root, then Wrench menu "
        "→ Firmware → update from the card, and restart.", "install")
s.check("The <b>Trash</b> button opens ML. ML → Modules: enable " + code("mlv_lite") + " and " + code("bench") +
        ", then restart.", "install")
s.check("Camera: Movie mode, <b>FHD 23.98p</b> (NTSC), M exposure, ISO 100–800, shutter 1/48, lens on <b>MF</b>.", "install")
s.para("On a Mac, run the checker from Terminal and avoid browsing the card in Finder: Finder's " + code("._") +
       " files corrupted this card's directory once.", small)

s.section("02", "Card speed")
s.check("ML → Debug → Benchmarks → Card Benchmarks → <b>Quick R/W benchmark (1 min)</b>.", "bench")
s.labelled_fields([("Write (MB/s)", 100), ("Read (MB/s)", 100)])
s.para("Rule of thumb: a format runs continuously when its MB/s is at most about 90 % of the write speed. "
       "Mark the ladder rows you expect to fit before you start, if you like.", small)

s.section("03", "Setting a format")
s.para("Movie tab → <b>RAW video</b> (the first entry) → SET.")
for t in [
    "<b>Resolution</b>: the 1080p list goes up to 2048; in crop mode it goes up to 3072. Each remembers its own pick. "
    "The line shows the exact size that will record: check it matches the table.",
    "<b>Aspect ratio</b>: <b>1.90:1 (DCI)</b> is at the end of the list. Inside it, <b>If too tall</b> is "
    "<i>Shrink width</i> (default) or <i>Clamp height</i> (the crop rows use Clamp height).",
    "<b>Data format</b>: after changing it, <b>close the ML menu</b>. The view zooms in and out once. Reopen it: the "
    "entry reads <b>live</b>. If it reads <i>at 14</i> with \"Lens AF blocks the zoom\", set the lens to MF and close "
    "the menu again (Canon's MENU and back also works).",
    "<b>Crop mode</b> → <i>1:1 (5x zoom)</i>: close the menu and the view goes to 5x. Crop always records at 29.97 fps.",
]:
    s.para("•  " + t, indent=8)

# ---------------------------------------------------------------- ladder
s.section("04", "Data-rate ladder")
s.para("Record each row for <b>up to 60 s</b>. Tick <b>cont</b> if it ran the full minute, otherwise write the second "
       "it stopped by itself. Within a block, once two rows in a row stop early, skip the rest of that block.")
rows = [
    "1080p - 23.976 fps",
    ("A", "1920", "2.39:1", "Shrink width", "12-bit", "1920×804", "55.5"),
    ("B", "2048", "2.39:1", "Shrink width", "12-bit", "2048×856", "63.0"),
    ("C", "2048", "1.90:1 (DCI)", "Shrink width", "10-bit", "2048×1080", "66.3"),
    ("D", "2048", "16:9", "Shrink width", "10-bit", "2048×1152", "70.7"),
    ("E", "1920", "16:9", "Shrink width", "12-bit", "1920×1080", "74.6"),
    ("F", "2048", "1.90:1 (DCI)", "Shrink width", "12-bit", "2048×1080", "79.5"),
    ("G", "2048", "16:9", "Shrink width", "12-bit", "2048×1152", "84.8"),
    ("H", "1920", "16:9", "Shrink width", "14-bit", "1920×1080", "87.0"),
    ("I", "2048", "1.90:1 (DCI)", "Shrink width", "14-bit", "2048×1080", "92.8"),
    "Crop mode - 29.97 fps, lens on MF",
    ("J", "1920", "2:1", "Clamp height", "10-bit", "1920×926", "66.6"),
    ("K", "1920", "2:1", "Clamp height", "12-bit", "1920×926", "79.9"),
    ("L", "2560", "2:1", "Clamp height", "10-bit", "2560×926", "88.8"),
    ("M", "3072", "2:1", "Clamp height", "10-bit", "3072×926", "106.6"),
]
cols = [("Row", 26), ("Res.", 34), ("Aspect", 66), ("If too tall", 70), ("Format", 42), ("Records", 64),
        ("MB/s", 38), ("cont", 32), ("stopped at (s)", 132)]
RH = 19
s.need(RH * 4)
x = M
c.setFont("Body-Bold", 7.5); c.setFillColor(MUTED)
for name, w in cols:
    if name == "MB/s":
        c.drawRightString(x + w - 6, s.y - 11, name.upper())
    else:
        c.drawString(x + 4, s.y - 11, name.upper())
    x += w
s.y -= 16
c.setStrokeColor(LINE); c.line(M, s.y, W - M, s.y)
for r in rows:
    if isinstance(r, str):
        s.need(RH * 2)
        c.setFillColor(SHADE); c.rect(M, s.y - RH + 3, COLW, RH - 3, stroke=0, fill=1)
        c.setFillColor(MUTED); c.setFont("Body-Bold", 8); c.drawString(M + 4, s.y - 12, r.upper())
        s.y -= RH
        continue
    s.need(RH)
    x = M; base = s.y - 13
    for i, (val, (name, w)) in enumerate(zip(r, cols)):
        if i == 0:
            c.setFont("Body-Bold", 11); c.setFillColor(REC); c.drawString(x + 6, base - 1, val)
        elif name == "MB/s":
            c.setFont("Mono", 9); c.setFillColor(INK); c.drawRightString(x + w - 6, base, val)
        else:
            c.setFont("Mono" if name in ("Res.", "Records") else "Body", 8.5); c.setFillColor(INK)
            c.drawString(x + 4, base, val)
        x += w
    c.acroForm.checkbox(name="ladder_%s_cont" % r[0], x=x + 10, y=s.y - 15, size=11, buttonStyle="check",
                        borderColor=MUTED, fillColor=white, textColor=REC, borderWidth=0.8,
                        tooltip="Row %s ran 60 s" % r[0])
    x += cols[-2][1]
    s.field("Row %s stopped at (seconds)" % r[0], x + 4, s.y - 16, cols[-1][1] - 8, 14, base="ladder_%s_stopped" % r[0])
    s.y -= RH
    c.setStrokeColor(LINE); c.setLineWidth(0.4); c.line(M, s.y + 1, W - M, s.y + 1)
s.y -= 6
s.check("<b>Then run the checker</b> (section 07) before formatting the card for section 05.", "ladder")

# ---------------------------------------------------------------- stability, audio, checker
s.section("05", "Stability")
s.check("Format the card in the camera.", "stab")
s.check("Fastest <b>1080p</b> row that ran cont: one <b>10-minute</b> take (written as .MLV, .M00, .M01 ... - keep them together).",
        "stab", field_ph="Row and result")
s.check("Same row: <b>five 30 s takes back to back</b>, each started within 3 s of the previous stop.",
        "stab", field_ph="Result")
s.check("Fastest <b>crop</b> row that ran cont: one <b>2-minute</b> take.", "stab", field_ph="Row and result")

s.section("06", "Scratch audio")
s.check("Movie tab → <b>Scratch audio</b> (the last entry) → Mode <b>Auto with RAW</b>. Record row A for about 20 s, "
        "clapping once near the start and once near the end.", "audio")
s.check("A .WAV named after the clip appeared in " + code("DCIM/AUDIO") + ", and nothing new in the card root.",
        "audio", field_ph="Notes")

s.section("07", "Run the checker")
s.para("With the card in the computer: " + code("python cardcheck.py E:\\") + "   (macOS: " +
       code("python3 cardcheck.py /Volumes/EOS_DIGITAL") + "). It only reads the card and saves "
       + code("cardcheck-report-&lt;date&gt;-&lt;time&gt;.txt") + " in the current folder.")
s.check("Ran it after the ladder (section 04).", "checker")
s.check("Ran it again at the end.", "checker")

s.section("08", "Send back")
s.para("This filled-in PDF, both " + code("cardcheck-report-*.txt") + " files, and any " + code("CRASH*") + " / " +
       code("ASSERT*") + " logs. Clips are only needed if a report lists a <b>PROBLEM</b> for them.")
s.need(110)
c.setFont("Body", 8); c.setFillColor(MUTED)
c.drawString(M, s.y - 9, "Anything odd: warnings on screen, takes that stopped on their own, an overheating icon")
s.field("Anything odd", M, s.y - 92, COLW, 78, multiline=True, base="notes")
s.y -= 100

c.save()
print("pages:", s.page, "fields:", s.n)
