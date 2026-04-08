"""
Markdown 文档批量翻译工具

使用 DeepSeek API 将 Markdown 技术文档批量翻译为简体中文。
支持多 API Key 轮询、文件/Chunk 两级并发、自动重试与容错。

用法:
    python translate_md.py <目标目录路径>

环境变量:
    DEEPSEEK_API_KEYS: 一个或多个 DeepSeek API Key，用英文逗号分隔。
"""

from __future__ import annotations

import logging
import os
import random
import re
import sys
import threading
import time
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass
from pathlib import Path
from typing import List, Optional

from openai import OpenAI


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  1. 全局配置（所有可调参数集中在此）
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

@dataclass(frozen=True)
class Config:
    """翻译工具全部可调参数。修改此处即可控制脚本行为。"""

    # ── 文本切分 ────────────────────────────────
    chunk_size: int = 4500              # 单个 Chunk 最大字符数（充分利用 8K 输出窗口）

    # ── DeepSeek API ────────────────────────────
    api_base_url: str = "https://api.deepseek.com"
    api_model: str = "deepseek-chat"
    api_temperature: float = 0.1
    api_max_tokens: int = 8192
    api_timeout: float = 180.0          # 单次请求超时（秒）
    api_max_retries: int = 5            # 单个 Chunk 最大重试次数

    # ── API Key 管理 ────────────────────────────
    env_var_name: str = "DEEPSEEK_API_KEYS"
    key_failure_threshold: int = 2      # 连续失败 N 次后禁用该 Key

    # ── 并发调度 ─────────────────────────────────
    min_file_workers: int = 3           # 文件级最小并发数
    chunk_worker_multiplier: int = 4    # Chunk 线程池 = 文件并发数 × 此系数

    # ── 文件筛选 ─────────────────────────────────
    file_glob: str = "*.md"             # 递归搜索的文件通配符


CFG = Config()


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  2. 翻译系统提示词
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

SYSTEM_PROMPT = """\
你是一位精通技术文档重构与翻译的专家。你的任务是将输入的 Markdown 技术文档翻译为流畅的简体中文，并优化文档格式。

请严格遵守以下执行步骤和规则：

### 1. 核心处理规则
*   **表格重构（智能排版）**：文档中如果出现"包含多行数据、且具有行列对应关系"的纯文本段落（如性能测试数据），**必须**将其重构为标准的 Markdown 表格（使用 `|` 分隔）。
*   **换行重构**：文档中如果出现纯文本风格（如固定字数换行），可进一步排版为 Markdown 形式（无需固定字数换行，拼接按连续一整段输出；合适的地方提供代码块等）。
*   **代码保持**：代码块（```...```）和行内代码（`...`）内部的内容**绝对禁止翻译**（包括变量名、函数名）。仅可翻译代码块中的注释，且需确保不破坏代码语法。
*   **链接与图片**：保持 URL、图片链接 `![]()` 和超链接 `[]()` 的结构和地址完全不变。
*   **标题处理**：如果输入文本以 `---` 开头（YAML Front Matter），允许删除元数据，但是必须提供一级标题。可选增加原文链接。

### 2. 翻译与语言风格
*   **语言**：简体中文。风格专业、简洁，符合中文技术社区习惯。
*   **中西文间隔**：在中文与英文、中文与数字、中文与行内代码符号（`）之间，**必须**添加一个半角空格。（例如：在 C# 中使用...）。
*   **专有名词**：保留常见的技术专有名词英文（如 .NET, Nuget, float, double），不要强行翻译。

### 3. 输出格式规范
*   **纯净输出**：不要在输出结果外层包裹 ```markdown ... ``` 标记，直接输出内容。
*   **结构完整**：不要遗漏任何段落。

#### 示例：
输入：
dataset
Value A
Value B

data1
100
200

输出：
| dataset | Value A | Value B |
| :--- | :--- | :--- |
| data1 | 100 | 200 |
"""


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  3. 日志初始化（logging 天然线程安全，替代手动 print_lock）
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

logging.basicConfig(level=logging.INFO, format="%(message)s")
log = logging.getLogger(__name__)


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  4. API Key 连接池管理
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

@dataclass
class _KeyState:
    """单个 API Key 的运行时状态。"""
    fail_count: int = 0
    active: bool = True


