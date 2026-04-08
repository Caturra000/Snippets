#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import sys
import shutil
import tempfile
import subprocess
from pathlib import Path


PROGRAM = "zxgrep"

RED = "\033[01;31m"
CYAN = "\033[01;36m"
RESET = "\033[0m"

# 精确匹配时的边界定义：
# 关键词前后都不能是 [A-Za-z0-9_]
LEFT_BOUNDARY = r"(?<![0-9A-Za-z_])"
RIGHT_BOUNDARY = r"(?![0-9A-Za-z_])"


BASH_COMPLETION_SCRIPT = r'''# bash completion for zxgrep
_zxgrep() {
    local cur prev i input_seen
    COMPREPLY=()

    cur="${COMP_WORDS[COMP_CWORD]}"
    if (( COMP_CWORD > 0 )); then
        prev="${COMP_WORDS[COMP_CWORD-1]}"
    else
        prev=""
    fi

    local opts="--help --install --print-bash-completion --file --case-sensitive --exact --regex --copy --move --list-files --name-only --color-path --no-color-path -h -s -x -r -l -N -o -O"

    if [[ "$prev" == "-o" ]]; then
        compopt -o filenames 2>/dev/null
        mapfile -t COMPREPLY < <(compgen -d -- "$cur")
        return 0
    fi

    if [[ "$cur" == -* ]]; then
        mapfile -t COMPREPLY < <(compgen -W "$opts" -- "$cur")
        return 0
    fi

    input_seen=0
    i=1
    while (( i < COMP_CWORD )); do
        case "${COMP_WORDS[i]}" in
            --help|-h|--install|--print-bash-completion|--file|--case-sensitive|-s|--exact|-x|--regex|-r|--copy|--move|--list-files|-l|--name-only|-N|--color-path|--no-color-path|-O)
                ;;
            -o)
                ((i++))
                ;;
            --)
                input_seen=1
                break
                ;;
            -*)
                ;;
            *)
                input_seen=1
                break
                ;;
        esac
        ((i++))
    done

    if (( input_seen == 0 )); then
        compopt -o filenames 2>/dev/null
        mapfile -t COMPREPLY < <(compgen -f -- "$cur")
        return 0
    fi

    COMPREPLY=()
    return 0
}

complete -F _zxgrep zxgrep
'''


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


def die(msg, code=2):
    eprint(f"{PROGRAM}: {msg}")
    raise SystemExit(code)


