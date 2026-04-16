# CLeonOS Syscall 文档

本文档描述 CLeonOS 用户态通过 `int 0x80` 进入内核的 syscall ABI 与当前实现行为。

## 1. 调用约定（x86_64）

用户态统一通过：

```c
u64 cleonos_syscall(u64 id, u64 arg0, u64 arg1, u64 arg2);
```

寄存器约定：

- `rax` = syscall id
- `rbx` = arg0
- `rcx` = arg1
- `rdx` = arg2
- 返回值在 `rax`

中断向量：

- `int 0x80`

头文件位置：

- `cleonos/c/include/cleonos_syscall.h`

内核分发位置：

- `clks/kernel/syscall.c`

## 2. 全局返回规则

- 成功时通常返回非负值（如长度、计数、状态）。
- 失败时多数接口返回 `0xFFFFFFFFFFFFFFFF`（即 `u64` 的 `-1`）。
- 部分接口失败返回 `0`（例如 `FS_READ` / `FS_WRITE` / `FS_APPEND` / `FS_REMOVE` / `LOG_JOURNAL_READ`）。

## 3. 当前实现中的长度/路径限制

以下限制由内核 `clks/kernel/syscall.c` 当前实现决定：

- 日志写入 `LOG_WRITE`：最大拷贝 `191` 字节。
- TTY 文本写入 `TTY_WRITE`：最大拷贝 `512` 字节。
- 文件读取 `FS_READ`：最多读取 `min(file_size, buffer_size)` 字节。
- 文件写入 `FS_WRITE` / `FS_APPEND`：单次最大 `65536` 字节。
- log journal 行读取缓冲：`256` 字节。
- 路径缓冲上限：`192` 字节（包含 `\0`）。
- 文件名输出上限：`96` 字节（与 `CLEONOS_FS_NAME_MAX` 对齐）。

文件系统写入类 syscall 的权限限制：

- `FS_MKDIR` / `FS_WRITE` / `FS_APPEND` / `FS_REMOVE` 仅允许 `/temp` 树下路径。

## 4. Syscall 列表（0~50）

### 0 `CLEONOS_SYSCALL_LOG_WRITE`

- 参数：
- `arg0`: `const char *message`
- `arg1`: `u64 length`
- 返回：实际写入长度
- 说明：写入内核日志通道（tag 为 `SYSCALL`），长度会被截断到 191。

### 1 `CLEONOS_SYSCALL_TIMER_TICKS`

- 参数：无
- 返回：系统 timer tick 计数

### 2 `CLEONOS_SYSCALL_TASK_COUNT`

- 参数：无
- 返回：任务总数

### 3 `CLEONOS_SYSCALL_CUR_TASK`

- 参数：无
- 返回：当前任务 ID

### 4 `CLEONOS_SYSCALL_SERVICE_COUNT`

- 参数：无
- 返回：服务总数

### 5 `CLEONOS_SYSCALL_SERVICE_READY_COUNT`

- 参数：无
- 返回：ready 服务数

### 6 `CLEONOS_SYSCALL_CONTEXT_SWITCHES`

- 参数：无
- 返回：上下文切换计数

### 7 `CLEONOS_SYSCALL_KELF_COUNT`

- 参数：无
- 返回：内核态 ELF 应用计数

### 8 `CLEONOS_SYSCALL_KELF_RUNS`

- 参数：无
- 返回：内核态 ELF 累计运行次数

### 9 `CLEONOS_SYSCALL_FS_NODE_COUNT`

- 参数：无
- 返回：VFS 节点总数

### 10 `CLEONOS_SYSCALL_FS_CHILD_COUNT`

- 参数：
- `arg0`: `const char *dir_path`
- 返回：子节点数量

### 11 `CLEONOS_SYSCALL_FS_GET_CHILD_NAME`

- 参数：
- `arg0`: `const char *dir_path`
- `arg1`: `u64 index`
- `arg2`: `char *out_name`
- 返回：成功 `1`，失败 `0`
- 说明：最多写入 96 字节（含终止符）。

### 12 `CLEONOS_SYSCALL_FS_READ`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `char *out_buffer`
- `arg2`: `u64 buffer_size`
- 返回：实际读取字节数，失败/文件空返回 `0`
- 说明：不会自动追加 `\0`，调用方应自行处理文本终止。

### 13 `CLEONOS_SYSCALL_EXEC_PATH`

