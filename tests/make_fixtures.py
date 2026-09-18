"""Generate the .docx conformance corpus.

The documents are built here rather than committed, so the repository carries no
binary Office files and the corpus is reproducible from source. Each .docx gets
a matching .expect listing what the converted RTF must and must not contain;
`OpenNote.exe --docx-check <dir>` reads both and reports a pass/fail count.

Usage:
    py tests/make_fixtures.py build/corpus
"""
import os
import sys
import zipfile

W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"

CONTENT_TYPES = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
  <Override PartName="/{target}" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
</Types>"""

RELS = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="{target}"/>
</Relationships>"""


def p(runs, props=""):
    return f"<w:p>{props}{''.join(runs)}</w:p>"


def r(text, props=""):
    return f'<w:r>{props}<w:t xml:space="preserve">{text}</w:t></w:r>'


def document(body):
    return (
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'
        f'<w:document xmlns:w="{W}"><w:body>'
        + "".join(body)
        + '<w:sectPr><w:pgSz w:w="12240" w:h="15840"/></w:sectPr>'
        "</w:body></w:document>"
    )


def write(outdir, name, body, expect, target="word/document.xml"):
    path = os.path.join(outdir, name + ".docx")
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", CONTENT_TYPES.format(target=target))
        z.writestr("_rels/.rels", RELS.format(target=target))
        z.writestr(target, document(body))
    with open(os.path.join(outdir, name + ".expect"), "w", encoding="utf-8") as f:
        f.write("\n".join(expect) + "\n")
    return path


# --------------------------------------------------------------------------
# formatting: the character and paragraph properties people actually use
# --------------------------------------------------------------------------
def fixture_formatting(outdir):
    body = [
        p([r("A Heading")], '<w:pPr><w:pStyle w:val="Heading1"/></w:pPr>'),
        p([
            r("Plain, "),
            r("bold", "<w:rPr><w:b/></w:rPr>"),
            r(", "),
            r("italic", "<w:rPr><w:i/></w:rPr>"),
            r(", "),
            r("underline", '<w:rPr><w:u w:val="single"/></w:rPr>'),
            r(", "),
            r("struck", "<w:rPr><w:strike/></w:rPr>"),
            r(", "),
            r("coloured", '<w:rPr><w:color w:val="C81E1E"/></w:rPr>'),
            r(", "),
            r("big", '<w:rPr><w:sz w:val="36"/></w:rPr>'),
            r(", "),
            r("Courier", '<w:rPr><w:rFonts w:ascii="Courier New" w:hAnsi="Courier New"/></w:rPr>'),
            r("."),
        ]),
        p([r("Centred")], '<w:pPr><w:jc w:val="center"/></w:pPr>'),
        p([r("Righted")], '<w:pPr><w:jc w:val="right"/></w:pPr>'),
        p([r("Justified")], '<w:pPr><w:jc w:val="both"/></w:pPr>'),
        p([r("Indented")], '<w:pPr><w:ind w:left="720"/></w:pPr>'),
        p([r("Listed")],
          '<w:pPr><w:numPr><w:ilvl w:val="0"/><w:numId w:val="1"/></w:numPr></w:pPr>'),
        p([r("Super"), r("script", '<w:rPr><w:vertAlign w:val="superscript"/></w:rPr>'),
           r(" and sub"), r("script", '<w:rPr><w:vertAlign w:val="subscript"/></w:rPr>')]),
        p([r("Before"), "<w:r><w:tab/></w:r>", r("after")]),
    ]
    expect = [
        "# character formatting must reach the RTF",
        "contains:\\b",
        "contains:\\i",
        "contains:\\ul",
        "contains:\\strike",
        "contains:\\super",
        "contains:\\sub",
        "contains:\\tab",
        "contains:Courier New",
        "contains:\\red200\\green30\\blue30",
        "# a Heading1 style must survive into the run, not be lost when it opens",
        "contains:\\b\\fs36 A Heading",
        "# paragraph formatting",
        "contains:\\qc",
        "contains:\\qr",
        "contains:\\qj",
        "contains:\\li720",
        "# a list paragraph gets a hanging indent and a bullet",
        "contains:\\fi-360",
        "contains:pnlvlblt",
    ]
    return write(outdir, "formatting", body, expect)


