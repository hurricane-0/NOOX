#!/usr/bin/env python3
"""
NOOX 设备完整部署脚本
自动化完成固件编译、上传、以及双文件系统部署
"""

import subprocess
import sys
import shutil
from pathlib import Path
import time

try:
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    print("Warning: pyserial not installed, will use default COM3")

class Colors:
    """终端颜色代码"""
    HEADER = '\033[95m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    GREEN = '\033[92m'
    YELLOW = '\033[93m'
    RED = '\033[91m'
    END = '\033[0m'
    BOLD = '\033[1m'

def print_step(step_num, total_steps, message):
    """打印步骤信息"""
    print(f"\n{Colors.BOLD}{Colors.CYAN}[{step_num}/{total_steps}] {message}{Colors.END}")
    print("=" * 60)

def print_success(message):
    """打印成功信息"""
    print(f"{Colors.GREEN}✓ {message}{Colors.END}")

def print_error(message):
    """打印错误信息"""
    print(f"{Colors.RED}✗ {message}{Colors.END}")

def print_warning(message):
    """打印警告信息"""
    print(f"{Colors.YELLOW}⚠ {message}{Colors.END}")

def run_command(cmd, description, check=True):
    """运行命令并处理输出"""
    print(f"\n{Colors.BLUE}Running: {description}{Colors.END}")
    print(f"Command: {cmd}")
    
    try:
        result = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            encoding='utf-8',
            errors='replace'
        )
        
        if result.stdout:
            print(result.stdout)
        
        if result.returncode != 0:
            if result.stderr:
                print(f"{Colors.RED}Error output:{Colors.END}")
                print(result.stderr)
            
            if check:
                raise subprocess.CalledProcessError(result.returncode, cmd)
            else:
                print_warning(f"Command failed with exit code {result.returncode}, continuing...")
        
        return result.returncode == 0
    
    except subprocess.CalledProcessError as e:
        print_error(f"Command failed with exit code {e.returncode}")
        return False
    except Exception as e:
        print_error(f"Unexpected error: {e}")
        return False

def backup_file(filepath):
    """备份文件"""
    backup_path = Path(str(filepath) + '.backup')
    if Path(filepath).exists():
        shutil.copy2(filepath, backup_path)
        return backup_path
    return None

def restore_file(filepath, backup_path):
    """恢复文件"""
    if backup_path and backup_path.exists():
        shutil.copy2(backup_path, filepath)
        backup_path.unlink()

def get_upload_port():
    """自动检测上传端口"""
    if HAS_SERIAL:
        ports = list(serial.tools.list_ports.comports())
        for port in ports:
            if 'USB' in port.description or 'Serial' in port.description or 'CP210' in port.description:
                return port.device
    # 默认使用COM3
    return "COM3"

def modify_platformio_ini(filesystem_type):
    """修改 platformio.ini 中的 filesystem 配置"""
    ini_path = Path('platformio.ini')
    
    if not ini_path.exists():
        print_error("platformio.ini not found!")
        return False
    
    content = ini_path.read_text(encoding='utf-8')
    
    # 替换 filesystem 配置
    if 'board_build.filesystem' in content:
        # 查找并替换
        lines = content.split('\n')
        for i, line in enumerate(lines):
            if 'board_build.filesystem' in line and not line.strip().startswith(';') and '=' in line:
                lines[i] = f'board_build.filesystem = {filesystem_type}'
                break
        
        content = '\n'.join(lines)
        ini_path.write_text(content, encoding='utf-8')
        print_success(f"Set filesystem to: {filesystem_type}")
        return True
    else:
        print_error("board_build.filesystem not found in platformio.ini")
        return False