- 参数：
- `arg0`: `const char *path`
- 返回：
- `-1`：请求失败
- 其他：由内核执行器返回状态（通常 `0` 表示 accepted）

### 14 `CLEONOS_SYSCALL_EXEC_REQUESTS`

- 参数：无
- 返回：执行请求累计数

### 15 `CLEONOS_SYSCALL_EXEC_SUCCESS`

- 参数：无
- 返回：执行成功累计数

### 16 `CLEONOS_SYSCALL_USER_SHELL_READY`

- 参数：无
- 返回：用户 shell ready（1/0）

### 17 `CLEONOS_SYSCALL_USER_EXEC_REQUESTED`

- 参数：无
- 返回：是否发起过用户侧 exec 请求（1/0）

### 18 `CLEONOS_SYSCALL_USER_LAUNCH_TRIES`

- 参数：无
- 返回：用户态启动尝试次数

### 19 `CLEONOS_SYSCALL_USER_LAUNCH_OK`

- 参数：无
- 返回：用户态启动成功次数

### 20 `CLEONOS_SYSCALL_USER_LAUNCH_FAIL`

- 参数：无
- 返回：用户态启动失败次数

### 21 `CLEONOS_SYSCALL_TTY_COUNT`

- 参数：无
- 返回：TTY 总数

### 22 `CLEONOS_SYSCALL_TTY_ACTIVE`

- 参数：无
- 返回：当前 active TTY 索引

### 23 `CLEONOS_SYSCALL_TTY_SWITCH`

- 参数：
- `arg0`: `u64 tty_index`
- 返回：切换后的 active TTY 索引

### 24 `CLEONOS_SYSCALL_TTY_WRITE`

- 参数：
- `arg0`: `const char *text`
- `arg1`: `u64 length`
- 返回：实际写入长度
- 说明：长度会被截断到 512。

### 25 `CLEONOS_SYSCALL_TTY_WRITE_CHAR`

- 参数：
- `arg0`: `u64 ch`（低 8 位有效）
- 返回：当前实现固定返回 `1`

### 26 `CLEONOS_SYSCALL_KBD_GET_CHAR`

- 参数：无
- 返回：
- 无输入时 `-1`
- 有输入时返回字符值（低 8 位；按当前进程/TTY 上下文读取）

### 27 `CLEONOS_SYSCALL_FS_STAT_TYPE`

- 参数：
- `arg0`: `const char *path`
- 返回：`1=FILE`，`2=DIR`，失败 `-1`

### 28 `CLEONOS_SYSCALL_FS_STAT_SIZE`

- 参数：
- `arg0`: `const char *path`
- 返回：文件大小；目录通常为 `0`；失败 `-1`

### 29 `CLEONOS_SYSCALL_FS_MKDIR`

- 参数：
- `arg0`: `const char *path`
- 返回：成功 `1`，失败 `0`
- 说明：仅允许 `/temp` 下创建目录。

### 30 `CLEONOS_SYSCALL_FS_WRITE`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *data`
- `arg2`: `u64 size`
- 返回：成功 `1`，失败 `0`
- 说明：覆盖写；仅允许 `/temp` 下文件。

### 31 `CLEONOS_SYSCALL_FS_APPEND`

- 参数：
- `arg0`: `const char *path`
- `arg1`: `const char *data`
- `arg2`: `u64 size`
- 返回：成功 `1`，失败 `0`
- 说明：追加写；仅允许 `/temp` 下文件。

### 32 `CLEONOS_SYSCALL_FS_REMOVE`

- 参数：
- `arg0`: `const char *path`
- 返回：成功 `1`，失败 `0`
- 说明：仅允许 `/temp` 下删除；目录需为空。

### 33 `CLEONOS_SYSCALL_LOG_JOURNAL_COUNT`

- 参数：无
- 返回：日志 journal 条目数量

### 34 `CLEONOS_SYSCALL_LOG_JOURNAL_READ`

- 参数：
- `arg0`: `u64 index_from_oldest`
- `arg1`: `char *out_line`
- `arg2`: `u64 out_size`
- 返回：成功 `1`，失败 `0`
- 说明：内核会保证输出字符串有 `\0` 终止。

### 35 `CLEONOS_SYSCALL_KBD_BUFFERED`

- 参数：无
- 返回：当前键盘队列中的字符数量

### 36 `CLEONOS_SYSCALL_KBD_PUSHED`

- 参数：无
- 返回：键盘累计入队计数

### 37 `CLEONOS_SYSCALL_KBD_POPPED`

