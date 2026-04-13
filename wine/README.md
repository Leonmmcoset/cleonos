# CLeonOS-Wine (Native)

CLeonOS-Wine 现在改为自研运行器：基于 Python + Unicorn，直接运行 CLeonOS x86_64 用户 ELF。

不再依赖 Qiling。

## 文件结构

- `wine/cleonos_wine.py`：兼容入口脚本
- `wine/cleonos_wine_lib/cli.py`：命令行参数与启动流程
- `wine/cleonos_wine_lib/runner.py`：ELF 装载、执行、syscall 分发
- `wine/cleonos_wine_lib/state.py`：内核态统计与共享状态
- `wine/cleonos_wine_lib/input_pump.py`：主机键盘输入线程
- `wine/cleonos_wine_lib/constants.py`：常量与 syscall ID
- `wine/cleonos_wine_lib/platform.py`：Unicorn 导入与平台适配
- `wine/requirements.txt`：Python 依赖（Unicorn）

## 安装

```bash
pip install -r wine/requirements.txt
```

## 运行

```bash
python wine/cleonos_wine.py /hello.elf --rootfs build/x86_64/ramdisk_root
python wine/cleonos_wine.py /shell/shell.elf --rootfs build/x86_64/ramdisk_root
```

也支持直接传宿主路径：

```bash
python wine/cleonos_wine.py build/x86_64/ramdisk_root/shell/shell.elf --rootfs build/x86_64/ramdisk_root
```

## 支持

- ELF64 (x86_64) PT_LOAD 段装载
- CLeonOS `int 0x80` syscall 0..39
- TTY 输出与键盘输入队列
- rootfs 文件/目录访问（`FS_*`）
- `/temp` 写入限制（`FS_MKDIR/WRITE/APPEND/REMOVE`）
- `EXEC_PATH` 递归执行 ELF（带深度限制）

## 参数

- `--no-kbd`：关闭输入线程
- `--max-exec-depth N`：设置 exec 嵌套深度上限
- `--verbose`：打印更多日志