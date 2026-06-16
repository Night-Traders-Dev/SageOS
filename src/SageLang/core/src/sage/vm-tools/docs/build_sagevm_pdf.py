#!/usr/bin/env python3
"""Generate professional SageVM Technical Reference PDF — systems developer style."""

from reportlab.lib import colors
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import cm
from reportlab.lib.enums import TA_LEFT, TA_CENTER, TA_JUSTIFY
from reportlab.platypus import (
    BaseDocTemplate, PageTemplate, Frame,
    Paragraph, Spacer, PageBreak,
    Table, TableStyle, HRFlowable, Preformatted,
    KeepTogether, NextPageTemplate,
)
from reportlab.lib.colors import HexColor, white

# ─── Palette ─────────────────────────────────────────────────────────────────
C_NAVY       = HexColor('#0b1c2c')
C_NAVY_MED   = HexColor('#1a3553')
C_CYAN       = HexColor('#00bcd4')
C_CYAN_DIM   = HexColor('#006e7f')
C_CODE_BG    = HexColor('#0d1117')
C_CODE_FG    = HexColor('#e6edf3')
C_TH_BG      = HexColor('#1a3553')
C_ALT_ROW    = HexColor('#f0f5fa')
C_BORDER     = HexColor('#c0cdd8')
C_DIVIDER    = HexColor('#dde6ee')
C_BODY       = HexColor('#1a202c')
C_MUTED      = HexColor('#718096')
C_EXEC_BG    = HexColor('#e8f4fd')
C_EXEC_LEFT  = HexColor('#0078c2')
C_ACCENT_BOX = HexColor('#f0fafb')
C_GREEN      = HexColor('#27ae60')
C_RED        = HexColor('#c0392b')

PAGE_W, PAGE_H = A4

LEFT    = 2.0 * cm
RIGHT   = 1.5 * cm
TOP     = 2.5 * cm
BOTTOM  = 2.2 * cm
CONTENT_W = PAGE_W - LEFT - RIGHT  # ~496 pts / ~17.5 cm


# ─── Page Drawing ────────────────────────────────────────────────────────────
def draw_content_page(canvas, doc):
    canvas.saveState()
    # ── Header bar ──
    canvas.setFillColor(C_NAVY)
    canvas.rect(0, PAGE_H - TOP + 0.15 * cm, PAGE_W, TOP - 0.15 * cm, fill=1, stroke=0)
    # Cyan rule under header
    canvas.setFillColor(C_CYAN)
    canvas.rect(0, PAGE_H - TOP + 0.12 * cm, PAGE_W, 0.05 * cm, fill=1, stroke=0)
    # Header text
    canvas.setFont('Courier-Bold', 7.5)
    canvas.setFillColor(C_CYAN)
    canvas.drawString(LEFT, PAGE_H - TOP + 0.65 * cm, 'SAGEVM TECHNICAL REFERENCE')
    canvas.setFont('Courier', 7.5)
    canvas.setFillColor(HexColor('#8bafc4'))
    canvas.drawRightString(PAGE_W - RIGHT, PAGE_H - TOP + 0.65 * cm,
                           'Night-Traders-Dev  ·  v0.9.7')
    # ── Footer ──
    canvas.setFillColor(C_DIVIDER)
    canvas.rect(LEFT, BOTTOM - 0.5 * cm, CONTENT_W, 0.03 * cm, fill=1, stroke=0)
    canvas.setFont('Courier', 7.5)
    canvas.setFillColor(C_MUTED)
    canvas.drawCentredString(PAGE_W / 2, BOTTOM - 0.42 * cm, f'— {doc.page} —')
    canvas.restoreState()


def draw_cover_page(canvas, doc):
    canvas.saveState()
    # Full dark background
    canvas.setFillColor(C_NAVY)
    canvas.rect(0, 0, PAGE_W, PAGE_H, fill=1, stroke=0)
    # Cyan accent stripe
    canvas.setFillColor(C_CYAN)
    canvas.rect(0, PAGE_H * 0.42 - 0.08 * cm, PAGE_W, 0.18 * cm, fill=1, stroke=0)
    # Corner accent
    canvas.setFillColor(C_CYAN)
    canvas.rect(0, PAGE_H - 1.5 * cm, 3.5 * cm, 0.06 * cm, fill=1, stroke=0)
    # Bottom strip
    canvas.setFillColor(C_NAVY_MED)
    canvas.rect(0, 0, PAGE_W, 1.8 * cm, fill=1, stroke=0)
    canvas.setFillColor(C_CYAN)
    canvas.rect(0, 1.8 * cm, PAGE_W, 0.04 * cm, fill=1, stroke=0)
    # Footer text
    canvas.setFont('Courier', 7.5)
    canvas.setFillColor(HexColor('#5d8aa8'))
    canvas.drawString(LEFT, 0.7 * cm, 'Night-Traders-Dev  ·  github.com/Night-Traders-Dev')
    canvas.drawRightString(PAGE_W - RIGHT, 0.7 * cm, 'SageVM Technical Reference')
    canvas.restoreState()


