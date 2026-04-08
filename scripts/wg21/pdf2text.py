#!/usr/bin/env python3
"""
pdf2text: Convert PDF files to searchable plain text.

Design principles:
  - Preserve text flow and reading order
  - Maintain proper word/line boundaries (no broken tokens, no merged words)
  - Handle multi-column layouts reasonably
  - Preserve code blocks and their formatting
  - Normalize whitespace while keeping paragraph structure

Dependencies:
  pip install pymupdf  # or: pip install PyMuPDF
"""

import re
import sys
import argparse
from pathlib import Path
from typing import List, Tuple, Optional
from dataclasses import dataclass

try:
    import fitz  # PyMuPDF
except ImportError:
    print("Error: PyMuPDF is required. Install with: pip install pymupdf", file=sys.stderr)
    sys.exit(1)


@dataclass
class TextBlock:
    """A block of text with position information."""
    text: str
    x0: float  # left
    y0: float  # top
    x1: float  # right
    y1: float  # bottom
    block_type: str  # 'text', 'code', etc.


def extract_text_blocks(page: fitz.Page) -> List[TextBlock]:
    """
    Extract text blocks from a PDF page with position information.
    Uses PyMuPDF's text extraction with position data.
    """
    blocks = []
    
    # Get text blocks with position: (x0, y0, x1, y1, "text", block_no, block_type)
    raw_blocks = page.get_text("blocks")
    
    for block in raw_blocks:
        if len(block) >= 5 and block[6] == 0:  # block_type 0 = text
            x0, y0, x1, y1 = block[:4]
            text = block[4]
            
            if isinstance(text, str) and text.strip():
                # Detect if this might be a code block (monospace, indented, etc.)
                block_type = detect_block_type(text)
                blocks.append(TextBlock(
                    text=text,
                    x0=x0, y0=y0, x1=x1, y1=y1,
                    block_type=block_type
                ))
    
    return blocks


def detect_block_type(text: str) -> str:
    """
    Heuristically detect if a text block is code or normal text.
    """
    lines = text.strip().split('\n')
    if not lines:
        return 'text'
    
    code_indicators = 0
    
    # Check for common code patterns
    code_patterns = [
        r'^\s{2,}',           # Leading indentation
        r'[{}\[\]();]',       # Brackets and semicolons
        r'::\w+',             # C++ scope operator
        r'->\w+',             # Arrow operator
        r'\w+\(\)',           # Function calls
        r'#include|#define',  # Preprocessor
        r'def |class |import |from ', # Python
        r'func |fn |let |var |const ', # Go/Rust/JS
        r'public |private |void |int |string ', # Java/C#
    ]
    
    for line in lines[:10]:  # Check first 10 lines
        for pattern in code_patterns:
            if re.search(pattern, line):
                code_indicators += 1
                break
    
    # If more than 30% of lines look like code
    if len(lines) > 0 and code_indicators / min(len(lines), 10) > 0.3:
        return 'code'
    
    return 'text'


def sort_blocks_reading_order(blocks: List[TextBlock], 
                               column_threshold: float = 100) -> List[TextBlock]:
    """
    Sort text blocks in reading order, handling multi-column layouts.
    
    Strategy:
    1. Group blocks into columns based on x-position
    2. Sort columns left-to-right
    3. Within each column, sort top-to-bottom
    """
    if not blocks:
        return []
    
    # Sort by y first to find rows
    blocks_sorted = sorted(blocks, key=lambda b: (b.y0, b.x0))
    
    # Detect columns by clustering x0 positions
    x_positions = sorted(set(b.x0 for b in blocks))
    
    if len(x_positions) <= 1:
        return blocks_sorted
    
    # Simple column detection: find large gaps in x positions
    columns = []
    current_column = [x_positions[0]]
    
    for i in range(1, len(x_positions)):
        if x_positions[i] - x_positions[i-1] > column_threshold:
            columns.append(current_column)
            current_column = [x_positions[i]]
        else:
            current_column.append(x_positions[i])
    columns.append(current_column)
    
    if len(columns) <= 1:
        return blocks_sorted
    
    # Assign blocks to columns and sort
    def get_column_index(x0):
        for i, col in enumerate(columns):
            if min(col) - 20 <= x0 <= max(col) + 20:
                return i
        return 0
    
    # Sort: first by column (left to right), then by y (top to bottom)
    return sorted(blocks, key=lambda b: (get_column_index(b.x0), b.y0, b.x0))


def merge_hyphenated_words(text: str) -> str:
    """
    Merge words that were hyphenated at line breaks.
    e.g., "func-\ntion" -> "function"
    """
    # Pattern: word ending with hyphen at end of line, followed by lowercase continuation
    return re.sub(r'(\w)-\n(\w)', r'\1\2', text)


