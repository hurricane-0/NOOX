import gzip
import os
import shutil

source_dir = 'source_data'
data_dir = 'data'

# 如果data目录不存在，则创建它
os.makedirs(data_dir, exist_ok=True)

for filename in os.listdir(source_dir):
    source_path = os.path.join(source_dir, filename)
    if os.path.isfile(source_path):
        # 定义gzip压缩文件的输出路径
        output_filename = filename + '.gz'
        output_path = os.path.join(data_dir, output_filename)

        with open(source_path, 'rb') as f_in:
            with gzip.open(output_path, 'wb') as f_out:
                shutil.copyfileobj(f_in, f_out)
        print(f"Compressed '{source_path}' to '{output_path}'")

print("Compression complete.")
