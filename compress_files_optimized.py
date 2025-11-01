import gzip
import os
import shutil
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

def main():
    source_dir = Path('source_data')
    output_dir = Path('data_littlefs')
    
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
    
    # 2. 复制代理程序文件（不压缩）
    print("\n2. Copying agent programs...")
    agent_source = source_dir / 'agent'
    agent_output = output_dir / 'agent'
    agent_output.mkdir(exist_ok=True)
    
    if agent_source.exists():
        for agent_file in agent_source.glob('*'):
            if agent_file.is_file():
                output_path = agent_output / agent_file.name
                size = copy_file(str(agent_file), str(output_path))
                total_compressed += size
    else:
        print(f'  Warning: {agent_source} directory not found')
        print(f'  Agent programs will not be available')
    
    print("=" * 60)
    print(f'\nTotal size: {total_compressed:,} bytes ({total_compressed/1024/1024:.2f} MB)')
    print(f'Output directory: {output_dir}')
    print('\nLittleFS data is ready for deployment.')
    print('Run: python deploy_all.py to deploy')

if __name__ == '__main__':
    main()