# ─── Styles ──────────────────────────────────────────────────────────────────
def make_styles():
    return {
        # Cover
        'cover_title': ParagraphStyle('cover_title',
            fontName='Courier-Bold', fontSize=44, textColor=white,
            leading=52, spaceAfter=2, alignment=TA_LEFT),
        'cover_sub': ParagraphStyle('cover_sub',
            fontName='Courier', fontSize=16, textColor=C_CYAN,
            spaceAfter=4, alignment=TA_LEFT),
        'cover_meta_k': ParagraphStyle('cover_meta_k',
            fontName='Courier-Bold', fontSize=8.5, textColor=HexColor('#5d8aa8'),
            alignment=TA_LEFT),
        'cover_meta_v': ParagraphStyle('cover_meta_v',
            fontName='Courier', fontSize=8.5, textColor=white, alignment=TA_LEFT),
        'cover_desc': ParagraphStyle('cover_desc',
            fontName='Helvetica', fontSize=10.5, textColor=HexColor('#a8c8e0'),
            leading=17, alignment=TA_LEFT),
        # Headings
        'h1': ParagraphStyle('h1',
            fontName='Courier-Bold', fontSize=15, textColor=C_NAVY,
            spaceBefore=4, spaceAfter=6, leading=20),
        'h2': ParagraphStyle('h2',
            fontName='Courier-Bold', fontSize=11, textColor=C_NAVY_MED,
            spaceBefore=12, spaceAfter=4, leading=15),
        'h3': ParagraphStyle('h3',
            fontName='Courier-Bold', fontSize=9.5, textColor=C_BODY,
            spaceBefore=8, spaceAfter=3, leading=13),
        # Body
        'body': ParagraphStyle('body',
            fontName='Helvetica', fontSize=9.5, textColor=C_BODY,
            alignment=TA_JUSTIFY, spaceBefore=3, spaceAfter=3, leading=15),
        'bullet': ParagraphStyle('bullet',
            fontName='Helvetica', fontSize=9.5, textColor=C_BODY,
            spaceBefore=2, spaceAfter=2, leading=14,
            leftIndent=16, firstLineIndent=-10),
        'numbered': ParagraphStyle('numbered',
            fontName='Helvetica', fontSize=9.5, textColor=C_BODY,
            spaceBefore=2, spaceAfter=2, leading=14,
            leftIndent=20, firstLineIndent=-14),
        # Code
        'code': ParagraphStyle('code',
            fontName='Courier', fontSize=7.8, textColor=C_CODE_FG,
            spaceBefore=0, spaceAfter=0, leading=11.5),
        # Tables
        'th': ParagraphStyle('th',
            fontName='Courier-Bold', fontSize=8, textColor=white, alignment=TA_LEFT),
        'td': ParagraphStyle('td',
            fontName='Courier', fontSize=8, textColor=C_BODY,
            alignment=TA_LEFT, leading=12),
        'td_mono': ParagraphStyle('td_mono',
            fontName='Courier', fontSize=7.8, textColor=C_NAVY_MED,
            alignment=TA_LEFT, leading=11.5),
        # Callout
        'exec_body': ParagraphStyle('exec_body',
            fontName='Helvetica', fontSize=10, textColor=HexColor('#0a3d62'),
            alignment=TA_JUSTIFY, leading=16),
        # Misc
        'caption': ParagraphStyle('caption',
            fontName='Helvetica-Oblique', fontSize=8, textColor=C_MUTED,
            alignment=TA_CENTER, spaceBefore=1, spaceAfter=5),
        'end_note': ParagraphStyle('end_note',
            fontName='Courier', fontSize=8, textColor=C_MUTED, alignment=TA_CENTER),
        'obs': ParagraphStyle('obs',
            fontName='Helvetica', fontSize=9, textColor=C_BODY,
            spaceBefore=2, spaceAfter=2, leading=13.5,
            leftIndent=16, firstLineIndent=-10),
    }


# ─── Helper Flowable Builders ────────────────────────────────────────────────
def h1(text, S):
    p = Paragraph(text, S['h1'])
    rule = HRFlowable(width='100%', thickness=2, color=C_CYAN,
                      spaceAfter=4, spaceBefore=2)
    return [Spacer(1, 10), p, rule]

def h2(text, S):
    return [Paragraph(text, S['h2'])]

def h3(text, S):
    return [Paragraph(text, S['h3'])]

def p(text, S):
    return [Paragraph(text, S['body'])]

def bullet(items, S):
    return [Paragraph(f'\u2022\u2002{item}', S['bullet']) for item in items]

def numbered(items, S):
    return [Paragraph(f'<b>{i}.</b>\u2003{item}', S['numbered'])
            for i, item in enumerate(items, 1)]

def sp(n=6):
    return [Spacer(1, n)]

def hr():
    return [Spacer(1, 8),
            HRFlowable(width='100%', thickness=0.5, color=C_DIVIDER),
            Spacer(1, 8)]

def code_block(text, S):
    lines = text.strip('\n').split('\n')
    # strip uniform leading indent
    nonempty = [l for l in lines if l.strip()]
    if nonempty:
        mi = min(len(l) - len(l.lstrip()) for l in nonempty)
        lines = [l[mi:] for l in lines]
    pre = Preformatted('\n'.join(lines), S['code'])
    tbl = Table([[pre]], colWidths=[CONTENT_W])
    tbl.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), C_CODE_BG),
        ('LEFTPADDING', (0, 0), (-1, -1), 10),
        ('RIGHTPADDING', (0, 0), (-1, -1), 10),
        ('TOPPADDING', (0, 0), (-1, -1), 8),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 8),
        ('BOX', (0, 0), (-1, -1), 0.5, HexColor('#30363d')),
        ('LINEABOVE', (0, 0), (-1, 0), 2, C_CYAN_DIM),
    ]))
    return [Spacer(1, 4), tbl, Spacer(1, 4)]