def usage():
    print(f"""用法:
  {PROGRAM} INPUT WORD1 [WORD2 ...]
  {PROGRAM} INPUT WORD1 [WORD2 ...] --file
  {PROGRAM} INPUT WORD1 [WORD2 ...] --file -o OUTDIR
  {PROGRAM} INPUT WORD1 [WORD2 ...] -O
  {PROGRAM} INPUT WORD1 [WORD2 ...] -x
  {PROGRAM} INPUT WORD1 [WORD2 ...] -r
  {PROGRAM} INPUT WORD1 [WORD2 ...] -s
  {PROGRAM} INPUT WORD1 [WORD2 ...] -l
  {PROGRAM} INPUT WORD1 [WORD2 ...] -N
  {PROGRAM} --install
  {PROGRAM} --print-bash-completion

INPUT 可自动识别为：
  1) *.tar.zst 归档
  2) 一个目录（递归处理其中文本文件）
  3) 一个单独文本文件

说明:
  1) 默认模式：
     按“行”搜索。
     要求同一行同时包含全部关键词。
     如果 INPUT 是 tar.zst，会先解压到 /dev/shm（若不可用则回退 /tmp）。

  2) --file 模式：
     按“文件”搜索。
     要求同一文件包含全部关键词，不要求同一行。
     默认输出这些文件中包含任一关键词/表达式的行，并高亮。

  3) 默认匹配方式：
     非精确匹配 + 不区分大小写
     即普通子串匹配。
     例如关键字 exec：
       可匹配: exec, execution, EXEC, my_exec_call

  4) -x / --exact：
     开启精确匹配。
     精确匹配定义为：
       关键词前后都不能是英文字母 / 数字 / 下划线
     例如关键字 exec：
       匹配: " exec ", "(exec)", "exec;"
       不匹配: "execution", "my_exec_var", "exec123"

  5) -r / --regex：
     开启正则匹配。
     此时每个 WORD 都按正则表达式处理。
     仍然支持多关键字。
     当前是逐行匹配，不支持跨行多行正则。

  6) -s / --case-sensitive：
     开启区分大小写。

  7) -l / --list-files：
     只列出匹配文件名，不输出匹配行。
     - 默认模式下：列出“至少有一行同时匹配全部关键词”的文件
     - --file 模式下：列出“文件内包含全部关键词”的文件

  8) -N / --name-only：
     只在“文件名本身”上搜索，不看文件内容。
     这里的“文件名”指 basename，不包含父目录。
     该模式会自动按文件级处理，并只输出匹配文件路径。
     也支持 -o/-O、--copy、--move。
     例如：
       {PROGRAM} ./docs report -N
       {PROGRAM} ./docs 'report.*2024' -N -r

  9) 默认输出带行号和列号：
     形如:
       path/to/file.txt:12:8: matched line

  10) 路径着色：
      默认对路径着色。
      为避免影响 VSCode 对 path:line:col 的定位识别，可以尝试不对路径着色。
      关闭可用：
        --no-color-path
      显式开启（默认模式）：
        --color-path

  11) -o / -O：
      把匹配到的文件输出到目标目录（不改变匹配模式）。
      默认行为是“复制”。
      如需改成移动，请加：
        --move

      相关选项：
        --copy   明确指定复制（默认）
        --move   改为移动

      为避免重名冲突，会保留相对目录结构。
      - 对 tar.zst：保留归档内路径
      - 对目录：保留相对于输入目录的路径
      - 对单文件：输出为目标目录下的同名文件

  12) 文本文件判定：
      对目录/单文件输入，会跳过明显的二进制文件（基于 NUL 字节的简单判断）。

  13) --install：
      安装到 /usr/local/bin/zxgrep
      并安装 bash completion。

示例:
  {PROGRAM} archive.tar.zst exec task
  {PROGRAM} ./docs exec task
  {PROGRAM} ./docs/a.txt exec task
  {PROGRAM} archive.tar.zst exec task --file
  {PROGRAM} ./docs exec task -O
  {PROGRAM} ./docs exec task --exact
  {PROGRAM} ./docs 'exec(ute)?' --regex
  {PROGRAM} ./docs 'exec.*task' --regex -s
  {PROGRAM} ./docs exec task -l
  {PROGRAM} ./docs exec task -O --move
  {PROGRAM} ./docs report -N
  {PROGRAM} ./docs 'report.*2024' -N -r
""")


def require_cmd(*cmds):
    for cmd in cmds:
        if shutil.which(cmd) is None:
            die(f"缺少依赖命令: {cmd}")


def pick_tmp_root():
    shm = Path("/dev/shm")
    if shm.exists() and os.access(str(shm), os.W_OK):
        return shm
    return Path(os.environ.get("TMPDIR", "/tmp"))


def abs_path(p):
    return Path(os.path.abspath(os.path.expanduser(str(p))))


def sanitize_component(s):
    return re.sub(r"[^A-Za-z0-9._+-]", "_", s)


def auto_outdir_name(words):
    return "zxgrep_" + "+".join(sanitize_component(w) for w in words)


def path_is_within(path, parent):
    try:
        return os.path.commonpath([os.path.abspath(path), os.path.abspath(parent)]) == os.path.abspath(parent)
    except ValueError:
        return False


def is_probably_text_file(path):
    try:
        with open(path, "rb") as f:
            chunk = f.read(8192)
        return b"\x00" not in chunk
    except Exception:
        return False


