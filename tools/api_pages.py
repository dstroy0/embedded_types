#!/usr/bin/env python3
# embedded_types - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
"""The API pages of the site: one Markdown page for each header, made from its Doxygen comments.

    python tools/api_pages.py            write docs/api/*.md and show each ASD-STE100 finding at its header line
    python tools/api_pages.py --check    write nothing, and fail where a page does not match its header

The pages are in the tree, and each commit holds them. After an edit to a header, run the tool and commit the
pages with the header. The site configuration names this file as a hook, and the hook runs the check before
each site build. The harness ste command runs the same check before each commit. The check also stops at each
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
    "embed_clock.h",
    "embed_ticket_ring.h",
    "embed_lane.h",
)

SECTIONS = (("type", "Types"), ("macro", "Macros"), ("function", "Functions"), ("table", "Tables"),
            ("assert", "Assertions"))
# The text of a tag starts at its first character that is not white space, so no two parts match one character
TAG = re.compile(r"^@(brief|note|warning|return|param(?:\[(in|out|in,\s*out)\])?|file|author|date)\b\s*(\S.*|)$")
IDENT = re.compile(r"[A-Za-z_]\w*")
ENDIF = "#endif"

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

    def tag(self, name, direction, rest):
        """Take one tag line. Answer the part that the next line continues, or None for no part."""
        if name == "file":
            self.file = True
            return None
        if name in ("author", "date"):
            return None
        if name == "brief":
            self.brief = rest
            return ("brief",)
        if name in ("note", "warning"):
            self.notes.append([name, rest])
            return ("note", len(self.notes) - 1)
        if name == "return":
            self.returns = rest
            return ("return",)
        pname, _, ptext = rest.partition(" ")
        self.params.append([pname, (direction or "").replace(" ", ""), ptext.strip()])
        return ("param", len(self.params) - 1)

    def extend(self, current, text):
        """Join a line to the part that it continues, or open a paragraph with it. Answer the part."""
        if current is None:
            self.paragraphs.append(text)
            return ("paragraph", len(self.paragraphs) - 1)
        if current[0] == "brief":
            self.brief += " " + text
        elif current[0] == "note":
            self.notes[current[1]][1] += " " + text
        elif current[0] == "return":
            self.returns += " " + text
        elif current[0] == "param":
            self.params[current[1]][2] += " " + text
        else:
            self.paragraphs[current[1]] += " " + text
        return current


class Decl:
    """One declaration: its type, its name, its source text and the #if arms around it."""

    def __init__(self, type, name, text, line, stack, members=None):
        self.type = type
        self.name = name
        self.text = text
        self.line = line
        self.stack = tuple(stack)
        self.members = members or []


class Entry:
    """One documented item: the blocks that document it and every declaration of its names."""

    def __init__(self, block, name=None, type=None):
        self.blocks = [block]
        self.names = [name] if name else []
        self.type = type
        self.decls = []

    def add(self, decl):
        self.decls.append(decl)
        if decl.name not in self.names:
            self.names.append(decl.name)
        if self.type is None:
            self.type = decl.type


def parse_block(lines, line, depth):
    """A Block from the text of a /** */ comment, with the star at the start of each line off."""
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
            current = block.tag(tag.group(1), tag.group(2), tag.group(3))
        else:
            current = block.extend(current, text)
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


def without_continuation(line):
    """The line with the continuation backslash at its end, and the white space on each side of it, cut off.

    A line with no backslash at its end comes back as it is.
    """
    trimmed = line.rstrip()
    if not trimmed.endswith("\\"):
        return line
    return trimmed[:-1].rstrip()


def tidy_line(line):
    """One line of a directive with its end white space cut, and one space before a continuation backslash."""
    trimmed = line.rstrip()
    if not trimmed.endswith("\\"):
        return trimmed
    return without_continuation(trimmed) + " \\"


def tidy_macro(lines):
    """A directive's lines with the padding before each continuation backslash cut to one space."""
    return "\n".join(tidy_line(line) for line in lines)