- 参数：无
- 返回：键盘累计出队计数

### 38 `CLEONOS_SYSCALL_KBD_DROPPED`

- 参数：无
- 返回：键盘队列溢出丢弃计数

### 39 `CLEONOS_SYSCALL_KBD_HOTKEY_SWITCHES`

- 参数：无
- 返回：ALT+F1..F4 热键切换计数

### 40 `CLEONOS_SYSCALL_GETPID`

- 参数：无
- 返回：当前进程 PID（无活动进程时为 `0`）

### 41 `CLEONOS_SYSCALL_SPAWN_PATH`

- 参数：
- `arg0`: `const char *path`
- 返回：
- 成功：子进程 PID
- 失败：`-1`
- 说明：当前实现为异步 spawn（进入 pending 队列，后续由调度 tick 派发执行）。

### 42 `CLEONOS_SYSCALL_WAITPID`

- 参数：
- `arg0`: `u64 pid`
- `arg1`: `u64 *out_status`（可为 `0`）
- 返回：
- `-1`：PID 不存在
- `0`：目标进程仍在运行
- `1`：目标进程已退出
- 说明：当返回 `1` 且 `arg1!=0` 时，会写入退出码。

### 43 `CLEONOS_SYSCALL_EXIT`

- 参数：
- `arg0`: `u64 status`
- 返回：
- `1`：已记录退出请求
- `0`：当前上下文不支持退出请求

### 44 `CLEONOS_SYSCALL_SLEEP_TICKS`

- 参数：
- `arg0`: `u64 ticks`
- 返回：实际休眠 tick 数

### 45 `CLEONOS_SYSCALL_YIELD`

- 参数：无
- 返回：当前 tick

### 46 `CLEONOS_SYSCALL_SHUTDOWN`

- 参数：无
- 返回：理论上不返回；成功路径会触发关机流程（当前 x86_64 走 QEMU/ACPI 关机端口）
- 说明：若关机流程未生效，内核会进入 halt 循环。

### 47 `CLEONOS_SYSCALL_RESTART`

- 参数：无
- 返回：理论上不返回；成功路径会触发重启流程（当前 x86_64 走 8042 reset）
- 说明：若重启流程未生效，内核会进入 halt 循环。

### 48 `CLEONOS_SYSCALL_AUDIO_AVAILABLE`

- 参数：无
- 返回：
- `1`：音频输出可用
- `0`：当前平台无音频输出

### 49 `CLEONOS_SYSCALL_AUDIO_PLAY_TONE`

- 参数：
- `arg0`: `u64 hz`（频率，`0` 表示静音等待）
- `arg1`: `u64 ticks`（持续 tick）
- 返回：成功 `1`，失败 `0`
- 说明：当前实现基于 PC Speaker（x86_64），用于最小音频链路。

### 50 `CLEONOS_SYSCALL_AUDIO_STOP`

- 参数：无
- 返回：当前实现固定返回 `1`
- 说明：立即停止当前音频输出。

## 5. 用户态封装函数

用户态封装位于：

- `cleonos/c/src/syscall.c`

常用封装示例：

- `cleonos_sys_fs_read()`
- `cleonos_sys_fs_write()` / `cleonos_sys_fs_append()` / `cleonos_sys_fs_remove()`
- `cleonos_sys_log_journal_count()` / `cleonos_sys_log_journal_read()`
- `cleonos_sys_exec_path()`
- `cleonos_sys_tty_write()`
- `cleonos_sys_kbd_get_char()` / `cleonos_sys_kbd_buffered()`
- `cleonos_sys_getpid()` / `cleonos_sys_spawn_path()` / `cleonos_sys_wait_pid()`
- `cleonos_sys_exit()` / `cleonos_sys_sleep_ticks()` / `cleonos_sys_yield()` / `cleonos_sys_shutdown()` / `cleonos_sys_restart()`
- `cleonos_sys_audio_available()` / `cleonos_sys_audio_play_tone()` / `cleonos_sys_audio_stop()`

## 6. 开发注意事项

- 传入的字符串/缓冲指针目前按“同地址空间可直接访问”模型处理，后续若引入严格用户态地址隔离，需要补充用户内存校验。
- `FS_READ` 不保证文本终止符；读取文本请预留 1 字节并手动 `buf[n] = '\0'`。
- `FS_WRITE`/`FS_APPEND` 仅允许 `/temp`，并有单次长度上限。