def main():
    """主函数"""
    print(f"\n{Colors.HEADER}{Colors.BOLD}")
    print("=" * 60)
    print("  NOOX 设备完整部署脚本")
    print("  Automated Dual Filesystem Deployment")
    print("=" * 60)
    print(Colors.END)
    
    # 检查当前目录
    if not Path('platformio.ini').exists():
        print_error("Not in NOOX project root directory!")
        print("Please run this script from the project root.")
        sys.exit(1)
    
    # 初始化变量
    platformio_backup = None
    data_dir = Path('data')
    
    try:
        # ===================================================================
        # 步骤 1: 压缩 Web 文件
        # ===================================================================
        print_step(1, 5, "Compressing web files")
        
        if not run_command(
            'python compress_files_optimized.py',
            'Compress HTML/CSS/JS files to data_littlefs/'
        ):
            raise Exception("Failed to compress web files")
        
        # 验证输出
        if not Path('data_littlefs').exists() or not list(Path('data_littlefs').glob('*.gz')):
            raise Exception("data_littlefs/ directory is empty or missing")
        
        print_success("Web files compressed successfully")
        
        # ===================================================================
        # 步骤 2: 编译固件
        # ===================================================================
        print_step(2, 5, "Compiling firmware")
        
        if not run_command(
            'pio run',
            'Compile firmware'
        ):
            raise Exception("Failed to compile firmware")
        
        print_success("Firmware compiled successfully")
        
        # ===================================================================
        # 步骤 3: 上传固件
        # ===================================================================
        print_step(3, 5, "Uploading firmware")
        
        if not run_command(
            'pio run --target upload',
            'Upload firmware to device'
        ):
            raise Exception("Failed to upload firmware")
        
        print_success("Firmware uploaded successfully")
        time.sleep(2)  # 等待设备重启
        
        # ===================================================================
        # 步骤 4: 上传 LittleFS (Web 文件)
        # ===================================================================
        print_step(4, 5, "Uploading LittleFS (Web files)")
        
        # 备份 platformio.ini
        print("Backing up platformio.ini...")
        platformio_backup = backup_file('platformio.ini')
        
        # 切换到 littlefs
        if not modify_platformio_ini('littlefs'):
            raise Exception("Failed to modify platformio.ini for littlefs")
        
        # 创建临时 data 目录并复制 littlefs 内容
        if data_dir.exists():
            shutil.rmtree(data_dir)
        data_dir.mkdir()
        
        print("Copying data_littlefs/ to data/...")
        for file in Path('data_littlefs').glob('*'):
            shutil.copy2(file, data_dir / file.name)
        
        # 手动构建文件系统镜像，强制使用正确的大小
        littlefs_size = 2097152  # 2MB (0x200000)
        littlefs_bin = Path('.pio/build/esp32s3_NOOX/littlefs.bin')
        
        # 确保输出目录存在
        littlefs_bin.parent.mkdir(parents=True, exist_ok=True)
        
        # 查找 mklittlefs 工具
        mklittlefs_tool = None
        pio_packages = Path.home() / '.platformio' / 'packages'
        
        if pio_packages.exists():
            for tool_dir in pio_packages.glob('tool-mklittlefs*'):
                mklittlefs_exe = tool_dir / 'mklittlefs.exe'
                if mklittlefs_exe.exists():
                    mklittlefs_tool = str(mklittlefs_exe)
                    break
        
        if not mklittlefs_tool:
            print_warning("mklittlefs tool not found, falling back to PlatformIO buildfs")
            if not run_command(
                'pio run --target buildfs',
                'Build LittleFS image'
            ):
                raise Exception("Failed to build LittleFS image")
        else:
            # 直接调用 mklittlefs，强制使用 2MB 大小
            mklittlefs_cmd = f'"{mklittlefs_tool}" -c data -s {littlefs_size} -p 256 -b 4096 "{littlefs_bin}"'
            print(f"Building LittleFS with forced size: {littlefs_size:,} bytes ({littlefs_size/1024/1024:.2f} MB)")
            
            if not run_command(mklittlefs_cmd, 'Build LittleFS image'):
                raise Exception("Failed to build LittleFS image")
        
        if not littlefs_bin.exists():
            raise Exception("LittleFS image not found")
        
        # 验证镜像大小
        expected_size = littlefs_size
        actual_size = littlefs_bin.stat().st_size
        print(f"LittleFS image size: {actual_size:,} bytes ({actual_size/1024/1024:.2f} MB)")
        
        if actual_size != expected_size:
            print_warning(f"WARNING: LittleFS image size ({actual_size} bytes) != expected size ({expected_size} bytes)")
            print_warning("Filesystem may not work correctly!")
        
        # 获取串口
        upload_port = get_upload_port()
        print(f"Using upload port: {upload_port}")
        
        # 使用esptool直接上传
        esptool_cmd = f'python -m esptool --chip esp32s3 --port {upload_port} --baud 460800 write_flash 0x810000 {littlefs_bin}'
        if not run_command(esptool_cmd, 'Upload LittleFS to 0x810000'):
            raise Exception("Failed to upload LittleFS")
        
        print_success("LittleFS uploaded successfully to 0x810000")
        time.sleep(2)
        
        # ===================================================================
        # 步骤 5: 上传 FFat (Agent 文件)
        # ===================================================================
        print_step(5, 5, "Uploading FFat (Agent files)")
        
        # 恢复 platformio.ini 并切换到 fatfs
        if platformio_backup:
            restore_file('platformio.ini', platformio_backup)
            platformio_backup = backup_file('platformio.ini')
        
        if not modify_platformio_ini('fatfs'):
            raise Exception("Failed to modify platformio.ini for fatfs")
        
        # 清空并复制 ffat 内容
        if data_dir.exists():
            shutil.rmtree(data_dir)
        data_dir.mkdir()
        
        print("Copying data_ffat/ to data/...")
        if Path('data_ffat').exists():
            for file in Path('data_ffat').glob('*'):
                shutil.copy2(file, data_dir / file.name)
        else:
            print_warning("data_ffat/ directory not found, skipping...")
        
        # 手动构建文件系统镜像，强制使用正确的大小
        ffat_size = 6225920  # ~6MB (0x5F0000)
        ffat_bin = Path('.pio/build/esp32s3_NOOX/fatfs.bin')
        
        # 确保输出目录存在
        ffat_bin.parent.mkdir(parents=True, exist_ok=True)
        
        # 查找 mkfatfs 工具
        mkfatfs_tool = None
        pio_packages = Path.home() / '.platformio' / 'packages'
        
        if pio_packages.exists():
            for tool_dir in pio_packages.glob('tool-mkfatfs*'):
                mkfatfs_exe = tool_dir / 'mkfatfs.exe'
                if mkfatfs_exe.exists():
                    mkfatfs_tool = str(mkfatfs_exe)
                    break
        
        if not mkfatfs_tool:
            print_warning("mkfatfs tool not found, falling back to PlatformIO buildfs")
            if not run_command(
                'pio run --target buildfs',
                'Build FFat image'
            ):
                raise Exception("Failed to build FFat image")
        else:
            # 直接调用 mkfatfs，强制使用 6MB 大小
            mkfatfs_cmd = f'"{mkfatfs_tool}" -c data -s {ffat_size} "{ffat_bin}"'
            print(f"Building FFat with forced size: {ffat_size:,} bytes ({ffat_size/1024/1024:.2f} MB)")
            
            if not run_command(mkfatfs_cmd, 'Build FFat image'):
                raise Exception("Failed to build FFat image")
        
        if not ffat_bin.exists():
            raise Exception("FFat image not found")
        
        # 验证镜像大小
        expected_size = ffat_size
        actual_size = ffat_bin.stat().st_size
        print(f"FFat image size: {actual_size:,} bytes ({actual_size/1024/1024:.2f} MB)")
        
        if actual_size != expected_size:
            print_warning(f"WARNING: FFat image size ({actual_size} bytes) != expected size ({expected_size} bytes)")
            print_warning("Filesystem may not work correctly!")
        
        # 获取串口
        upload_port = get_upload_port()
        print(f"Using upload port: {upload_port}")
        
        # 使用esptool直接上传
        esptool_cmd = f'python -m esptool --chip esp32s3 --port {upload_port} --baud 460800 write_flash 0xA10000 {ffat_bin}'
        if not run_command(esptool_cmd, 'Upload FFat to 0xA10000'):
            raise Exception("Failed to upload FFat")
        
        print_success("FFat uploaded successfully to 0xA10000")
        
        # ===================================================================
        # 完成
        # ===================================================================
        print(f"\n{Colors.GREEN}{Colors.BOLD}")
        print("=" * 60)
        print("  ✓ Deployment Complete!")
        print("=" * 60)
        print(Colors.END)
        
        print(f"\n{Colors.CYAN}Next steps:{Colors.END}")
        print("1. Open serial monitor: pio device monitor")
        print("2. Check device logs for successful initialization")
        print("3. Connect device WiFi and access web interface")
        
    except Exception as e:
        print(f"\n{Colors.RED}{Colors.BOLD}")
        print("=" * 60)
        print("  ✗ Deployment Failed!")
        print("=" * 60)
        print(Colors.END)
        print(f"\nError: {e}")
        print("\nRolling back changes...")
        
        sys.exit(1)
    
    finally:
        # 清理：恢复配置和删除临时目录
        print("\nCleaning up...")
        
        if platformio_backup and platformio_backup.exists():
            restore_file('platformio.ini', platformio_backup)
            print_success("Restored platformio.ini")
        
        if data_dir.exists():
            shutil.rmtree(data_dir)
            print_success("Removed temporary data/ directory")
        
        print("\nCleanup complete.")

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n\n{Colors.YELLOW}Deployment interrupted by user{Colors.END}")
        sys.exit(1)

