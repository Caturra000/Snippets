#!/usr/bin/env python3
"""
html2html: Clean HTML by removing non-content elements for LLM processing.

Library usage:
    from html2html import clean, clean_file, clean_directory

    # Clean HTML string
    cleaned = clean(html_string)

    # Clean with options
    cleaned = clean(html_string, aggressive=True, minify=True)

    # Clean file
    cleaned = clean_file("input.html", "output.html")

    # Clean directory
    count = clean_directory("./input", "./output")

CLI usage:
    python html2html.py input.html -o output.html
    python html2html.py ./docs -d ./clean_docs
    cat input.html | python html2html.py > output.html
"""

import re
import sys
import argparse
from pathlib import Path
from typing import Set, List, Optional, Union
from html.parser import HTMLParser
from dataclasses import dataclass, field


__all__ = ['clean', 'clean_file', 'clean_directory', 'HTMLCleaner']
__version__ = '1.0.0'


# ============================================================================
# Configuration
# ============================================================================

# Tags to completely remove (including all content)
REMOVE_TAGS: Set[str] = {
    "script", "style", "noscript",
    "form", "input", "button", "select", "textarea", "option", "optgroup",
    "iframe", "frame", "frameset", "object", "embed", "applet",
    "video", "audio", "source", "track", "canvas",
    "svg", "math",
    "template", "slot",
    "map", "area",
}

# Tags to unwrap (remove tag but keep content)
UNWRAP_TAGS: Set[str] = {
    "font", "center", "big", "small",
    "nobr", "wbr", "bdi", "bdo",
}

# Void/self-closing tags
VOID_TAGS: Set[str] = {
    "br", "hr", "img", "area", "base", "col", "embed",
    "input", "link", "meta", "param", "source", "track", "wbr",
}

# Attributes to preserve
KEEP_ATTRIBUTES: Set[str] = {
    "href", "src", "alt", "title",
    "id", "name",
    "colspan", "rowspan",
    "lang", "dir",
    "datetime", "cite",
    "data-level", "data-md",
}

# Patterns for aggressive removal
REMOVE_PATTERNS: List[str] = [
    r"^ad[-_]?box", r"^ad[-_]?container", r"^ad[-_]?wrapper", r"^advert",
    r"^sponsor", r"^banner[-_]?ad",
    r"google[-_]?analytics", r"tracking[-_]?pixel",
    r"^modal$", r"^popup$", r"^overlay$", r"^lightbox$",
    r"cookie[-_]?(notice|banner|consent)", r"gdpr[-_]?consent",
    r"share[-_]?buttons?$", r"social[-_]?share$",
    r"^hidden$", r"^hide$", r"^sr[-_]only$", r"visually[-_]hidden",
    r"^d[-_]none$",
]

REMOVE_REGEX = re.compile(
    r'(?:^|\s)(' + '|'.join(REMOVE_PATTERNS) + r')(?:\s|$)',
    re.IGNORECASE
)


# ============================================================================
# Data Structures
# ============================================================================

@dataclass
class Element:
    """Represents an HTML element."""
    tag: str
    attrs: dict
    children: List[Union['Element', str]] = field(default_factory=list)
    parent: Optional['Element'] = None
    is_removed: bool = False

    def add_child(self, child: Union['Element', str]):
        if isinstance(child, Element):
            child.parent = self
        self.children.append(child)

    def get_text(self) -> str:
        texts = []
        for child in self.children:
            if isinstance(child, str):
                texts.append(child)
            elif isinstance(child, Element) and not child.is_removed:
                texts.append(child.get_text())
        return ''.join(texts)


# ============================================================================
# HTML Cleaner
# ============================================================================

