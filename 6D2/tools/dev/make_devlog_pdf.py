"""6D2/docs/<doc>.md -> <doc>.pdf next to it, via headless Edge (Windows).
Usage: python make_devlog_pdf.py [devlog|findings]   (default devlog; needs the markdown package)"""
import os, re, subprocess, sys, markdown

HERE = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "docs"))
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
REPO_URL = "https://github.com/charlie-schomp/ML-Canon6DmkII"

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
a { color: #1f5fbf; }
code { font-family: Consolas, "Courier New", monospace; font-size: 8.8pt; background: #f1f3f6;
       padding: 0 2pt; border-radius: 2px; }
pre { background: #f1f3f6; padding: 6pt 8pt; border-radius: 3px; page-break-inside: avoid; }
pre code { background: none; padding: 0; }
blockquote { margin: 8pt 0; padding: 6pt 10pt; border-left: 3px solid #1f5fbf; background: #f3f6fb;
             color: #1b1f24; page-break-inside: avoid; }
blockquote p { margin: 0; }
table { border-collapse: collapse; width: 100%; margin: 6pt 0 10pt 0; font-size: 8.9pt;
        page-break-inside: auto; }
tr { page-break-inside: avoid; }
th { background: #e8eef8; color: #0f2f63; text-align: left; font-weight: 600; }
th, td { border: 1px solid #c9d2de; padding: 3pt 5pt; vertical-align: top; }
img { width: 100%; margin: 6pt 0 10pt 0; page-break-inside: avoid; }
hr { border: none; border-top: 1px solid #d5dbe3; margin: 14pt 0; }
strong { color: #111; }
"""

DOCS = {
    "devlog": {
        "title": "6D Mark II ML development log",
        "breaks": ['<h2>Part 2', '<h2>Part 3', '<h2>Part 4', '<h2>Part 5', '<h2>Part 6',
                   '<h2>Part 7', '<h2>Appendix A'],
        "css": "td:first-child { white-space: nowrap; }",
    },
    "findings": {
        "title": "DIGIC 7 findings from the 6D Mark II port",
        "breaks": ['<h2>3. New on DIGIC 7'],
        "css": "table:first-of-type td:first-child { width: 24%; }",
    },
}


def main():
    name = sys.argv[1] if len(sys.argv) > 1 else "devlog"
    doc = DOCS[name]
    md = open(os.path.join(HERE, name + ".md"), encoding="utf-8").read()
    # the PDF travels without the repository: point relative .md links at GitHub
    md = re.sub(r"\]\(([\w.-]+\.md)\)", r"](%s/blob/main/6D2/docs/\1)" % REPO_URL, md)
    body = markdown.markdown(md, extensions=["tables", "fenced_code", "sane_lists"])
    # tables written with an empty header row ("| | |") get no header at all
    body = re.sub(r"<thead>\s*<tr>(\s*<th>\s*</th>)+\s*</tr>\s*</thead>", "", body)
    for marker in doc["breaks"]:
        body = body.replace(marker, marker.replace('<h2>', '<h2 class="part">'), 1)
    html = ('<!doctype html><html><head><meta charset="utf-8"><title>%s</title>'
            '<style>%s%s</style></head><body>%s</body></html>'
            % (doc["title"], CSS, doc["css"], body))
    out_html = os.path.join(HERE, name + ".html")
    open(out_html, "w", encoding="utf-8").write(html)
    out_pdf = os.path.join(HERE, name + ".pdf")
    subprocess.run([EDGE, "--headless", "--disable-gpu", "--no-pdf-header-footer",
                    "--print-to-pdf=" + out_pdf, "file:///" + out_html.replace("\\", "/")],
                   check=False, capture_output=True, timeout=120)
    os.remove(out_html)   # only needed next to the charts while Edge prints it
    print("pdf:", out_pdf, os.path.getsize(out_pdf) if os.path.exists(out_pdf) else "MISSING")


if __name__ == "__main__":
    main()