def data_table(headers, rows, col_widths, S, mono_cols=None):
    mono_cols = mono_cols or []
    data = [[Paragraph(h, S['th']) for h in headers]]
    for row in rows:
        data.append([
            Paragraph(str(c), S['td_mono'] if i in mono_cols else S['td'])
            for i, c in enumerate(row)
        ])
    cmds = [
        ('BACKGROUND', (0, 0), (-1, 0), C_TH_BG),
        ('GRID', (0, 0), (-1, -1), 0.5, C_BORDER),
        ('LEFTPADDING', (0, 0), (-1, -1), 7),
        ('RIGHTPADDING', (0, 0), (-1, -1), 7),
        ('TOPPADDING', (0, 0), (-1, -1), 5),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 5),
        ('VALIGN', (0, 0), (-1, -1), 'TOP'),
        ('LINEBELOW', (0, 0), (-1, 0), 1.5, C_CYAN_DIM),
    ]
    for i in range(1, len(data)):
        if i % 2 == 0:
            cmds.append(('BACKGROUND', (0, i), (-1, i), C_ALT_ROW))
    tbl = Table(data, colWidths=col_widths)
    tbl.setStyle(TableStyle(cmds))
    return [Spacer(1, 4), tbl, Spacer(1, 6)]

def exec_box(text, S):
    p = Paragraph(text, S['exec_body'])
    tbl = Table([[p]], colWidths=[CONTENT_W])
    tbl.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), C_EXEC_BG),
        ('LEFTPADDING', (0, 0), (-1, -1), 14),
        ('RIGHTPADDING', (0, 0), (-1, -1), 14),
        ('TOPPADDING', (0, 0), (-1, -1), 12),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 12),
        ('LINEBEFORE', (0, 0), (0, -1), 4, C_EXEC_LEFT),
        ('BOX', (0, 0), (-1, -1), 0.5, C_BORDER),
    ]))
    return [Spacer(1, 4), tbl, Spacer(1, 8)]

def note_box(text, S, color=None):
    color = color or C_ACCENT_BOX
    p = Paragraph(text, ParagraphStyle('nb', fontName='Helvetica', fontSize=9,
                                        textColor=C_BODY, leading=14))
    tbl = Table([[p]], colWidths=[CONTENT_W])
    tbl.setStyle(TableStyle([
        ('BACKGROUND', (0, 0), (-1, -1), color),
        ('LEFTPADDING', (0, 0), (-1, -1), 10),
        ('RIGHTPADDING', (0, 0), (-1, -1), 10),
        ('TOPPADDING', (0, 0), (-1, -1), 8),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 8),
        ('LINEBEFORE', (0, 0), (0, -1), 3, C_CYAN),
        ('BOX', (0, 0), (-1, -1), 0.5, C_BORDER),
    ]))
    return [Spacer(1, 4), tbl, Spacer(1, 4)]

# short tag helpers
def mc(t): return f'<font face="Courier">{t}</font>'
def mb(t): return f'<b>{t}</b>'


