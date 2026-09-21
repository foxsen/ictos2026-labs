# 实验：跨越保护边界

## 1. 三个层次

| 层次 | 学生完成的工作 | 交付内容 |
|---|---|---|
| **基本要求：Linux** | 运行并读懂已给基准，分析调用与 socket 往返成本 | L1 耗时表、L2 曲线、环境与机制说明、原始数据 |
| **进阶实验：共享内存通信** | 补写传输和通知协议，通过正确性检查，与 socket 公平比较 | 源码、协议说明、检查日志、同核 / 异核对照和评价 |
| **可选实验：seL4** | 使用预构建包或源码环境观察 IPC 配置差异 | 独立附录；不替代前两项 |

基本要求使用现成代码，训练测量口径、复现与结果解释。进阶要求学生亲自实现协议，训练同步正确性、成本分析和受控比较。现成框架只提供 socket 对照、计时、数据校验、进程管理和汇总，不提供共享内存通信答案。

建议评分为 **基本部分 60 分 + 进阶部分 40 分**；seL4 作为独立选做反馈，不挤占上述分值。完成基本部分即满足基本提交要求。无论做哪个层次，都按实际结果报告，不以更快或更大的倍率作为评分条件。

## 2. 环境与文件

基本和进阶部分只需 Linux、C11 编译器、make、Python 3，无需 seL4、QEMU、Docker 或 Python 第三方包。例如 Ubuntu 可安装：

```bash
sudo apt-get install build-essential python3
```

所有命令在本目录执行。若收到教师压缩包，先在解压目录运行 `sha256sum -c SHA256SUMS`；校验记录对应原始实验包，开始修改后保留这份记录即可。

```text
linux/src/bench_syscall.c       # 基础调用测试（已完成）
linux/src/bench_ipc.c           # 基础 socket 测试（已完成）
linux/src/advanced/
  transport.h                  # 共同接口和协议约定
  bench_compare.c              # 共同测量与正确性检查
  transport_socket.c           # 进阶对照实现（已完成）
  transport_shm.c              # 学生补写：请求、接收、回复
scripts/run-linux.sh            # 基础运行与汇总
scripts/run-advanced.py         # 先检查、再计时；记录配置与源码
ADVANCED.md                    # 进阶任务说明
report-template.md             # 分层报告模板
sel4/README.md                 # 可选 seL4 指导
```

## 3. 基本要求：Linux 测量与报告

运行默认实验：

```bash
./scripts/run-linux.sh
```

脚本会从当前进程允许使用的 CPU 集合中选择第一个 CPU，并将实验固定到该核。可以显式指定：

```bash
CPU=3 ./scripts/run-linux.sh
```

快速烟雾测试可减少采样数：

```bash
SYSCALL_SAMPLES=5 SYSCALL_ITERATIONS=1000 \
IPC_SAMPLES=5 IPC_ITERATIONS=100 \
./scripts/run-linux.sh results/linux-smoke
```

输出包括：

- `syscall.csv`：空循环、普通函数、raw `getpid`、libc `clock_gettime` 和 raw `clock_gettime`；
- `ipc.csv`：1、64、1024、4096 字节 Unix-domain socket ping-pong；
- `summary.csv`：各批次平均单次耗时的 min、median、mean、P95、P99 和 max；
- `environment.txt`：CPU、内核、频率策略和安全缓解状态；
- `*.stderr.txt`：实际 CPU、样本数和迭代次数。

每个 CSV 样本都是一整批操作的总时间除以迭代次数。因此 `summary.csv` 中的 P95/P99 描述的是**批次均值之间的波动**，不是单次调用的尾延迟；若要研究逐次调用尾延迟，需要另行设计计时方式，并评估计时器本身的扰动。

### L1：整理五种调用 / 基线的耗时

1. 用默认参数运行脚本：调用部分为每组 **101 批、每批 10000 次**；保留脚本选择的 CPU 编号。烟雾测试仅用于检查环境，不替代正式采样。
2. 从 `summary.csv` 选出 `empty_loop`、`ordinary_function`、`raw_getpid`、`clock_gettime_vdso_candidate`、`raw_clock_gettime` 五行。
3. 报告每行的 `samples`、`median_batch_mean_ns_per_op`、`mean_batch_mean_ns_per_op` 和 `p95_batch_mean_ns_per_op`，单位为 ns/操作；表头注明“批次均值统计”。直接报告这些值，不扣除空循环基线。
4. 对照 `linux/src/bench_syscall.c` 的 `run_batch()`，说明普通函数、`syscall(SYS_getpid)`、libc `clock_gettime()` 和 `syscall(SYS_clock_gettime, ...)` 的调用方式。两个取时接口均使用 `CLOCK_MONOTONIC`。
5. 用两种取时接口的 median 计算 `raw − libc` 的差值及 `raw / libc` 的比值，描述观察到的差异。libc 接口可能由 vDSO 完成；未验证实际路径时，在报告中写“vDSO 候选路径”，不要仅靠耗时作确定判断。

### L2：绘制 socket 消息长度曲线

