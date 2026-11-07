#!/usr/bin/env python3
# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The API pages of the site: one Markdown page for each header, made from its Doxygen comments.

    python tools/api_pages.py            write docs/api/*.md and show each ASD-STE100 finding at its header line
    python tools/api_pages.py --check    write nothing, and fail where a page does not match its header

The pages are in the tree, and each commit holds them. After an edit to a header, run the tool and commit the
pages with the header. mkdocs.yml names this file as a hook, and the hook runs the check before each site
build. The harness ste command runs the same check before each commit. The check also stops at each
ASD-STE100 finding in a page.

The reader takes each Doxygen block and the declaration under it. A block before a #ifndef of a name
documents that name. The declarations below a block, up to a blank line at the depth of the block, are
one entry. A declaration of a name that an entry holds joins that entry in every #if arm, and the
page shows each arm. A block in the body of a macro documents each function that an invocation of the
macro writes. The reader stops at a line it cannot read and at a block with no declaration.
"""

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INCLUDE = os.path.join(ROOT, "include")
OUT = os.path.join(ROOT, "docs", "api")
SOURCE_URL = "https://github.com/dstroy0/embedded_types/blob/main/include/"

# The order of the pages in the site navigation, and the order of the README file table
HEADERS = (
    "embed_types.h",
    "embed_compiler_directives.h",
    "embed_dispatch_layout.h",
    "embed_atomic.h",
    "embed_turnstile.h",
    "embed_lane.h",
)

SECTIONS = (("type", "Types"), ("macro", "Macros"), ("function", "Functions"), ("assert", "Assertions"))
TAG = re.compile(r"^@(brief|note|warning|return|param(?:\[(in|out|in,\s*out)\])?|file|author|date)\b\s*(.*)$")
IDENT = re.compile(r"[A-Za-z_]\w*")

sys.path.insert(0, os.path.join(ROOT, "test"))
import ste_check  # noqa: E402


class ReadError(Exception):
    """A line or a block the reader cannot place."""


class Block:
    """One Doxygen block: the brief, the free paragraphs, the parameters, the return and the notes."""

    def __init__(self, line, depth):
        self.line = line
        self.depth = depth
        self.file = False
        self.brief = ""
        self.paragraphs = []
        self.params = []
        self.returns = ""
        self.notes = []


class Decl:
    """One declaration: its kind, its name, its source text and the #if arms around it."""

    def __init__(self, kind, name, text, line, stack, members=None):
        self.kind = kind
        self.name = name
        self.text = text
        self.line = line
        self.stack = tuple(stack)
        self.members = members or []


class Entry:
    """One documented item: the blocks that document it and every declaration of its names."""

    def __init__(self, block, name=None, kind=None):
        self.blocks = [block]
        self.names = [name] if name else []
        self.kind = kind
        self.decls = []

    def add(self, decl):
        self.decls.append(decl)
        if decl.name not in self.names:
            self.names.append(decl.name)
        if self.kind is None:
            self.kind = decl.kind


def parse_block(lines, line, depth):
    """A Block from the text of a /** */ comment, its leading stars off."""
    block = Block(line, depth)
    current = None
    for raw in lines:
        text = raw.strip()
        if text.startswith("*"):
            text = text[1:].strip()
        if not text:
            current = None
            continue
        tag = TAG.match(text)
        if tag:
            name, direction, rest = tag.group(1), tag.group(2), tag.group(3)
            if name == "file":
                block.file = True
                current = None
            elif name in ("author", "date"):
                current = None
            elif name == "brief":
                block.brief = rest
                current = ("brief",)
            elif name in ("note", "warning"):
                block.notes.append([name, rest])
                current = ("note", len(block.notes) - 1)
            elif name == "return":
                block.returns = rest
                current = ("return",)
            else:
                pname, _, ptext = rest.partition(" ")
                block.params.append([pname, (direction or "").replace(" ", ""), ptext.strip()])
                current = ("param", len(block.params) - 1)
            continue
        if current is None:
            block.paragraphs.append(text)
            current = ("paragraph", len(block.paragraphs) - 1)
        elif current[0] == "brief":
            block.brief += " " + text
        elif current[0] == "note":
            block.notes[current[1]][1] += " " + text
        elif current[0] == "return":
            block.returns += " " + text
        elif current[0] == "param":
            block.params[current[1]][2] += " " + text
        else:
            block.paragraphs[current[1]] += " " + text
    return block


def code_only(text):
    """The text with its comments and string literals out, for the count of parentheses and braces."""
    text = re.sub(r'"(?:\\.|[^"\\])*"', '""', text)
    text = re.sub(r"/\*.*?\*/", " ", text)
    return re.sub(r"//.*", "", text)


def macro_lines(lines, at):
    """The lines of a directive from line at, through each line that a backslash continues."""
    out = [lines[at]]
    while out[-1].rstrip().endswith("\\") and at + 1 < len(lines):
        at += 1
        out.append(lines[at])
    return out, at


def tidy_macro(lines):
    """A directive's lines with the padding before each continuation backslash cut to one space."""
    return "\n".join(re.sub(r"\s*\\\s*$", lambda hit: " \\", line.rstrip()) for line in lines)