def called_name(text):
    """The name before the first parenthesis that follows a name, with or without white space between, or None."""
    at = text.find("(")
    while at >= 0:
        end = len(text[:at].rstrip())
        start = end
        while start > 0 and (text[start - 1].isalnum() or text[start - 1] == "_"):
            start -= 1
        if start < end:
            return text[start:end]
        at = text.find("(", at + 1)
    return None


ATTRIBUTE = re.compile(r"[A-Z][A-Z0-9_]*")


def without_attribute(flat):
    """The text with its last word cut where that word is an attribute: a capital name, with or without arguments.

    The words of the text have one space between each two. Arguments are in parentheses and hold no parenthesis.
    """
    head = flat
    if flat.endswith(")"):
        opening = flat.rfind("(")
        if opening < 0 or ")" in flat[opening + 1 : -1]:
            return flat
        head = flat[:opening]
    before, space, word = head.rpartition(" ")
    if not space or not ATTRIBUTE.fullmatch(word):
        return flat
    return before


def typedef_name(text):
    """The name a typedef declares: a function pointer's name, else the last name before the attributes."""
    flat = " ".join(code_only(text).split()).rstrip(";").strip()
    pointer = re.search(r"\(\s*\*\s*(?:const\s+)?(\w+)", flat)
    if pointer:
        return pointer.group(1)
    while True:
        cut = without_attribute(flat)
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
    found_names = set()
    for line in lines:
        opening = line.find("/**<")
        closing = line.find("*/", opening + 4) if opening >= 0 else -1
        if closing >= 0:
            name, comment = member_of(line[:opening], line[opening + 4 : closing].strip())
            if name not in found_names:
                found_names.add(name)
                found.append((name, comment))
    return found