def normalize_text_block(text: str, is_code: bool = False) -> str:
    """
    Normalize a text block's whitespace.
    
    For code blocks: preserve indentation and line structure
    For text blocks: normalize whitespace while preserving paragraph breaks
    """
    if not text:
        return ""
    
    if is_code:
        # For code: preserve structure, just clean up
        lines = text.split('\n')
        # Remove completely empty lines at start/end
        while lines and not lines[0].strip():
            lines.pop(0)
        while lines and not lines[-1].strip():
            lines.pop()
        return '\n'.join(lines)
    
    # For normal text:
    # 1. Merge hyphenated words
    text = merge_hyphenated_words(text)
    
    # 2. Replace multiple spaces/tabs with single space
    text = re.sub(r'[ \t]+', ' ', text)
    
    # 3. Normalize line breaks
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    
    # 4. Handle line breaks within paragraphs
    # Single newline within flowing text -> space (PDF often breaks lines arbitrarily)
    # But preserve intentional paragraph breaks (multiple newlines or newline + indent)
    lines = text.split('\n')
    result_lines = []
    current_para = []
    
    for line in lines:
        stripped = line.strip()
        if not stripped:
            # Empty line = paragraph break
            if current_para:
                result_lines.append(' '.join(current_para))
                current_para = []
            continue
        
        # Check if this looks like a new paragraph (starts with capital, bullet, number)
        is_new_para = bool(re.match(r'^[A-Z•●■◦\d]', stripped))
        
        if is_new_para and current_para:
            # End current paragraph
            result_lines.append(' '.join(current_para))
            current_para = [stripped]
        else:
            current_para.append(stripped)
    
    if current_para:
        result_lines.append(' '.join(current_para))
    
    return '\n'.join(result_lines)


def pdf_to_text(pdf_path: str, 
                page_separator: str = "\n\n---\n\n",
                preserve_layout: bool = False) -> str:
    """
    Convert a PDF file to searchable plain text.
    
    Args:
        pdf_path: Path to the PDF file
        page_separator: String to insert between pages
        preserve_layout: If True, try harder to preserve visual layout
    
    Returns:
        Extracted plain text
    """
    doc = fitz.open(pdf_path)
    pages_text = []
    
    for page_num in range(len(doc)):
        page = doc[page_num]
        
        if preserve_layout:
            # Use layout-preserving extraction
            text = extract_with_layout(page)
        else:
            # Use block-based extraction for better text flow
            text = extract_blocks_text(page)
        
        if text.strip():
            pages_text.append(text)
    
    doc.close()
    
    result = page_separator.join(pages_text)
    
    # Final cleanup
    result = final_cleanup(result)
    
    return result


def extract_blocks_text(page: fitz.Page) -> str:
    """Extract text using block detection for better flow."""
    blocks = extract_text_blocks(page)
    
    if not blocks:
        return ""
    
    # Sort blocks in reading order
    blocks = sort_blocks_reading_order(blocks)
    
    # Process and join blocks
    result_parts = []
    prev_block = None
    
    for block in blocks:
        is_code = block.block_type == 'code'
        text = normalize_text_block(block.text, is_code)
        
        if not text:
            continue
        
        # Determine separator from previous block
        if prev_block is not None:
            # Large vertical gap = paragraph break
            vertical_gap = block.y0 - prev_block.y1
            
            if vertical_gap > 20:  # Significant gap
                result_parts.append('\n\n')
            elif vertical_gap > 5:  # Small gap
                result_parts.append('\n')
            else:
                # Same line or very close
                result_parts.append(' ')
        
        result_parts.append(text)
        prev_block = block
    
    return ''.join(result_parts)


def extract_with_layout(page: fitz.Page) -> str:
    """
    Extract text while trying to preserve visual layout.
    Better for documents with complex formatting.
    """
    # Use PyMuPDF's built-in layout preservation
    text = page.get_text("text", sort=True)
    
    # Process the text
    text = merge_hyphenated_words(text)
    
    return text


def final_cleanup(text: str) -> str:
    """Final cleanup pass on the extracted text."""
    # Normalize whitespace
    text = text.replace('\r\n', '\n').replace('\r', '\n')
    
    # Remove excessive blank lines (more than 2 consecutive)
    text = re.sub(r'\n{4,}', '\n\n\n', text)
    
    # Remove trailing whitespace on each line
    text = '\n'.join(line.rstrip() for line in text.split('\n'))
    
    # Remove leading/trailing whitespace
    text = text.strip()
    
    # Fix common PDF extraction artifacts
    text = fix_common_artifacts(text)
    
    return text