def pretty_local_path(path):
    path = abs_path(path)
    cwd = abs_path(".")
    try:
        rel = path.relative_to(cwd)
        rel_s = rel.as_posix()
        if rel_s == ".":
            return "."
        return "./" + rel_s
    except ValueError:
        return path.as_posix()


def maybe_color_path_text(text, color_path):
    if color_path and sys.stdout.isatty():
        return f"{CYAN}{text}{RESET}"
    return text


def make_location_label(display_path, lineno, colno, color_path=False):
    text = f"{display_path}:{lineno}:{colno}"
    return maybe_color_path_text(text, color_path)


def print_path_line(display_path, color_path=False):
    print(maybe_color_path_text(display_path, color_path))


def detect_input_kind(input_path):
    p = abs_path(input_path)
    if not p.exists():
        die(f"输入不存在: {p}")
    if p.is_dir():
        return {"kind": "dir", "path": p}
    if p.is_file():
        if p.name.endswith(".tar.zst"):
            return {"kind": "archive", "path": p}
        return {"kind": "file", "path": p}
    die(f"不支持的输入类型: {p}")


def extract_archive(archive, dest):
    require_cmd("tar", "zstd")
    subprocess.run(
        ["tar", "-I", "zstd -T0", "-xf", str(archive), "-C", str(dest)],
        check=True
    )


def iter_dir_files(root, exclude_dir=None):
    root = abs_path(root)
    exclude_abs = abs_path(exclude_dir) if exclude_dir is not None else None

    for current, dirnames, filenames in os.walk(str(root), followlinks=False):
        if exclude_abs is not None:
            new_dirnames = []
            for d in dirnames:
                full = os.path.join(current, d)
                if path_is_within(full, str(exclude_abs)):
                    continue
                new_dirnames.append(d)
            dirnames[:] = sorted(new_dirnames)
        else:
            dirnames[:] = sorted(dirnames)

        filenames.sort()

        for name in filenames:
            p = Path(current) / name
            if exclude_abs is not None and path_is_within(str(p), str(exclude_abs)):
                continue
            if p.is_symlink():
                continue
            if not p.is_file():
                continue
            if not is_probably_text_file(p):
                continue
            rel = p.relative_to(root).as_posix()
            yield {
                "rel": rel,
                "path": p,
                "display_path": pretty_local_path(p),
            }


def iter_archive_files(extracted_root):
    extracted_root = abs_path(extracted_root)
    for current, dirnames, filenames in os.walk(str(extracted_root), followlinks=False):
        dirnames[:] = sorted(dirnames)
        filenames.sort()

        for name in filenames:
            p = Path(current) / name
            if p.is_symlink():
                continue
            if not p.is_file():
                continue
            if not is_probably_text_file(p):
                continue
            rel = p.relative_to(extracted_root).as_posix()
            yield {
                "rel": rel,
                "path": p,
                "display_path": rel,
            }


def iter_single_file(file_path):
    p = abs_path(file_path)
    if p.is_file() and is_probably_text_file(p):
        yield {
            "rel": p.name,
            "path": p,
            "display_path": pretty_local_path(p),
        }


def compile_pattern(word, mode, case_sensitive):
    flags = 0 if case_sensitive else re.IGNORECASE

    if mode == "substr":
        expr = re.escape(word)
    elif mode == "exact":
        expr = LEFT_BOUNDARY + re.escape(word) + RIGHT_BOUNDARY
    elif mode == "regex":
        expr = word
    else:
        die(f"内部错误：未知匹配模式 {mode}")

    try:
        pat = re.compile(expr, flags)
    except re.error as ex:
        die(f"正则编译失败: {word!r}: {ex}")

    if mode == "regex":
        m = pat.search("")
        if m is not None and m.start() == m.end():
            die(f"正则不允许匹配空串: {word!r}")

    return pat


def build_patterns(words, mode, case_sensitive):
    return [compile_pattern(w, mode, case_sensitive) for w in words]


