#!/usr/bin/env python3
"""Render docs/laboratory-report.md to docs/laboratory-report.pdf.

The PDF is a submission artefact, so it must never be edited by hand: it is
generated from the Markdown, which is the single source of truth. Run this
script after any change to the report and commit both files together.

Requires WeasyPrint and the `markdown` package:

    pip install weasyprint markdown

Usage:
    python3 tools/generate_report_pdf.py [--check]

`--check` regenerates into a temporary file and fails (exit 1) if it differs
from the committed PDF, which is what CI and tools/verify/run_all.sh use.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
SOURCE = DOCS / "laboratory-report.md"
TARGET = DOCS / "laboratory-report.pdf"

CSS = """
@page {
  size: A4;
  margin: 20mm 18mm 18mm 18mm;
  @bottom-center {
    content: counter(page) " of " counter(pages);
    font-family: "Liberation Sans", sans-serif;
    font-size: 8pt;
    color: #666;
  }
}
@page :first {
  @bottom-center { content: ""; }
}

html { font-size: 10pt; }
body {
  font-family: "Liberation Serif", serif;
  line-height: 1.45;
  color: #111;
  margin: 0;
}

.cover {
  text-align: center;
  margin-top: 35mm;
  margin-bottom: 18mm;
}
.cover h1 {
  font-family: "Liberation Sans", sans-serif;
  font-size: 20pt;
  margin: 0 0 4mm 0;
  border: none;
  color: #111;
}
.cover .subtitle {
  font-size: 14pt;
  font-style: italic;
  margin-bottom: 10mm;
}
.cover .author { font-size: 12pt; margin-bottom: 1mm; }
.cover .institution { font-size: 10.5pt; margin-bottom: 10mm; }
.cover .course { font-size: 10.5pt; }

h1, h2, h3, h4 {
  font-family: "Liberation Sans", sans-serif;
  color: #111;
  line-height: 1.25;
}
h1 {
  font-size: 15pt;
  border-bottom: 1.2pt solid #333;
  padding-bottom: 1.5mm;
  margin: 8mm 0 4mm 0;
  page-break-before: always;
}
h1.first { page-break-before: avoid; }
h2 { font-size: 12.5pt; margin: 6mm 0 2.5mm 0; }
h3 { font-size: 11pt; margin: 5mm 0 2mm 0; }
h4 { font-size: 10pt; margin: 4mm 0 1.5mm 0; }

p, li { orphans: 2; widows: 2; }
ul, ol { margin: 2mm 0 3mm 0; padding-left: 6mm; }
li { margin-bottom: 0.8mm; }

code {
  font-family: "Liberation Mono", monospace;
  font-size: 8.5pt;
  background: #f4f4f4;
  padding: 0 0.6mm;
  border-radius: 1pt;
}
pre {
  background: #f7f7f7;
  border: 0.5pt solid #ddd;
  border-left: 2pt solid #999;
  padding: 2.5mm 3mm;
  margin: 2.5mm 0 3.5mm 0;
  page-break-inside: avoid;
}
pre code {
  background: none;
  padding: 0;
  font-size: 8pt;
  line-height: 1.35;
}

table {
  width: 100%;
  border-collapse: collapse;
  margin: 3mm 0 4mm 0;
  font-size: 8.5pt;
  page-break-inside: avoid;
}
th, td {
  border: 0.5pt solid #bbb;
  padding: 1.4mm 2mm;
  text-align: left;
  vertical-align: top;
}
th {
  background: #ececec;
  font-family: "Liberation Sans", sans-serif;
  font-size: 8.5pt;
}
tr { page-break-inside: avoid; }

blockquote {
  margin: 3mm 0;
  padding: 2mm 3mm;
  border-left: 2pt solid #bbb;
  background: #fafafa;
  font-size: 9pt;
}
blockquote p { margin: 1.5mm 0; }

hr { border: none; border-top: 0.5pt solid #ccc; margin: 5mm 0; }

figure { margin: 4mm 0; text-align: center; page-break-inside: avoid; }
figure img { max-width: 100%; }
figcaption {
  font-size: 8.5pt;
  color: #444;
  margin-top: 1.5mm;
  text-align: left;
}

a { color: #14528c; text-decoration: none; }
"""


def parse_frontmatter(text: str) -> tuple[dict[str, str], str]:
    if not text.startswith("---\n"):
        return {}, text
    end = text.find("\n---\n", 4)
    if end == -1:
        return {}, text
    meta: dict[str, str] = {}
    for line in text[4:end].splitlines():
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        meta[key.strip()] = value.strip().strip('"')
    return meta, text[end + 5 :]


def strip_duplicate_header(meta: dict[str, str], body: str) -> str:
    """Drop the Markdown H1/table that restate the cover page metadata."""
    body = re.sub(r"\A#\s+[^\n]*\n", "", body, count=1)
    if "**Laboratory Report**" in body.split("\n\n---", 1)[0]:
        head, sep, tail = body.partition("\n---\n")
        if sep:
            body = tail
    return body.lstrip("\n")


def build_html(meta: dict[str, str], body: str) -> str:
    import markdown

    html_body = markdown.markdown(
        body,
        extensions=["tables", "fenced_code", "attr_list", "sane_lists"],
        output_format="html5",
    )
    # The first <h1> after the cover should not force a fresh page: the cover
    # already ended with a break, and a leading blank page looks like an error.
    html_body = html_body.replace("<h1>", '<h1 class="first">', 1)

    cover = f"""<section class="cover">
  <h1>{meta.get('title', '')}</h1>
  <div class="subtitle">{meta.get('subtitle', '')}</div>
  <div class="author">{meta.get('author', '')} &nbsp;|&nbsp; {meta.get('date', '')}</div>
  <div class="institution">{meta.get('department', '')}</div>
  <div class="course">{meta.get('institution', '')}<br>{meta.get('course', '')}</div>
</section>"""

    return f"""<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<title>{meta.get('title', 'Laboratory Report')}</title>
<style>{CSS}</style>
</head><body>
{cover}
{html_body}
</body></html>
"""


def render(target: Path) -> None:
    import weasyprint

    text = SOURCE.read_text(encoding="utf-8")
    meta, body = parse_frontmatter(text)
    html = build_html(meta, strip_duplicate_header(meta, body))
    weasyprint.HTML(string=html, base_url=str(DOCS)).write_pdf(str(target))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--check",
        action="store_true",
        help="fail if the committed PDF is out of date instead of rewriting it",
    )
    args = ap.parse_args()

    if not args.check:
        render(TARGET)
        print(f"[pdf] wrote {TARGET.relative_to(ROOT)}")
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        fresh = Path(tmp) / "laboratory-report.pdf"
        render(fresh)
        if not TARGET.exists():
            print("[pdf] FAIL: docs/laboratory-report.pdf is missing")
            return 1
        # PDF streams are not byte-reproducible across runs, so compare text.
        import subprocess

        def as_text(path: Path) -> str:
            out = subprocess.run(
                ["pdftotext", "-layout", str(path), "-"],
                capture_output=True,
                text=True,
                check=False,
            )
            return re.sub(r"\s+", " ", out.stdout).strip()

        if as_text(fresh) != as_text(TARGET):
            print("[pdf] FAIL: docs/laboratory-report.pdf is older than the Markdown")
            print("[pdf]       run: python3 tools/generate_report_pdf.py")
            return 1
    print("[pdf] OK: docs/laboratory-report.pdf matches the Markdown")
    return 0


if __name__ == "__main__":
    sys.exit(main())
