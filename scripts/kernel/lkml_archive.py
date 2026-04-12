#!/usr/bin/env python3
"""
LKML 子模块离线归档一键整理脚本

用法:
    python lkml_archive.py <模块名> <卷号>
    python lkml_archive.py linux-nvme 0
    python lkml_archive.py linux-nvme 1

中间过程在 /dev/shm 中进行，最终结果输出到 ~/lkml/<模块名>/<卷号>/out/
"""

import argparse
import os
import shutil
import subprocess
import sys
import email
from email import policy
from email.utils import parsedate_to_datetime
import re
from collections import defaultdict


# ─────────────────────────── 配置 ───────────────────────────

SHM_BASE = "/dev/shm/lkml_work"
OUTPUT_BASE = os.path.expanduser("~/lkml")


# ──────────────────────── 头部安全读取 ────────────────────────

def _safe_get_header(msg, name: str, default: str = "") -> str:
    """安全获取邮件头部，遇到包含 CR/LF 导致的解析异常时回退到原始数据并清洗"""
    try:
        val = msg.get(name, default)
        if val is None:
            return default
        return str(val).replace("\r", "").replace("\n", " ")
    except Exception:
        # 回退：从底层 _headers 取出原始字符串并清洗
        for k, v in msg._headers:
            if k.lower() == name.lower():
                return v.replace("\r", "").replace("\n", " ")
        return default


def _safe_get_all(msg, name: str) -> list[str]:
    """安全获取多值邮件头部"""
    try:
        vals = msg.get_all(name, [])
        return [str(v).replace("\r", "").replace("\n", " ") for v in vals]
    except Exception:
        values = []
        for k, v in msg._headers:
            if k.lower() == name.lower():
                values.append(v.replace("\r", "").replace("\n", " "))
        return values


# ─────────────────────────── 步骤一：克隆 ───────────────────────────

def step_clone(module: str, volume: str, git_dir: str) -> None:
    repo_url = f"https://lore.kernel.org/{module}/{volume}"
    if os.path.exists(git_dir):
        print(f"[步骤一] git 目录已存在，跳过克隆: {git_dir}")
        return
    os.makedirs(os.path.dirname(git_dir), exist_ok=True)
    cmd = ["git", "clone", "--bare", repo_url, git_dir]
    print(f"[步骤一] 正在克隆: {repo_url}")
    subprocess.check_call(cmd)
    print(f"[步骤一] 克隆完成: {git_dir}")


# ─────────────────────────── 步骤二：提取原始邮件 ───────────────────────────

def step_extract_emails(git_dir: str, raw_dir: str) -> None:
    os.makedirs(raw_dir, exist_ok=True)

    print("[步骤二] 正在获取所有 commit 列表...")
    log_cmd = ["git", f"--git-dir={git_dir}", "log", "--format=%H"]
    commits = subprocess.check_output(log_cmd).decode("utf-8").splitlines()
    total = len(commits)
    print(f"[步骤二] 共发现 {total} 个 Commit，准备极速提取...")

    cat_cmd = ["git", f"--git-dir={git_dir}", "cat-file", "--batch"]
    proc = subprocess.Popen(cat_cmd, stdin=subprocess.PIPE, stdout=subprocess.PIPE)

    count = 0
    missing = 0

    for commit in commits:
        query = f"{commit}:m\n".encode("utf-8")
        proc.stdin.write(query)
        proc.stdin.flush()

        header = proc.stdout.readline().decode("utf-8").strip()
        if header.endswith("missing"):
            missing += 1
            continue

        parts = header.split()
        size = int(parts[2])
        content = proc.stdout.read(size)
        proc.stdout.read(1)  # 消费尾部换行

        with open(os.path.join(raw_dir, f"{commit}.eml"), "wb") as f:
            f.write(content)

        count += 1
        if count % 5000 == 0:
            print(f"[步骤二] 已提取 {count} / {total} 封邮件...")

    proc.stdin.close()
    proc.wait()

    print(f"[步骤二] 提取完成！成功 {count} 封，跳过 {missing} 个无 'm' 文件的 commit。")


# ─────────────────────────── 步骤三：线程归档 ───────────────────────────

def _extract_message_ids(text: str) -> list[str]:
    if not text:
        return []
    return re.findall(r"<([^>]+)>", text)


def _clean_filename(msg) -> str:
    try:
        dt = parsedate_to_datetime(_safe_get_header(msg, "Date", ""))
        date_prefix = dt.strftime("%Y%m%d")
    except Exception:
        date_prefix = "00000000"

    subject = _safe_get_header(msg, "Subject", "No-Subject").strip()
    subject = re.sub(r"^(Re|Fw|Aw|Replies):\s*", "", subject, flags=re.IGNORECASE)
    subject = re.sub(r'[\s:;,/\\|<>?*"]+', "-", subject)
    subject = re.sub(r"-+", "-", subject).strip("-")
    if len(subject) > 100:
        subject = subject[:97] + "..."

    return f"{date_prefix}-{subject}.txt"


# ── 并查集 ──
_parent: dict[str, str] = {}


def _find(i: str) -> str:
    if i not in _parent:
        _parent[i] = i
    if _parent[i] == i:
        return i
    _parent[i] = _find(_parent[i])
    return _parent[i]