def typedef_name(text):
    """The name a typedef declares: a function pointer's name, else the last name before the attributes."""
    flat = " ".join(code_only(text).split()).rstrip(";").strip()
    pointer = re.search(r"\(\s*\*\s*(?:const\s+)?(\w+)", flat)
    if pointer:
        return pointer.group(1)
    while True:
        cut = re.sub(r"\s+[A-Z][A-Z0-9_]*(\([^()]*\))?$", "", flat)
        if cut == flat:
            break
        flat = cut
    return IDENT.findall(flat)[-1]


def member_of(code, comment):
    """(name, comment) for a struct member or an enumerator with a /**< comment."""
    code = re.sub(r"\[[^\]]*\]", "", code_only(code)).strip().rstrip(";,").strip()
    code = code.split(",")[-1].strip()
    code = code.split("=")[0].strip()
    names = IDENT.findall(code)
    return names[-1], " ".join(comment.split())


def members_in(lines):
    """Each /**< member comment in the lines, once a name."""
    found = []
    seen = set()
    for line in lines:
        hit = re.search(r"/\*\*<\s*(.*?)\s*\*/", line)
        if hit:
            name, comment = member_of(line[: hit.start()], hit.group(1))
            if name not in seen:
                seen.add(name)
                found.append((name, comment))
    return found


def substitute(text, params, args):
    """The macro text with each parameter replaced by its argument and each ## paste closed up."""
    table = dict(zip(params, args))
    text = IDENT.sub(lambda hit: table.get(hit.group(0), hit.group(0)), text)
    return re.sub(r"\s*##\s*", "", text)


def split_args(text):
    """The arguments of a macro invocation, split at the commas outside parentheses."""
    inner = text[text.index("(") + 1 : text.rindex(")")]
    args = []
    depth = 0
    current = ""
    for char in inner:
        if char == "," and depth == 0:
            args.append(current.strip())
            current = ""
            continue
        depth += char == "("
        depth -= char == ")"
        current += char
    args.append(current.strip())
    return args


