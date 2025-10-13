import gzip
import os
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

def main():
    source_dir = Path('source_data')
    output_dir = Path('data_littlefs')
    
    # 确保输出目录存在
    output_dir.mkdir(exist_ok=True)
    
    # 要压缩的文件列表
    files_to_compress = [
        'index.html',
        'style.css',
        'script.js'
    ]
    
    print("Compressing web files for LittleFS...")
    print("=" * 60)
    
    total_compressed = 0
    
    for filename in files_to_compress:
        input_path = source_dir / filename
        output_path = output_dir / f'{filename}.gz'
        
        if input_path.exists():
            print(f'Compressing {filename}...')
            size = compress_file(str(input_path), str(output_path))
            total_compressed += size
        else:
            print(f'Warning: {filename} not found in source_data directory')
    
    print("=" * 60)
    print(f'\nTotal compressed size: {total_compressed:,} bytes')
    print(f'Output directory: {output_dir}')
    print('\nWeb files are ready for LittleFS deployment.')
    print('Run: python deploy_all.py to deploy')

if __name__ == '__main__':
    main()