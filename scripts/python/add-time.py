import os
import re
import datetime

# ================= 配置区 =================
# 在这里填入你想要处理的文件夹路径 (支持绝对路径或相对路径)
# 注意：Windows 路径请使用双反斜杠 \\ 或前缀 r，例如 r"D:\Obsidian\MyVault"
TARGET_DIRECTORY = r"./" 
# ==========================================

def format_time(timestamp):
    """格式化为 Obsidian 标准支持的 ISO 8601 格式"""
    return datetime.datetime.fromtimestamp(timestamp).strftime('%Y-%m-%dT%H:%M:%S')

def process_markdown_file(filepath):
    # 1. 获取文件当前的各种系统时间
    stat = os.stat(filepath)
    atime = stat.st_atime # 访问时间
    mtime = stat.st_mtime # 修改时间
    
    # 获取创建时间 (兼容 Windows/macOS 和 Linux)
    try:
        ctime = stat.st_birthtime
    except AttributeError:
        ctime = stat.st_ctime
        
    created_str = format_time(ctime)
    updated_str = format_time(mtime)
    
    # 2. 读取文件内容
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    # 正则表达式匹配文件开头的 YAML Frontmatter
    frontmatter_match = re.match(r'^---\n(.*?)\n---\n', content, re.DOTALL)
    
    is_modified = False
    new_content = ""

    # 3. 解析并添加时间到元数据
    if frontmatter_match:
        # 文件已经有 YAML 头
        yaml_content = frontmatter_match.group(1)
        original_yaml = yaml_content
        
        # 提取正文并去除开头多余的换行符
        body_content = content[frontmatter_match.end():].lstrip('\n')
        
        # 如果不存在 created 属性，则添加
        if not re.search(r'^created:', yaml_content, re.MULTILINE):
            yaml_content += f"\ncreated: {created_str}"
            
        # 如果不存在 updated 属性，则添加
        if not re.search(r'^updated:', yaml_content, re.MULTILINE):
            yaml_content += f"\nupdated: {updated_str}"
            
        # 如果内容有变化才准备重写
        if yaml_content != original_yaml:
            yaml_content = yaml_content.strip() # 清理首尾多余换行
            # 组装新内容：YAML区 + 1个空行 + 正文
            new_content = f"---\n{yaml_content}\n---\n\n{body_content}"
            is_modified = True
            
    else:
        # 文件没有 YAML 头
        # 提取正文并去除开头多余的换行符
        body_content = content.lstrip('\n')
        
        # 直接在最前面加上完整的 YAML 和 1个空行
        new_content = f"---\ncreated: {created_str}\nupdated: {updated_str}\n---\n\n{body_content}"
        is_modified = True
        
    # 4. 如果内容被修改了，则写入文件并【恢复时间】
    if is_modified:
        # 写入新内容 (这会导致文件的 mtime 变成当前时间)
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
            
        # ⚠️ 关键步骤：恢复文件的原始系统时间 (访问时间和修改时间)
        os.utime(filepath, (atime, mtime))
        
        return True
        
    return False

def main():
    if not os.path.exists(TARGET_DIRECTORY):
        print(f"❌ 目录不存在: {TARGET_DIRECTORY}")
        return

    print(f"🚀 开始扫描目录: {TARGET_DIRECTORY}")
    processed_count = 0
    skipped_count = 0

    # 递归遍历目录
    for root, dirs, files in os.walk(TARGET_DIRECTORY):
        for filename in files:
            if filename.lower().endswith('.md'):
                filepath = os.path.join(root, filename)
                try:
                    is_modified = process_markdown_file(filepath)
                    if is_modified:
                        print(f"✅ 已更新(保留原时间并规范空行): {filename}")
                        processed_count += 1
                    else:
                        skipped_count += 1
                except Exception as e:
                    print(f"❌ 处理失败: {filepath} | 错误: {e}")

    print("\n" + "="*45)
    print(f"🎉 处理完成！")
    print(f"✅ 成功写入属性并恢复系统时间的有 {processed_count} 个文件")
    print(f"⏭️ 跳过了 {skipped_count} 个文件 (已包含完整日期)")
    print("="*45)

if __name__ == "__main__":
    # ⚠️ 强烈建议在运行前备份你的 Obsidian 仓库 ⚠️
    response = input("⚠️ 准备批量修改 Markdown 文件。建议您先备份您的笔记仓库。\n确认继续吗？(y/n): ")
    if response.lower() == 'y':
        main()
    else:
        print("已取消操作。")