# --------------------------------------------------------------------------
# tables: the grid must drive \cellx, and cells must not gain a blank line
# --------------------------------------------------------------------------
def fixture_table(outdir):
    grid = '<w:tblGrid><w:gridCol w:w="3000"/><w:gridCol w:w="2400"/><w:gridCol w:w="1600"/></w:tblGrid>'
    rows = [("Product", "Status", "Cost"),
            ("WordPad", "Removed", "n/a"),
            ("OpenNote", "Shipping", "Free")]
    trs = []
    for row in rows:
        tcs = "".join(f"<w:tc>{p([r(c)])}</w:tc>" for c in row)
        trs.append(f"<w:tr>{tcs}</w:tr>")
    body = [
        p([r("Before the table.")]),
        f"<w:tbl>{grid}{''.join(trs)}</w:tbl>",
        p([r("After the table.")]),
    ]
    expect = [
        "# cell edges come from w:tblGrid, cumulative, before the row content",
        "contains:\\cellx3000\\cellx5400\\cellx7000",
        "# cell paragraphs are marked in-table and terminated by \\cell",
        "contains:\\intbl",
        "contains:Product\\cell",
        "contains:\\row",
        "# a cell's last paragraph must not carry a paragraph mark as well",
        "absent:Product\\par",
        "contains:After the table.",
    ]
    return write(outdir, "table", body, expect)


# --------------------------------------------------------------------------
# revisions: a tracked deletion is history, not document text
# --------------------------------------------------------------------------
def fixture_revisions(outdir):
    body = [
        p([
            r("Kept before. "),
            '<w:del w:id="1" w:author="a"><w:r><w:delText>REMOVEDTEXT</w:delText></w:r></w:del>',
            r("Kept after."),
        ]),
        p([
            '<w:ins w:id="2" w:author="a"><w:r><w:t xml:space="preserve">INSERTEDTEXT</w:t></w:r></w:ins>',
        ]),
    ]
    expect = [
        "contains:Kept before.",
        "contains:Kept after.",
        "# deleted content must not appear in the document",
        "absent:REMOVEDTEXT",
        "# an accepted insertion is part of the document",
        "contains:INSERTEDTEXT",
    ]
    return write(outdir, "revisions", body, expect)


# --------------------------------------------------------------------------
# escaping: RTF control characters and non-ASCII
# --------------------------------------------------------------------------
def fixture_escaping(outdir):
    body = [
        p([r("Entities: &amp; &lt; &gt; &quot; &apos;")]),
        p([r("Braces and slash: { } \\")]),
        p([r("Unicode: em dash — café naïve 中文")]),
    ]
    expect = [
        "# RTF control characters must be escaped, not passed through",
        "contains:\\{",
        "contains:\\}",
        "contains:\\\\",
        "# non-ASCII goes out as \\uN with a fallback character",
        "contains:\\u8212?",
        "contains:\\u233?",
        "contains:\\u20013?",
        "# XML entities are decoded by the parser and must arrive as characters",
        "contains:Entities: & < > \" '",
    ]
    return write(outdir, "escaping", body, expect)


# --------------------------------------------------------------------------
# relocated: the main part is found via the relationship, not a fixed path
# --------------------------------------------------------------------------
def fixture_relocated(outdir):
    body = [p([r("Found through the package relationship.")])]
    expect = [
        "# the main part is not at /word/document.xml in this package",
        "contains:Found through the package relationship.",
    ]
    return write(outdir, "relocated", body, expect,
                 target="word/document2.xml")


def main():
    outdir = sys.argv[1] if len(sys.argv) > 1 else "build/corpus"
    os.makedirs(outdir, exist_ok=True)

    made = [
        fixture_formatting(outdir),
        fixture_table(outdir),
        fixture_revisions(outdir),
        fixture_escaping(outdir),
        fixture_relocated(outdir),
    ]
    for path in made:
        print(f"  {os.path.basename(path)}  {os.path.getsize(path)} bytes")
    print(f"{len(made)} documents in {outdir}")


if __name__ == "__main__":
    main()
