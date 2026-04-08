#!/usr/bin/env python3
"""
html2text: Convert HTML to searchable plain text.

Library usage:
    from html2text import convert_html_to_text, convert_html_file_to_text

    # Convert HTML string
    text = convert_html_to_text(html_string)

    # Convert file
    text = convert_html_file_to_text("input.html", "output.txt")

    # Use Trafilatura fallback (if main method returns insufficient text)
    from html2text import convert_html_file_with_trafilatura
    fallback_text = convert_html_file_with_trafilatura("input.html")

CLI usage:
    python html2text.py input.html -o output.txt
    python html2text.py ./docs -d ./text_docs
    cat input.html | python html2text.py > output.txt

Design principles:
  - Custom regex method (Primary): Ideal for technical docs. Inline elements (span, a, code, etc.) 
    produce NO separator, preserving code tokens like "std::move".
  - Trafilatura method (Fallback): Best for generic articles. Used when the custom method 
    fails to extract meaningful content.
"""

import re
import sys
import argparse
from pathlib import Path
from typing import Set


# Block-level elements: produce line breaks
BLOCK_ELEMENTS: Set[str] = {
    "address", "article", "aside", "blockquote", "canvas", "dd", "details",
    "dialog", "div", "dl", "dt", "fieldset", "figcaption", "figure", "footer",
    "form", "h1", "h2", "h3", "h4", "h5", "h6", "header", "hgroup", "hr",
    "li", "main", "nav", "noscript", "ol", "output", "p", "pre", "section",
    "summary", "table", "tbody", "td", "tfoot", "th", "thead", "tr", "ul",
    "video", "audio",
}

# Elements whose content should be completely ignored
IGNORED_ELEMENTS: Set[str] = {
    "script", "style", "template", "svg", "math",
}

# Void/self-closing elements
VOID_ELEMENTS: Set[str] = {
    "area", "base", "br", "col", "embed", "hr", "img", "input",
    "link", "meta", "param", "source", "track", "wbr",
}

# Common HTML entities
ENTITIES = {
    "amp": "&", "lt": "<", "gt": ">", "quot": '"', "apos": "'",
    "nbsp": " ", "ndash": "–", "mdash": "—", "lsquo": "'", "rsquo": "'",
    "ldquo": '"', "rdquo": '"', "bull": "•", "hellip": "…",
    "copy": "©", "reg": "®", "trade": "™", "deg": "°",
    "plusmn": "±", "times": "×", "divide": "÷", "ne": "≠",
    "le": "≤", "ge": "≥", "larr": "←", "rarr": "→", "uarr": "↑", "darr": "↓",
    "middot": "·", "sect": "§", "para": "¶", "dagger": "†", "Dagger": "‡",
    "permil": "‰", "prime": "′", "Prime": "″", "euro": "€", "pound": "£",
    "yen": "¥", "cent": "¢", "infin": "∞", "radic": "√",
    "sum": "∑", "prod": "∏", "int": "∫", "part": "∂",
    "alpha": "α", "beta": "β", "gamma": "γ", "delta": "δ", "epsilon": "ε",
    "theta": "θ", "lambda": "λ", "mu": "μ", "pi": "π", "sigma": "σ",
    "omega": "ω", "Omega": "Ω", "Delta": "Δ", "Sigma": "Σ",
}


def decode_entity(entity: str) -> str:
    """Decode a single HTML entity like &amp; or &#60; or &#x3C;"""
    if entity.startswith("&#x") or entity.startswith("&#X"):
        try:
            return chr(int(entity[3:-1], 16))
        except (ValueError, OverflowError):
            return entity
    elif entity.startswith("&#"):
        try:
            return chr(int(entity[2:-1]))
        except (ValueError, OverflowError):
            return entity
    else:
        name = entity[1:-1]
        return ENTITIES.get(name, entity)


def decode_entities(text: str) -> str:
    """Decode all HTML entities in text."""
    return re.sub(r'&(#[xX]?[0-9a-fA-F]+|\w+);', lambda m: decode_entity(m.group(0)), text)


