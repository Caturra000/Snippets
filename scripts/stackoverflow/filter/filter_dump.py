#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Streaming filter for a Stack Exchange data dump (Posts / Comments / Tags).

Reads the (possibly enormous) XML dump line by line so memory usage stays
bounded, and writes new filtered XML files next to it.  Nothing is deleted.

Typical usage:
    python filter_dump.py                 # uses config.json next to this file
    python filter_dump.py -c my.json      # custom config
    python filter_dump.py --help

The Stack Exchange dump format is one <row ... /> element per line, so a line
based reader is both correct and far faster/lighter than a full DOM parse.
"""

import argparse
import array
import bisect
import json
import os
import re
import sys
import time

# Attributes never contain a raw double quote (values are XML-escaped), so this
# is a safe and very fast way to pull attributes out of a single <row> line.
ATTR_RE = re.compile(rb'([A-Za-z][A-Za-z0-9]*)="([^"]*)"')
TAG_RE = re.compile(r'<([^>]*)>')

POSTS_HEADER = (
    b'<?xml version="1.0" encoding="utf-8"?>\n'
    b'<!--\n'
    b'  ContentLicense\n\n'
    b'  CC BY-SA 2.5 - Url: https://creativecommons.org/licenses/by-sa/2.5/\n'
    b'  CC BY-SA 3.0 - Url: https://creativecommons.org/licenses/by-sa/3.0/\n'
    b'  CC BY-SA 4.0 - Url: https://creativecommons.org/licenses/by-sa/4.0/\n'
    b'  -->\n'
)
POSTS_OPEN = b'<posts>\n'
POSTS_CLOSE = b'</posts>\n'
COMMENTS_HEADER = POSTS_HEADER
COMMENTS_OPEN = b'<comments>\n'
COMMENTS_CLOSE = b'</comments>\n'
TAGS_HEADER = POSTS_HEADER
TAGS_OPEN = b'<tags>\n'
TAGS_CLOSE = b'</tags>\n'

READ_BUFFER = 1024 * 1024  # 1 MiB read buffer
PROGRESS_EVERY = 2_000_000  # report progress every N rows


def iter_rows(path):
    """Yield raw bytes lines that contain a '<row' element."""
    with open(path, 'rb', buffering=READ_BUFFER) as fin:
        for line in fin:
            if b'<row' in line:
                yield line


def parse_attrs(line):
    return {m.group(1): m.group(2) for m in ATTR_RE.finditer(line)}


def _unescape(value):
    # xml.sax.saxutils.unescape is a single pass, so it will not double-decode.
    from xml.sax.saxutils import unescape
    return unescape(value)


def parse_tags(raw_bytes):
    """Return the lower-cased list of tags from a Posts 'Tags' attribute."""
    text = _unescape(raw_bytes.decode('utf-8', 'replace'))
    return [t.lower() for t in TAG_RE.findall(text)]


def contains_sorted(arr, value):
    """Membership test for a sorted array.array (memory-cheap id store)."""
    i = bisect.bisect_left(arr, value)
    return i < len(arr) and arr[i] == value


def write_row(out, line):
    out.write(line if line.endswith(b'\n') else line + b'\n')


def _to_int(b, default=None):
    try:
        return int(b)
    except (TypeError, ValueError):
        return default


class Progress:
    def __init__(self, label):
        self.label = label
        self.count = 0
        self.start = time.time()
        self.last = self.start

    def tick(self, n=1):
        self.count += n
        if self.count % PROGRESS_EVERY < n:
            now = time.time()
            rate = self.count / max(now - self.start, 1e-6)
            sys.stderr.write(
                '[%s] %d rows, %.0f rows/s, %.1fs elapsed\n'
                % (self.label, self.count, rate, now - self.start)
            )
            sys.stderr.flush()


def process_posts(cfg, selected_ids):
    """Filter Posts.xml. Fills selected_ids (sorted array of kept post ids)."""
    post_conf = cfg['filter']
    in_path = cfg['input']['posts']
    out_path = cfg['output']['posts']

    question_type = post_conf.get('question_post_type', 1)
    if not isinstance(question_type, (list, tuple, set)):
        question_type = (question_type,)
    else:
        question_type = tuple(question_type)
    answer_type = post_conf.get('answer_post_type', 2)
    include_answers = post_conf.get('include_answers', True)
    min_score = post_conf.get('min_score', 0)
    filter_answer_score = post_conf.get('apply_score_filter_to_answers', True)
    require_answers = post_conf.get('require_answers', True)
    assume_sorted = post_conf.get('assume_posts_sorted_by_id', False)
    target_tags = set(t.lower() for t in post_conf['tags'])

    stats = {'kept_questions': 0, 'kept_answers': 0}

    def is_selected_question(attrs):
        if _to_int(attrs.get(b'PostTypeId'), -1) not in question_type:
            return False
        if require_answers:
            if _to_int(attrs.get(b'AnswerCount'), 0) < 1:
                return False
        score = _to_int(attrs.get(b'Score'), 0)
        if score < min_score:
            return False
        raw_tags = attrs.get(b'Tags')
        if not raw_tags:
            return False
        for tag in parse_tags(raw_tags):
            if tag in target_tags:
                return True
        return False

    def is_kept_answer(attrs):
        if _to_int(attrs.get(b'PostTypeId'), -1) != answer_type:
            return False
        parent = _to_int(attrs.get(b'ParentId'), None)
        if parent is None or not contains_sorted(selected_ids, parent):
            return False
        if filter_answer_score and _to_int(attrs.get(b'Score'), 0) < min_score:
            return False
        return True

    with open(out_path, 'wb', buffering=READ_BUFFER) as out:
        out.write(POSTS_HEADER)
        out.write(POSTS_OPEN)

        prog = Progress('Posts (questions)')
        answer_ids = array.array('q')
        with open(in_path, 'rb', buffering=READ_BUFFER) as fin:
            for line in fin:
                if b'<row' not in line:
                    continue
                prog.tick()
                attrs = parse_attrs(line)
                if is_selected_question(attrs):
                    write_row(out, line)
                    selected_ids.append(_to_int(attrs[b'Id'], 0))
                    stats['kept_questions'] += 1
                elif include_answers and assume_sorted and is_kept_answer(attrs):
                    write_row(out, line)
                    selected_ids.append(_to_int(attrs[b'Id'], 0))
                    stats['kept_answers'] += 1

        # The dump is NOT guaranteed to be ordered by Id (a question's row can
        # appear after its answers), so by default a second pass is used: pass 1
        # collects the matching question ids, pass 2 picks their answers.
        if include_answers and not assume_sorted:
            # Pass 1 ids came in file order, so sort them for binary lookup.
            selected_ids[:] = array.array('q', sorted(selected_ids))
            prog = Progress('Posts (answers)')
            with open(in_path, 'rb', buffering=READ_BUFFER) as fin:
                for line in fin:
                    if b'<row' not in line:
                        continue
                    prog.tick()
                    attrs = parse_attrs(line)
                    if is_kept_answer(attrs):
                        write_row(out, line)
                        answer_ids.append(_to_int(attrs[b'Id'], 0))
                        stats['kept_answers'] += 1
            selected_ids[:] = _merge_sorted(
                selected_ids, array.array('q', sorted(answer_ids)))

        out.write(POSTS_CLOSE)

    return stats


def _merge_sorted(a, b):
    """Merge two ascending arrays/iterables into one ascending array.array."""
    merged = array.array('q')
    i = j = 0
    na, nb = len(a), len(b)
    while i < na and j < nb:
        if a[i] <= b[j]:
            merged.append(a[i])
            i += 1
        else:
            merged.append(b[j])
            j += 1
    if i < na:
        merged.extend(a[i:])
    if j < nb:
        merged.extend(b[j:])
    return merged


def process_comments(cfg, selected_ids):
    post_conf = cfg['filter']
    in_path = cfg['input']['comments']
    out_path = cfg['output']['comments']
    min_score = post_conf.get('min_score', 0)

    kept = 0
    with open(out_path, 'wb', buffering=READ_BUFFER) as out:
        out.write(COMMENTS_HEADER)
        out.write(COMMENTS_OPEN)
        prog = Progress('Comments')
        for line in iter_rows(in_path):
            prog.tick()
            attrs = parse_attrs(line)
            post_id = _to_int(attrs.get(b'PostId'), None)
            if post_id is None or not contains_sorted(selected_ids, post_id):
                continue
            if _to_int(attrs.get(b'Score'), 0) < min_score:
                continue
            write_row(out, line)
            kept += 1
        out.write(COMMENTS_CLOSE)
    return {'kept_comments': kept}


def process_tags(cfg):
    post_conf = cfg['filter']
    in_path = cfg['input'].get('tags')
    out_path = cfg['output'].get('tags')
    if not in_path or not out_path or not post_conf.get('export_tags', True):
        return {'kept_tags': 0}
    if not os.path.exists(in_path):
        sys.stderr.write('[Tags] input not found, skipped: %s\n' % in_path)
        return {'kept_tags': 0}

    target_tags = set(t.lower() for t in post_conf['tags'])
    kept = 0
    with open(out_path, 'wb', buffering=READ_BUFFER) as out:
        out.write(TAGS_HEADER)
        out.write(TAGS_OPEN)
        for line in iter_rows(in_path):
            attrs = parse_attrs(line)
            name = attrs.get(b'TagName')
            if not name:
                continue
            if name.decode('utf-8', 'replace').lower() in target_tags:
                write_row(out, line)
                kept += 1
        out.write(TAGS_CLOSE)
    return {'kept_tags': kept}


def resolve(base_dir, path):
    if not path:
        return None
    return path if os.path.isabs(path) else os.path.join(base_dir, path)


def load_config(config_path):
    with open(config_path, 'r', encoding='utf-8') as f:
        cfg = json.load(f)
    base = os.path.dirname(os.path.abspath(config_path))
    for section in ('input', 'output'):
        for key, value in list(cfg.get(section, {}).items()):
            cfg[section][key] = resolve(base, value)
    return cfg


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    default_cfg = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'config.json')
    parser.add_argument('-c', '--config', default=default_cfg,
                        help='path to JSON config (default: config.json next to this script)')
    parser.add_argument('--posts-only', action='store_true', help='only process Posts.xml')
    parser.add_argument('--comments-only', action='store_true', help='only process Comments.xml')
    parser.add_argument('--skip-comments', action='store_true', help='skip Comments.xml')
    parser.add_argument('--skip-tags', action='store_true', help='skip Tags.xml')
    parser.add_argument('--post-types', type=int, nargs='+', default=None,
                        help='override which PostTypeId values are kept as questions')
    args = parser.parse_args(argv)

    cfg = load_config(args.config)
    if args.post_types:
        cfg['filter']['question_post_type'] = args.post_types

    # selected_ids grows monotonically in file (Id) order, so it stays sorted.
    selected_ids = array.array('q')
    overall_start = time.time()
    results = {}

    if not args.comments_only:
        results.update(process_posts(cfg, selected_ids))
    if not args.posts_only and not args.skip_comments:
        results.update(process_comments(cfg, selected_ids))
    if not args.posts_only and not args.skip_tags:
        results.update(process_tags(cfg))

    sys.stderr.write(
        'Done in %.1fs: %s\n'
        % (time.time() - overall_start,
           ', '.join('%s=%s' % (k, v) for k, v in results.items()))
    )
    return 0


if __name__ == '__main__':
    sys.exit(main())
