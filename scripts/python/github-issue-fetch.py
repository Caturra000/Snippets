#!/usr/bin/env python3
"""
GitHub Issues 拉取工具 - 支持断点续传、Rate Limit 自动等待、多格式导出

用法:
    python fetch_issues.py owner/repo
    python fetch_issues.py owner/repo --state open --format md
    GITHUB_TOKEN=ghp_xxx python fetch_issues.py owner/repo
"""

import argparse
import requests
import json
import csv
import time
import os
import sys
from datetime import datetime


class GitHubIssueFetcher:

    def __init__(self, token=None):
        self.session = requests.Session()
        self.session.headers.update({"Accept": "application/vnd.github.v3+json"})
        if token:
            self.session.headers["Authorization"] = f"token {token}"
        self.rate_remaining = None
        self.rate_reset_time = None

    # ---------- HTTP ----------

    def _request(self, url, params=None, max_retries=3):
        for attempt in range(max_retries):
            if self.rate_remaining is not None and self.rate_remaining < 5:
                self._wait_for_reset()

            resp = self.session.get(url, params=params)
            self.rate_remaining = int(resp.headers.get("X-RateLimit-Remaining", 999))
            self.rate_reset_time = int(resp.headers.get("X-RateLimit-Reset", 0))

            if resp.status_code == 200:
                return resp.json()
            if resp.status_code == 403 and "rate limit" in resp.text.lower():
                print(f"\n⚠️  Rate limit hit (remaining={self.rate_remaining})")
                self._wait_for_reset()
                continue

            print(f"❌ HTTP {resp.status_code}: {resp.text[:200]}")
            if attempt < max_retries - 1:
                time.sleep(5)
        return None

    def _wait_for_reset(self):
        wait = max(self.rate_reset_time - int(time.time()), 0) + 10 if self.rate_reset_time else 3600
        reset_str = (datetime.fromtimestamp(self.rate_reset_time).strftime("%H:%M:%S")
                     if self.rate_reset_time else "?")
        print(f"⏳ Rate limit resets at {reset_str}, waiting {wait}s ... (Ctrl+C safe)")
        try:
            for r in range(wait, 0, -15):
                print(f"   {r}s ...", end="\r", flush=True)
                time.sleep(min(15, r))
        except KeyboardInterrupt:
            raise
        print()

    def check_rate_limit(self):
        resp = self.session.get("https://api.github.com/rate_limit")
        if resp.status_code != 200:
            print(f"⚠️  Cannot check rate limit (HTTP {resp.status_code})")
            return
        d = resp.json()["resources"]["core"]
        reset_str = datetime.fromtimestamp(d["reset"]).strftime("%H:%M:%S")
        print(f"📊 Rate limit: {d['remaining']}/{d['limit']}, resets {reset_str}")

    # ---------- 断点续传 ----------

    @staticmethod
    def _ckpt_path(owner, repo):
        return f".ckpt_{owner}_{repo}.json"

    def _save_ckpt(self, owner, repo, issues, next_page):
        path = self._ckpt_path(owner, repo)
        with open(path, "w", encoding="utf-8") as f:
            json.dump({
                "issues": issues,
                "next_page": next_page,
                "saved_at": datetime.now().isoformat(),
            }, f, ensure_ascii=False)
        print(f"💾 Checkpoint: {len(issues)} issues, next=page {next_page}")

    def _load_ckpt(self, owner, repo):
        path = self._ckpt_path(owner, repo)
        if not os.path.exists(path):
            return None
        with open(path, "r", encoding="utf-8") as f:
            ckpt = json.load(f)
        count = len(ckpt["issues"])
        page = ckpt["next_page"]
        print(f"📂 Checkpoint found: {count} issues, next=page {page} (saved {ckpt['saved_at']})")
        ans = input("   Resume? [Y/n]: ").strip().lower()
        return ckpt if ans in ("", "y", "yes") else None

    def _del_ckpt(self, owner, repo):
        path = self._ckpt_path(owner, repo)
        if os.path.exists(path):
            os.remove(path)

    # ---------- 拉取 ----------

    def fetch(self, owner, repo, state="all"):
        # 加载断点
        ckpt = self._load_ckpt(owner, repo)
        if ckpt:
            all_issues = ckpt["issues"]
            seen = {i["number"] for i in all_issues}
            page = ckpt["next_page"]
            print(f"▶️  Resuming from page {page}, {len(all_issues)} issues loaded\n")
        else:
            all_issues, seen, page = [], set(), 1

        # 仓库信息
        info = self._request(f"https://api.github.com/repos/{owner}/{repo}")
        if info:
            print(f"📦 {owner}/{repo}  (open issues+PRs: {info.get('open_issues_count', '?')})\n")

        empty_streak = 0

        try:
            while True:
                params = {
                    "state": state,
                    "per_page": 100,
                    "page": page,
                    "sort": "created",
                    "direction": "asc",     # ← 关键：从旧到新，断点续传才有意义
                }

                data = self._request(
                    f"https://api.github.com/repos/{owner}/{repo}/issues", params
                )

                # API 返回空 = 没有更多了
                if not data:
                    break

                added = 0
                for item in data:
                    if "pull_request" in item:
                        continue
                    n = item["number"]
                    if n in seen:
                        continue
                    seen.add(n)
                    all_issues.append({
                        "number":   n,
                        "title":    item["title"],
                        "body":     (item.get("body") or "")[:3000],
                        "state":    item["state"],
                        "labels":   [l["name"] for l in item.get("labels", [])],
                        "user":     item["user"]["login"],
                        "created":  item["created_at"][:10],
                        "comments": item["comments"],
                        "url":      item["html_url"],
                    })
                    added += 1

                print(f"  page {page}: +{added} new, total={len(all_issues)}  "
                      f"(rate remaining: {self.rate_remaining})")

                # 连续空页检测（比如 resume 后几页全是已有的 PR）
                if added == 0:
                    empty_streak += 1
                    if empty_streak >= 3:
                        print("   (3 consecutive empty pages, stopping)")
                        break
                else:
                    empty_streak = 0

                # 每 5 页存一次断点
                if page % 5 == 0:
                    self._save_ckpt(owner, repo, all_issues, page + 1)

                page += 1
                time.sleep(0.3)

        except KeyboardInterrupt:
            print(f"\n\n⚡ Interrupted!")
            self._save_ckpt(owner, repo, all_issues, page)
            print("   Run again to resume.\n")
            return all_issues

        # 完成
        self._del_ckpt(owner, repo)
        print(f"\n✅ Done: {len(all_issues)} issues\n")
        return all_issues