class HTMLCleaner(HTMLParser):
    """
    Parse and clean HTML with conservative removal strategy.

    Usage:
        cleaner = HTMLCleaner()
        cleaner.feed(html_string)
        cleaned = cleaner.get_html()
    """

    def __init__(self,
                 remove_empty: bool = True,
                 remove_comments: bool = True,
                 simplify_whitespace: bool = True,
                 aggressive: bool = False):
        super().__init__(convert_charrefs=True)
        self.root = Element(tag='root', attrs={})
        self.current = self.root
        self.remove_empty = remove_empty
        self.remove_comments = remove_comments
        self.simplify_whitespace = simplify_whitespace
        self.aggressive = aggressive
        self.ignore_depth = 0

    def reset_state(self):
        """Reset parser state for reuse."""
        self.root = Element(tag='root', attrs={})
        self.current = self.root
        self.ignore_depth = 0
        self.reset()

    def handle_starttag(self, tag: str, attrs: list):
        tag = tag.lower()
        attrs_dict = dict(attrs)

        if self.ignore_depth > 0:
            if tag not in VOID_TAGS:
                self.ignore_depth += 1
            return

        if self._should_remove_tag(tag, attrs_dict):
            self.ignore_depth = 1
            return

        clean_attrs = self._clean_attributes(attrs_dict)
        element = Element(tag=tag, attrs=clean_attrs)
        self.current.add_child(element)

        if tag not in VOID_TAGS:
            self.current = element

    def handle_endtag(self, tag: str):
        tag = tag.lower()

        if self.ignore_depth > 0:
            self.ignore_depth -= 1
            return

        node = self.current
        while node and node.tag != 'root':
            if node.tag == tag:
                self.current = node.parent if node.parent else self.root
                return
            node = node.parent

    def handle_data(self, data: str):
        if self.ignore_depth > 0:
            return

        if self.simplify_whitespace and not self._in_pre():
            data = re.sub(r'[ \t]+', ' ', data)

        if data:
            self.current.add_child(data)

    def handle_comment(self, data: str):
        if not self.remove_comments:
            self.current.add_child(f'<!--{data}-->')

    def handle_decl(self, decl: str):
        if decl.lower().startswith('doctype'):
            self.current.add_child(f'<!{decl}>')

    def _in_pre(self) -> bool:
        node = self.current
        while node:
            if node.tag in ('pre', 'code', 'textarea'):
                return True
            node = node.parent
        return False

    def _should_remove_tag(self, tag: str, attrs: dict) -> bool:
        if tag in REMOVE_TAGS:
            return True

        if attrs.get('hidden') is not None:
            return True

        style = attrs.get('style', '').replace(' ', '').lower()
        if 'display:none' in style or 'visibility:hidden' in style:
            return True

        if attrs.get('aria-hidden') == 'true':
            return True

        if self.aggressive and tag in ('div', 'span', 'aside'):
            class_str = attrs.get('class', '')
            id_str = attrs.get('id', '')
            combined = f" {class_str} {id_str} "
            if REMOVE_REGEX.search(combined):
                return True

        return False

    def _clean_attributes(self, attrs: dict) -> dict:
        result = {}
        for k, v in attrs.items():
            k_lower = k.lower()
            if k_lower in KEEP_ATTRIBUTES:
                result[k_lower] = v
            elif k_lower.startswith('data-') and k_lower in KEEP_ATTRIBUTES:
                result[k_lower] = v
        return result

    def get_html(self) -> str:
        """Get the cleaned HTML output."""
        if self.remove_empty:
            self._mark_empty_elements(self.root)
        return self._serialize(self.root)

    def _mark_empty_elements(self, element: Element):
        for child in element.children:
            if isinstance(child, Element):
                self._mark_empty_elements(child)

        keep_even_empty = {
            'br', 'hr', 'img', 'root', 'td', 'th', 'li',
            'html', 'head', 'body', 'main', 'article', 'section',
            'header', 'footer', 'nav', 'aside',
        }

        if element.tag not in keep_even_empty:
            has_content = any(
                (isinstance(c, str) and c.strip()) or
                (isinstance(c, Element) and not c.is_removed)
                for c in element.children
            )
            if not has_content:
                element.is_removed = True

    def _serialize(self, element: Element) -> str:
        if element.tag == 'root':
            return ''.join(self._serialize_child(c) for c in element.children)

        if element.is_removed:
            return ''

        if element.tag in UNWRAP_TAGS:
            return ''.join(self._serialize_child(c) for c in element.children)

        attrs_str = ''.join(
            f' {k}="{str(v).replace("&", "&amp;").replace(chr(34), "&quot;")}"'
            if v is not None else f' {k}'
            for k, v in element.attrs.items()
        )

        if element.tag in VOID_TAGS:
            return f'<{element.tag}{attrs_str}>'

        content = ''.join(self._serialize_child(c) for c in element.children)
        return f'<{element.tag}{attrs_str}>{content}</{element.tag}>'

    def _serialize_child(self, child: Union[Element, str]) -> str:
        if isinstance(child, str):
            return child
        if isinstance(child, Element) and not child.is_removed:
            return self._serialize(child)
        return ''


# ============================================================================
# Public API
# ============================================================================