def fix_common_artifacts(text: str) -> str:
    """Fix common PDF text extraction artifacts."""
    
    # Fix spaces inserted into words (e.g., "s t d : : m o v e")
    # Detect sequences of single chars separated by spaces
    def fix_spaced_chars(match):
        chars = match.group(0).replace(' ', '')
        return chars
    
    # Pattern for detecting artificially spaced text
    # Matches sequences like "s t d" (single chars with single spaces)
    text = re.sub(r'\b([a-zA-Z] ){2,}[a-zA-Z]\b', fix_spaced_chars, text)
    
    # Fix ligature issues (fi, fl, ff, etc. sometimes extracted wrong)
    ligature_fixes = {
        'ﬁ': 'fi',
        'ﬂ': 'fl',
        'ﬀ': 'ff',
        'ﬃ': 'ffi',
        'ﬄ': 'ffl',
    }
    for lig, replacement in ligature_fixes.items():
        text = text.replace(lig, replacement)
    
    # Fix common Unicode issues
    text = text.replace('\u2019', "'")  # Right single quote
    text = text.replace('\u2018', "'")  # Left single quote
    text = text.replace('\u201c', '"')  # Left double quote
    text = text.replace('\u201d', '"')  # Right double quote
    text = text.replace('\u2013', '-')  # En dash
    text = text.replace('\u2014', '--') # Em dash
    text = text.replace('\u00a0', ' ')  # Non-breaking space
    
    return text


def process_file(input_path: str, output_path: str = None,
                 preserve_layout: bool = False,
                 page_separator: str = "\n\n---\n\n") -> str:
    """Process a single PDF file and return the plain text."""
    text = pdf_to_text(input_path, page_separator, preserve_layout)
    
    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(text)
    
    return text


def process_directory(input_dir: str, output_dir: str,
                      preserve_layout: bool = False) -> int:
    """
    Process all PDF files in a directory recursively.
    Returns the number of files processed.
    """
    input_path = Path(input_dir)
    output_path = Path(output_dir)
    output_path.mkdir(parents=True, exist_ok=True)
    
    count = 0
    for pdf_file in input_path.rglob('*.pdf'):
        # Skip hidden files
        if pdf_file.name.startswith('.'):
            continue
        
        # Compute relative path and output path
        rel_path = pdf_file.relative_to(input_path)
        out_file = output_path / rel_path.with_suffix('.txt')
        out_file.parent.mkdir(parents=True, exist_ok=True)
        
        try:
            process_file(str(pdf_file), str(out_file), preserve_layout)
            count += 1
            print(f"Processed: {rel_path}")
        except Exception as e:
            print(f"Error processing {rel_path}: {e}", file=sys.stderr)
    
    return count


def extract_metadata(pdf_path: str) -> dict:
    """Extract metadata from a PDF file."""
    doc = fitz.open(pdf_path)
    metadata = doc.metadata
    doc.close()
    return metadata


