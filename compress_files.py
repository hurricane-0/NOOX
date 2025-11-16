"""
NOOX 文件压缩脚本

功能：
1. 压缩前端文件（HTML/CSS/JS）为 .gz 格式，用于 LittleFS 文件系统
2. 使用 UPX 压缩主机代理程序（noox-host-agent.exe），减小文件大小
3. 将压缩后的文件输出到指定目录（默认：data/）

使用：
    python compress_files.py

要求：
    - UPX 工具（可选，用于压缩可执行文件）
      - 下载地址：https://upx.github.io/
      - 需要将 UPX 添加到系统 PATH
    - 如果 UPX 不可用，脚本会直接复制文件而不压缩

输出：
    - data/index.html.gz
    - data/style.css.gz
    - data/script.js.gz
    - data/agent/noox-host-agent.exe (UPX 压缩)
"""

import gzip
import os
import shutil
import subprocess
from pathlib import Path

def compress_file(input_path, output_path, compression_level=9):
    """使用最高压缩级别压缩文件"""
    with open(input_path, 'rb') as f_in:
        with gzip.open(output_path, 'wb', compresslevel=compression_level) as f_out:
            f_out.write(f_in.read())
    
    # 打印压缩比
    original_size = os.path.getsize(input_path)
    compressed_size = os.path.getsize(output_path)
    ratio = (1 - compressed_size / original_size) * 100
    print(f'{input_path}: {original_size:,} bytes -> {compressed_size:,} bytes ({ratio:.1f}% reduction)')
    return compressed_size

def copy_file(input_path, output_path):
    """直接复制文件（不压缩）"""
    shutil.copy2(input_path, output_path)
    size = os.path.getsize(input_path)
    print(f'Copying {input_path}: {size:,} bytes')
    return size

def compress_with_upx(input_path, output_path):
    """使用 UPX 压缩可执行文件"""
    original_size = os.path.getsize(input_path)
    
    # 检查 UPX 是否可用
    try:
        result = subprocess.run(['upx', '--version'], 
                              capture_output=True, 
                              text=True, 
                              timeout=5)
        if result.returncode != 0:
            print(f'  Warning: UPX not found or not working, copying file without compression')
            shutil.copy2(input_path, output_path)
            return original_size
    except (FileNotFoundError, subprocess.TimeoutExpired):
        print(f'  Warning: UPX not found in PATH, copying file without compression')
        print(f'  Install UPX from https://upx.github.io/ to enable compression')
        shutil.copy2(input_path, output_path)
        return original_size
    
    # 使用 UPX 压缩（--best 最高压缩率，--lzma 使用 LZMA 算法）
    try:
        # 先复制到临时位置，UPX 会直接修改原文件
        temp_path = str(output_path) + '.tmp'
        shutil.copy2(input_path, temp_path)
        
        # 运行 UPX 压缩
        result = subprocess.run(['upx', '--best', '--lzma', temp_path],
                              capture_output=True,
                              text=True,
                              timeout=60)
        
        if result.returncode == 0:
            # 压缩成功，重命名
            if os.path.exists(output_path):
                os.remove(output_path)
            os.rename(temp_path, output_path)
            compressed_size = os.path.getsize(output_path)
            ratio = (1 - compressed_size / original_size) * 100
            print(f'  UPX compressed: {original_size:,} bytes -> {compressed_size:,} bytes ({ratio:.1f}% reduction)')
            return compressed_size
        else:
            # 压缩失败，使用原文件
            print(f'  Warning: UPX compression failed: {result.stderr}')
            if os.path.exists(temp_path):
                os.remove(temp_path)
            shutil.copy2(input_path, output_path)
            return original_size
    except subprocess.TimeoutExpired:
        print(f'  Warning: UPX compression timed out, copying file without compression')
        if os.path.exists(temp_path):
            os.remove(temp_path)
        shutil.copy2(input_path, output_path)
        return original_size
    except Exception as e:
        print(f'  Warning: UPX compression error: {e}, copying file without compression')
        if os.path.exists(temp_path):
            os.remove(temp_path)
        shutil.copy2(input_path, output_path)
        return original_size

def main():
    source_dir = Path('source_data')
    output_dir = Path('data')
    
    # 确保输出目录存在
    output_dir.mkdir(exist_ok=True)
    
    # 要压缩的 Web 文件列表
    files_to_compress = [
        'index.html',
        'style.css',
        'script.js'
    ]
    
    print("Preparing LittleFS data...")
    print("=" * 60)
    
    # 1. 压缩 Web 文件
    print("\n1. Compressing web files...")
    total_compressed = 0
    
    for filename in files_to_compress:
        input_path = source_dir / filename
        output_path = output_dir / f'{filename}.gz'
        
        if input_path.exists():
            print(f'  Compressing {filename}...')
            size = compress_file(str(input_path), str(output_path))
            total_compressed += size
        else:
            print(f'  Warning: {filename} not found in source_data directory')
    
    # 2. 处理代理程序文件（使用 UPX 压缩）
    print("\n2. Processing agent programs...")
    agent_output = output_dir / 'agent'
    agent_output.mkdir(exist_ok=True)
    
    # 从 host-agent 目录查找编译好的 exe
    host_agent_exe = Path('host-agent') / 'noox-host-agent.exe'
    
    if host_agent_exe.exists():
        output_path = agent_output / 'noox-host-agent.exe'
        print(f'  Found agent executable: {host_agent_exe}')
        print(f'  Compressing with UPX...')
        size = compress_with_upx(str(host_agent_exe), str(output_path))
        total_compressed += size
        
    else:
        print(f'  Warning: noox-host-agent.exe not found in host-agent/ directory')
        print(f'  Agent programs will not be available')
    
    print("=" * 60)
    print(f'\nTotal size: {total_compressed:,} bytes ({total_compressed/1024/1024:.2f} MB)')
    print(f'Output directory: {output_dir}')
    print('\nLittleFS data is ready for deployment.')
    print('Run: python deploy_all.py to deploy')

if __name__ == '__main__':
    main()