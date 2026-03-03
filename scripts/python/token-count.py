#!/usr/bin/env python3
"""
Markdown Token 分析工具
- 递归扫描目录中的 .md 文件
- 统计每个文件的 token 数（基于 tiktoken，兼容 OpenAI GPT 系列模型）
- 生成 token 分布直方图
- 筛选出 token < threshold 的文件列表

# 默认阈值 1000
python md_token_analyzer.py ./docs

# 阈值改为 500
python md_token_analyzer.py ./docs -t 500

# 阈值改为 3000，指定模型
python md_token_analyzer.py ./docs -t 3000 -m gpt-3.5-turbo

# 阈值 2000，输出图片到指定路径
python md_token_analyzer.py ./docs -t 2000 -o report.png
"""

import os
import sys
import argparse
from pathlib import Path
from collections import defaultdict

try:
    import tiktoken
except ImportError:
    print("❌ 请先安装 tiktoken: pip install tiktoken")
    sys.exit(1)

try:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    import numpy as np
    HAS_MATPLOTLIB = True
except ImportError:
    HAS_MATPLOTLIB = False
    print("⚠️  未安装 matplotlib，将跳过分布图生成。安装命令: pip install matplotlib")


# ────────────────────────────── 核心函数 ──────────────────────────────

def count_tokens(text: str, encoding) -> int:
    return len(encoding.encode(text))


def scan_markdown_files(root_dir: str) -> list[Path]:
    root = Path(root_dir)
    if not root.exists():
        print(f"❌ 路径不存在: {root_dir}")
        sys.exit(1)
    if not root.is_dir():
        print(f"❌ 路径不是目录: {root_dir}")
        sys.exit(1)
    return sorted(root.rglob("*.md"))


def analyze_files(md_files: list[Path], encoding) -> list[dict]:
    results = []
    for filepath in md_files:
        try:
            text = filepath.read_text(encoding="utf-8", errors="replace")
            tokens = count_tokens(text, encoding)
            results.append({
                "path": filepath,
                "tokens": tokens,
                "size_kb": filepath.stat().st_size / 1024,
            })
        except Exception as e:
            print(f"⚠️  读取失败: {filepath} -> {e}")
    return results


# ────────────────────────────── 动态分桶 ──────────────────────────────

def build_buckets(max_tokens: int, threshold: int) -> list[tuple]:
    """
    根据实际最大 token 数和用户阈值动态生成分桶。
    保证 threshold 一定是某个桶的边界，便于直观看到
    有多少文件落在阈值以下/以上。
    """
    # 候选边界点（可按需扩展）
    candidates = [0, 50, 100, 200, 500, 1_000, 2_000, 5_000,
                  10_000, 20_000, 50_000, 100_000, 200_000, 500_000, 1_000_000]
    # 确保 threshold 在候选列表中
    if threshold not in candidates:
        candidates.append(threshold)
    candidates = sorted(set(candidates))

    # 只保留 <= max_tokens 的边界 + 一个上界
    boundaries = [b for b in candidates if b <= max_tokens]
    if not boundaries or boundaries[-1] < max_tokens:
        boundaries.append(float("inf"))
    else:
        boundaries.append(float("inf"))

    # 去重 & 排序
    boundaries = sorted(set(boundaries))

    # 生成 (lo, hi) 对
    buckets = []
    for i in range(len(boundaries) - 1):
        buckets.append((boundaries[i], boundaries[i + 1]))
    return buckets


# ────────────────────────────── 统计输出 ──────────────────────────────