def substitute(text, params, args):
    """The macro text with each parameter replaced by its argument and each ## paste closed up."""
    table = dict(zip(params, args))
    parts = IDENT.sub(lambda hit: table.get(hit.group(0), hit.group(0)), text).split("##")
    # Each paste closes up with the white space on each side of it
    for index in range(len(parts) - 1):
        parts[index] = parts[index].rstrip()
        parts[index + 1] = parts[index + 1].lstrip()
    return "".join(parts)


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

    def new_entry(self, block, decl=None, name=None, type=None):
        """The entry of a block: a new one, or the one that holds the declaration's name already."""
        key = decl.name if decl else name
        if key in self.by_name:
            entry = self.by_name[key]
            entry.blocks.append(block)
        else:
            entry = Entry(block, name, type)
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
        """Read every line of the header into the file block and the entries."""
        self.stack = []
        self.guard = None
        self.guard_name = None
        self.pending = None
        self.owner = None
        self.owner_depth = 0
        self.orphans = []
        at = 0
        while at < len(self.lines):
            at = self.read_line(at)
        self.finish()

    def read_line(self, at):
        """Read the line at `at` and the lines that it opens. Answer the line after them."""
        text = self.lines[at].strip()
        depth = len(self.stack)
        if not text:
            if self.owner is not None and depth <= self.owner_depth:
                self.owner = None
            return at + 1
        if text.startswith("/**") and not text.startswith("/**<"):
            return self.read_block(at, text, depth)
        if text.startswith("/*"):
            while "*/" not in self.lines[at]:
                at += 1
            return at + 1
        if text.startswith("//") or text in ("EMBED_BEGIN_DECLS", "EMBED_END_DECLS"):
            return at + 1
        if text.startswith("#"):
            return self.read_directive(at)
        decl, at = self.statement(at, self.stack, self.guard)
        if decl is not None:
            self.attach(decl)
        return at

    def read_block(self, at, text, depth):
        """Read the Doxygen block that opens at `at`. Answer the line after it."""
        start = at
        body = [text[3:]]
        while "*/" not in body[-1]:
            at += 1
            body.append(self.lines[at])
        body[-1] = body[-1][: body[-1].index("*/")]
        block = parse_block(body, start, depth)
        if block.file:
            self.file_block = block
        else:
            if self.pending is not None:
                self.fail(self.pending.line, "a Doxygen block with no declaration under it")
            self.pending = block
        return at + 1

    def read_directive(self, at):
        """Read the directive that starts at `at`. Answer the line after it."""
        directive, last = macro_lines(self.lines, at)
        joined = " ".join(part.strip().rstrip("\\").strip() for part in directive)
        word = re.match(r"#\s*(\w+)", joined).group(1)
        if word in ("if", "ifdef", "ifndef"):
            self.open_group(joined)
        elif word in ("elif", "else"):
            gid = self.stack[-1][0]
            self.groups[gid].append(joined)
            self.stack[-1] = (gid, len(self.groups[gid]) - 1)
        elif word == "endif":
            self.stack.pop()
            if self.owner is not None and len(self.stack) < self.owner_depth:
                self.owner = None
        elif word == "define":
            self.define(at, directive, joined)
        elif word not in ("include", "error", "warning", "pragma", "undef"):
            self.fail(at, "a directive the reader does not know: " + joined)
        return last + 1

    def open_group(self, joined):
        """Open the #if group of a directive. The first #ifndef of a name that ends in _H is the include guard."""
        gid = len(self.groups)
        self.groups[gid] = [joined]
        named = re.match(r"#\s*ifndef\s+(\w+)$", joined)
        if self.guard is None and named and named.group(1).endswith("_H"):
            self.guard = gid
            self.guard_name = named.group(1)
        elif self.pending is not None and named:
            # A block above a #ifndef of a name documents that name in every arm below it
            self.new_entry(self.pending, name=named.group(1), type="macro")
            self.pending = None
            self.owner = None
        self.stack.append((gid, 0))

    def define(self, at, directive, joined):
        """Read a #define, but the define of the include guard."""
        name = re.match(r"#\s*define\s+(\w+)", joined).group(1)
        if name == self.guard_name:
            return
        decl = Decl("macro", name, tidy_macro(directive), at, self.arms(self.stack, self.guard))
        if any("/**" in part for part in directive):
            self.template(name, directive)
        self.attach(decl)

    def finish(self):
        """Give each declaration with no entry yet to the entry of its name, and test that each entry has one."""
        if self.pending is not None:
            self.fail(self.pending.line, "a Doxygen block with no declaration under it")
        for decl in self.orphans:
            if decl.name in self.by_name:
                self.place(decl, self.by_name[decl.name])
        for entry in self.entries:
            entry.decls.sort(key=lambda decl: decl.line)
            if not entry.decls:
                self.fail(entry.blocks[0].line, "a block for %s, which nothing here declares" % entry.names[0])

    def arms(self, stack, guard):
        return [arm for arm in stack if arm[0] != guard]

    def attach(self, decl):
        """Give the declaration to its entry, and move on the block that waits and the owner."""
        if decl.type == "invocation":
            self.expand(decl)
            return
        if self.pending is not None:
            self.owner = self.new_entry(self.pending, decl=decl, name=decl.name, type=decl.type)
            self.owner_depth = self.pending.depth
            self.pending = None
            return
        if decl.name in self.by_name:
            self.place(decl, self.by_name[decl.name])
        elif self.owner is not None:
            self.place(decl, self.owner)
        else:
            self.orphans.append(decl)

    def statement(self, at, stack, guard):
        """The declaration that starts at line at, and the line after it."""
        text = self.lines[at].strip()
        arms = self.arms(stack, guard)
        if re.match(r"typedef\s+(struct|enum|union)\b", text) and not text.rstrip().endswith(";"):
            return self.record(at, arms)
        if text.startswith("typedef"):
            return self.typedef(at, arms)
        if text.startswith("static const"):
            return self.table(at, arms)
        head = re.match(r"([A-Z][A-Z0-9_]*)\s*\(", text)
        body, end = self.parenthesized(at)
        joined = "\n".join(part.rstrip() for part in body)
        if head:
            return self.invocation(head.group(1), at, end, body, joined, arms)
        if "(" in text:
            return self.function(at, end, joined, arms)
        self.fail(at, "a line the reader cannot place: " + text)
        return None, end + 1

    def record(self, at, arms):
        """The struct, enum or union typedef that starts at `at`, and the line after it."""
        start = at
        depth = 0
        body = []
        while True:
            body.append(self.lines[at])
            depth += code_only(self.lines[at]).count("{") - code_only(self.lines[at]).count("}")
            if depth == 0 and "}" in self.lines[at] and ";" in self.lines[at]:
                break
            at += 1
        name = IDENT.findall(code_only(body[-1]).split("}")[-1])[-1]
        return Decl("type", name, "\n".join(body), start, arms, members_in(body)), at + 1

    def typedef(self, at, arms):
        """The typedef of no body that starts at `at`, and the line after it."""
        start = at
        body = [self.lines[at]]
        while not code_only(body[-1]).rstrip().endswith(";"):
            at += 1
            body.append(self.lines[at])
        joined = "\n".join(part.rstrip() for part in body)
        return Decl("type", typedef_name(joined), joined, start, arms), at + 1

    def table(self, at, arms):
        """The static const dispatch table that starts at `at`, and the line after it."""
        start = at
        body = [self.lines[at]]
        while "};" not in code_only(body[-1]):
            at += 1
            body.append(self.lines[at])
        name = IDENT.findall(code_only(body[0]).split("=")[0])[-1]
        return Decl("table", name, "\n".join(part.rstrip() for part in body), start, arms), at + 1

    def parenthesized(self, at):
        """The lines from `at` to the line that closes its parentheses, and the index of that line."""
        body = [self.lines[at]]
        depth = code_only(self.lines[at]).count("(") - code_only(self.lines[at]).count(")")
        while depth > 0:
            at += 1
            body.append(self.lines[at])
            depth += code_only(self.lines[at]).count("(") - code_only(self.lines[at]).count(")")
        return body, at

    def invocation(self, macro, start, at, body, joined, arms):
        """The declaration that a macro invocation makes, and the line after it."""
        lines = self.lines
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
        # A table layout asserts the slots of the type above it. A table invocation defines a table, and a block in
        # the body of its macro documents that table.
        if macro in ("EMBED_BUILD_WARNING", "EMBED_TABLE_LAYOUT", "EMBED_ATOMIC_TABLE"):
            return None, at + 1
        self.fail(start, "a macro invocation the reader does not know: " + macro)
        return None, at + 1

    def function(self, start, at, joined, arms):
        """The function declaration or definition from `start`, and the line after it. A body is not part of it."""
        lines = self.lines
        signature = joined
        if not code_only(lines[at]).rstrip().endswith(";"):
            if at + 1 >= len(lines) or lines[at + 1].strip() != "{":
                self.fail(start, "a function with no body and no semicolon")
            at = self.body_end(at + 1)
            signature += ";"
        name = called_name(code_only(joined))
        if name is None:
            self.fail(start, "a function with no name before its parenthesis")
        return Decl("function", name, signature, start, arms), at + 1

    def body_end(self, at):
        """The line that closes the braces that open on line `at`."""
        depth = 0
        while True:
            depth += code_only(self.lines[at]).count("{") - code_only(self.lines[at]).count("}")
            if depth == 0:
                return at
            at += 1

    def template(self, name, directive):
        """Retain a macro whose body holds Doxygen blocks, for the functions its invocations write."""
        head = re.match(r"\s*#\s*define\s+\w+\(([^)]*)\)", directive[0])
        params = [part.strip() for part in head.group(1).split(",")]
        body = [without_continuation(part) for part in directive[1:]]
        self.templates[name] = (params, body)

    def expand_type(self, decl, params, args, comment, body, at, index):
        """The entry of a typedef in a macro body, for the type each invocation writes, and the line after it."""
        lines = [body[at]]
        if not code_only(body[at]).rstrip().endswith(";"):
            while not (lines[-1].strip().startswith("}") and lines[-1].strip().endswith(";")):
                at += 1
                lines.append(body[at])
        text = substitute("\n".join(line.rstrip() for line in lines), params, args)
        name = IDENT.findall(code_only(text.split("\n")[-1]).split("}")[-1])[-1]
        key = (decl.name, index)
        entry = self.by_name.get(key)
        if entry is None:
            entry = Entry(parse_block(comment, decl.line, 0))
            self.entries.append(entry)
            self.by_name[key] = entry
        entry.add(Decl("type", name, text, decl.line, decl.stack, members_in(text.split("\n"))))
        self.by_name.setdefault(name, entry)
        return at + 1

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
                if body[at].strip().startswith("typedef"):
                    at = self.expand_type(decl, params, args, comment, body, at, index)
                    index += 1
                    continue
                signature = []
                while body[at].strip() != "{":
                    signature.append(body[at].strip())
                    at += 1
                text = substitute(" ".join(signature), params, args) + ";"
                name = called_name(text)
                if name is None:
                    self.fail(decl.line, "a function in %s with no name before its parenthesis" % decl.name)
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


