#!/usr/bin/env python3
"""
递归扫描目录中的 .md 文件，基于 tiktoken 统计 token 数，
输出 small.txt / large.txt 文件列表及统计报告。
"""

import os
import sys
import argparse
import tiktoken


def count_tokens(text: str, enc) -> int:
    return len(enc.encode(text))


def scan(root_dir: str,
         large_threshold: int = 24_000,
         small_threshold: int = 1_000,
         encoding_name: str = 'cl100k_base',
         output_dir: str = '.'):

    enc = tiktoken.get_encoding(encoding_name)

    total_files = 0
    total_tokens = 0
    large_files = []
    large_tokens = 0
    small_files = []
    small_tokens = 0

    for dirpath, _, filenames in os.walk(root_dir):
        for fn in sorted(filenames):
            if not fn.lower().endswith('.md'):
                continue
            path = os.path.join(dirpath, fn)
            try:
                with open(path, 'r', encoding='utf-8') as f:
                    content = f.read()
            except Exception as e:
                print(f'[ERR] {path}: {e}')
                continue

            tokens = count_tokens(content, enc)
            total_files += 1
            total_tokens += tokens

            if tokens > large_threshold:
                large_files.append(path)
                large_tokens += tokens
            elif tokens < small_threshold:
                small_files.append(path)
                small_tokens += tokens

    # 写出列表
    large_path = os.path.join(output_dir, 'large.txt')
    small_path = os.path.join(output_dir, 'small.txt')

    with open(large_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(large_files) + ('\n' if large_files else ''))
    with open(small_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(small_files) + ('\n' if small_files else ''))

    # 报告
    normal_count = total_files - len(large_files) - len(small_files)
    normal_tokens = total_tokens - large_tokens - small_tokens

    def pct(n):
        return f'{n / total_tokens * 100:5.1f}%' if total_tokens else ' 0.0%'

    print('=' * 60)
    print(f'  目录        {os.path.abspath(root_dir)}')
    print(f'  编码        {encoding_name}')
    print(f'  文件总数    {total_files}')
    print(f'  Token 总计  {total_tokens:,}')
    print('-' * 60)
    print(f'  Large (>{large_threshold:,}):  {len(large_files):>5} 个 │'
          f' {large_tokens:>12,} tok │ {pct(large_tokens)}')
    print(f'  Small (<{small_threshold:,}):   {len(small_files):>5} 个 │'
          f' {small_tokens:>12,} tok │ {pct(small_tokens)}')
    print(f'  Normal:         {normal_count:>5} 个 │'
          f' {normal_tokens:>12,} tok │ {pct(normal_tokens)}')
    print('-' * 60)
    print(f'  已写入  {large_path}  ({len(large_files)} 条)')
    print(f'  已写入  {small_path}  ({len(small_files)} 条)')
    print('=' * 60)


def main():
    ap = argparse.ArgumentParser(description='统计 Markdown token 数并输出 large/small 列表')
    ap.add_argument('directory', help='要扫描的根目录')
    ap.add_argument('--large', type=int, default=24_000, help='> 此值归入 large（默认 24000）')
    ap.add_argument('--small', type=int, default=1_000, help='< 此值归入 small（默认 1000）')
    ap.add_argument('--encoding', default='cl100k_base', help='tiktoken 编码名')
    ap.add_argument('--output-dir', default='.', help='large.txt / small.txt 输出目录')
    args = ap.parse_args()

    if not os.path.isdir(args.directory):
        print(f'错误: 目录不存在 → {args.directory}', file=sys.stderr)
        sys.exit(1)

    scan(args.directory,
         large_threshold=args.large,
         small_threshold=args.small,
         encoding_name=args.encoding,
         output_dir=args.output_dir)


if __name__ == '__main__':
    main()