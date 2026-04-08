#!/usr/bin/env python3
"""
文档处理器
处理下载的 HTML、PDF、MD 文件
- HTML: 转换为文本 (主方法: 正则解析, 备选方法: trafilatura)
- PDF: 使用 pdf2text 转换为文本
- MD: 保持不变
"""

import argparse
import json
import logging
import os
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from threading import Lock

from tqdm import tqdm

# 导入处理库
try:
    from html2text import convert_html_file_to_text, convert_html_file_with_trafilatura
except ImportError:
    print("错误: 找不到 html2text 模块，请确保 html2text.py 在当前目录或 PYTHONPATH 中")
    sys.exit(1)

try:
    from pdf2text import pdf_to_text
except ImportError:
    print("错误: 找不到 pdf2text 模块，请确保 pdf2text.py 在当前目录或 PYTHONPATH 中")
    sys.exit(1)


class FileType(Enum):
    HTML = "html"
    PDF = "pdf"
    MD = "md"
    OTHER = "other"


class ProcessStatus(Enum):
    SUCCESS = "success"
    FAILED = "failed"
    SKIPPED = "skipped"
    EMPTY_OUTPUT = "empty_output"
    TOO_SMALL = "too_small"
    ERROR = "error"


@dataclass
class ProcessResult:
    filepath: str
    file_type: FileType
    status: ProcessStatus
    message: str = ""
    original_size: int = 0
    output_size: int = 0


@dataclass
class ProcessStats:
    total: int = 0
    success: int = 0
    failed: int = 0
    skipped: int = 0
    html_processed: int = 0
    pdf_processed: int = 0
    md_skipped: int = 0
    errors: list = field(default_factory=list)
    
    def add_result(self, result: ProcessResult):
        self.total += 1
        if result.status == ProcessStatus.SUCCESS:
            self.success += 1
            if result.file_type == FileType.HTML:
                self.html_processed += 1
            elif result.file_type == FileType.PDF:
                self.pdf_processed += 1
        elif result.status == ProcessStatus.SKIPPED:
            self.skipped += 1
            if result.file_type == FileType.MD:
                self.md_skipped += 1
        else:
            self.failed += 1
            self.errors.append((result.filepath, result.status, result.message))


