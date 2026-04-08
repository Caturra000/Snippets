import os
import re
import shutil
import sys
from collections import defaultdict

def normalize_title(title):
    """
    清理标题，去除各种形式的修订版后缀，以便将相同的提案归为一组。
    """
    title = title.lower()
    title = re.sub(r'\.txt$', '', title)
    
    patterns = [
        r'\s*[\(\,\-\—]\s*rev(?:ised|ision)?\.?\s*[r\d]*\s*\)?$', 
        r'\s*[\(\,\-\—]\s*v(?:ersion)?\.?\s*\d+\s*\)?$',
        r'\s*revision\s*\d+$',
        r'\s*v\.?\d+$'
    ]
    
    while True:
        old_title = title
        for pat in patterns:
            title = re.sub(pat, '', title).strip()
        if old_title == title:
            break
            
    title = re.sub(r'\s+', ' ', title)
    return title

def main():
    # 必须通过命令行参数指定目标目录
    if len(sys.argv) < 2:
        print("错误：请指定要处理的目标目录。")
        print("用法：python cleanup.py <目标目录路径>")
        sys.exit(1)
    
    target_dir = sys.argv[1]
    target_dir = os.path.abspath(target_dir)
    
    if not os.path.isdir(target_dir):
        print(f"错误：目录 '{target_dir}' 不存在或不是有效目录。")
        sys.exit(1)
    
    backup_dir = os.path.join(target_dir, "_old_revisions_backup")
    file_pattern = re.compile(r'^N(\d{4})\s*-\s*(.+?)(?:\.txt)?$')
    proposals = defaultdict(list)
    
    print(f"正在处理目录：{target_dir}")
    
    for filename in os.listdir(target_dir):
        if not filename.endswith('.txt'):
            continue
            
        match = file_pattern.match(filename)
        if match:
            n_number = int(match.group(1))
            raw_title = match.group(2)
            clean_title = normalize_title(raw_title)
            proposals[clean_title].append((n_number, filename))
    
    if not proposals:
        print("没有找到符合 Nxxxx 格式的 txt 文件。")
        return

    os.makedirs(backup_dir, exist_ok=True)
    
    moved_count = 0
    kept_count = 0

    for clean_title, files in proposals.items():
        if len(files) == 1:
            kept_count += 1
            continue
            
        files.sort(key=lambda x: x[0], reverse=True)
        latest_file = files[0][1]
        old_files = files[1:]
        
        print(f"\n[发现重复提案] 归一化标题: '{clean_title}'")
        print(f"  ✅ 保留最新: {latest_file}")
        kept_count += 1
        
        for old_n, old_filename in old_files:
            print(f"  📦 移至备份: {old_filename}")
            src_path = os.path.join(target_dir, old_filename)
            dst_path = os.path.join(backup_dir, old_filename)
            try:
                shutil.move(src_path, dst_path)
                moved_count += 1
            except Exception as e:
                print(f"     移动失败 {old_filename}: {e}")

    print("\n" + "="*50)
    print("处理完成！")
    print(f"目标目录：{target_dir}")
    print(f"总计保留了 {kept_count} 个最新版提案。")
    print(f"共清理（移动）了 {moved_count} 个历史旧版本到 '{backup_dir}' 文件夹。")
    print("请检查备份文件夹无误后，可以安全删除该文件夹。")

if __name__ == "__main__":
    main()