# The Markdown of the pages


def known_names(headers):
    """Every entry name of every header, to the page and the anchor that show it."""
    table = {}
    for header in headers:
        for entry in header.entries:
            if entry.type == "assert":
                continue
            for name in entry.names:
                table.setdefault(name, (header.slug, entry.names[0]))
    return table


# The forms of a code token in prose, in the order that each position tries them. A form with a test is a token
# only where the test passes. In order: a directive, a header, a compiler option, a name with an underscore and
# its call, a call, a hex number, and a power.
CODE_FORMS = (
    (re.compile(r"#\w+"), None),
    (re.compile(r"<([\w./]+)>"), lambda hit: len(hit.group(1)) >= 3 and hit.group(1).endswith(".h")),
    (re.compile(r"(?<![\w-])(?:-W|--|-m)[\w-]+"), None),
    (re.compile(r"\b(\w+)\b(?:\(\w*\))?"), lambda hit: "_" in hit.group(1)),
    (re.compile(r"\b\w+\(\)"), None),
    (re.compile(r"\b0x[0-9A-Fa-f]\w*"), None),
    (re.compile(r"\d+\^\w+"), None),
)


def code_token_end(part, at):
    """The end of the code token that starts at `at` in the part, or None where no token starts there."""
    for form, test in CODE_FORMS:
        hit = form.match(part, at)
        if hit and (test is None or test(hit)):
            return hit.end()
    return None