class DocumentProcessor:
    MIN_OUTPUT_SIZE = 512  # 最小输出大小（字节），低于此值将触发 Fallback
    
    def __init__(
        self,
        input_dir: str,
        workers: int = 8,
        log_file: str = "process.log",
    ):
        self.input_dir = Path(input_dir)
        self.workers = workers
        self.log_file = log_file
        
        self.stats = ProcessStats()
        self.stats_lock = Lock()
        
        self._setup_logging()
    
    def _setup_logging(self):
        """配置日志"""
        self.logger = logging.getLogger("DocumentProcessor")
        self.logger.setLevel(logging.DEBUG)
        
        # 清除现有处理器
        self.logger.handlers.clear()
        
        # 文件处理器 - 详细日志
        file_handler = logging.FileHandler(self.log_file, encoding="utf-8")
        file_handler.setLevel(logging.DEBUG)
        file_formatter = logging.Formatter(
            "%(asctime)s | %(levelname)-8s | %(message)s",
            datefmt="%Y-%m-%d %H:%M:%S"
        )
        file_handler.setFormatter(file_formatter)
        self.logger.addHandler(file_handler)
        
        # 控制台处理器 - 仅警告和错误
        console_handler = logging.StreamHandler()
        console_handler.setLevel(logging.WARNING)
        console_formatter = logging.Formatter("%(levelname)s: %(message)s")
        console_handler.setFormatter(console_formatter)
        self.logger.addHandler(console_handler)
    
    def _get_file_type(self, filepath: Path) -> FileType:
        """判断文件类型"""
        suffix = filepath.suffix.lower()
        if suffix in [".html", ".htm"]:
            return FileType.HTML
        elif suffix == ".pdf":
            return FileType.PDF
        elif suffix == ".md":
            return FileType.MD
        else:
            return FileType.OTHER
    
    def _process_html(self, filepath: Path) -> ProcessResult:
        """处理 HTML 文件"""
        original_size = filepath.stat().st_size
        
        try:
            # 1. 主方法：使用定制的正则提取方案（擅长保留代码文档）
            text = convert_html_file_to_text(str(filepath))
            output_size = len(text.encode("utf-8")) if text else 0
            used_fallback = False
            
            # 2. 备选方案：如果主方法提取内容过少，尝试使用 trafilatura 提取
            if not text or not text.strip() or output_size < self.MIN_OUTPUT_SIZE:
                self.logger.debug(f"[{filepath.name}] 主方法输出不足 ({output_size} 字节)，尝试 fallback")
                
                fallback_text = convert_html_file_with_trafilatura(str(filepath))
                fallback_size = len(fallback_text.encode("utf-8")) if fallback_text else 0
                
                if fallback_text and fallback_text.strip() and fallback_size > output_size:
                    text = fallback_text
                    output_size = fallback_size
                    used_fallback = True
            
            # 3. 最终判定结果
            if text is None or not text.strip():
                return ProcessResult(
                    filepath=str(filepath),
                    file_type=FileType.HTML,
                    status=ProcessStatus.EMPTY_OUTPUT,
                    message="转换后输出为空",
                    original_size=original_size,
                    output_size=0
                )
            
            if output_size < self.MIN_OUTPUT_SIZE:
                return ProcessResult(
                    filepath=str(filepath),
                    file_type=FileType.HTML,
                    status=ProcessStatus.TOO_SMALL,
                    message=f"输出太小: {output_size} 字节",
                    original_size=original_size,
                    output_size=output_size
                )
            
            # 4. 覆盖写入新的文本文件
            txt_filepath = filepath.with_suffix(".txt")
            with open(txt_filepath, "w", encoding="utf-8") as f:
                f.write(text)
            
            # 删除原 HTML 文件
            filepath.unlink()
            
            msg = f"转换为 {txt_filepath.name}"
            if used_fallback:
                msg += " (Trafilatura Fallback)"
                
            return ProcessResult(
                filepath=str(filepath),
                file_type=FileType.HTML,
                status=ProcessStatus.SUCCESS,
                message=msg,
                original_size=original_size,
                output_size=output_size
            )
            
        except Exception as e:
            return ProcessResult(
                filepath=str(filepath),
                file_type=FileType.HTML,
                status=ProcessStatus.ERROR,
                message=str(e),
                original_size=original_size
            )
    
    def _process_pdf(self, filepath: Path) -> ProcessResult:
        """处理 PDF 文件"""
        original_size = filepath.stat().st_size
        
        try:
            # 转换 PDF 为文本
            text = pdf_to_text(str(filepath))
            
            if text is None or not text.strip():
                return ProcessResult(
                    filepath=str(filepath),
                    file_type=FileType.PDF,
                    status=ProcessStatus.EMPTY_OUTPUT,
                    message="转换后输出为空",
                    original_size=original_size,
                    output_size=0
                )
            
            output_size = len(text.encode("utf-8"))
            
            if output_size < self.MIN_OUTPUT_SIZE:
                return ProcessResult(
                    filepath=str(filepath),
                    file_type=FileType.PDF,
                    status=ProcessStatus.TOO_SMALL,
                    message=f"输出太小: {output_size} 字节",
                    original_size=original_size,
                    output_size=output_size
                )
            
            # 写入新的文本文件
            txt_filepath = filepath.with_suffix(".txt")
            with open(txt_filepath, "w", encoding="utf-8") as f:
                f.write(text)
            
            # 删除原 PDF 文件
            filepath.unlink()
            
            return ProcessResult(
                filepath=str(filepath),
                file_type=FileType.PDF,
                status=ProcessStatus.SUCCESS,
                message=f"转换为 {txt_filepath.name}",
                original_size=original_size,
                output_size=output_size
            )
            
        except Exception as e:
            return ProcessResult(
                filepath=str(filepath),
                file_type=FileType.PDF,
                status=ProcessStatus.ERROR,
                message=str(e),
                original_size=original_size
            )
    
    def _process_file(self, filepath: Path) -> ProcessResult:
        """处理单个文件"""
        file_type = self._get_file_type(filepath)
        
        if file_type == FileType.HTML:
            return self._process_html(filepath)
        elif file_type == FileType.PDF:
            return self._process_pdf(filepath)
        elif file_type == FileType.MD:
            # MD 文件跳过不处理
            return ProcessResult(
                filepath=str(filepath),
                file_type=FileType.MD,
                status=ProcessStatus.SKIPPED,
                message="MD 文件保持不变",
                original_size=filepath.stat().st_size
            )
        else:
            return ProcessResult(
                filepath=str(filepath),
                file_type=FileType.OTHER,
                status=ProcessStatus.SKIPPED,
                message=f"不支持的文件类型: {filepath.suffix}"
            )
    
    def _collect_files(self) -> list[Path]:
        """收集需要处理的文件"""
        files = []
        for ext in ["*.html", "*.htm", "*.pdf", "*.md"]:
            files.extend(self.input_dir.glob(ext))
            files.extend(self.input_dir.glob(f"**/{ext}"))  # 递归子目录
        
        # 去重
        files = list(set(files))
        return sorted(files)
    
    def process_all(self, dry_run: bool = False):
        """处理所有文件"""
        files = self._collect_files()
        
        if not files:
            print("没有找到需要处理的文件")
            return
        
        # 统计文件类型
        type_counts = {FileType.HTML: 0, FileType.PDF: 0, FileType.MD: 0, FileType.OTHER: 0}
        for f in files:
            type_counts[self._get_file_type(f)] += 1
        
        print(f"\n找到 {len(files)} 个文件:")
        print(f"  HTML: {type_counts[FileType.HTML]}")
        print(f"  PDF:  {type_counts[FileType.PDF]}")
        print(f"  MD:   {type_counts[FileType.MD]}")
        if type_counts[FileType.OTHER] > 0:
            print(f"  其他: {type_counts[FileType.OTHER]}")
        
        if dry_run:
            print("\n[Dry Run] 以下文件将被处理:")
            for f in files[:20]:
                file_type = self._get_file_type(f)
                action = {
                    FileType.HTML: "转换为TXT",
                    FileType.PDF: "转换为TXT",
                    FileType.MD: "跳过",
                    FileType.OTHER: "跳过"
                }[file_type]
                print(f"  [{action}] {f.name}")
            if len(files) > 20:
                print(f"  ... 还有 {len(files) - 20} 个文件")
            return
        
        self.logger.info("=" * 60)
        self.logger.info(f"开始处理 {len(files)} 个文件")
        self.logger.info(f"输入目录: {self.input_dir}")
        self.logger.info(f"并发数: {self.workers}")
        self.logger.info("=" * 60)
        
        print(f"\n开始处理，{self.workers} 并发")
        print(f"日志文件: {self.log_file}\n")
        
        try:
            with ThreadPoolExecutor(max_workers=self.workers) as executor:
                futures = {
                    executor.submit(self._process_file, f): f 
                    for f in files
                }
                
                with tqdm(total=len(files), desc="处理进度") as pbar:
                    for future in as_completed(futures):
                        filepath = futures[future]
                        try:
                            result = future.result()
                            
                            with self.stats_lock:
                                self.stats.add_result(result)
                            
                            # 记录日志
                            if result.status == ProcessStatus.SUCCESS:
                                self.logger.info(
                                    f"[SUCCESS] {result.filepath} | {result.message}"
                                )
                            elif result.status == ProcessStatus.SKIPPED:
                                self.logger.debug(
                                    f"[SKIPPED] {result.filepath} | {result.message}"
                                )
                            else:
                                self.logger.warning(
                                    f"[{result.status.name}] {result.filepath} | {result.message}"
                                )
                                tqdm.write(f"警告: {filepath.name} - {result.message}")
                                
                        except Exception as e:
                            self.logger.error(f"[EXCEPTION] {filepath} | {e}")
                            with self.stats_lock:
                                self.stats.failed += 1
                                self.stats.total += 1
                                self.stats.errors.append((str(filepath), ProcessStatus.ERROR, str(e)))
                            tqdm.write(f"错误: {filepath.name} - {e}")
                        
                        pbar.update(1)
        
        except KeyboardInterrupt:
            print("\n\n处理被中断")
        
        # 输出统计
        self._print_stats()
        self._save_report()
    
    def _print_stats(self):
        """打印统计信息"""
        print("\n" + "=" * 40)
        print("处理完成统计")
        print("=" * 40)
        print(f"总计处理: {self.stats.total}")
        print(f"  成功: {self.stats.success}")
        print(f"    HTML 转换: {self.stats.html_processed}")
        print(f"    PDF 转换: {self.stats.pdf_processed}")
        print(f"  跳过: {self.stats.skipped}")
        print(f"    MD 文件: {self.stats.md_skipped}")
        print(f"  失败: {self.stats.failed}")
        
        if self.stats.errors:
            print(f"\n失败详情 (前10个):")
            for filepath, status, message in self.stats.errors[:10]:
                print(f"  [{status.name}] {Path(filepath).name}: {message}")
            if len(self.stats.errors) > 10:
                print(f"  ... 还有 {len(self.stats.errors) - 10} 个错误")
        
        print(f"\n详细日志: {self.log_file}")
    
    def _save_report(self):
        """保存处理报告"""
        report = {
            "timestamp": datetime.now().isoformat(),
            "input_dir": str(self.input_dir),
            "stats": {
                "total": self.stats.total,
                "success": self.stats.success,
                "failed": self.stats.failed,
                "skipped": self.stats.skipped,
                "html_processed": self.stats.html_processed,
                "pdf_processed": self.stats.pdf_processed,
                "md_skipped": self.stats.md_skipped,
            },
            "errors": [
                {"file": f, "status": s.name, "message": m}
                for f, s, m in self.stats.errors
            ]
        }
        
        report_file = Path(self.log_file).with_suffix(".json")
        with open(report_file, "w", encoding="utf-8") as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        
        self.logger.info(f"报告已保存: {report_file}")


def main():
    parser = argparse.ArgumentParser(
        description="文档处理器 - 转换 HTML 和 PDF 为纯文本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  %(prog)s ./downloads                   # 处理 downloads 目录
  %(prog)s ./downloads -w 16             # 16 并发
  %(prog)s ./downloads --dry-run         # 预览模式
        """
    )
    
    parser.add_argument("input_dir", help="输入目录")
    parser.add_argument("-w", "--workers", type=int, default=8,
                        help="并发数 (默认: 8)")
    parser.add_argument("-l", "--log-file", default="process.log",
                        help="日志文件 (默认: process.log)")
    parser.add_argument("--dry-run", action="store_true",
                        help="预览模式")

    args = parser.parse_args()

    if not os.path.isdir(args.input_dir):
        print(f"错误: 目录不存在 {args.input_dir}")
        return 1

    processor = DocumentProcessor(
        input_dir=args.input_dir,
        workers=args.workers,
        log_file=args.log_file,
    )

    processor.process_all(dry_run=args.dry_run)
    return 0


if __name__ == "__main__":
    exit(main())