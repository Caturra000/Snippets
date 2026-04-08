#!/usr/bin/env python3
"""
删除指定目录下包含关键字的文件中，除字典序最大者之外的所有文件。
用法: python cleanup_manually.py --dir <目录> --keyword <关键字>
"""

import os
import sys
import argparse

def get_matching_files(directory, keyword):
    """返回目录下（非递归）文件名包含关键字的文件完整路径列表"""
    try:
        entries = os.listdir(directory)
    except FileNotFoundError:
        print(f"错误：目录不存在 - {directory}")
        sys.exit(1)
    except PermissionError:
        print(f"错误：没有权限访问目录 - {directory}")
        sys.exit(1)

    files = []
    for entry in entries:
        full_path = os.path.join(directory, entry)
        # 只处理文件，不处理子目录（可根据需要修改）
        if os.path.isfile(full_path) and keyword in entry:
            files.append(full_path)
    return files

def main():
    parser = argparse.ArgumentParser(
        description="删除指定目录下包含关键字的文件中，除字典序最大者之外的所有文件。"
    )
    parser.add_argument("--dir", "-d", default=".",
                        help="要操作的目录（默认为当前目录）")
    parser.add_argument("--keyword", "-k", required=True,
                        help="用于匹配文件名的关键字（子串匹配）")
    args = parser.parse_args()

    directory = args.dir
    keyword = args.keyword

    # 获取匹配的文件列表（完整路径）
    matched_files = get_matching_files(directory, keyword)

    if len(matched_files) <= 1:
        print(f"匹配到 {len(matched_files)} 个文件，无需删除。")
        return

    # 按字典序排序（基于完整路径的文件名部分，也可直接按路径字符串排序）
    # 为了保证只比较文件名，我们使用 os.path.basename 排序
    matched_files.sort(key=lambda x: os.path.basename(x))

    # 字典序最大的文件（最后一个）
    max_file = matched_files[-1]
    to_delete = matched_files[:-1]

    print(f"保留文件: {max_file}")
    print("\n以下文件将被删除：")
    for f in to_delete:
        print(f"  {f}")

    # 请求用户确认
    confirm = input("\n确认删除以上文件吗？(y/N): ").strip().lower()
    if confirm in ('y', 'yes'):
        deleted_count = 0
        for f in to_delete:
            try:
                os.remove(f)
                print(f"已删除: {f}")
                deleted_count += 1
            except Exception as e:
                print(f"删除失败 {f}: {e}")
        print(f"\n操作完成，共删除 {deleted_count} 个文件。")
    else:
        print("操作已取消。")

if __name__ == "__main__":
    main()