def convert_html_to_text(html: str) -> str:
    """
    Convert HTML to searchable plain text using custom regex parsing.
    Ideal for technical documentation as it preserves code tokens strictly.

    Examples:
        >>> convert_html_to_text('std::<span class="kw">move</span>(x)')
        'std::move(x)'

        >>> convert_html_to_text('<p>hello</p><p>world</p>')
        'hello\\nworld'
    """
    # Tokenize: split HTML into tags and text
    token_pattern = re.compile(
        r'(<!--.*?-->|'           # Comments
        r'<!\[CDATA\[.*?\]\]>|'   # CDATA
        r'<!DOCTYPE[^>]*>|'       # DOCTYPE
        r'<[^>]+>)',              # Tags
        re.DOTALL | re.IGNORECASE
    )

    parts = []
    ignore_depth = 0      # Depth inside ignored elements (script, style, etc.)
    pre_depth = 0         # Depth inside <pre> elements
    need_separator = False  # Whether we need a separator before next text

    last_end = 0
    for match in token_pattern.finditer(html):
        start, end = match.start(), match.end()

        # Process text before this token
        if start > last_end:
            text = html[last_end:start]
            if ignore_depth == 0:
                text = decode_entities(text)
                if pre_depth > 0:
                    # Inside <pre>: preserve whitespace exactly
                    if text:
                        if need_separator and parts and not parts[-1].endswith('\n'):
                            parts.append('\n')
                        parts.append(text)
                        need_separator = False
                else:
                    # Normal flow: will normalize whitespace later
                    if text.strip():
                        if need_separator and parts:
                            parts.append('\n')
                        parts.append(text)
                        need_separator = False

        # Process the token
        token = match.group(1)

        # Skip comments, CDATA content extraction, DOCTYPE
        if token.startswith('<!--') or token.startswith('<!DOCTYPE'):
            last_end = end
            continue

        if token.startswith('<![CDATA['):
            # Extract CDATA content
            if ignore_depth == 0:
                content = token[9:-3]
                if content:
                    if need_separator and parts:
                        parts.append('\n')
                    parts.append(content)
                    need_separator = False
            last_end = end
            continue

        # It's a tag
        tag_match = re.match(r'<(/?)([a-zA-Z][a-zA-Z0-9]*)', token)
        if not tag_match:
            last_end = end
            continue

        is_closing = tag_match.group(1) == '/'
        tag_name = tag_match.group(2).lower()
        is_self_closing = token.rstrip().endswith('/>')

        if not is_closing:
            # Opening tag
            if tag_name in IGNORED_ELEMENTS:
                ignore_depth += 1
            elif ignore_depth == 0:
                if tag_name in BLOCK_ELEMENTS:
                    need_separator = True
                if tag_name == "pre":
                    pre_depth += 1
                if tag_name == "br":
                    parts.append('\n')
                    need_separator = False
        else:
            # Closing tag
            if tag_name in IGNORED_ELEMENTS:
                ignore_depth = max(0, ignore_depth - 1)
            elif ignore_depth == 0:
                if tag_name == "pre":
                    pre_depth = max(0, pre_depth - 1)
                if tag_name in BLOCK_ELEMENTS:
                    need_separator = True

        last_end = end

    # Process remaining text after last token
    if last_end < len(html):
        text = html[last_end:]
        if ignore_depth == 0:
            text = decode_entities(text)
            if text.strip():
                if need_separator and parts:
                    parts.append('\n')
                parts.append(text)

    # Join and normalize whitespace
    result = ''.join(parts)

    # Normalize whitespace (but preserve intentional newlines)
    result = result.replace('\r\n', '\n').replace('\r', '\n')

    # Collapse spaces/tabs (but not newlines) into single space
    result = re.sub(r'[ \t]+', ' ', result)

    # Remove spaces adjacent to newlines
    result = re.sub(r' *\n *', '\n', result)

    # Collapse multiple newlines into at most two (one blank line)
    result = re.sub(r'\n{3,}', '\n\n', result)

    # Strip leading/trailing whitespace
    result = result.strip()

    return result


def convert_html_file_to_text(input_path: str, output_path: str = None) -> str:
    """Process a single HTML file and return the plain text using the primary method."""
    with open(input_path, 'r', encoding='utf-8', errors='replace') as f:
        html = f.read()

    text = convert_html_to_text(html)

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(text)

    return text