def clean(html: str,
          remove_empty: bool = True,
          remove_comments: bool = True,
          simplify_whitespace: bool = True,
          aggressive: bool = False,
          minify: bool = False) -> str:
    """
    Clean HTML by removing non-content elements.

    Args:
        html: Input HTML string
        remove_empty: Remove elements with no content (default: True)
        remove_comments: Remove HTML comments (default: True)
        simplify_whitespace: Collapse excessive whitespace (default: True)
        aggressive: More aggressive removal of potential noise (default: False)
        minify: Compact output, remove all extra whitespace (default: False)

    Returns:
        Cleaned HTML string

    Example:
        >>> clean('<p>Hello</p><script>evil()</script>')
        '<p>Hello</p>'
    """
    cleaner = HTMLCleaner(
        remove_empty=remove_empty,
        remove_comments=remove_comments,
        simplify_whitespace=simplify_whitespace,
        aggressive=aggressive,
    )

    cleaner.feed(html)
    result = cleaner.get_html()

    if minify:
        result = re.sub(r'>\s+<', '><', result)
        result = re.sub(r'\s+', ' ', result)
    else:
        result = re.sub(r'\n\s*\n\s*\n', '\n\n', result)

    return result.strip()


def clean_file(input_path: str,
               output_path: str = None,
               **kwargs) -> str:
    """
    Clean an HTML file.

    Args:
        input_path: Path to input HTML file
        output_path: Path to output file (optional, if None returns string only)
        **kwargs: Additional arguments passed to clean()

    Returns:
        Cleaned HTML string

    Example:
        >>> clean_file("input.html", "output.html")
        >>> clean_file("input.html", aggressive=True)
    """
    with open(input_path, 'r', encoding='utf-8', errors='replace') as f:
        html = f.read()

    result = clean(html, **kwargs)

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(result)

    return result


def clean_directory(input_dir: str,
                    output_dir: str,
                    extensions: tuple = ('.html', '.htm', '.xhtml'),
                    **kwargs) -> int:
    """
    Clean all HTML files in a directory recursively.

    Args:
        input_dir: Path to input directory
        output_dir: Path to output directory
        extensions: File extensions to process (default: .html, .htm, .xhtml)
        **kwargs: Additional arguments passed to clean()

    Returns:
        Number of files processed

    Example:
        >>> clean_directory("./docs", "./clean_docs")
        42
    """
    input_path = Path(input_dir)
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)

    count = 0
    for html_file in input_path.rglob('*'):
        if html_file.is_file() and html_file.suffix.lower() in extensions:
            rel_path = html_file.relative_to(input_path)
            out_file = output_path / rel_path
            out_file.parent.mkdir(parents=True, exist_ok=True)

            try:
                clean_file(str(html_file), str(out_file), **kwargs)
                count += 1
                print(f"Cleaned: {rel_path}")
            except Exception as e:
                print(f"Error: {rel_path}: {e}", file=sys.stderr)

    return count


# ============================================================================
# Tests
# ============================================================================