def _union(i: str, j: str) -> None:
    ri, rj = _find(i), _find(j)
    if ri != rj:
        _parent[ri] = rj


def step_build_threads(raw_dir: str, output_dir: str) -> None:
    print("[步骤三] 正在读取并解析所有邮件...")
    emails_data: dict[str, email.message.EmailMessage] = {}

    for root, _dirs, files in os.walk(raw_dir):
        for file in files:
            filepath = os.path.join(root, file)
            if not os.path.isfile(filepath):
                continue
            try:
                with open(filepath, "rb") as f:
                    msg = email.message_from_binary_file(f, policy=policy.default)
            except Exception:
                continue

            msg_ids = _extract_message_ids(_safe_get_header(msg, "Message-ID", ""))
            if not msg_ids:
                continue

            primary_id = msg_ids[0]
            emails_data[primary_id] = msg

            refs = _safe_get_all(msg, "References") + _safe_get_all(msg, "In-Reply-To")
            for ref_header in refs:
                for ref_id in _extract_message_ids(ref_header):
                    _union(primary_id, ref_id)

    print(f"[步骤三] 共解析 {len(emails_data)} 封邮件，正在按 Thread 分组...")

    threads: dict[str, list] = defaultdict(list)
    for msg_id, msg in emails_data.items():
        root_id = _find(msg_id)
        threads[root_id].append(msg)

    print(f"[步骤三] 共发现 {len(threads)} 个 Thread，正在生成文件...")
    os.makedirs(output_dir, exist_ok=True)

    saved_count = 0
    name_counter: dict[str, int] = defaultdict(int)

    for _root_id, msgs in threads.items():
        def _get_date(m):
            try:
                return parsedate_to_datetime(_safe_get_header(m, "Date", "")).timestamp()
            except Exception:
                return 0.0

        msgs.sort(key=_get_date)

        first_msg = msgs[0]
        base_filename = _clean_filename(first_msg)

        name_counter[base_filename] += 1
        if name_counter[base_filename] > 1:
            name, ext = os.path.splitext(base_filename)
            filename = f"{name}-{name_counter[base_filename]}{ext}"
        else:
            filename = base_filename

        filepath = os.path.join(output_dir, filename)

        with open(filepath, "w", encoding="utf-8") as f:
            subject = _safe_get_header(first_msg, "Subject", "No-Subject").strip()
            f.write(f"Thread Topic: {subject}\n")
            f.write(f"Total Messages: {len(msgs)}\n")
            f.write(f"Date: {_safe_get_header(first_msg, 'Date', '')}\n\n")

            for m in msgs:
                f.write("=" * 80 + "\n")
                f.write(f"From:       {_safe_get_header(m, 'From', '')}\n")
                f.write(f"Date:       {_safe_get_header(m, 'Date', '')}\n")
                f.write(f"Subject:    {_safe_get_header(m, 'Subject', '')}\n")
                f.write(f"Message-ID: {_safe_get_header(m, 'Message-ID', '')}\n")
                f.write("-" * 80 + "\n\n")

                body = ""
                if m.is_multipart():
                    for part in m.walk():
                        if part.get_content_type() == "text/plain":
                            try:
                                body += part.get_content()
                            except Exception:
                                pass
                else:
                    try:
                        body = m.get_content()
                    except Exception:
                        pass
                f.write(body.strip() + "\n\n")

        saved_count += 1
        if saved_count % 500 == 0:
            print(f"[步骤三] 已生成 {saved_count} 个文件...")

    print(f"[步骤三] 完成！共保存 {saved_count} 个 Thread 文件至: {output_dir}")


# ─────────────────────────── 主流程 ───────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="一键整理 LKML 子模块离线归档"
    )
    parser.add_argument("module", help="模块名，如 linux-nvme")
    parser.add_argument("volume", help="卷号，如 0、1、2")
    parser.add_argument(
        "--keep", action="store_true", help="保留 /dev/shm 中的中间文件（默认清理）"
    )
    args = parser.parse_args()

    module = args.module
    volume = args.volume

    # 路径规划
    work_dir = os.path.join(SHM_BASE, f"{module}_{volume}")
    git_dir = os.path.join(work_dir, "git", f"{volume}.git")
    raw_dir = os.path.join(work_dir, "raw_emails")
    output_dir = os.path.join(OUTPUT_BASE, module, volume, "out")

    print(f"{'=' * 60}")
    print(f"  模块:   {module}")
    print(f"  卷号:   {volume}")
    print(f"  工作区: {work_dir}")
    print(f"  输出:   {output_dir}")
    print(f"{'=' * 60}\n")

    try:
        step_clone(module, volume, git_dir)
        print()
        step_extract_emails(git_dir, raw_dir)
        print()
        step_build_threads(raw_dir, output_dir)
        print()
        print(f"✅ 全部完成！结果已保存至: {output_dir}")
    except subprocess.CalledProcessError as e:
        print(f"\n❌ 命令执行失败: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"\n❌ 发生错误: {e}", file=sys.stderr)
        sys.exit(1)
    finally:
        if not args.keep and os.path.exists(work_dir):
            print(f"\n🧹 正在清理工作区: {work_dir}")
            shutil.rmtree(work_dir, ignore_errors=True)
            print("🧹 清理完成。")


if __name__ == "__main__":
    main()