def convert_html_file_with_trafilatura(filepath: str) -> str:
    """
    Fallback mechanism for processing HTML using trafilatura.
    Used when the primary regex method yields poor/empty results.
    """
    try:
        import trafilatura
    except ImportError:
        print("Warning: trafilatura is not installed. Fallback will return empty string.", file=sys.stderr)
        return ""

    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        html_content = f.read()

    text = trafilatura.extract(
        html_content,
        include_comments=False,
        include_tables=True,
        no_fallback=False,
        output_format='txt'
    )
    
    return text if text else ""


def convert_directory_to_text(input_dir: str, output_dir: str,
                              extensions: tuple = ('.html', '.htm', '.xhtml')) -> int:
    """
    Process all HTML files in a directory recursively.
    Returns the number of files processed.
    """
    input_path = Path(input_dir)
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    count = 0
    for html_file in input_path.rglob('*'):
        if html_file.is_file() and html_file.suffix.lower() in extensions:
            rel_path = html_file.relative_to(input_path)
            out_file = output_path / rel_path.with_suffix('.txt')
            out_file.parent.mkdir(parents=True, exist_ok=True)

            try:
                convert_html_file_to_text(str(html_file), str(out_file))
                count += 1
                print(f"Processed: {rel_path}")
            except Exception as e:
                print(f"Error: {rel_path}: {e}", file=sys.stderr)

    return count


def run_tests():
    """Run self-tests to verify correctness."""
    tests = [
        # (description, html_input, expected_substring, should_not_contain)

        # Core requirement 1: Inline tags don't break code tokens
        ("Inline span preserves std::move",
         'std::<span class="kw">move</span>(x)',
         "std::move(x)", None),

        ("Inline bold preserves std::vector",
         '<code>std::<b>vector</b>&lt;int&gt;</code>',
         "std::vector<int>", None),

        ("Multiple inline tags",
         '<em><strong><code>text</code></strong></em>',
         "text", None),

        ("Adjacent inline elements concatenate",
         '<a>hello</a><span>world</span>',
         "helloworld", None),

        # Core requirement 2: Block tags create separators
        ("Block p creates separator",
         '<p>hello</p><p>world</p>',
         "hello\nworld", "helloworld"),

        ("Block div creates separator",
         '<div>aaa</div><div>bbb</div>',
         None, "aaabbb"),

        ("List items separated",
         '<ul><li>item1</li><li>item2</li></ul>',
         None, "item1item2"),

        # HTML entities
        ("Entity &amp;",
         'a &amp; b',
         "a & b", None),

        ("Entity &lt; &gt;",
         'std::vector&lt;int&gt;',
         "std::vector<int>", None),

        ("Numeric entity &#60;",
         'x &#60; y',
         "x < y", None),

        ("Hex entity &#x3E;",
         'a &#x3E; b',
         "a > b", None),

        # Ignored elements
        ("Script content ignored",
         '<p>visible</p><script>var x = "hidden";</script><p>also</p>',
         "visible", "hidden"),

        ("Style content ignored",
         '<style>.x{color:red}</style><p>content</p>',
         "content", "color"),

        # PRESERVED elements (nav, header, footer, aside, etc.)
        ("Nav content PRESERVED",
         '<nav><h2>Contents</h2><ul><li>Item 1</li></ul></nav>',
         "Contents", None),

        ("Nav content PRESERVED 2",
         '<nav><h2>Contents</h2><ul><li>Item 1</li></ul></nav>',
         "Item 1", None),

        ("Header content PRESERVED",
         '<header><h1>Page Title</h1></header>',
         "Page Title", None),

        ("Footer content PRESERVED",
         '<footer><p>Copyright 2024</p></footer>',
         "Copyright 2024", None),

        ("Aside content PRESERVED",
         '<aside><p>Related info</p></aside>',
         "Related info", None),

        # Whitespace handling
        ("Collapse multiple spaces",
         '<p>hello    world</p>',
         "hello world", "hello    world"),

        ("BR creates newline",
         'line1<br>line2<br/>line3',
         "line1\nline2\nline3", None),

        # Complex real-world patterns
        ("C++ template with highlighting",
         '<span class="k">template</span>&lt;<span class="k">typename</span> '
         '<span class="n">T</span>&gt;',
         "template<typename T>", None),

        ("cppreference std::move style",
         '<span class="n">std</span><span class="o">::</span><span class="n">move</span>',
         "std::move", None),

        # Custom tag names (like c- tags in the example)
        ("Custom c- tags for code highlighting",
         '<c- n>std</c-><c- o>::</c-><c- n>execution</c->',
         "std::execution", None),

        ("Custom c- tags complex",
         '<code class="highlight"><c- n>std</c-><c- o>::</c-><c- n>move</c-></code>',
         "std::move", None),

        # Edge cases
        ("Empty HTML",
         '',
         "", None),

        ("Only tags",
         '<div><p></p></div>',
         "", None),

        ("Unicode content",
         '<p>你好 Hello 🌍</p>',
         "你好 Hello 🌍", None),

        # Pre block
        ("Pre preserves formatting",
         '<pre>  code\n    indented</pre>',
         "code\n    indented", None),

        # TOC structure preservation
        ("TOC structure preserved",
         '''<nav id="toc">
            <h2>Table of Contents</h2>
            <ol class="toc">
              <li><a href="#intro">1. Introduction</a></li>
              <li><a href="#design">2. Design</a></li>
            </ol>
         </nav>''',
         "Table of Contents", None),

        ("TOC links preserved",
         '''<nav id="toc">
            <ol class="toc">
              <li><a href="#intro">1. Introduction</a></li>
            </ol>
         </nav>''',
         "Introduction", None),

        # Technical document with mixed content
        ("Technical doc structure",
         '''<body>
         <nav id="toc"><h2>Contents</h2><ol><li>Section 1</li></ol></nav>
         <main>
           <h1>Title</h1>
           <p>Paragraph with <code>std::move</code> in it.</p>
           <pre><code>
int main() {
    return 0;
}
           </code></pre>
         </main>
         </body>''',
         "std::move", None),

        ("Technical doc preserves TOC",
         '''<nav id="toc"><h2>Contents</h2><ol><li>Section 1</li></ol></nav>
         <main><p>Body text</p></main>''',
         "Contents", None),
    ]

    passed = 0
    failed = 0

    print("Running tests...\n")

    for desc, html_input, expected, should_not_contain in tests:
        result = convert_html_to_text(html_input)

        ok = True
        details = []

        if expected is not None and expected not in result:
            ok = False
            details.append(f"Expected '{expected}' not found")

        if should_not_contain is not None and should_not_contain in result:
            ok = False
            details.append(f"Should not contain '{should_not_contain}'")

        if ok:
            passed += 1
            print(f"✅ {desc}")
        else:
            failed += 1
            print(f"❌ {desc}")
            print(f"   Input: {repr(html_input[:80])}")
            print(f"   Output: {repr(result[:80])}")
            for d in details:
                print(f"   {d}")

    print(f"\nResults: {passed}/{passed + failed} passed")

    if failed > 0:
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Convert HTML files to searchable plain text',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Convert a single file, print to stdout
  %(prog)s file.html

  # Convert a single file to output.txt
  %(prog)s file.html -o output.txt

  # Convert all HTML files in a directory
  %(prog)s ./html_docs -d ./text_output

  # Read from stdin
  cat page.html | %(prog)s > output.txt

  # Run self-tests
  %(prog)s --test