def code_split(part):
    """The part split at each code token, the tokens at the odd places, from the left and with no overlap."""
    pieces = []
    done = 0
    at = 0
    while at < len(part):
        end = code_token_end(part, at)
        if end is None:
            at += 1
            continue
        pieces.extend([part[done:at], part[at:end]])
        done = at = end
    pieces.append(part[done:])
    return pieces


def prose(text, names, slug):
    """Doxygen text as Markdown: code as code spans, a known name as a link, the Markdown marks escaped."""
    out = []
    for at, part in enumerate(re.split(r"(`[^`]*`)", text)):
        if at % 2 == 1:
            out.append(link(part[1:-1], names, slug))
            continue
        pieces = []
        for index, piece in enumerate(code_split(part)):
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


def common_arms(current, target):
    """The count of #if arms at the start of two stacks that are the same."""
    same = 0
    while same < len(current) and same < len(target) and current[same] == target[same]:
        same += 1
    return same


def arm_lines(current, target, groups):
    """The directives that close the #if arms of current that target leaves, and open the arms that it enters."""
    out = []
    same = common_arms(current, target)
    if same < len(current) and same < len(target) and current[same][0] == target[same][0]:
        out.extend([ENDIF] * (len(current) - same - 1))
        gid = target[same][0]
        out.extend(groups[gid][arm] for arm in range(current[same][1] + 1, target[same][1] + 1))
        opened = same + 1
    else:
        out.extend([ENDIF] * (len(current) - same))
        opened = same
    for gid, arm in target[opened:]:
        out.extend(groups[gid][index] for index in range(0, arm + 1))
    return out


def skeleton(decls, groups):
    """The declarations of one entry in the #if arms they are in, as C."""
    out = []
    current = []
    for decl in decls:
        target = list(decl.stack)
        out.extend(arm_lines(current, target, groups))
        current = target
        out.append(decl.text)
    out.extend([ENDIF] * len(current))
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
        for type, text in block.notes:
            label = "_**Warning:**_" if type == "warning" else "**Note:**"
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
        for type, title in SECTIONS:
            entries = [entry for entry in header.entries if entry.type == type]
            if not entries:
                continue
            self.emit("## %s" % title)
            self.emit()
            for entry in entries:
                self.entry(type, entry)

    def entry(self, type, entry):
        """The heading, the C, the blocks and the members of one entry."""
        header = self.header
        if type == "assert":
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