1. 同一次脚本运行已覆盖 1、64、1024、4096 B；默认每种长度 **51 批、每批 1000 次往返**，两个进程固定在同一个 CPU。
2. 从 `summary.csv` 选择 `unix_socket_round_trip` 的四行。画一张图：横轴为消息字节数，纵轴为 `median_batch_mean_ns_per_op`，单位写作 **ns/往返**。同时列出四组样本数及 P95；可放在图旁表格中。
3. 用 median 计算 `4096 B − 1 B` 的差值与 `4096 B / 1 B` 的比值。保留中间两种长度，描述曲线是否近似线性、是否有明显转折；没有明显变化也如实报告。
4. 根据 `linux/src/bench_ipc.c` 说明一次往返包含客户发送、服务接收并回复、客户接收。解释消息传输、系统调用和调度怎样参与成本，不把差值直接当成纯复制成本。

### Linux 数据口径

- `ns_per_op` 是一批操作的平均值。L1 和 L2 的 P95 是这些批次均值的分位数，不是单次操作的尾延迟。
- 空循环、普通函数、取 PID 与取时间完成的工作不同；本实验比较其观测耗时，不要求相减求“纯模式切换成本”。
- vDSO 的动态路径验证属于可选扩展；如果使用跟踪工具，其结果另存为路径验证材料，不用于填正式计时表。

## 4. 进阶实验入口

阅读 [ADVANCED.md](ADVANCED.md)，实现“共享内存传数据、同步机制传通知”的跨进程请求—回复。

先验证给定 socket 对照，熟悉框架：

```bash
python3 scripts/run-advanced.py --mode check --transport socket
```

然后补写 `linux/src/advanced/transport_shm.c`，依次运行：

```bash
python3 scripts/run-advanced.py --mode check
python3 scripts/run-advanced.py --mode bench
```

未补完时 `shm` 检查报 `STUDENT TODO` 并失败，这是预期行为。`bench` 也会先检查全部请求配置；任何错误或超时都会停止，且不产生性能汇总。默认分别测同核与不同 CPU，共四种消息长度；只有一个可用 CPU 时先加 `--placements same`，并在报告中说明缺少异核结果。

结果写入新建的 `results/advanced-.../`，不会覆盖已有数据。进阶 socket 与基础 L2 协议不同：进阶统一增加 8 字节序号头，应与同一框架里的 socket 比较，不能直接沿用基础 L2 的旧数值。

## 5. 可选 seL4

选择此项再阅读 [sel4/README.md](sel4/README.md)。固定版本、构建脚本、预构建包工具继续提供，但基本与进阶不依赖它们。默认 QEMU TCG 输出不是宿主 CPU 周期；与 Linux 原生纳秒分别分析。

## 6. 提交要求

**基本部分：**

- `report.md` 基础各节；L1 五行耗时表，L2 四点曲线及变化量。
- `results/linux/` 的全部 CSV、环境记录和 stderr；生成图表的脚本或可编辑表格。
- 操作定义、默认参数或修改说明、计时统计口径，以及结合 `run_batch()` 和 socket 收发流程的解释。
- 从环境准备到运行、汇总、制图的复现步骤。无需另交假设卡。

**完成进阶时另附：**

- 学生实现源码、与 starter 的差异、状态机与内存顺序说明。
- 完整进阶结果目录，含源码快照、哈希、正确性检查、CSV、环境与 `run.json`。
- 同核 / 异核两种配置的 socket 与共享内存比较，以及哪些成本减少、哪些成本转移或增加的分析。
- 一条课程论文论断的联系：引用自己的数据，说明适用条件和未被本实验覆盖的部分，不超过一页。

**选择 seL4 时：** 将数据、图表和解释放在独立可选附录，不填入 Linux 或进阶比较表。

## 7. 数据要求与评分

- 基础和进阶的 Linux P95 都是批次均值分位数，不是单次往返尾延迟。
- 保存失败运行与原始数据，说明重新运行原因。不得只保留最好的一次或隐藏异常。
- 共享内存方案可能更慢；正确实现、公平比较和有依据的解释同样可获满分。
- 正确性测试通过不是无数据竞争或无丢失唤醒的形式证明，进阶报告仍需解释协议为什么成立。

| 部分 | 分值 | 具体检查 |
|---|---:|---|
| 基础：方法与复现 | 20 | 运行、环境、CPU、采样参数、原始数据与命令 |
| 基础：表格与统计 | 20 | L1/L2 完整，单位与计算正确，图表可追溯 |
| 基础：机制与范围 | 20 | 调用方式、往返过程、vDSO 候选路径及批次均值口径 |
| 进阶：实现与正确性 | 20 | 自行实现共享数据与通知；所有权、状态转换、校验和错误处理 |
| 进阶：对照与评价 | 20 | 同一框架、同 / 异核、长度变化、成本解释与论文联系 |

seL4 选做独立反馈，不代替共享内存实现。教师可通过更换 seed、消息长度和 CPU 配置检查实现。

## 8. 参考资料

- [Linux vDSO](https://man7.org/linux/man-pages/man7/vdso.7.html)
- [Linux syscall](https://man7.org/linux/man-pages/man2/syscall.2.html)
- [Linux futex](https://man7.org/linux/man-pages/man2/futex.2.html)
- [FUTEX_WAIT](https://man7.org/linux/man-pages/man2/FUTEX_WAIT.2const.html)
- [seL4 IPC](https://docs.sel4.systems/Tutorials/ipc)