def build_highlight_pattern(words, mode, case_sensitive):
    flags = 0 if case_sensitive else re.IGNORECASE

    if mode == "substr":
        uniq = list(dict.fromkeys(words))
        uniq.sort(key=len, reverse=True)
        expr = "|".join(re.escape(w) for w in uniq)
    elif mode == "exact":
        uniq = list(dict.fromkeys(words))
        uniq.sort(key=len, reverse=True)
        body = "|".join(re.escape(w) for w in uniq)
        expr = LEFT_BOUNDARY + f"(?:{body})" + RIGHT_BOUNDARY
    elif mode == "regex":
        parts = [f"(?:{w})" for w in words]
        expr = "|".join(parts)
    else:
        die(f"内部错误：未知匹配模式 {mode}")

    try:
        pat = re.compile(expr, flags)
    except re.error as ex:
        die(f"高亮正则编译失败: {ex}")

    return pat


def colorize_line(line, any_pattern):
    text = line.rstrip("\r\n")
    return any_pattern.sub(lambda m: f"{RED}{m.group(0)}{RESET}", text)


def first_match_column(line, any_pattern):
    m = any_pattern.search(line)
    if m is None:
        return 1
    return m.start() + 1


def emit_line(display_path, lineno, colno, line, any_pattern, color_path=False):
    prefix = make_location_label(display_path, lineno, colno, color_path=color_path)
    sys.stdout.write(f"{prefix}: {colorize_line(line, any_pattern)}\n")


def file_contains_all_patterns(path, patterns):
    remaining = set(range(len(patterns)))
    if not remaining:
        return True

    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        for line in f:
            if not remaining:
                return True
            for idx in tuple(remaining):
                if patterns[idx].search(line):
                    remaining.remove(idx)
                    if not remaining:
                        return True

    return not remaining


def file_has_line_matching_all_patterns(path, patterns):
    with open(path, "r", encoding="utf-8", errors="replace", newline="") as f:
        for line in f:
            ok = True
            for pat in patterns:
                if pat.search(line) is None:
                    ok = False
                    break
            if ok:
                return True
    return False


def basename_matches_all_patterns(rel_path, patterns):
    name = Path(rel_path).name
    for pat in patterns:
        if pat.search(name) is None:
            return False
    return True


def print_lines_matching_all_patterns(item, all_patterns, any_pattern, color_path=False):
    printed = False
    with open(item["path"], "r", encoding="utf-8", errors="replace", newline="") as f:
        for lineno, line in enumerate(f, start=1):
            ok = True
            for pat in all_patterns:
                if pat.search(line) is None:
                    ok = False
                    break
            if ok:
                printed = True
                colno = first_match_column(line, any_pattern)
                emit_line(item["display_path"], lineno, colno, line, any_pattern, color_path=color_path)
    return printed


def print_lines_matching_any_pattern(item, any_pattern, color_path=False):
    printed = False
    with open(item["path"], "r", encoding="utf-8", errors="replace", newline="") as f:
        for lineno, line in enumerate(f, start=1):
            if any_pattern.search(line):
                printed = True
                colno = first_match_column(line, any_pattern)
                emit_line(item["display_path"], lineno, colno, line, any_pattern, color_path=color_path)
    return printed


def remove_path_if_exists(path):
    path = abs_path(path)
    if path.exists() or path.is_symlink():
        if path.is_dir() and not path.is_symlink():
            shutil.rmtree(path)
        else:
            path.unlink()


def safe_move(src, dst):
    src = abs_path(src)
    dst = abs_path(dst)

    if os.path.abspath(src) == os.path.abspath(dst):
        return

    dst.parent.mkdir(parents=True, exist_ok=True)
    remove_path_if_exists(dst)
    shutil.move(str(src), str(dst))


def safe_copy(src, dst):
    src = abs_path(src)
    dst = abs_path(dst)

    if os.path.abspath(src) == os.path.abspath(dst):
        return

    dst.parent.mkdir(parents=True, exist_ok=True)
    remove_path_if_exists(dst)
    shutil.copy2(str(src), str(dst))


