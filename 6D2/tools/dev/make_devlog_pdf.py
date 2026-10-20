"""6D2/docs/devlog.md -> devlog.pdf next to it, via headless Edge (Windows).
Usage: python make_devlog_pdf.py   (needs the markdown package)"""
import os, subprocess, markdown

HERE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs"))
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"

CSS = """
@page { size: Letter; margin: 18mm 17mm 18mm 17mm; }
body { font-family: "Segoe UI", Helvetica, Arial, sans-serif; font-size: 10pt; line-height: 1.45;
       color: #1b1f24; max-width: 100%; }
h1 { font-size: 21pt; margin: 0 0 4pt 0; color: #111; }
h1 + p { color: #444; margin-top: 2pt; }
h2 { font-size: 14.5pt; margin: 20pt 0 6pt 0; padding-bottom: 3pt; border-bottom: 2px solid #1f5fbf;
     color: #0f2f63; page-break-after: avoid; }
h2.part { page-break-before: always; }
h3 { font-size: 11.5pt; margin: 14pt 0 4pt 0; color: #1f5fbf; page-break-after: avoid; }
p, li { orphans: 3; widows: 3; }
ul, ol { padding-left: 16pt; margin: 4pt 0; }
li { margin: 2pt 0; }
code { font-family: Consolas, "Courier New", monospace; font-size: 8.8pt; background: #f1f3f6;
       padding: 0 2pt; border-radius: 2px; }
pre { background: #f1f3f6; padding: 6pt 8pt; border-radius: 3px; page-break-inside: avoid; }
pre code { background: none; padding: 0; }
table { border-collapse: collapse; width: 100%; margin: 6pt 0 10pt 0; font-size: 8.9pt;
        page-break-inside: auto; }
tr { page-break-inside: avoid; }
th { background: #e8eef8; color: #0f2f63; text-align: left; font-weight: 600; }
th, td { border: 1px solid #c9d2de; padding: 3pt 5pt; vertical-align: top; }
td:first-child { white-space: nowrap; }
img { width: 100%; margin: 6pt 0 10pt 0; page-break-inside: avoid; }
hr { border: none; border-top: 1px solid #d5dbe3; margin: 14pt 0; }
strong { color: #111; }
"""


def main():
    md = open(os.path.join(HERE, "devlog.md"), encoding="utf-8").read()
    body = markdown.markdown(md, extensions=["tables", "fenced_code", "sane_lists"])
    # tables written with an empty header row ("| | |") get no header at all
    import re
    body = re.sub(r"<thead>\s*<tr>(\s*<th>\s*</th>)+\s*</tr>\s*</thead>", "", body)
    # start each Part and the first appendix on a new page
    for marker in ('<h2>Part 2', '<h2>Part 3', '<h2>Part 4', '<h2>Part 5', '<h2>Part 6', '<h2>Part 7', '<h2>Appendix A'):
        body = body.replace(marker, marker.replace('<h2>', '<h2 class="part">'), 1)
    html = ('<!doctype html><html><head><meta charset="utf-8">'
            '<title>6D Mark II ML development log</title><style>%s</style></head><body>%s</body></html>'
            % (CSS, body))
    out_html = os.path.join(HERE, "devlog.html")
    open(out_html, "w", encoding="utf-8").write(html)
    out_pdf = os.path.join(HERE, "devlog.pdf")
    subprocess.run([EDGE, "--headless", "--disable-gpu", "--no-pdf-header-footer",
                    "--print-to-pdf=" + out_pdf, "file:///" + out_html.replace("\\", "/")],
                   check=False, capture_output=True, timeout=120)
    os.remove(out_html)   # only needed next to the charts while Edge prints it
    print("pdf:", out_pdf, os.path.getsize(out_pdf) if os.path.exists(out_pdf) else "MISSING")


if __name__ == "__main__":
    main()