def print_statistics(results: list[dict]):
    if not results:
        print("未找到任何 .md 文件。")
        return

    tokens_list = [r["tokens"] for r in results]
    total = sum(tokens_list)
    avg = total / len(tokens_list)
    median = sorted(tokens_list)[len(tokens_list) // 2]

    print("\n" + "=" * 60)
    print("📊  整体统计")
    print("=" * 60)
    print(f"  文件总数:        {len(results)}")
    print(f"  Token 总计:      {total:,}")
    print(f"  平均 Token:      {avg:,.1f}")
    print(f"  中位数 Token:    {median:,}")
    print(f"  最小 Token:      {min(tokens_list):,}")
    print(f"  最大 Token:      {max(tokens_list):,}")


def print_distribution_table(results: list[dict], threshold: int):
    if not results:
        return

    max_tokens = max(r["tokens"] for r in results)
    buckets = build_buckets(max_tokens, threshold)

    counts = defaultdict(int)
    for r in results:
        for lo, hi in buckets:
            if lo <= r["tokens"] < hi:
                counts[(lo, hi)] += 1
                break

    print("\n" + "=" * 60)
    print(f"📈  Token 分布  （阈值线 = {threshold:,}）")
    print("=" * 60)
    print(f"  {'区间':<22} {'文件数':>8}  {'占比':>7}  {'柱状图'}")
    print("  " + "-" * 60)

    total = len(results)
    for lo, hi in buckets:
        c = counts[(lo, hi)]
        pct = c / total * 100
        hi_label = f"{hi:,}" if hi != float("inf") else "∞"
        label = f"[{lo:,}, {hi_label})"
        bar = "█" * int(pct / 2)
        # 用标记突出阈值边界
        marker = " ◀── threshold" if hi == threshold or lo == threshold else ""
        print(f"  {label:<22} {c:>8}  {pct:>6.1f}%  {bar}{marker}")


def print_small_files(results: list[dict], threshold: int):
    small = [r for r in results if r["tokens"] < threshold]
    small.sort(key=lambda x: x["tokens"])

    print("\n" + "=" * 60)
    print(f"📋  Token < {threshold:,} 的文件列表 （共 {len(small)} 个）")
    print("=" * 60)

    if not small:
        print("  （无）")
        return

    print(f"  {'#':>4}  {'Token':>8}  {'大小(KB)':>9}  文件路径")
    print("  " + "-" * 70)
    for i, r in enumerate(small, 1):
        print(f"  {i:>4}  {r['tokens']:>8,}  {r['size_kb']:>9.1f}  {r['path']}")

    print(f"\n  ✅ 共 {len(small)} 个文件 token < {threshold:,}"
          f"（占总数 {len(small)}/{len(results)}，"
          f"{len(small) / len(results) * 100:.1f}%）")


# ────────────────────────────── 分布图 ──────────────────────────────

def plot_distribution(results: list[dict], threshold: int,
                      output_path: str = "token_distribution.png"):
    if not HAS_MATPLOTLIB:
        return

    tokens_list = [r["tokens"] for r in results]
    fig, axes = plt.subplots(1, 2, figsize=(16, 6))

    # ---- 左图: 线性直方图 ----
    ax1 = axes[0]
    ax1.hist(tokens_list, bins=50, color="#4C72B0", edgecolor="white", alpha=0.85)
    ax1.set_title("Token Distribution (Linear Scale)", fontsize=14)
    ax1.set_xlabel("Tokens", fontsize=12)
    ax1.set_ylabel("File Count", fontsize=12)
    ax1.axvline(x=threshold, color="red", linestyle="--", linewidth=1.5,
                label=f"threshold = {threshold:,}")
    ax1.legend()

    # ---- 右图: 对数直方图 ----
    ax2 = axes[1]
    tokens_nonzero = [t for t in tokens_list if t > 0]
    if tokens_nonzero:
        log_bins = np.logspace(
            np.log10(max(1, min(tokens_nonzero))),
            np.log10(max(tokens_nonzero)),
            50,
        )
        ax2.hist(tokens_nonzero, bins=log_bins, color="#55A868",
                 edgecolor="white", alpha=0.85)
        ax2.set_xscale("log")
        ax2.axvline(x=threshold, color="red", linestyle="--", linewidth=1.5,
                    label=f"threshold = {threshold:,}")
        ax2.legend()
    ax2.set_title("Token Distribution (Log Scale)", fontsize=14)
    ax2.set_xlabel("Tokens (log)", fontsize=12)
    ax2.set_ylabel("File Count", fontsize=12)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    print(f"\n📊 分布图已保存至: {output_path}")


# ────────────────────────────── 主入口 ──────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="分析目录中 Markdown 文件的 Token 数分布（基于 tiktoken）"
    )
    parser.add_argument("directory", help="要扫描的根目录路径")
    parser.add_argument(
        "-m", "--model", default="gpt-4o",
        help="用于 token 编码的模型名（默认: gpt-4o）"
    )
    parser.add_argument(
        "-t", "--threshold", type=int, default=1000,
        help="筛选阈值，列出 token 数小于此值的文件（默认: 1000）"
    )
    parser.add_argument(
        "-o", "--output", default="token_distribution.png",
        help="分布图输出路径（默认: token_distribution.png）"
    )
    parser.add_argument(
        "--encoding", default=None,
        help="直接指定 tiktoken 编码名（如 cl100k_base / o200k_base），"
             "优先级高于 --model"
    )
    parser.add_argument(
        "--top", type=int, default=10,
        help="显示 token 数最多的前 N 个文件（默认: 10）"
    )

    args = parser.parse_args()

    # ---------- 初始化编码器 ----------
    if args.encoding:
        encoding = tiktoken.get_encoding(args.encoding)
        enc_name = args.encoding
    else:
        try:
            encoding = tiktoken.encoding_for_model(args.model)
            enc_name = f"{args.model} -> {encoding.name}"
        except KeyError:
            print(f"⚠️  未知模型 '{args.model}'，回退到 cl100k_base 编码")
            encoding = tiktoken.get_encoding("cl100k_base")
            enc_name = "cl100k_base (fallback)"

    print(f"🔤 使用编码: {enc_name}")
    print(f"🎯 筛选阈值: token < {args.threshold:,}")

    # ---------- 扫描 & 分析 ----------
    print(f"📂 扫描目录: {args.directory}")
    md_files = scan_markdown_files(args.directory)
    print(f"   找到 {len(md_files)} 个 .md 文件")

    if not md_files:
        return

    print("⏳ 正在分析 token 数...")
    results = analyze_files(md_files, encoding)

    # ---------- 输出 ----------
    print_statistics(results)
    print_distribution_table(results, threshold=args.threshold)

    # Top N
    top_n = sorted(results, key=lambda x: x["tokens"], reverse=True)[: args.top]
    print(f"\n{'=' * 60}")
    print(f"🏆  Token 数最多的前 {args.top} 个文件")
    print("=" * 60)
    print(f"  {'#':>4}  {'Token':>10}  {'大小(KB)':>9}  文件路径")
    print("  " + "-" * 70)
    for i, r in enumerate(top_n, 1):
        print(f"  {i:>4}  {r['tokens']:>10,}  {r['size_kb']:>9.1f}  {r['path']}")

    print_small_files(results, threshold=args.threshold)

    # 分布图
    if HAS_MATPLOTLIB and len(results) > 0:
        plot_distribution(results, threshold=args.threshold,
                          output_path=args.output)


if __name__ == "__main__":
    main()