def safe_transfer(src, dst, transfer_mode):
    if transfer_mode == "copy":
        safe_copy(src, dst)
    elif transfer_mode == "move":
        safe_move(src, dst)
    else:
        die(f"内部错误：未知输出模式 {transfer_mode}")


def build_source_items(source_info, extracted_root=None, exclude_dir=None):
    kind = source_info["kind"]

    if kind == "archive":
        if extracted_root is None:
            die("内部错误：archive 模式缺少 extracted_root")
        return iter_archive_files(extracted_root)

    if kind == "dir":
        return iter_dir_files(source_info["path"], exclude_dir=exclude_dir)

    if kind == "file":
        return iter_single_file(source_info["path"])

    die(f"内部错误：未知输入类型 {kind}")


def run_line_mode(items, all_patterns, any_pattern, outdir=None, transfer_mode="copy", list_files=False, color_path=False):
    matched_items = []

    for item in items:
        if file_has_line_matching_all_patterns(item["path"], all_patterns):
            matched_items.append(item)

    if not matched_items:
        return False

    final_items = matched_items

    if outdir is not None:
        outdir = abs_path(outdir)
        outdir.mkdir(parents=True, exist_ok=True)

        transferred_items = []
        for item in matched_items:
            target = outdir / item["rel"]
            safe_transfer(item["path"], target, transfer_mode)
            transferred_items.append({
                "rel": item["rel"],
                "path": target,
                "display_path": pretty_local_path(target),
            })

        final_items = transferred_items
        action_text = "复制" if transfer_mode == "copy" else "移动"
        eprint(f"已将匹配文件{action_text}到: {pretty_local_path(outdir)}")

    for item in final_items:
        if list_files:
            print_path_line(item["display_path"], color_path=color_path)
        else:
            print_lines_matching_all_patterns(item, all_patterns, any_pattern, color_path=color_path)

    return True


def run_file_mode(items, all_patterns, any_pattern, outdir=None, transfer_mode="copy", list_files=False, color_path=False):
    matched_items = []
    for item in items:
        if file_contains_all_patterns(item["path"], all_patterns):
            matched_items.append(item)

    if not matched_items:
        return False

    final_items = matched_items

    if outdir is not None:
        outdir = abs_path(outdir)
        outdir.mkdir(parents=True, exist_ok=True)

        transferred_items = []
        for item in matched_items:
            target = outdir / item["rel"]
            safe_transfer(item["path"], target, transfer_mode)
            transferred_items.append({
                "rel": item["rel"],
                "path": target,
                "display_path": pretty_local_path(target),
            })

        final_items = transferred_items

        action_text = "复制" if transfer_mode == "copy" else "移动"
        eprint(f"已将匹配文件{action_text}到: {pretty_local_path(outdir)}")

    if list_files:
        for item in final_items:
            print_path_line(item["display_path"], color_path=color_path)
        return True

    for item in final_items:
        print_lines_matching_any_pattern(item, any_pattern, color_path=color_path)

    return True


def run_name_only_mode(items, all_patterns, outdir=None, transfer_mode="copy", color_path=False):
    matched_items = []
    for item in items:
        if basename_matches_all_patterns(item["rel"], all_patterns):
            matched_items.append(item)

    if not matched_items:
        return False

    final_items = matched_items

    if outdir is not None:
        outdir = abs_path(outdir)
        outdir.mkdir(parents=True, exist_ok=True)

        transferred_items = []
        for item in matched_items:
            target = outdir / item["rel"]
            safe_transfer(item["path"], target, transfer_mode)
            transferred_items.append({
                "rel": item["rel"],
                "path": target,
                "display_path": pretty_local_path(target),
            })

        final_items = transferred_items

        action_text = "复制" if transfer_mode == "copy" else "移动"
        eprint(f"已将匹配文件{action_text}到: {pretty_local_path(outdir)}")

    for item in final_items:
        print_path_line(item["display_path"], color_path=color_path)

    return True