class Header:
    """One header read into its file block and its entries."""

    def __init__(self, name):
        self.name = name
        self.slug = name[:-2]
        self.file_block = None
        self.entries = []
        self.by_name = {}
        self.groups = {}
        self.templates = {}
        with open(os.path.join(INCLUDE, name), encoding="utf-8") as handle:
            self.lines = handle.read().split("\n")
        self.read()

    def fail(self, line, what):
        raise ReadError("include/%s:%d: %s" % (self.name, line + 1, what))

    def new_entry(self, block, decl=None, name=None, kind=None):
        """The entry of a block: a new one, or the one that holds the declaration's name already."""
        key = decl.name if decl else name
        if key in self.by_name:
            entry = self.by_name[key]
            entry.blocks.append(block)
        else:
            entry = Entry(block, name, kind)
            self.entries.append(entry)
            if name:
                self.by_name[name] = entry
        if decl:
            self.place(decl, entry)
        return entry

    def place(self, decl, entry):
        entry.add(decl)
        self.by_name.setdefault(decl.name, entry)

    def read(self):
        lines = self.lines
        stack = []
        guard = None
        guard_name = None
        pending = None
        owner = None
        owner_depth = 0
        orphans = []
        at = 0
        while at < len(lines):
            line = lines[at]
            text = line.strip()
            depth = len(stack)

            if not text:
                if owner is not None and depth <= owner_depth:
                    owner = None
                at += 1
                continue

            if text.startswith("/**") and not text.startswith("/**<"):
                start = at
                body = [text[3:]]
                while "*/" not in body[-1]:
                    at += 1
                    body.append(lines[at])
                body[-1] = body[-1][: body[-1].index("*/")]
                block = parse_block(body, start, depth)
                if block.file:
                    self.file_block = block
                else:
                    if pending is not None:
                        self.fail(pending.line, "a Doxygen block with no declaration under it")
                    pending = block
                at += 1
                continue

            if text.startswith("/*"):
                while "*/" not in lines[at]:
                    at += 1
                at += 1
                continue

            if text.startswith("//"):
                at += 1
                continue

            if text.startswith("#"):
                directive, last = macro_lines(lines, at)
                joined = " ".join(part.strip().rstrip("\\").strip() for part in directive)
                word = re.match(r"#\s*(\w+)", joined).group(1)
                if word in ("if", "ifdef", "ifndef"):
                    gid = len(self.groups)
                    self.groups[gid] = [joined]
                    named = re.match(r"#\s*ifndef\s+(\w+)$", joined)
                    if guard is None and named and named.group(1).endswith("_H"):
                        guard = gid
                        guard_name = named.group(1)
                    elif pending is not None and named:
                        # A block above a #ifndef of a name documents that name in every arm below it
                        self.new_entry(pending, name=named.group(1), kind="macro")
                        pending = None
                        owner = None
                    stack.append((gid, 0))
                elif word in ("elif", "else"):
                    gid = stack[-1][0]
                    self.groups[gid].append(joined)
                    stack[-1] = (gid, len(self.groups[gid]) - 1)
                elif word == "endif":
                    stack.pop()
                    if owner is not None and len(stack) < owner_depth:
                        owner = None
                elif word == "define":
                    name = re.match(r"#\s*define\s+(\w+)", joined).group(1)
                    if name == guard_name:
                        at = last + 1
                        continue
                    decl = Decl("macro", name, tidy_macro(directive), at, self.arms(stack, guard))
                    if any("/**" in part for part in directive):
                        self.template(name, directive)
                    pending, owner, owner_depth = self.attach(decl, pending, owner, owner_depth, depth, orphans)
                elif word not in ("include", "error", "pragma", "undef"):
                    self.fail(at, "a directive the reader does not know: " + joined)
                at = last + 1
                continue

            if text in ("EMBED_BEGIN_DECLS", "EMBED_END_DECLS"):
                at += 1
                continue

            decl, at = self.statement(at, stack, guard)
            if decl is None:
                continue
            pending, owner, owner_depth = self.attach(decl, pending, owner, owner_depth, depth, orphans)

        if pending is not None:
            self.fail(pending.line, "a Doxygen block with no declaration under it")
        for decl in orphans:
            if decl.name in self.by_name:
                self.place(decl, self.by_name[decl.name])
        for entry in self.entries:
            entry.decls.sort(key=lambda decl: decl.line)
            if not entry.decls:
                self.fail(entry.blocks[0].line, "a block for %s, which nothing here declares" % entry.names[0])

    def arms(self, stack, guard):
        return [arm for arm in stack if arm[0] != guard]

    def attach(self, decl, pending, owner, owner_depth, depth, orphans):
        """Give the declaration to its entry; answer the pending block, the owner and the owner's depth."""
        if decl.kind == "invocation":
            self.expand(decl)
            return pending, owner, owner_depth
        if pending is not None:
            entry = self.new_entry(pending, decl=decl, name=decl.name, kind=decl.kind)
            return None, entry, pending.depth
        if decl.name in self.by_name:
            self.place(decl, self.by_name[decl.name])
        elif owner is not None:
            self.place(decl, owner)
        else:
            orphans.append(decl)
        return None, owner, owner_depth

    def statement(self, at, stack, guard):
        """The declaration that starts at line at, and the line after it."""
        lines = self.lines
        text = lines[at].strip()
        start = at
        arms = self.arms(stack, guard)

        if re.match(r"typedef\s+(struct|enum|union)\b", text) and not text.rstrip().endswith(";"):
            depth = 0
            body = []
            while True:
                body.append(lines[at])
                depth += code_only(lines[at]).count("{") - code_only(lines[at]).count("}")
                if depth == 0 and "}" in lines[at] and ";" in lines[at]:
                    break
                at += 1
            name = IDENT.findall(code_only(body[-1]).split("}")[-1])[-1]
            return Decl("type", name, "\n".join(body), start, arms, members_in(body)), at + 1

        if text.startswith("typedef"):
            body = [lines[at]]
            while not code_only(body[-1]).rstrip().endswith(";"):
                at += 1
                body.append(lines[at])
            joined = "\n".join(part.rstrip() for part in body)
            return Decl("type", typedef_name(joined), joined, start, arms), at + 1

        head = re.match(r"([A-Z][A-Z0-9_]*)\s*\(", text)
        body = [lines[at]]
        depth = code_only(lines[at]).count("(") - code_only(lines[at]).count(")")
        while depth > 0:
            at += 1
            body.append(lines[at])
            depth += code_only(lines[at]).count("(") - code_only(lines[at]).count(")")
        joined = "\n".join(part.rstrip() for part in body)

        if head:
            macro = head.group(1)
            if lines[at].rstrip().endswith(";") is False and at + 1 < len(lines) and lines[at + 1].strip() == ";":
                at += 1
            if macro == "EMBED_STATIC_ASSERT":
                condition = split_args(code_only(joined))[0]
                return Decl("assert", " ".join(condition.split()), joined, start, arms), at + 1
            if macro == "EMBED_ENUM_BYTE":
                name = split_args(code_only(joined))[0]
                return Decl("type", name, joined, start, arms, members_in(body)), at + 1
            if macro in self.templates:
                return Decl("invocation", macro, joined, start, arms), at + 1
            self.fail(start, "a macro invocation the reader does not know: " + macro)

        if "(" in text:
            signature = joined
            if not code_only(body[-1]).rstrip().endswith(";"):
                if at + 1 >= len(lines) or lines[at + 1].strip() != "{":
                    self.fail(start, "a function with no body and no semicolon")
                at += 1
                depth = 0
                while True:
                    depth += code_only(lines[at]).count("{") - code_only(lines[at]).count("}")
                    if depth == 0:
                        break
                    at += 1
                signature += ";"
            name = re.search(r"(\w+)\s*\(", code_only(joined)).group(1)
            return Decl("function", name, signature, start, arms), at + 1

        self.fail(start, "a line the reader cannot place: " + text)
        return None, at + 1

    def template(self, name, directive):
        """Keep a macro whose body holds Doxygen blocks, for the functions its invocations write."""
        head = re.match(r"\s*#\s*define\s+\w+\(([^)]*)\)", directive[0])
        params = [part.strip() for part in head.group(1).split(",")]
        body = [re.sub(r"\s*\\\s*$", "", part) for part in directive[1:]]
        self.templates[name] = (params, body)

    def expand(self, decl):
        """An entry for each block in a macro body, with the function each invocation writes."""
        params, body = self.templates[decl.name]
        args = split_args(code_only(decl.text))
        at = 0
        index = 0
        while at < len(body):
            text = body[at].strip()
            if text.startswith("/**"):
                comment = [text[3:]]
                while "*/" not in comment[-1]:
                    at += 1
                    comment.append(body[at])
                comment[-1] = comment[-1][: comment[-1].index("*/")]
                at += 1
                signature = []
                while body[at].strip() != "{":
                    signature.append(body[at].strip())
                    at += 1
                text = substitute(" ".join(signature), params, args) + ";"
                name = re.search(r"(\w+)\s*\(", text).group(1)
                key = (decl.name, index)
                entry = self.by_name.get(key)
                if entry is None:
                    entry = Entry(parse_block(comment, decl.line, 0))
                    self.entries.append(entry)
                    self.by_name[key] = entry
                function = Decl("function", name, text, decl.line, decl.stack)
                entry.add(function)
                self.by_name.setdefault(name, entry)
                index += 1
            at += 1