'''
    )

    parser.add_argument('input', nargs='?', help='Input HTML file or directory')
    parser.add_argument('-o', '--output', help='Output file (for single file mode)')
    parser.add_argument('-d', '--output-dir', help='Output directory (for directory mode)')
    parser.add_argument('--test', action='store_true', help='Run self-tests')
    parser.add_argument('--ext', nargs='+', default=['.html', '.htm', '.xhtml'],
                        help='File extensions to process (default: .html .htm .xhtml)')

    args = parser.parse_args()

    if args.test:
        run_tests()
        return

    if not args.input:
        if sys.stdin.isatty():
            parser.print_help()
            return
        # Read from stdin
        html = sys.stdin.read()
        print(convert_html_to_text(html))
        return

    input_path = Path(args.input)

    if not input_path.exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)

    if input_path.is_file():
        # Single file mode
        text = convert_html_file_to_text(str(input_path), args.output)
        if not args.output:
            print(text)
    elif input_path.is_dir():
        # Directory mode
        if not args.output_dir:
            print("Error: --output-dir required for directory input", file=sys.stderr)
            sys.exit(1)
        count = convert_directory_to_text(str(input_path), args.output_dir, tuple(args.ext))
        print(f"\nProcessed {count} files")
    else:
        print(f"Error: {args.input} is not a file or directory", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()