def choose_completion_target():
    candidates = [
        Path("/usr/share/bash-completion/completions"),
        Path("/usr/local/share/bash-completion/completions"),
        Path("/etc/bash_completion.d"),
    ]
    for d in candidates:
        if d.exists() and d.is_dir():
            return d / PROGRAM
    return Path("/usr/local/share/bash-completion/completions") / PROGRAM


def install_file(src, dst, mode, use_sudo):
    require_cmd("install")
    if use_sudo:
        require_cmd("sudo")
        subprocess.run(["sudo", "mkdir", "-p", str(dst.parent)], check=True)
        subprocess.run(["sudo", "install", "-m", f"{mode:o}", str(src), str(dst)], check=True)
    else:
        dst.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["install", "-m", f"{mode:o}", str(src), str(dst)], check=True)


def install_self():
    self_path = Path(__file__).resolve()
    bin_target = Path("/usr/local/bin") / PROGRAM
    comp_target = choose_completion_target()
    use_sudo = (os.geteuid() != 0)

    tmp_completion = None
    try:
        fd, tmp_name = tempfile.mkstemp(prefix="zxgrep_completion_", text=True)
        os.close(fd)
        tmp_completion = Path(tmp_name)
        tmp_completion.write_text(BASH_COMPLETION_SCRIPT, encoding="utf-8")

        install_file(self_path, bin_target, 0o755, use_sudo)
        install_file(tmp_completion, comp_target, 0o644, use_sudo)

    finally:
        if tmp_completion and tmp_completion.exists():
            try:
                tmp_completion.unlink()
            except Exception:
                pass

    print(f"已安装主程序到: {bin_target}")
    print(f"已安装 Bash 补全到: {comp_target}")
    print("如果当前 shell 还不能补全，重新打开一个 Bash，或执行：")
    print(f"  source {comp_target}")


def print_bash_completion():
    print(BASH_COMPLETION_SCRIPT, end="")


def parse_args(argv):
    if not argv:
        usage()
        raise SystemExit(1)

    if len(argv) == 1 and argv[0] in ("-h", "--help"):
        usage()
        raise SystemExit(0)

    if len(argv) == 1 and argv[0] == "--install":
        return {"action": "install"}

    if len(argv) == 1 and argv[0] == "--print-bash-completion":
        return {"action": "print-completion"}

    input_path = None
    words = []
    file_mode = False
    outdir = None
    auto_out = False
    case_sensitive = False
    list_files = False
    name_only = False
    color_path = True
    mode = "substr"           # substr / exact / regex
    transfer_mode = "copy"    # copy / move
    transfer_mode_set = False

    stop_opts = False
    i = 0

    while i < len(argv):
        arg = argv[i]

        if not stop_opts and arg == "--":
            stop_opts = True
            i += 1
            continue

        if not stop_opts:
            if arg in ("-h", "--help"):
                usage()
                raise SystemExit(0)
            elif arg == "--install":
                die("--install 不能和搜索参数一起使用")
            elif arg == "--print-bash-completion":
                die("--print-bash-completion 不能和搜索参数一起使用")
            elif arg == "--file":
                file_mode = True
                i += 1
                continue
            elif arg in ("-s", "--case-sensitive"):
                case_sensitive = True
                i += 1
                continue
            elif arg in ("-x", "--exact"):
                if mode == "regex":
                    die("--exact 与 --regex 不能同时使用")
                mode = "exact"
                i += 1
                continue
            elif arg in ("-r", "--regex"):
                if mode == "exact":
                    die("--exact 与 --regex 不能同时使用")
                mode = "regex"
                i += 1
                continue
            elif arg in ("-l", "--list-files"):
                list_files = True
                i += 1
                continue
            elif arg in ("-N", "--name-only"):
                name_only = True
                file_mode = True
                list_files = True
                i += 1
                continue
            elif arg == "--color-path":
                color_path = True
                i += 1
                continue
            elif arg == "--no-color-path":
                color_path = False
                i += 1
                continue
            elif arg == "--copy":
                if transfer_mode_set and transfer_mode != "copy":
                    die("--copy 与 --move 不能同时使用")
                transfer_mode = "copy"
                transfer_mode_set = True
                i += 1
                continue
            elif arg == "--move":
                if transfer_mode_set and transfer_mode != "move":
                    die("--copy 与 --move 不能同时使用")
                transfer_mode = "move"
                transfer_mode_set = True
                i += 1
                continue
            elif arg == "-o":
                i += 1
                if i >= len(argv):
                    die("-o 需要指定输出目录")
                if auto_out:
                    die("-o 与 -O 不能同时使用")
                outdir = abs_path(argv[i])
                i += 1
                continue
            elif arg == "-O":
                if outdir is not None:
                    die("-o 与 -O 不能同时使用")
                auto_out = True
                i += 1
                continue
            elif arg.startswith("-"):
                die(f"不支持的选项: {arg}")

        if input_path is None:
            input_path = arg
        else:
            words.append(arg)
        i += 1

    if input_path is None:
        die("缺少 INPUT")
    if not words:
        die("至少需要一个关键词/表达式")
    if any(w == "" for w in words):
        die("关键词/表达式不能为空")

    if auto_out:
        outdir = abs_path(auto_outdir_name(words))

    if transfer_mode_set and outdir is None:
        die("--copy/--move 只能和 -o 或 -O 一起使用")

    return {
        "action": "search",
        "input_path": abs_path(input_path),
        "words": words,
        "file_mode": file_mode,
        "outdir": outdir,
        "case_sensitive": case_sensitive,
        "mode": mode,
        "list_files": list_files,
        "name_only": name_only,
        "transfer_mode": transfer_mode,
        "color_path": color_path,
    }