# The rendering


def known_names(headers):
    """Every entry name of every header, to the page and the anchor that show it."""
    table = {}
    for header in headers:
        for entry in header.entries:
            if entry.kind == "assert":
                continue
            for name in entry.names:
                table.setdefault(name, (header.slug, entry.names[0]))
    return table


CODE_TOKEN = re.compile(
    r"(#\w+|<[\w./]+\.h>|(?<![\w-])(?:-W|--|-m)[\w-]+|\b\w*_\w*\b(?:\(\w*\))?|\b\w+\(\)|\b0x[0-9A-Fa-f]+\w*|\d+\^\w+)"
)


def prose(text, names, slug):
    """Doxygen text as Markdown: code as code spans, a known name as a link, the Markdown marks escaped."""
    out = []
    for at, part in enumerate(re.split(r"(`[^`]*`)", text)):
        if at % 2 == 1:
            out.append(link(part[1:-1], names, slug))
            continue
        pieces = []
        for index, piece in enumerate(CODE_TOKEN.split(part)):
            if index % 2 == 1 and piece:
                pieces.append(link(piece, names, slug))
            else:
                piece = piece.replace("\\", "\\\\").replace("*", "\\*").replace("[", "\\[").replace("]", "\\]")
                pieces.append(piece.replace("<", "&lt;").replace(">", "&gt;").replace("|", "\\|"))
        out.append("".join(pieces))
    return "".join(out)


