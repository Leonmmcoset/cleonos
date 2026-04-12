# CLeonOS-Wine (Native)

CLeonOS-Wine 现在改为自研运行器：基于 Python + Unicorn，直接运行 CLeonOS x86_64 用户 ELF。

不再依赖 Qiling。

## 文件

- `wine/cleonos_wine.py`：主运行器（ELF 装载 + `int 0x80` syscall 桥接）
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
- CLeonOS `int 0x80` syscall 0..26
- TTY 输出与键盘输入队列
- rootfs 文件/目录访问（`FS_*`）
- `EXEC_PATH` 递归执行 ELF（带深度限制）

## 参数

- `--no-kbd`：关闭输入线程
- `--max-exec-depth N`：设置 exec 嵌套深度上限
- `--verbose`：打印更多日志
