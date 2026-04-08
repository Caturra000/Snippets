#!/usr/bin/env python3
"""
WG21 文档下载器
从 wg21.link 的 index.json 下载指定的提案文档
支持并发下载
"""

import json
import os
import re
import time
import argparse
from pathlib import Path
from urllib.parse import urlparse
from collections import defaultdict
from typing import Optional
from concurrent.futures import ThreadPoolExecutor, as_completed
from threading import Lock
import requests
from tqdm import tqdm


class DownloadResult:
    """下载结果"""
    SUCCESS = "success"
    SKIP = "skip"
    FAIL = "fail"
    
    def __init__(self, key: str, status: str, message: str = ""):
        self.key = key
        self.status = status
        self.message = message


class WG21Downloader:
    SKIP_STATUS_CODES = {401, 403, 404, 410, 451}
    
    def __init__(
        self,
        index_file: str,
        output_dir: str = "downloads",
        progress_file: str = ".download_progress.json",
        delay: float = 0.1,
        max_retries: int = 3,
        timeout: int = 60,
        workers: int = 8,
    ):
        self.index_file = index_file
        self.output_dir = Path(output_dir)
        self.progress_file = Path(progress_file)
        self.delay = delay
        self.max_retries = max_retries
        self.timeout = timeout
        self.workers = workers

        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # 线程安全锁
        self.progress_lock = Lock()
        self.progress = self._load_progress()

    def _create_session(self) -> requests.Session:
        """为每个线程创建独立的 session"""
        session = requests.Session()
        session.headers.update({
            "User-Agent": "WG21-Downloader/1.0 (Educational/Research Purpose)"
        })
        # 连接池设置
        adapter = requests.adapters.HTTPAdapter(
            pool_connections=self.workers,
            pool_maxsize=self.workers * 2,
            max_retries=0  # 我们自己处理重试
        )
        session.mount("http://", adapter)
        session.mount("https://", adapter)
        return session

    def _load_progress(self) -> dict:
        """加载下载进度"""
        if self.progress_file.exists():
            try:
                with open(self.progress_file, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    if "downloaded" not in data:
                        data["downloaded"] = []
                    if "failed" not in data:
                        data["failed"] = {}
                    if "skipped" not in data:
                        data["skipped"] = {}
                    return data
            except (json.JSONDecodeError, IOError):
                pass
        return {"downloaded": [], "failed": {}, "skipped": {}}

    def _save_progress(self):
        """保存下载进度（线程安全）"""
        with self.progress_lock:
            with open(self.progress_file, "w", encoding="utf-8") as f:
                json.dump(self.progress, f, indent=2, ensure_ascii=False)

    def _update_progress(self, result: DownloadResult):
        """更新进度（线程安全）"""
        with self.progress_lock:
            if result.status == DownloadResult.SUCCESS:
                if result.key not in self.progress["downloaded"]:
                    self.progress["downloaded"].append(result.key)
                self.progress["failed"].pop(result.key, None)
                self.progress["skipped"].pop(result.key, None)
            elif result.status == DownloadResult.SKIP:
                self.progress["skipped"][result.key] = result.message
                self.progress["failed"].pop(result.key, None)
            else:
                self.progress["failed"][result.key] = result.message

    def _sanitize_filename(self, filename: str) -> str:
        """清理文件名"""
        illegal_chars = r'[<>:"/\\|?*\x00-\x1f]'
        filename = re.sub(illegal_chars, "_", filename)
        filename = filename.strip(" .")
        if len(filename) > 200:
            filename = filename[:200]
        return filename

    def _get_extension(self, url: str, content_type: Optional[str] = None) -> str:
        """获取文件扩展名"""
        parsed = urlparse(url)
        path = parsed.path.lower()
        
        for ext in [".pdf", ".html", ".htm", ".md", ".txt", ".rst"]:
            if path.endswith(ext):
                return ".html" if ext == ".htm" else ext
        
        if content_type:
            if "pdf" in content_type:
                return ".pdf"
            elif "html" in content_type:
                return ".html"
            elif "plain" in content_type:
                return ".txt"
        
        return ".html"

    def _parse_paper_key(self, key: str) -> tuple[str, Optional[int]]:
        """解析提案 key"""
        p_match = re.match(r"^(P\d+)R(\d+)$", key, re.IGNORECASE)
        if p_match:
            return (p_match.group(1).upper(), int(p_match.group(2)))
        
        p_no_r_match = re.match(r"^(P\d+)$", key, re.IGNORECASE)
        if p_no_r_match:
            return (p_no_r_match.group(1).upper(), None)
        
        n_match = re.match(r"^(N\d+)$", key, re.IGNORECASE)
        if n_match:
            return (n_match.group(1).upper(), None)
        
        return (key.upper(), None)

    def load_index(self) -> dict:
        """加载 index.json"""
        with open(self.index_file, "r", encoding="utf-8") as f:
            return json.load(f)

    def filter_documents(self, index: dict) -> dict:
        """过滤文档"""
        filtered = {}
        p_papers = defaultdict(list)

        for key, value in index.items():
            if "long_link" not in value or not value["long_link"]:
                continue

            upper_key = key.upper()
            
            if upper_key.startswith("N") and re.match(r"^N\d+$", upper_key):
                filtered[key] = value
            elif upper_key.startswith("P"):
                base, revision = self._parse_paper_key(upper_key)
                if base.startswith("P"):
                    p_papers[base].append((key, revision, value))

        for base, versions in p_papers.items():
            versions.sort(key=lambda x: x[1] if x[1] is not None else -1, reverse=True)
            key, _, value = versions[0]
            filtered[key] = value

        return filtered

    def _download_single(
        self, 
        session: requests.Session,
        key: str, 
        value: dict
    ) -> DownloadResult:
        """下载单个文件（在线程中执行）"""
        title = value.get("title", "Unknown")
        long_link = value.get("long_link", "")

        if not long_link:
            return DownloadResult(key, DownloadResult.SKIP, "No long_link")

        # 构建文件名
        safe_title = self._sanitize_filename(title)
        safe_key = self._sanitize_filename(key)
        base_filename = f"{safe_key} - {safe_title}"
        
        ext = self._get_extension(long_link)
        filepath = self.output_dir / f"{base_filename}{ext}"

        for attempt in range(self.max_retries):
            try:
                response = session.get(
                    long_link,
                    timeout=self.timeout,
                    stream=True,
                    allow_redirects=True
                )
                
                # 检查需要跳过的状态码
                if response.status_code in self.SKIP_STATUS_CODES:
                    return DownloadResult(
                        key, 
                        DownloadResult.SKIP,
                        f"HTTP {response.status_code}"
                    )
                
                response.raise_for_status()
                
                # 更新扩展名
                content_type = response.headers.get("Content-Type", "")
                actual_ext = self._get_extension(long_link, content_type)
                if filepath.suffix.lower() != actual_ext.lower():
                    filepath = filepath.with_suffix(actual_ext)

                # 写入文件
                with open(filepath, "wb") as f:
                    for chunk in response.iter_content(chunk_size=8192):
                        if chunk:
                            f.write(chunk)

                # 限流
                if self.delay > 0:
                    time.sleep(self.delay)

                return DownloadResult(key, DownloadResult.SUCCESS)

            except requests.exceptions.HTTPError as e:
                if e.response is not None and e.response.status_code in self.SKIP_STATUS_CODES:
                    return DownloadResult(
                        key, 
                        DownloadResult.SKIP,
                        f"HTTP {e.response.status_code}"
                    )
                if attempt == self.max_retries - 1:
                    return DownloadResult(key, DownloadResult.FAIL, str(e))
                time.sleep((attempt + 1) * 2)
                    
            except requests.exceptions.RequestException as e:
                if attempt == self.max_retries - 1:
                    return DownloadResult(key, DownloadResult.FAIL, str(e))
                time.sleep((attempt + 1) * 2)

        return DownloadResult(key, DownloadResult.FAIL, "未知错误")

    def download_all(self, documents: dict, dry_run: bool = False):
        """并发下载所有文档"""
        downloaded_set = set(self.progress.get("downloaded", []))
        skipped_set = set(self.progress.get("skipped", {}).keys())
        
        to_download = {
            k: v for k, v in documents.items() 
            if k not in downloaded_set and k not in skipped_set
        }
        
        print(f"\n总计 {len(documents)} 个文档")
        print(f"已下载: {len(downloaded_set)}")
        print(f"已跳过: {len(skipped_set)}")
        print(f"待下载: {len(to_download)}")
        
        if dry_run:
            print("\n[Dry Run] 以下文档将被下载:")
            for i, (key, value) in enumerate(to_download.items(), 1):
                print(f"  {i}. {key}: {value.get('title', 'Unknown')}")
                if i >= 20:
                    print(f"  ... 还有 {len(to_download) - 20} 个文档")
                    break
            return

        if not to_download:
            print("\n所有文档已处理完成！")
            return

        print(f"\n开始下载: {self.workers} 并发, {self.delay}s 间隔")
        print("按 Ctrl+C 可安全中断\n")

        stats = {"success": 0, "skip": 0, "fail": 0}
        items = list(to_download.items())
        save_interval = max(10, self.workers * 2)  # 保存间隔

        try:
            with ThreadPoolExecutor(max_workers=self.workers) as executor:
                # 每个线程使用独立的 session
                sessions = {i: self._create_session() for i in range(self.workers)}
                
                # 提交所有任务
                future_to_key = {}
                for i, (key, value) in enumerate(items):
                    session = sessions[i % self.workers]
                    future = executor.submit(self._download_single, session, key, value)
                    future_to_key[future] = key

                # 处理完成的任务
                with tqdm(total=len(items), desc="下载进度") as pbar:
                    completed = 0
                    for future in as_completed(future_to_key):
                        try:
                            result = future.result()
                            self._update_progress(result)
                            
                            if result.status == DownloadResult.SUCCESS:
                                stats["success"] += 1
                            elif result.status == DownloadResult.SKIP:
                                stats["skip"] += 1
                                tqdm.write(f"跳过 {result.key}: {result.message}")
                            else:
                                stats["fail"] += 1
                                tqdm.write(f"失败 {result.key}: {result.message}")
                                
                        except Exception as e:
                            key = future_to_key[future]
                            stats["fail"] += 1
                            tqdm.write(f"异常 {key}: {e}")

                        completed += 1
                        pbar.update(1)
                        
                        # 定期保存
                        if completed % save_interval == 0:
                            self._save_progress()

                # 关闭所有 session
                for session in sessions.values():
                    session.close()

        except KeyboardInterrupt:
            print("\n\n下载被中断，正在保存进度...")
        finally:
            self._save_progress()
            print(f"\n进度已保存")
            print(f"本次: 成功 {stats['success']}, 跳过 {stats['skip']}, 失败 {stats['fail']}")
            print(f"累计: 已下载 {len(self.progress['downloaded'])}, "
                  f"已跳过 {len(self.progress['skipped'])}, "
                  f"失败 {len(self.progress['failed'])}")

    def show_stats(self, documents: dict):
        """显示统计信息"""
        n_count = sum(1 for k in documents if k.upper().startswith("N"))
        p_count = sum(1 for k in documents if k.upper().startswith("P"))
        
        print(f"\n文档统计:")
        print(f"  N 系列: {n_count}")
        print(f"  P 系列: {p_count}")
        print(f"  总计: {len(documents)}")
        
        print(f"\n下载进度:")
        print(f"  已下载: {len(self.progress.get('downloaded', []))}")
        print(f"  已跳过: {len(self.progress.get('skipped', {}))}")
        print(f"  失败: {len(self.progress.get('failed', {}))}")

    def retry_failed(self, documents: dict):
        """重试失败的下载"""
        failed = self.progress.get("failed", {})
        if not failed:
            print("没有失败的下载需要重试")
            return

        print(f"\n重试 {len(failed)} 个失败的下载...")
        retry_docs = {k: documents[k] for k in failed if k in documents}
        
        with self.progress_lock:
            for key in retry_docs:
                if key in self.progress.get("downloaded", []):
                    self.progress["downloaded"].remove(key)
            self.progress["failed"] = {}
        
        self._save_progress()
        self.download_all(retry_docs)

    def show_skipped(self):
        """显示跳过的文档"""
        skipped = self.progress.get("skipped", {})
        if not skipped:
            print("没有被跳过的文档")
            return
        print(f"\n被跳过的文档 ({len(skipped)} 个):")
        for key, reason in list(skipped.items())[:50]:
            print(f"  {key}: {reason}")
        if len(skipped) > 50:
            print(f"  ... 还有 {len(skipped) - 50} 个")

    def show_failed(self):
        """显示失败的文档"""
        failed = self.progress.get("failed", {})
        if not failed:
            print("没有失败的文档")
            return
        print(f"\n失败的文档 ({len(failed)} 个):")
        for key, reason in failed.items():
            print(f"  {key}: {reason}")


def main():
    parser = argparse.ArgumentParser(
        description="WG21 文档下载器 - 支持并发下载",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s index.json                    # 默认 8 并发下载
  %(prog)s index.json -w 16              # 16 并发
  %(prog)s index.json -w 4 -d 0.5        # 4 并发，0.5s 间隔
  %(prog)s index.json --dry-run          # 预览
  %(prog)s index.json --retry-failed     # 重试失败的
        """
    )
    
    parser.add_argument("index_file", help="index.json 文件路径")
    parser.add_argument("-o", "--output", default="downloads", 
                        help="输出目录 (默认: downloads)")
    parser.add_argument("-w", "--workers", type=int, default=8,
                        help="并发数 (默认: 8)")
    parser.add_argument("-d", "--delay", type=float, default=0.1,
                        help="每个请求后的延迟秒数 (默认: 0.1)")
    parser.add_argument("-p", "--progress-file", default=".download_progress.json",
                        help="进度文件")
    parser.add_argument("--timeout", type=int, default=60,
                        help="超时秒数 (默认: 60)")
    parser.add_argument("--max-retries", type=int, default=3,
                        help="重试次数 (默认: 3)")
    parser.add_argument("--dry-run", action="store_true",
                        help="预览模式")
    parser.add_argument("--stats", action="store_true",
                        help="显示统计")
    parser.add_argument("--retry-failed", action="store_true",
                        help="重试失败的下载")
    parser.add_argument("--show-skipped", action="store_true",
                        help="显示跳过的文档")
    parser.add_argument("--show-failed", action="store_true",
                        help="显示失败的文档")
    parser.add_argument("--reset-progress", action="store_true",
                        help="重置进度")
    parser.add_argument("--include-skipped", action="store_true",
                        help="包含之前跳过的文档")

    args = parser.parse_args()

    if not os.path.exists(args.index_file):
        print(f"错误: 找不到 {args.index_file}")
        return 1

    if args.reset_progress:
        progress_path = Path(args.progress_file)
        if progress_path.exists():
            progress_path.unlink()
            print("进度已重置")

    downloader = WG21Downloader(
        index_file=args.index_file,
        output_dir=args.output,
        progress_file=args.progress_file,
        delay=args.delay,
        max_retries=args.max_retries,
        timeout=args.timeout,
        workers=args.workers,
    )

    print(f"加载 {args.index_file}...")
    index = downloader.load_index()
    print(f"原始条目: {len(index)}")

    documents = downloader.filter_documents(index)
    print(f"过滤后: {len(documents)} (N/P 开头，P 只保留最新版)")

    if args.show_skipped:
        downloader.show_skipped()
        return 0
    
    if args.show_failed:
        downloader.show_failed()
        return 0

    downloader.show_stats(documents)

    if args.stats:
        return 0

    if args.include_skipped:
        downloader.progress["skipped"] = {}
        downloader._save_progress()
        print("\n已清除跳过记录")

    if args.retry_failed:
        downloader.retry_failed(documents)
        return 0

    downloader.download_all(documents, dry_run=args.dry_run)
    return 0


if __name__ == "__main__":
    exit(main())