def link(code, names, slug):
    """A code span, a link where the code is a name that a page shows."""
    bare = code[:-2] if code.endswith("()") else code
    span = "`%s`" % code
    if bare in names:
        page, anchor = names[bare]
        target = "#" + anchor if page == slug else "%s.md#%s" % (page, anchor)
        return "[%s](%s)" % (span, target)
    return span


def skeleton(decls, groups):
    """The declarations of one entry in the #if arms they stand in, as C."""
    out = []
    current = []
    for decl in decls:
        target = list(decl.stack)
        same = 0
        while same < len(current) and same < len(target) and current[same] == target[same]:
            same += 1
        if same < len(current) and same < len(target) and current[same][0] == target[same][0]:
            for _ in range(len(current) - same - 1):
                out.append("#endif")
            gid = target[same][0]
            for arm in range(current[same][1] + 1, target[same][1] + 1):
                out.append(groups[gid][arm])
            opened = same + 1
        else:
            for _ in range(len(current) - same):
                out.append("#endif")
            opened = same
        for gid, arm in target[opened:]:
            for index in range(0, arm + 1):
                out.append(groups[gid][index])
        current = target
        out.append(decl.text)
    for _ in current:
        out.append("#endif")
    return "\n".join(out)


class Page:
    """The Markdown of one header, and the header line of each of its lines of prose."""

    def __init__(self, header, names):
        self.header = header
        self.names = names
        self.lines = []
        self.source = []
        self.write()

    def emit(self, text="", line=None):
        self.lines.append(text)
        self.source.append(line)

    def paragraph(self, text, line):
        self.emit(prose(" ".join(text.split()), self.names, self.header.slug), line)
        self.emit()

    def block(self, block):
        slug = self.header.slug
        if block.brief:
            self.paragraph(block.brief, block.line)
        for text in block.paragraphs:
            self.paragraph(text, block.line)
        if block.params:
            for name, direction, text in block.params:
                where = " (%s)" % direction.replace(",", ", ") if direction else ""
                self.emit("- `%s`%s: %s" % (name, where, prose(" ".join(text.split()), self.names, slug)), block.line)
            self.emit()
        if block.returns:
            self.paragraph("**Returns:** " + block.returns, block.line)
        # A warning's label is bold italic and a note's is bold, and the stylesheet tells the two apart by that
        for kind, text in block.notes:
            label = "_**Warning:**_" if kind == "warning" else "**Note:**"
            body = prose(" ".join(text.split()), self.names, slug)
            self.emit("> %s %s" % (label, body), block.line)
            self.emit()

    def write(self):
        header = self.header
        self.emit("<!-- tools/api_pages.py writes this page from include/%s. Edit the header, then run the tool. -->"
                  % header.name)
        self.emit()
        self.emit("# %s" % header.name)
        self.emit()
        self.emit("Source: [`include/%s`](%s%s)" % (header.name, SOURCE_URL, header.name))
        self.emit()
        if header.file_block:
            self.block(header.file_block)
        for kind, title in SECTIONS:
            entries = [entry for entry in header.entries if entry.kind == kind]
            if not entries:
                continue
            self.emit("## %s" % title)
            self.emit()
            for entry in entries:
                if kind == "assert":
                    self.emit("### `%s`" % entry.names[0].replace("`", ""))
                else:
                    self.emit("### `%s` { #%s }" % (entry.names[0], entry.names[0]))
                self.emit()
                self.emit("```c")
                for code in skeleton(entry.decls, header.groups).split("\n"):
                    self.emit(code)
                self.emit("```")
                self.emit()
                for block in entry.blocks:
                    self.block(block)
                members = []
                for decl in entry.decls:
                    members.extend(member for member in decl.members if member not in members)
                if members:
                    for name, text in members:
                        self.emit("- `%s`: %s" % (name, prose(text, self.names, header.slug)), entry.blocks[0].line)
                    self.emit()

    def text(self):
        return "\n".join(self.lines).rstrip() + "\n"