class ApiKeyPool:
    """
    线程安全的 API Key 轮询池。

    - 正常错误（限流/超时）连续达 key_failure_threshold 次后禁用。
    - 致命错误（鉴权失败/欠费）立即禁用。
    """

    FATAL_KEYWORDS = ("401", "402", "insufficient_quota")

    def __init__(self, keys: list[str]) -> None:
        if not keys:
            log.error("错误: 未提供任何 API Key，请设置环境变量 %s", CFG.env_var_name)
            sys.exit(1)

        self._keys: list[str] = list(dict.fromkeys(keys))  # 去重且保序
        self._state: dict[str, _KeyState] = {k: _KeyState() for k in self._keys}
        self._lock = threading.Lock()
        self._index = 0

    @property
    def total(self) -> int:
        return len(self._keys)

    def acquire(self) -> Optional[str]:
        """轮询获取下一个可用 Key；全部耗尽返回 None。"""
        with self._lock:
            active = [k for k in self._keys if self._state[k].active]
            if not active:
                return None
            key = active[self._index % len(active)]
            self._index += 1
            return key

    def report_success(self, key: str) -> None:
        """调用成功：清零该 Key 的连续失败计数。"""
        with self._lock:
            if key in self._state:
                self._state[key].fail_count = 0

    def report_failure(self, key: str, error: Exception) -> bool:
        """
        调用失败：记录并判定是否禁用。

        Returns:
            True  — 该 Key 已被禁用。
            False — 仅记录，Key 仍可用。
        """
        error_lower = str(error).lower()
        with self._lock:
            state = self._state.get(key)
            if state is None or not state.active:
                return False

            if any(kw in error_lower for kw in self.FATAL_KEYWORDS):
                state.active = False
                return True

            state.fail_count += 1
            if state.fail_count >= CFG.key_failure_threshold:
                state.active = False
                return True

            return False

    def print_summary(self) -> None:
        active_n = sum(1 for s in self._state.values() if s.active)
        dead_n = self.total - active_n

        log.info("\n================ API Key 状态报告 ================")
        log.info("总计: %d | 存活: %d | 已禁用: %d", self.total, active_n, dead_n)
        for key, state in self._state.items():
            icon = "🟢 存活" if state.active else "🔴 已禁用"
            log.info("  [%s] %s (连续失败: %d)", icon, _mask_key(key), state.fail_count)
        log.info("==================================================")


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  5. 文本处理工具
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def split_markdown(text: str, max_chars: int = CFG.chunk_size) -> list[str]:
    """
    按段落（双换行）切分 Markdown，确保不在代码块中间断开。
    """
    blocks = text.split("\n\n")
    chunks: list[str] = []
    buffer: list[str] = []
    length = 0
    in_code_block = False

    for block in blocks:
        if block.count("```") % 2 != 0:
            in_code_block = not in_code_block

        buffer.append(block)
        length += len(block) + 2

        if length >= max_chars and not in_code_block:
            chunks.append("\n\n".join(buffer))
            buffer.clear()
            length = 0

    if buffer:
        chunks.append("\n\n".join(buffer))

    return [c.strip() for c in chunks if c.strip()]


def strip_markdown_fences(text: str) -> str:
    """移除 LLM 回复中常见的多余 ```markdown 外层包裹。"""
    text = text.strip()
    if re.match(r"^```(?:markdown|md)\s*\n", text, re.IGNORECASE):
        text = re.sub(r"^```(?:markdown|md)\s*\n", "", text, flags=re.IGNORECASE)
        text = re.sub(r"\n```\s*$", "", text)
    return text.strip()


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  6. 单文件翻译上下文
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