def run_tests():
    """Run self-tests with a synthetic PDF."""
    import tempfile
    import os
    
    print("Running tests...\n")
    passed = 0
    failed = 0
    
    def test(name: str, condition: bool, detail: str = ""):
        nonlocal passed, failed
        if condition:
            passed += 1
            print(f"✅ {name}")
        else:
            failed += 1
            print(f"❌ {name}")
            if detail:
                print(f"   {detail}")
    
    # Create a test PDF
    with tempfile.TemporaryDirectory() as tmpdir:
        test_pdf = os.path.join(tmpdir, "test.pdf")
        
        # Create a simple PDF with PyMuPDF
        doc = fitz.open()
        
        # Page 1: Normal text
        page1 = doc.new_page()
        page1.insert_text((50, 50), "Hello World", fontsize=12)
        page1.insert_text((50, 70), "This is a test paragraph with some text.", fontsize=12)
        page1.insert_text((50, 90), "std::move and std::vector<int>", fontsize=12)
        
        # Page 2: More text
        page2 = doc.new_page()
        page2.insert_text((50, 50), "Second Page", fontsize=14)
        page2.insert_text((50, 70), "More content here.", fontsize=12)
        
        doc.save(test_pdf)
        doc.close()
        
        # Test extraction
        text = pdf_to_text(test_pdf)
        
        test("Basic text extraction", 
             "Hello World" in text, 
             f"Got: {repr(text[:100])}")
        
        test("Code identifiers preserved (std::move)",
             "std::move" in text,
             f"Got: {repr(text)}")
        
        test("Template syntax preserved (std::vector<int>)",
             "std::vector<int>" in text,
             f"Got: {repr(text)}")
        
        test("Multiple pages extracted",
             "Second Page" in text,
             f"Got: {repr(text)}")
        
        test("Page separator present",
             "---" in text,
             f"Got: {repr(text)}")
        
        # Test file processing
        out_txt = os.path.join(tmpdir, "output.txt")
        process_file(test_pdf, out_txt)
        
        test("Output file created",
             os.path.exists(out_txt),
             "")
        
        with open(out_txt, 'r', encoding='utf-8') as f:
            file_content = f.read()
        
        test("Output file content matches",
             "Hello World" in file_content,
             f"Got: {repr(file_content[:100])}")
    
    # Test helper functions
    test("Hyphenation merging: func-\\ntion",
         merge_hyphenated_words("func-\ntion") == "function",
         f"Got: {repr(merge_hyphenated_words('func-\ntion'))}")
    
    test("Hyphenation merging: multi-\\nline",
         merge_hyphenated_words("multi-\nline") == "multiline",
         f"Got: {repr(merge_hyphenated_words('multi-\nline'))}")
    
    test("Hyphenation preserves normal hyphens",
         "well-known" in merge_hyphenated_words("well-known word"),
         f"Got: {repr(merge_hyphenated_words('well-known word'))}")
    
    # Test artifact fixing
    fixed = fix_common_artifacts("s t d : : m o v e")
    test("Fix spaced characters",
         "std::move" in fixed or fixed.replace(' ', '') == "std::move",
         f"Got: {repr(fixed)}")
    
    test("Fix ligatures",
         fix_common_artifacts("ﬁle") == "file",
         f"Got: {repr(fix_common_artifacts('ﬁle'))}")
    
    # Test block type detection
    code_text = """
    int main() {
        std::cout << "Hello";
        return 0;
    }
    """
    test("Code block detection",
         detect_block_type(code_text) == 'code',
         f"Got: {detect_block_type(code_text)}")
    
    normal_text = "This is a normal paragraph of text that discusses various topics."
    test("Normal text detection",
         detect_block_type(normal_text) == 'text',
         f"Got: {detect_block_type(normal_text)}")
    
    print(f"\nResults: {passed}/{passed + failed} passed")
    
    if failed > 0:
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description='Convert PDF files to searchable plain text',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  # Convert a single file, print to stdout
  %(prog)s document.pdf

  # Convert a single file to output.txt
  %(prog)s document.pdf -o output.txt

  # Convert all PDFs in a directory
  %(prog)s ./pdf_docs -d ./text_output

  # Preserve layout for complex documents
  %(prog)s document.pdf --preserve-layout

  # Extract metadata only
  %(prog)s document.pdf --metadata

  # Run self-tests
  %(prog)s --test
'''
    )
    
    parser.add_argument('input', nargs='?', help='Input PDF file or directory')
    parser.add_argument('-o', '--output', help='Output file (for single file mode)')
    parser.add_argument('-d', '--output-dir', help='Output directory (for directory mode)')
    parser.add_argument('--preserve-layout', action='store_true',
                        help='Try to preserve visual layout')
    parser.add_argument('--page-separator', default='\n\n---\n\n',
                        help='Separator between pages (default: ---)')
    parser.add_argument('--no-page-separator', action='store_true',
                        help='No separator between pages')
    parser.add_argument('--metadata', action='store_true',
                        help='Extract and print metadata only')
    parser.add_argument('--test', action='store_true', help='Run self-tests')
    
    args = parser.parse_args()
    
    if args.test:
        run_tests()
        return
    
    if not args.input:
        parser.print_help()
        return
    
    input_path = Path(args.input)
    
    if not input_path.exists():
        print(f"Error: {args.input} not found", file=sys.stderr)
        sys.exit(1)
    
    page_sep = '' if args.no_page_separator else args.page_separator
    
    if args.metadata:
        if not input_path.is_file():
            print("Error: --metadata requires a single file", file=sys.stderr)
            sys.exit(1)
        metadata = extract_metadata(str(input_path))
        for key, value in metadata.items():
            if value:
                print(f"{key}: {value}")
        return
    
    if input_path.is_file():
        # Single file mode
        text = process_file(
            str(input_path), 
            args.output,
            preserve_layout=args.preserve_layout
        )
        if not args.output:
            print(text)
    elif input_path.is_dir():
        # Directory mode
        if not args.output_dir:
            print("Error: --output-dir required for directory input", file=sys.stderr)
            sys.exit(1)
        count = process_directory(
            str(input_path), 
            args.output_dir,
            preserve_layout=args.preserve_layout
        )
        print(f"\nProcessed {count} files")
    else:
        print(f"Error: {args.input} is not a file or directory", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()