def run_tests():
    """Run self-tests."""
    print("Running tests...\n")
    passed = 0
    failed = 0

    def test(name: str, html_in: str, should_contain: list = None,
             should_not_contain: list = None, **kwargs):
        nonlocal passed, failed
        try:
            result = clean(html_in, **kwargs)
        except Exception as e:
            failed += 1
            print(f"❌ {name}")
            print(f"   Exception: {e}")
            return

        ok = True
        details = []

        for s in (should_contain or []):
            if s not in result:
                ok = False
                details.append(f"Missing: {repr(s)}")

        for s in (should_not_contain or []):
            if s in result:
                ok = False
                details.append(f"Unexpected: {repr(s)}")

        if ok:
            passed += 1
            print(f"✅ {name}")
        else:
            failed += 1
            print(f"❌ {name}")
            print(f"   Output: {repr(result[:300])}")
            for d in details:
                print(f"   {d}")

    # Basic removal
    test("Remove script",
         '<p>Hello</p><script>alert(1)</script><p>World</p>',
         ['Hello', 'World'], ['script', 'alert'])

    test("Remove style",
         '<style>.x{}</style><p>Content</p>',
         ['Content'], ['style'])

    test("Remove form",
         '<form><input></form><p>Text</p>',
         ['Text'], ['<form>', '<input>'])

    # Content preservation
    test("Keep nav",
         '<nav id="toc"><h2>Contents</h2><ol><li>Item</li></ol></nav>',
         ['<nav', 'Contents', 'Item'])

    test("Keep header",
         '<header class="page-header"><h1>Title</h1></header>',
         ['<header', 'Title'])

    test("Keep aside",
         '<aside class="sidebar"><p>Related</p></aside>',
         ['Related'])

    test("Keep code",
         '<pre><code>std::move(x);</code></pre>',
         ['<pre>', '<code>', 'std::move'])

    test("Keep tables",
         '<table><tr><th>H</th></tr><tr><td>D</td></tr></table>',
         ['<table>', '<tr>', '<th>', '<td>'])

    test("Keep links",
         '<a href="https://example.com">Click</a>',
         ['href="https://example.com"', 'Click'])

    test("Keep img alt",
         '<img src="x.jpg" alt="Photo">',
         ['alt="Photo"'])

    # Hidden elements
    test("Remove hidden",
         '<div hidden>Secret</div><p>Visible</p>',
         ['Visible'], ['Secret'])

    test("Remove display:none",
         '<div style="display:none">Hidden</div><p>Shown</p>',
         ['Shown'], ['Hidden'])

    # Attributes
    test("Remove onclick",
         '<a href="/x" onclick="track()">Link</a>',
         ['href="/x"'], ['onclick'])

    test("Keep id",
         '<h1 id="intro">Intro</h1>',
         ['id="intro"'])

    # Comments
    test("Remove comments",
         '<!-- comment --><p>Text</p>',
         ['Text'], ['<!--'])

    # Aggressive mode
    test("Keep ad-container in normal mode",
         '<div class="ad-container"><p>Content</p></div>',
         ['Content'])

    test("Remove ad-container in aggressive mode",
         '<div class="ad-container">Ad</div><p>Text</p>',
         ['Text'], ['ad-container'], aggressive=True)

    # Technical doc
    test("Technical doc",
         '''<html><body>
         <nav id="toc"><h2>TOC</h2><ol><li>Section 1</li></ol></nav>
         <main><article><h1>Title</h1><p>Content</p>
         <pre><code>code();</code></pre></article></main>
         </body></html>''',
         ['TOC', 'Section 1', 'Title', 'Content', 'code()'])

    # Custom tags (like c- tags in spec documents)
    test("Custom tags",
         '<code><c- n>std</c-><c- o>::</c-><c- n>move</c-></code>',
         ['std', '::', 'move'])

    print(f"\nResults: {passed}/{passed + failed} passed")
    if failed > 0:
        sys.exit(1)


# ============================================================================
# CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='Clean HTML for LLM processing',
        epilog='''
Examples:
  %(prog)s page.html                    # Print cleaned HTML
  %(prog)s page.html -o clean.html      # Save to file
  %(prog)s ./docs -d ./clean            # Process directory
  %(prog)s page.html --aggressive       # Aggressive cleaning
  %(prog)s page.html --minify           # Minified output
  cat page.html | %(prog)s              # From stdin
  %(prog)s --test                       # Run tests
''',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument('input', nargs='?', help='Input HTML file or directory')
    parser.add_argument('-o', '--output', help='Output file')
    parser.add_argument('-d', '--output-dir', help='Output directory')
    parser.add_argument('--minify', action='store_true', help='Minify output')
    parser.add_argument('--aggressive', action='store_true',
                        help='Aggressive noise removal')
    parser.add_argument('--keep-comments', action='store_true',
                        help='Keep HTML comments')
    parser.add_argument('--keep-empty', action='store_true',
                        help='Keep empty elements')
    parser.add_argument('--test', action='store_true', help='Run tests')
    parser.add_argument('--version', action='version', version=f'%(prog)s {__version__}')

    args = parser.parse_args()

    if args.test:
        run_tests()
        return

    kwargs = {
        'minify': args.minify,
        'aggressive': args.aggressive,
        'remove_comments': not args.keep_comments,
        'remove_empty': not args.keep_empty,
    }

    if not args.input:
        if sys.stdin.isatty():
            parser.print_help()
            return
        html = sys.stdin.read()
        print(clean(html, **kwargs))
        return

    input_path = Path(args.input)

    if not input_path.exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)

    if input_path.is_file():
        result = clean_file(str(input_path), args.output, **kwargs)
        if not args.output:
            print(result)
    elif input_path.is_dir():
        if not args.output_dir:
            print("Error: --output-dir required for directory", file=sys.stderr)
            sys.exit(1)
        count = clean_directory(str(input_path), args.output_dir, **kwargs)
        print(f"\nCleaned {count} files")


if __name__ == '__main__':
    main()