def pages():
    """Every header read and rendered, in the order of HEADERS."""
    present = sorted(name for name in os.listdir(INCLUDE) if name.endswith(".h"))
    if present != sorted(HEADERS):
        raise ReadError("include/ holds %s and HEADERS names %s" % (present, sorted(HEADERS)))
    headers = [Header(name) for name in HEADERS]
    names = known_names(headers)
    return [Page(header, names) for header in headers]


def names_allowed():
    path = os.path.join(ROOT, "test", "ste_names.txt")
    return ste_check.word_list(path) if os.path.isfile(path) else set()


def findings(built, folder):
    """Write each page to the folder, and answer each ASD-STE100 finding at its header line."""
    os.makedirs(folder, exist_ok=True)
    allowed = names_allowed()
    found = []
    for page in built:
        path = os.path.join(folder, page.header.slug + ".md")
        with open(path, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(page.text())
        for number, rule, what in ste_check.findings(path, set(), allowed):
            line = page.source[number - 1]
            where = "include/%s:%d" % (page.header.name, line + 1) if line is not None else path
            found.append("%s: %s: %s" % (where, rule, what))
    return found


def stale(built):
    """Each page in docs/api that does not match its header, and each page there that no header makes."""
    found = []
    expected = {page.header.slug + ".md": page.text() for page in built}
    present = sorted(name for name in os.listdir(OUT) if name.endswith(".md")) if os.path.isdir(OUT) else []
    for name in sorted(set(expected) | set(present)):
        path = os.path.join(OUT, name)
        if name not in expected:
            found.append("docs/api/%s: no header makes this page, so remove it" % name)
            continue
        if not os.path.isfile(path):
            found.append("docs/api/%s: the page is absent, run tools/api_pages.py" % name)
            continue
        with open(path, encoding="utf-8", newline="") as handle:
            if handle.read() != expected[name]:
                found.append("docs/api/%s: the page does not match its header, run tools/api_pages.py" % name)
    return found


def check():
    """Every finding of the check mode: a header that the reader cannot read, a stale page, an ASD-STE100 finding."""
    try:
        built = pages()
    except ReadError as error:
        return [str(error)]
    return stale(built) + findings(built, os.path.join(ROOT, "build", "api_check"))


def on_pre_build(config):
    """The MkDocs hook: the build stops where docs/api does not match the headers, or at a finding in a page."""
    from mkdocs.exceptions import PluginError

    found = check()
    if found:
        raise PluginError("the API pages:\n  " + "\n  ".join(found))


def main(argv):
    sys.stdout.reconfigure(errors="replace")
    if "--check" in argv:
        found = check()
        for row in found:
            print(row)
        print("api_pages: %d findings in docs/api" % len(found))
        return 1 if found else 0
    try:
        built = pages()
    except ReadError as error:
        print("api_pages: %s" % error)
        return 2
    found = findings(built, OUT)
    for row in found:
        print(row)
    print("api_pages: %d pages written to docs/api, %d findings" % (len(built), len(found)))
    return 1 if found else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