# ---------- 导出 ----------

def save_json(issues, path):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(issues, f, ensure_ascii=False, indent=2)

def save_md(issues, path):
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"# Issues ({len(issues)})\n\n")
        for i in issues:
            emoji = "🟢" if i["state"] == "open" else "🔴"
            labels = " ".join(f"`{l}`" for l in i["labels"])
            body = (i["body"][:800] or "(empty)").strip()
            f.write(f"## {emoji} #{i['number']}: {i['title']}\n\n")
            f.write(f"{i['state']} | {labels} | @{i['user']} | {i['created']}\n\n")
            f.write(f"{body}\n\n---\n\n")

def save_csv(issues, path):
    with open(path, "w", encoding="utf-8-sig", newline="") as f:
        w = csv.writer(f)
        w.writerow(["#", "state", "title", "labels", "user", "date",
                     "comments", "url", "body_preview"])
        for i in issues:
            w.writerow([i["number"], i["state"], i["title"],
                        ",".join(i["labels"]), i["user"], i["created"],
                        i["comments"], i["url"],
                        (i["body"][:300] or "").replace("\n", " ")])

def save_txt(issues, path):
    with open(path, "w", encoding="utf-8") as f:
        for i in issues:
            body = (i["body"][:500] or "").replace("\n", " ").strip()
            labels = f" [{','.join(i['labels'])}]" if i["labels"] else ""
            f.write(f"#{i['number']} ({i['state']}){labels} {i['title']}\n  {body}\n\n")

SAVERS = {"json": save_json, "md": save_md, "csv": save_csv, "txt": save_txt}


# ---------- CLI ----------

def main():
    p = argparse.ArgumentParser(
        description="Fetch GitHub issues for LLM analysis",
        epilog="Token: use --token, or set GITHUB_TOKEN env var, or omit (60 req/h)")
    p.add_argument("repo", help="owner/repo  e.g. vuejs/core")
    p.add_argument("--token",  default=None, help="GitHub personal access token")
    p.add_argument("--state",  default="all", choices=["open", "closed", "all"])
    p.add_argument("--format", default="json,md,txt",
                   help="comma-separated: json,md,csv,txt  (default: json,md,txt)")
    p.add_argument("--output", default=None, help="output base name (default: <repo>_issues)")
    p.add_argument("--rate",   action="store_true", help="check rate limit and exit")
    args = p.parse_args()

    token = args.token or os.environ.get("GITHUB_TOKEN")
    if token:
        print(f"🔑 Using token: {token[:10]}...")
    else:
        print("⚠️  No token → 60 req/h. Set GITHUB_TOKEN or --token for 5000 req/h")

    fetcher = GitHubIssueFetcher(token=token)

    if args.rate:
        fetcher.check_rate_limit()
        return

    parts = args.repo.strip("/").split("/")
    if len(parts) != 2:
        print(f"❌ Bad format: '{args.repo}', expected owner/repo")
        sys.exit(1)
    owner, repo = parts

    fetcher.check_rate_limit()
    print()
    issues = fetcher.fetch(owner, repo, state=args.state)
    if not issues:
        print("No issues fetched.")
        return

    base = args.output or f"{repo}_issues"
    for fmt in (f.strip() for f in args.format.split(",")):
        if fmt not in SAVERS:
            print(f"⚠️  Unknown format '{fmt}', skip")
            continue
        path = f"{base}.{fmt}"
        SAVERS[fmt](issues, path)
        print(f"💾 {path}  ({os.path.getsize(path) / 1024:.0f} KB)")


if __name__ == "__main__":
    main()