def main(argv):
    args = parse_args(argv)

    if args["action"] == "install":
        install_self()
        return 0

    if args["action"] == "print-completion":
        print_bash_completion()
        return 0

    input_path = args["input_path"]
    words = args["words"]
    file_mode = args["file_mode"]
    outdir = args["outdir"]
    case_sensitive = args["case_sensitive"]
    mode = args["mode"]
    list_files = args["list_files"]
    name_only = args["name_only"]
    transfer_mode = args["transfer_mode"]
    color_path = args["color_path"]

    source_info = detect_input_kind(input_path)

    all_patterns = build_patterns(words, mode=mode, case_sensitive=case_sensitive)
    any_pattern = build_highlight_pattern(words, mode=mode, case_sensitive=case_sensitive)

    temp_root = None
    extracted_root = None

    try:
        if source_info["kind"] == "archive":
            temp_root = Path(tempfile.mkdtemp(prefix="zxgrep.", dir=str(pick_tmp_root())))
            extract_archive(source_info["path"], temp_root)
            extracted_root = temp_root

        exclude_dir = None
        if source_info["kind"] == "dir" and outdir is not None:
            if path_is_within(str(outdir), str(source_info["path"])):
                exclude_dir = outdir

        items = build_source_items(
            source_info,
            extracted_root=extracted_root,
            exclude_dir=exclude_dir,
        )

        if name_only:
            found = run_name_only_mode(
                items,
                all_patterns=all_patterns,
                outdir=outdir,
                transfer_mode=transfer_mode,
                color_path=color_path,
            )
        elif file_mode:
            found = run_file_mode(
                items,
                all_patterns=all_patterns,
                any_pattern=any_pattern,
                outdir=outdir,
                transfer_mode=transfer_mode,
                list_files=list_files,
                color_path=color_path,
            )
        else:
            found = run_line_mode(
                items,
                all_patterns=all_patterns,
                any_pattern=any_pattern,
                outdir=outdir,
                transfer_mode=transfer_mode,
                list_files=list_files,
                color_path=color_path,
            )

    finally:
        if temp_root is not None:
            shutil.rmtree(temp_root, ignore_errors=True)

    return 0 if found else 1


if __name__ == "__main__":
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        raise SystemExit(130)
    except subprocess.CalledProcessError as e:
        die(f"外部命令执行失败，退出码={e.returncode}")