class _FileContext:
    """保证同文件的所有 Chunk 共享同一个 API Key，并在出错时统一切换。"""

    def __init__(self, initial_key: str) -> None:
        self.current_key: Optional[str] = initial_key
        self.lock = threading.Lock()


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  7. 翻译引擎（单 Chunk）
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def _translate_chunk(
    chunk_text: str,
    filename: str,
    chunk_idx: int,
    total_chunks: int,
    file_ctx: _FileContext,
    key_pool: ApiKeyPool,
) -> Optional[str]:
    """翻译单个文本块，包含重试与 Key 自动切换逻辑。"""

    tag = f"{filename} (块 {chunk_idx}/{total_chunks})"
    log.info("      [->] 并发请求中: %s ...", tag)

    for attempt in range(CFG.api_max_retries):
        with file_ctx.lock:
            key = file_ctx.current_key
        if not key:
            log.error("      [💀] %s — 所有 API Key 已失效！", tag)
            return None

        client = OpenAI(
            api_key=key,
            base_url=CFG.api_base_url,
            timeout=CFG.api_timeout,
        )

        try:
            response = client.chat.completions.create(
                model=CFG.api_model,
                messages=[
                    {"role": "system", "content": SYSTEM_PROMPT},
                    {"role": "user", "content": f"待翻译文本：\n\n{chunk_text}"},
                ],
                temperature=CFG.api_temperature,
                max_tokens=CFG.api_max_tokens,
            )
            result = response.choices[0].message.content
            key_pool.report_success(key)
            log.info("      [√] 翻译完成: %s", tag)
            return strip_markdown_fences(result)

        except Exception as exc:
            disabled = key_pool.report_failure(key, exc)
            masked = _mask_key(key)

            if disabled:
                log.warning("      [🚫] 封杀 Key %s (连续错误/欠费) -> %s", masked, exc)
            else:
                log.warning("      [!]  Key %s 报错 (累积1次) -> %s", masked, exc)

            # 统一为当前文件切换新 Key（避免多 Chunk 同时出错时重复切换）
            with file_ctx.lock:
                if file_ctx.current_key == key:
                    file_ctx.current_key = key_pool.acquire()
                next_masked = _mask_key(file_ctx.current_key) if file_ctx.current_key else "None"

            backoff = (2 ** attempt) + random.uniform(0.5, 3.0)
            log.info("      [↻] %s 切至 Key %s，等待 %.1fs 重试...", tag, next_masked, backoff)
            time.sleep(backoff)

    log.error("      [x] 彻底失败: %s — 已达最大重试次数", tag)
    return None


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  8. 文件 / 目录处理
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def _process_file(
    filepath: Path,
    key_pool: ApiKeyPool,
    chunk_executor: ThreadPoolExecutor,
) -> str:
    """处理单个文件：读取 → 切分 → 并发翻译 → 回写。返回结果摘要。"""

    filename = filepath.name

    try:
        content = filepath.read_text(encoding="utf-8")
    except Exception as exc:
        return f"× 读取 {filename} 失败: {exc}"

    if not content.strip():
        return f"  {filename} 跳过（文件为空）"

    initial_key = key_pool.acquire()
    if not initial_key:
        return f"× {filename} 放弃 — 无可用 API Key"

    file_ctx = _FileContext(initial_key)
    log.info("\n[开始] %s (首发 Key: %s)", filename, _mask_key(initial_key))

    chunks = split_markdown(content)
    total = len(chunks)
    log.info("       %s 切分为 %d 个块", filename, total)

    # 并发提交所有 Chunk（顺序与原文一致）
    futures = [
        chunk_executor.submit(
            _translate_chunk, chunk, filename, i + 1, total, file_ctx, key_pool,
        )
        for i, chunk in enumerate(chunks)
    ]

    # 按提交顺序收集结果（保持段落顺序）
    translated: list[str] = []
    for i, future in enumerate(futures):
        result = future.result()
        if result is None:
            return f"× {filename} (块 {i + 1}/{total} 失败，中止写入以保护原文件)"
        translated.append(result)

    try:
        filepath.write_text("\n\n".join(translated), encoding="utf-8")
        return f"====> √ {filename} (全部 {total} 块拼合写入)"
    except Exception as exc:
        return f"====> × 写入 {filename} 失败: {exc}"


def run(directory: str) -> None:
    """主入口：扫描目录 → 初始化资源 → 启动两级并发翻译流水线。"""

    dir_path = Path(directory)
    if not dir_path.is_dir():
        log.error("错误: 目录 '%s' 不存在。", directory)
        sys.exit(1)

    md_files = sorted(dir_path.rglob(CFG.file_glob))
    if not md_files:
        log.info("未找到任何匹配 '%s' 的文件。", CFG.file_glob)
        return

    key_pool = ApiKeyPool(_load_api_keys())

    file_workers = max(CFG.min_file_workers, key_pool.total)
    chunk_workers = file_workers * CFG.chunk_worker_multiplier

    _print_banner(len(md_files), key_pool.total, file_workers, chunk_workers)

    chunk_executor = ThreadPoolExecutor(max_workers=chunk_workers)
    success_count = 0
    t0 = time.time()

    with ThreadPoolExecutor(max_workers=file_workers) as file_executor:
        future_map = {
            file_executor.submit(_process_file, fp, key_pool, chunk_executor): fp
            for fp in md_files
        }
        for future in as_completed(future_map):
            msg = future.result()
            log.info(msg)
            if "√" in msg:
                success_count += 1

    chunk_executor.shutdown(wait=True)

    elapsed = time.time() - t0
    log.info("\n================ 翻译任务收官 ================")
    log.info("成功: %d / %d 个文件", success_count, len(md_files))
    log.info("耗时: %.2f 秒", elapsed)
    key_pool.print_summary()


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  9. 内部辅助函数
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def _load_api_keys() -> list[str]:
    """从环境变量解析 API Key 列表。"""
    raw = os.environ.get(CFG.env_var_name, "")
    keys = [k.strip() for k in raw.split(",") if k.strip()]
    if not keys:
        log.error("错误: 请设置环境变量 %s（多个 Key 用逗号分隔）", CFG.env_var_name)
        sys.exit(1)
    return keys


def _mask_key(key: Optional[str]) -> str:
    """API Key 脱敏：sk-abcd...wxyz"""
    if not key:
        return "None"
    return f"{key[:6]}...{key[-4:]}" if len(key) > 10 else key


def _print_banner(
    file_count: int, key_count: int, file_workers: int, chunk_workers: int,
) -> None:
    log.info("==================================================")
    log.info(" 批量 Markdown 翻译（多 Key 轮询 · 两级并发 · 自动容错）")
    log.info(" 目标文件  : %d 个", file_count)
    log.info(" API Keys  : %d 个", key_count)
    log.info(" 文件并发  : %d | Chunk 并发池: %d", file_workers, chunk_workers)
    log.info("==================================================\n")


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
#  入口
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("用法: python translate_md.py <目标目录路径>")
        sys.exit(1)
    run(sys.argv[1])