# ─── Main Build ───────────────────────────────────────────────────────────────
def build():
    OUTPUT = '/mnt/user-data/outputs/SageVM_Technical_Reference.pdf'

    doc = BaseDocTemplate(
        OUTPUT,
        pagesize=A4,
        leftMargin=LEFT, rightMargin=RIGHT,
        topMargin=TOP, bottomMargin=BOTTOM,
        title='SageVM Technical Reference',
        author='Night-Traders-Dev',
        subject='SageVM Bytecode Compiler and Interpreter — v0.9.7',
        creator='SageLang Documentation Pipeline',
    )

    content_frame = Frame(LEFT, BOTTOM, CONTENT_W, PAGE_H - TOP - BOTTOM, id='body')

    cover_tmpl   = PageTemplate(id='Cover',   frames=[content_frame], onPage=draw_cover_page)
    content_tmpl = PageTemplate(id='Content', frames=[content_frame], onPage=draw_content_page)
    doc.addPageTemplates([cover_tmpl, content_tmpl])

    S = make_styles()
    story = []

    # ════════════════════════════════════════════════════════════════════════
    # COVER PAGE
    # ════════════════════════════════════════════════════════════════════════
    story.append(NextPageTemplate('Cover'))
    story += [
        Spacer(1, PAGE_H * 0.12),
        Paragraph('SAGEVM', S['cover_title']),
        Paragraph('Technical Reference', S['cover_sub']),
        Spacer(1, 0.3 * cm),
        HRFlowable(width='100%', thickness=0.5, color=HexColor('#30607a'), spaceAfter=12),
    ]

    meta = [
        ('Version',         'v0.9.7'),
        ('Organization',    'Night-Traders-Dev'),
        ('Primary Language','SageLang / C'),
        ('Document Type',   'Internal Technical Reference'),
        ('Status',          'Active Development'),
    ]
    meta_tbl = Table(
        [[Paragraph(k, S['cover_meta_k']), Paragraph(v, S['cover_meta_v'])]
         for k, v in meta],
        colWidths=[3.8 * cm, CONTENT_W - 3.8 * cm],
    )
    meta_tbl.setStyle(TableStyle([
        ('LEFTPADDING',   (0, 0), (-1, -1), 0),
        ('RIGHTPADDING',  (0, 0), (-1, -1), 6),
        ('TOPPADDING',    (0, 0), (-1, -1), 4),
        ('BOTTOMPADDING', (0, 0), (-1, -1), 4),
        ('LINEBELOW',     (0, 0), (-1, -2), 0.5, HexColor('#1e3a54')),
    ]))
    story.append(meta_tbl)
    story += [
        Spacer(1, 1.6 * cm),
        Paragraph(
            'Pure-SageLang implementation of the Sage General Virtual Machine (SGVM) toolchain. '
            'Comprising a bytecode compiler and interpreter, SageVM enables self-hosted, portable '
            'Sage bytecode execution across all platforms where the Sage interpreter is available. '
            'Serves as both the reference implementation and bootstrap tool for SageLang\'s '
            'broader platform support strategy.',
            S['cover_desc']
        ),
    ]
    story.append(PageBreak())

    # ════════════════════════════════════════════════════════════════════════
    # CONTENT PAGES
    # ════════════════════════════════════════════════════════════════════════
    story.append(NextPageTemplate('Content'))

    # ── Executive Summary ────────────────────────────────────────────────────
    story += h1('Executive Summary', S)
    story += exec_box(
        'SageVM is a pure-SageLang implementation of the Sage General Virtual Machine (SGVM) '
        'toolchain, comprising a bytecode compiler (<b>sgvmc</b>) and interpreter (<b>sgvm</b>). '
        'It represents a critical piece of the SageLang ecosystem: a self-hosted, portable VM '
        'that enables Sage bytecode to run in environments where only the Sage interpreter is '
        'available, serving as both a reference implementation and a bootstrap tool for the '
        "language's broader platform support.",
        S
    )

    # ── §1 Architecture ──────────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1('1.  Architecture &amp; Design Philosophy', S)

    story += h2('1.1  Position in the SageLang Stack', S)
    story += p(
        'SageVM sits at a specific layer in SageLang\'s multi-backend architecture, bridging the '
        'C-based frontend compiler and the pure-Sage runtime:', S)
    story += data_table(
        ['Layer', 'Component', 'Implementation'],
        [
            ['Source',        '.sage files',             'User code'],
            ['Frontend',      'Sage compiler (C)',        'Lexer, Parser, AST'],
            ['Intermediate',  '.svm output',             'Human-readable VM assembly'],
            ['Tooling',       'SageVM (sgvmc / sgvm)',   'Pure SageLang implementation'],
            ['Binary',        '.sgvm files',             'Serialized bytecode binaries'],
            ['Execution',     'MetalVM / SGVM',          'C-based VM or pure Sage interpreter'],
        ],
        col_widths=[3.0 * cm, 6.0 * cm, 8.5 * cm], S=S, mono_cols=[0, 1, 2]
    )
    story += p(
        'The repository\'s stated purpose is to allow "compilation and execution of SageLang '
        'bytecode in a pure SageLang environment" — meaning you can compile and run Sage bytecode '
        'using only Sage itself, without requiring the C runtime or MetalVM.', S)

    story += h2('1.2  Core Components', S)
    story += p('The repository contains three primary source modules:', S)
    story += numbered([
        f'{mc("sgvm_core.sage")} — Opcode definitions and shared utilities ({mc("SGVMUtils")} class)',
        f'{mc("sgvm_compiler.sage")} — The bytecode compiler/linker ({mc("SGVMCompiler")} class)',
        f'{mc("sgvm.sage")} — The interpreter/executor (stack-based VM)',
    ], S)
    story += p(f'Plus diagnostic variants ({mc("sgvm_compiler_debug.sage")}) and unified CLI tooling.', S)

    # ── §2 Bytecode Format ───────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1('2.  The Bytecode Format (.sgvm)', S)

    story += h2('2.1  Binary Structure', S)
    story += p(f'The {mc(".sgvm")} format is a custom binary serialization. '
               f'Header structure (from {mc("sgvm_compiler.sage")}):',  S)
    story += code_block(
        '[Shebang line]      (optional, e.g. #!/usr/bin/env sgvm\\n)\n'
        '"SGVM"              (4-byte magic)\n'
        '0x01 0x00           (version major/minor)\n'
        'function_count      (2 bytes, big-endian)\n'
        'constant_count      (2 bytes, big-endian)\n'
        '[Constants Pool]    (variable length)\n'
        '[Chunk Table]       (4-byte count, then chunks)',
        S
    )

    story += h2('2.2  Constant Pool', S)
    story += p('The constant pool supports two entry types:', S)
    story += bullet([
        f'{mb("Numbers")} ({mc("type=1")}): IEEE 754 double-precision, manually encoded via bit manipulation',
        f'{mb("Strings")} ({mc("type=3")}): Length-prefixed (2-byte BE) UTF-8 data',
    ], S)
    story += p(
        f'The compiler deduplicates constants using a {mc("const_map")} dictionary keyed by '
        f'{mc(chr(34)+"n"+chr(34))} + stringified number or {mc(chr(34)+"s"+chr(34))} + string content.',
        S)

    story += h2('2.3  Instruction Encoding', S)
    story += p('Instructions use a stack-machine architecture with variable-length operands:', S)
    story += data_table(
        ['Operand Size', 'Usage'],
        [
            ['1 byte',         'Small immediates (e.g., OP_CALL arg count, OP_DUP depth)'],
            ['2 bytes (BE16)', 'Constant indices, local indices, jump offsets'],
            ['4 bytes (BE32)', 'Chunk code lengths, chunk table entries'],
        ],
        col_widths=[4.5 * cm, 13.0 * cm], S=S, mono_cols=[0]
    )

    # ── §3 Compiler ──────────────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1(f'3.  The Compiler (sgvm_compiler.sage)', S)

    story += h2('3.1  Two-Pass Architecture', S)
    story += p('The compiler implements a classic two-pass assembler:', S)

    story += h3('Pass 1 — Symbol Resolution', S)
    story += bullet([
        f'Parses {mc(".svm")} intermediate format (generated by {mc("sage --emit-vm")})',
        'Counts functions and chunks; builds local-to-global constant mapping tables',
        f'Handles parameter name resolution (mapping parameter names to {mc("__argN")} slots)',
    ], S)

    story += h3('Pass 2 — Code Generation', S)
    story += bullet([
        'Emits binary header and serializes constant pool',
        'Translates opcodes and remaps local indices to global constant pool indices',
        'Emits chunk metadata and bytecode with proper big-endian encoding',
    ], S)

    story += h2('3.2  Opcode Remapping', S)
    story += p(
        f'A critical function of the compiler is opcode translation between the host compiler\'s '
        f'bytecode numbering and the VM\'s expected numbering. The {mc("second_pass")} '
        f'contains explicit mappings:', S)
    story += code_block(
        'if op == 0x34: op = 52  # BC_OP_IMPORT\n'
        'elif op == 0x26: op = 38 # BC_OP_CALL_METHOD\n'
        'elif op == 0x25: op = 37 # BC_OP_CALL\n'
        '# ... additional remappings ...',
        S
    )
    story += note_box(
        f'This indicates the {mc(".svm")} format uses a different opcode numbering scheme than the '
        f'runtime VM — SageVM acts as a binary translator bridging these two numbering worlds.',
        S
    )

    story += h2('3.3  Security Considerations', S)
    story += p(f'The compiler includes command injection protection when shelling out to {mc("sage --emit-vm")}:', S)
    story += bullet([
        'Validates input/output paths against forbidden shell metacharacters '
        f'({mc(";")}, {mc("&amp;")}, {mc("|")}, {mc("$")}, etc.)',
        'Explicitly allows spaces but rejects quotes and redirection operators',
    ], S)

    # ── §3a: sage --emit-vm ──────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1(f'3a.  How {mc("sage --emit-vm")} Works', S)
    story += p(
        f'The {mc("sage --emit-vm")} command is the SageLang compiler\'s bytecode emission mode. '
        f'It takes a {mc(".sage")} source file and produces a human-readable intermediate '
        f'representation ({mc(".svm")}) rather than a binary. This {mc(".svm")} file is then consumed '
        f'by {mc("sgvmc")} to produce the final {mc(".sgvm")} binary.', S)

    story += h2('Compilation Pipeline', S)
    story += code_block(
        '.sage source  ──►  sage --emit-vm  ──►  .svm file  ──►  sgvmc  ──►  .sgvm binary\n'
        '     │                                       │                │\n'
        '  Frontend                              Intermediate      Linker /\n'
        ' (C compiler)                           (text format)    Serializer',
        S
    )
    story += p(
        f'The {mc(".svm")} format serves as a stable textual ABI between the C-based Sage compiler '
        f'and the pure-SageLang VM toolchain. This decoupling means:', S)
    story += bullet([
        f'The C compiler doesn\'t need to know the {mc(".sgvm")} binary format',
        f'The SageVM compiler doesn\'t need to parse Sage source code directly',
        f'Changes to either side only require updating the {mc(".svm")} parser, not both',
    ], S)

    story += h2('The .svm Output Structure', S)
    story += p(
        f'The {mc(".svm")} format is a line-oriented text format with explicit section markers:', S)

    story += h3('1 — Top-Level Metadata', S)
    story += code_block('functions <count>\nchunks <count>', S)
    story += p('Declares the total number of function definitions and code chunks in the file.', S)

    story += h3('2 — Chunk / Function Declaration', S)
    story += code_block(
        'chunk       # top-level script code (main program body)\n'
        'function    # a named function definition',
        S)

    story += h3('3 — Parameter Section (Functions Only)', S)
    story += code_block(
        'params <count>\n'
        '<param_len>\n'
        '<hex_encoded_param_name>     # repeated <count> times\n\n'
        '# Example: function("name", "age")\n'
        'function\n'
        'params 2\n'
        '4\n'
        '6E616D65        # "name"\n'
        '3\n'
        '616765          # "age"',
        S)

    story += h3('4 — Constants Section', S)
    story += code_block(
        'constants <count>\n\n'
        '# Number constant:\n'
        'number 3.14159\n\n'
        '# String constant:\n'
        'string 5\n'
        '48656C6C6F      # "Hello"',
        S)

    story += h3('5 — Code Section', S)
    story += code_block(
        'code <byte_count>\n'
        '<hex_encoded_bytecode>     # two hex digits per byte\n\n'
        '# Example: code 5 followed by 0102030405\n'
        '# Represents bytes: 0x01 0x02 0x03 0x04 0x05',
        S)

    story += h2('Opcode Remapping Table', S)
    story += p('Opcodes not present in this table pass through the compiler unchanged:', S)
    story += data_table(
        ['Host Opcode', 'Host Name', 'VM Opcode', 'VM Name'],
        [
            ['0x08', 'BC_OP_DEFINE_FUNCTION', '8',  'OP_DEFINE_FUNCTION'],
            ['0x09', 'BC_OP_GET_PROPERTY',    '9',  'OP_GET_PROPERTY'],
            ['0x0a', 'BC_OP_SET_PROPERTY',    '10', 'OP_SET_PROPERTY'],
            ['0x0b', 'BC_OP_GET_INDEX',       '11', 'OP_GET_INDEX'],
            ['0x0c', 'BC_OP_SET_INDEX',       '12', 'OP_SET_INDEX'],
            ['0x0d', 'BC_OP_LOAD_FUNCTION',   '13', 'OP_LOAD_FUNCTION'],
            ['0x0e', 'BC_OP_SLICE',           '14', 'OP_SLICE'],
            ['0x25', 'BC_OP_CALL',            '37', 'OP_CALL'],
            ['0x26', 'BC_OP_CALL_METHOD',     '38', 'OP_CALL_METHOD'],
            ['0x33', 'BC_OP_LOOP_BACK',       '51', 'OP_LOOP_BACK'],
            ['0x34', 'BC_OP_IMPORT',          '52', 'OP_IMPORT'],
            ['0x35', 'BC_OP_CLASS',           '53', 'OP_CLASS'],
            ['0x36', 'BC_OP_METHOD',          '54', 'OP_METHOD'],
            ['0x37', 'BC_OP_INHERIT',         '55', 'OP_INHERIT'],
            ['0x38', 'BC_OP_SETUP_TRY',       '56', 'OP_SETUP_TRY'],
            ['0x39', 'BC_OP_END_TRY',         '57', 'OP_END_TRY'],
            ['0x44', 'BC_OP_RAISE',           '58', 'OP_RAISE'],
        ],
        col_widths=[3.0 * cm, 5.5 * cm, 3.0 * cm, 5.5 * cm],
        S=S, mono_cols=[0, 1, 2, 3]
    )

    story += h2('Design Rationale', S)
    story += p(f'The {mc(".svm")} intermediate format serves several architectural purposes:', S)
    story += numbered([
        f'{mb("Decoupling:")} The C compiler and pure-Sage VM evolve independently',
        f'{mb("Debugging:")} Human-readable text makes it trivial to inspect compiler output',
        f'{mb("Verification:")} The {mc("diff_bytecode.sage")} tool can compare {mc(".svm")} traces between interpreted and compiled compiler runs',
        f'{mb("Portability:")} The {mc(".svm")} parser is simple enough to reimplement in any language',
        f'{mb("Self-Hosting Path:")} Eventually a SageLang-written compiler could emit {mc(".svm")} directly, completing the bootstrap',
    ], S)
    story += p(
        'The hex encoding of strings and bytecode ensures exact byte-level reproducibility '
        'and eliminates ambiguity about string encoding or byte values — critical when the '
        'same format must be parsed identically by C and SageLang.', S)

    # ── §4 Interpreter ───────────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1(f'4.  The Interpreter (sgvm.sage)', S)

    story += h2('4.1  Execution Model', S)
    story += bullet([
        f'{mb("Stack-based execution:")} All operations work on an operand stack',
        f'{mb("Environment frames:")} {mc("OP_PUSH_ENV")} / {mc("OP_POP_ENV")} for scope management',
        f'{mb("Chunk-based organization:")} Functions and top-level code are separate chunks',
        f'{mb("Constant pool access:")} Global constants referenced by 16-bit indices',
    ], S)

    story += h2('4.2  Opcode Set (89 Opcodes)', S)
    story += p('The VM supports 89 opcodes (0–87, plus 255 for HALT), categorized as follows:', S)
    story += data_table(
        ['Category', 'Key Opcodes', 'Count'],
        [
            ['Stack Operations',  'CONSTANT, NIL, TRUE, FALSE, POP, DUP',                          '6'],
            ['Variable Access',   'GET_GLOBAL, DEFINE_GLOBAL, SET_GLOBAL, GET/SET_PROPERTY, GET/SET_INDEX', '7'],
            ['Arithmetic',        'ADD, SUB, MUL, DIV, MOD, NEGATE',                               '6'],
            ['Comparison',        'EQUAL, NOT_EQUAL, GREATER, GREATER_EQUAL, LESS, LESS_EQUAL',    '6'],
            ['Bitwise',           'BIT_AND, BIT_OR, BIT_XOR, BIT_NOT, SHIFT_LEFT, SHIFT_RIGHT',   '6'],
            ['Logic',             'NOT, TRUTHY',                                                    '2'],
            ['Control Flow',      'JUMP, JUMP_IF_FALSE, LOOP_BACK, BREAK, CONTINUE',               '5'],
            ['Functions',         'DEFINE_FUNCTION, LOAD_FUNCTION, CALL, CALL_METHOD, RETURN',     '5'],
            ['Data Structures',   'ARRAY, TUPLE, DICT, ARRAY_LEN, SLICE',                          '5'],
            ['OOP',               'CLASS, METHOD, INHERIT',                                         '3'],
            ['Exceptions',        'SETUP_TRY, END_TRY, RAISE',                                     '3'],
            ['Modules',           'IMPORT, EXEC_AST_STMT',                                         '2'],
            ['Environment',       'PUSH_ENV, POP_ENV',                                             '2'],
            ['I/O',               'PRINT',                                                          '1'],
            ['GPU Hot-Path',      'GPU_POLL_EVENTS through GPU_CMD_DISPATCH (opcodes 59–86)',      '28'],
            ['Math',              'MATH_PRINTM',                                                    '1'],
            ['System',            'HALT (0xFF)',                                                     '1'],
        ],
        col_widths=[4.0 * cm, 10.5 * cm, 2.5 * cm], S=S, mono_cols=[]
    )

    story += h2('4.3  GPU Acceleration Opcodes', S)
    story += p(
        'Notably, 28 opcodes (59–86) are dedicated to GPU hot-path operations, enabling '
        'real-time graphics without leaving the VM:', S)
    story += bullet([
        f'{mb("Window/input handling:")} {mc("POLL_EVENTS")}, {mc("KEY_PRESSED")}, {mc("MOUSE_POS")}',
        f'{mb("Command buffer building:")} {mc("BEGIN_COMMANDS")}, {mc("END_COMMANDS")}',
        f'{mb("Rendering operations:")} {mc("CMD_DRAW")}, {mc("CMD_BIND_VB")}, {mc("CMD_DISPATCH")}',
        f'{mb("Synchronization:")} {mc("SUBMIT_SYNC")}, {mc("WAIT_FENCE")}',
    ], S)
    story += note_box(
        'This is a significant architectural decision — the VM provides first-class GPU '
        'abstractions, not just CPU abstraction. SageVM targets game engines, simulations, '
        'and graphical applications running in pure-Sage environments.', S)

    # ── §5 Core Utilities ────────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1(f'5.  The sgvm_core.sage Utilities', S)
    story += p(
        f'The {mc("SGVMUtils")} class provides low-level binary manipulation. Its manual '
        f'implementations reveal important design priorities:', S)

    story += h2('5.1  Manual IEEE 754 Double Encoding', S)
    story += p('Custom double-precision floating-point serialization without built-in binary packing:', S)
    story += bullet([
        'Manual extraction of sign, exponent, and 52-bit mantissa',
        'Bit-by-bit construction of 8 output bytes',
        f'Special-case handling for zero ({mc("0.0")}) and nil',
    ], S)

    story += h2('5.2  Hex Parsing', S)
    story += p(f'Custom hex-to-byte conversion for reading the {mc(".svm")} intermediate format:', S)
    story += bullet([
        f'Case-insensitive ({mc("A-F")} normalized to {mc("a-f")})',
        f'Character-by-character lookup against {mc("&quot;0123456789abcdef&quot;")}',
    ], S)

    story += h2('5.3  String Handling', S)
    story += bullet([
        f'Custom {mc("split_lines")} supporting both {mc("\\\\n")} and {mc("\\\\r\\\\n")}',
        f'Custom {mc("trim")} using ASCII codepoint comparison ({mc("ord(c) &lt;= 32")})',
        f'Custom {mc("my_substr")} for bounded string slicing',
    ], S)
    story += p(
        'These utility implementations prioritize explicit control over standard library '
        'dependencies — likely for self-hosting reliability and educational transparency.', S)

    # ── §6 Tooling ───────────────────────────────────────────────────────────
    story += h1('6.  Tooling &amp; Developer Experience', S)

    story += h2('6.1  Unified CLI (sagevm)', S)
    story += p('The project uses a modern unified binary with subcommands:', S)
    story += code_block(
        'sagevm run <file.sgvm>      # Execute bytecode\n'
        'sagevm compile <file.sage>  # Compile to binary\n'
        'sagevm dis <file.sgvm>      # Disassemble\n'
        'sagevm hex <file.sgvm>      # Hexdump\n'
        'sagevm version              # Version info',
        S)
    story += p('Legacy symlinks maintain backward compatibility:', S)
    story += bullet([
        f'{mc("sgvm")} → {mc("sagevm run")}',
        f'{mc("sgvmc")} → {mc("sagevm compile")}',
    ], S)

    story += h2('6.2  Diagnostic Tools', S)
    story += data_table(
        ['Tool', 'Purpose'],
        [
            ['diff_bytecode.sage',
             'Compare execution traces or hex diffs between interpreted vs compiled compiler runs'],
            ['sgvm_disassembler.sage',
             'Convert .sgvm binaries back to readable Sage source representation'],
            ['sgvm_compiler_debug.sage',
             'Instrumented compiler emitting DIAG traces for every opcode — Phase 2 debugging'],
        ],
        col_widths=[5.5 * cm, 12.0 * cm], S=S, mono_cols=[0]
    )
    story += p(
        f'The diff tool compares {mc("j_after")} (stream position after opcode), '
        f'{mc("global_idx")} (constant pool resolution), and {mc("op")} (opcode numbers). '
        f'Its existence indicates the project has reached a maturity level requiring '
        f'formal verification of compiler correctness.', S)

    # ── §7 Performance ───────────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1('7.  Performance Characteristics', S)

    story += h2('7.1  Benchmarks (v0.9.7)', S)
    story += data_table(
        ['Benchmark', 'Duration (ms)', 'Workload Type'],
        [
            ['01_fibonacci.sage (fib(22))', '2,256',  'Recursive computation'],
            ['02_loop_sum.sage',            '4,513',  'Integer accumulation'],
            ['03_string_concat.sage',       '495',    'String operations'],
            ['04_array_ops.sage',           '2,644',  'Array manipulation'],
            ['05_dict_ops.sage',            '2,542',  'Hash map operations'],
            ['06_class_method.sage',        '7,876',  'OOP dispatch overhead'],
            ['07_nested_loops.sage',        '16,991', 'Deep loop nesting'],
            ['08_exception_handling.sage',  '417',    'Try / catch / finally'],
            ['10_primes_sieve.sage',        '1,131',  'Algorithmic (Sieve of Eratosthenes)'],
        ],
        col_widths=[7.5 * cm, 3.5 * cm, 6.5 * cm], S=S, mono_cols=[0, 1]
    )

    story += h3('Key Observations:', S)
    story += bullet([
        f'{mb("Exception handling is fast (417 ms)")} — suggests efficient stack unwinding implementation',
        f'{mb("Class methods show significant overhead (7,876 ms)")} — OOP dispatch in a pure interpreter is expensive; each method call traverses the environment chain',
        f'{mb("Nested loops are the slowest (16,991 ms)")} — likely due to environment frame allocation/deallocation overhead on each iteration',
        f'{mb("String concatenation is surprisingly fast (495 ms)")} — possibly due to immutable string optimizations or special-casing in the runtime',
    ], S)

    story += h2('7.2  Performance Context', S)
    story += p(
        'These numbers represent pure-interpreter overhead — the VM is implemented in SageLang, '
        'which itself runs on the C-based Sage interpreter. This is effectively a meta-circular '
        'interpreter (interpreter interpreting bytecode), so performance is expected to be orders '
        'of magnitude slower than native execution. The numbers are expected for this architecture '
        'and do not reflect a fundamental design flaw.', S)

    # ── §8 Integration ───────────────────────────────────────────────────────
    story += h1('8.  Integration &amp; Ecosystem Role', S)

    story += h2('8.1  Submodule Architecture', S)
    story += p(
        f'SageVM is designed as a submodule at {mc("core/src/sage/vm-tools")} within the main '
        f'SageLang repository. This positioning enables three strategic scenarios:', S)
    story += numbered([
        f'{mb("Bootstrap path:")} The C-based {mc("sage")} compiler generates {mc(".svm")} files; '
        f'SageVM compiles them to {mc(".sgvm")} binaries — no additional C tooling required',
        f'{mb("Self-hosting bridge:")} The self-hosted Sage interpreter (written in Sage) can use '
        f'SageVM to execute bytecode entirely without C dependencies',
        f'{mb("Cross-platform portability:")} Pure Sage code runs anywhere Sage runs, making '
        f'{mc(".sgvm")} files executable on platforms where C compilation is unavailable',
    ], S)

    story += h2('8.2  Specification Lockstep', S)
    story += note_box(
        f'The documentation mandates that opcodes remain in "100% lockstep" with '
        f'{mc("core/src/vm/bytecode.h")} in the main repository. This is critical — '
        f'the pure-Sage VM must behave identically to the C-based MetalVM to ensure '
        f'ecosystem consistency and portability guarantees.', S)

    # ── §9 Strengths ─────────────────────────────────────────────────────────
    story.append(PageBreak())
    story += h1('9.  Strengths &amp; Technical Merits', S)
    story += numbered([
        f'{mb("Educational Clarity:")} Explicit, manual implementations (IEEE 754 encoding, hex parsing) make every internal mechanism transparent to students and contributors',
        f'{mb("Self-Hosting Completeness:")} Enables a full bootstrap path from C compiler → Sage self-host → pure-Sage VM; a rare achievement for a language at this scale',
        f'{mb("GPU-First Design:")} 28 dedicated GPU opcodes demonstrate forward-thinking architecture for graphics and simulation workloads — unusual for a VM at this maturity level',
        f'{mb("Diagnostic Rigor:")} The diff tool and DIAG trace system constitute production-grade debugging infrastructure, not just development scaffolding',
        f'{mb("Security Consciousness:")} Path validation against command injection reflects a security-aware implementation posture',
        f'{mb("Unified UX:")} Modern CLI design ({mc("sagevm")} with subcommands) with backward-compatible legacy symlinks for ecosystem continuity',
    ], S)

    # ── §10 Weaknesses ───────────────────────────────────────────────────────
    story += h1('10.  Weaknesses &amp; Areas for Improvement', S)
    story += numbered([
        f'{mb("Performance:")} Meta-circular interpreter architecture means performance is inherently limited — no JIT compilation within the pure-Sage layer',
        f'{mb("Memory Overhead:")} Stack machine with environment frames on each scope boundary likely causes significant GC pressure in the host interpreter',
        f'{mb("Opcode Hardcoding:")} Explicit remapping tables in {mc("second_pass")} are brittle — any change to {mc("bytecode.h")} requires a coordinated manual update',
        f'{mb("Error Handling:")} The compiler returns {mc("false")} on errors with no structured error reporting system; absence of source line numbers makes debugging difficult',
        f'{mb("Limited Optimizations:")} No constant folding, dead code elimination, or peephole optimization at the VM level',
        f'{mb("String Handling:")} Custom string manipulation primitives instead of built-ins suggest either compatibility constraints or unnecessary divergence from idiomatic SageLang',
    ], S)

    # ── §11 Comparative Context ───────────────────────────────────────────────
    story.append(PageBreak())
    story += h1('11.  Comparative Context', S)
    story += data_table(
        ['Feature', 'SageVM', 'Python VM', 'Lua VM', 'JVM'],
        [
            ['Implementation',    'Pure SageLang',        'C',                'C',               'C / C++'],
            ['Self-Hosted',       'Yes (partial)',         'No',               'No',              'No'],
            ['GPU Opcodes',       'Yes (28)',              'No',               'No',              'No (JNI only)'],
            ['Stack Model',       'Stack machine',         'Stack machine',    'Register + Stack','Stack machine'],
            ['Binary Format',     'Custom (.sgvm)',        '.pyc (marshal)',   '.luac',           '.class'],
            ['Exception Support', 'Native try / catch',   'Native',           'Native (pcall)',   'Native'],
            ['OOP',               'Classes + Inheritance', 'Classes',          'Prototypes',      'Classes'],
        ],
        col_widths=[4.0 * cm, 3.7 * cm, 3.7 * cm, 3.0 * cm, 3.1 * cm],
        S=S, mono_cols=[]
    )
    story += p(
        'SageVM occupies a unique niche: more capable than a toy VM (GPU support, full OOP, '
        'exceptions) but intentionally simpler than industrial VMs by virtue of being self-hosted '
        'and educational. It demonstrates that a meaningful VM can be built in a high-level, '
        'indentation-based language without sacrificing architectural coherence.', S)

    # ── §12 Conclusion ───────────────────────────────────────────────────────
    story += h1('12.  Conclusion', S)
    story += p(
        'SageVM v0.9.7 is a sophisticated, well-architected educational and tooling VM that serves '
        'as a critical bridge in the SageLang ecosystem. Its pure-SageLang implementation enables '
        'bootstrap scenarios and cross-platform portability that would be impossible with C-only '
        'tooling. The inclusion of GPU opcodes, comprehensive OOP support, and rigorous diagnostic '
        'tools demonstrate a project that has evolved beyond a simple learning exercise into a '
        'genuine piece of language infrastructure.', S)
    story += p(
        'The codebase prioritizes clarity, correctness, and ecosystem integration over raw '
        'performance — an appropriate tradeoff for its role as a reference implementation and '
        'bootstrap tool.', S)
    story += p('Future development would benefit most from:', S)
    story += bullet([
        'A register-based VM redesign to reduce environment frame allocation overhead',
        'A structured error reporting system with source line number propagation',
        'Automated opcode synchronization tooling to eliminate manual remapping table maintenance',
    ], S)
    story += p(
        'For developers interested in VM construction, SageVM provides an excellent case study '
        'in how to build a complete, self-hosted bytecode toolchain — with modern language '
        'features including exceptions, classes, and GPU compute — in a high-level, '
        'indentation-based language.', S)

    story += hr()
    story += [
        Spacer(1, 8),
        Paragraph(
            'End of Document  ·  SageVM Technical Reference v0.9.7  ·  Night-Traders-Dev',
            S['end_note']
        ),
    ]

    doc.build(story)
    print(f'✓  PDF written to {OUTPUT}')

if __name__ == '__main__':
    build()
