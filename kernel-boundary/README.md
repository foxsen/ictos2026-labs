# 实验：跨越保护边界

本实验研究 Linux 系统调用、Linux 进程间通信和 seL4 IPC 的成本。目标不是用一个数字宣布“哪个操作系统更快”，而是学习如何定义可比较的操作、控制变量、保存原始证据，并把延迟变化解释为具体的操作系统机制。

## 1. 学习目标

完成实验后，你应当能够：

1. 区分普通函数调用、vDSO 调用、真实系统调用和跨进程 IPC；
2. 解释特权级切换、地址空间切换、调度、Cache/TLB 和消息长度可能产生的成本；
3. 设计可重复的微基准实验，并报告批次均值的分布和异常运行；
4. 判断两个数字是否具有可比性，而不是只计算倍率；
5. 使用数据验证或质疑一条关于微内核性能的论断。

## 2. 实验边界

本实验包含两条证据链：

- **Linux 原生实验**：测量同一 Linux 环境中的用户态基线、raw syscall、vDSO 候选路径和 Unix-domain socket 往返。
- **seL4 实验**：使用固定的 seL4bench 16.0.0，比较相同环境内的同/异地址空间、消息长度以及 fastpath 开关。

如果 seL4 运行在 QEMU TCG 中，输出的 cycle 数是仿真环境中的观测值。同一 TCG 配置下的相对变化也只反映该仿真执行路径，不能据此定量解释真实硬件的 Cache/TLB 或流水线成本；**不得拿来除以宿主 Linux 的纳秒数，也不得据此声称 Linux 或 seL4 更快**。只有在教师提供相同硬件或严格配对的虚拟机环境时，才能进行受限的跨系统绝对比较。

## 3. 目录与提交物

```text
kernel-boundary/
├── linux/                 # Linux C 基准程序
├── scripts/               # 环境采集、运行、汇总和 seL4 结果解析
├── sel4/                  # 固定版本的 seL4bench 下载、构建与运行脚本
├── environment/           # 可选 Docker 环境
├── teacher/               # 教师准备预构建镜像的工具
└── report-template.md     # 报告模板
```

必做内容为 **L1 调用耗时表、L2 消息长度曲线、S1–S3 三类 seL4 对照，以及一条论文联系**。按后文指定的数据和步骤完成，使用 `report-template.md` 整理报告。运行前记录环境与参数即可，不再单独提交假设卡；不要求改内核或做路径插桩。

| 任务 | 固定要求 | 报告产出 |
|---|---|---|
| L1 | Linux 默认 5 种调用 / 基线 | 一张 median、mean、P95 耗时表 |
| L2 | socket 1、64、1024、4096 B 完整往返 | 一张消息长度—median 耗时图及变化量 |
| S1–S3 | seL4 两个镜像，各选 3 个指定配置 | 六组统计表、四对比较（S3 有两种长度） |
| 论文联系 | 一条课程论文论断，附原文位置 | 不超过一页的对应分析 |

最终提交：

- 填写完成的 `report.md`；
- `results/linux/` 中的 CSV、环境信息和 stderr；
- `results/sel4-*/` 中的 serial log、JSON、CSV 和 pinned manifest；
- `figures/` 中的图表，以及生成图表的脚本或可编辑表格；
- 你修改过的代码与脚本；
- 从环境准备到运行、汇总、制图的完整命令及参数；使用预构建包时附包名和校验结果。

不要只提交截图或只提交汇总表。原始数据是结论成立的必要证据。

## 4. 环境准备

推荐环境为 x86_64 Ubuntu 22.04/24.04。Linux 实验至少需要：

```bash
cc make python3
```

seL4 源码构建还需要 `git`、Google `repo`、CMake、Ninja、QEMU、`protoc` 和 Python venv。可以直接使用教师提供的预构建包，也可以使用本实验的容器：

```bash
./environment/build-container.sh
./environment/container-shell.sh
```

进入容器后，当前目录为 `/lab`。首次同步 seL4 源码需要网络，体积和耗时会明显大于 Linux 部分。

## 5. 实验一：Linux 边界成本

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

## 6. 实验二：seL4 IPC

### 6.1 教师提供预构建包时

在本目录下安装教师发放的包：

```bash
mkdir -p .work/sel4bench
tar -xzf /path/to/sel4-tcg-prebuilt.tar.gz -C .work/sel4bench
(cd .work/sel4bench && sha256sum -c SHA256SUMS)
```

然后分别运行 fastpath 开、关两个镜像：

```bash
./sel4/run.sh tcg on
./sel4/run.sh tcg off
```

### 6.2 从源码构建时

```bash
./sel4/setup.sh
./sel4/build.sh tcg on
./sel4/run.sh tcg on
./sel4/build.sh tcg off
./sel4/run.sh tcg off
```

也可以一次执行：

```bash
./sel4/run-all.sh tcg
```

TCG 配置为了兼容纯软件 QEMU，关闭了 PCID、1 GiB huge page 和 FSGSBASE 指令路径。因此它是**课程仿真配置**，不是 seL4 在真实硬件上的性能配置。

若教师确认机器可访问 `/dev/kvm`，可以把 `tcg` 换成 `kvm`。KVM 结果仍需记录虚拟化边界，不能默认等同于裸机。

成功运行后，目录中应出现：

- `serial.log`：完整启动和基准输出；
- `results.json`：从串口日志抽取的原始 sel4bench JSON；
- `ipc.csv`：便于制图的扁平表；
- `pinned-manifest.xml`：所有 seL4 组件的精确提交版本。

串口中可能出现一系列从极大对象尺寸逐级下降的 `Failed to allocate object`，这是该版本启动时探测可分配对象尺寸产生的输出。自动脚本在看到 `END JSON OUTPUT` 后就会结束 QEMU；`results.json` 能被解析且 `ipc.csv` 非空即为成功。若手工让镜像继续运行，随后还会看到 `All is well in the universe`。不要用日志中是否出现 `Failed` 字样来判断实验成败。

### 6.3 选出六组指定结果

两次运行只改变 fastpath 开关，保持机器、QEMU profile、seL4 版本与其他构建配置一致。`run.sh` 的默认输出目录分别是：

```text
results/sel4-tcg-fastpath-on/
results/sel4-tcg-fastpath-off/
```

在每份 `ipc.csv` 中作以下筛选：

- `benchmark` = `One way IPC microbenchmarks`；
- `function` **恰好**为 `seL4_Call`，不选带 `(FPU)` 后缀的行；
- `direction` = `client->server`；
- 取下表三个地址空间 / 消息长度配置。

| 配置 | same_vspace | ipc_length_words | 每个镜像需保留的记录 |
|---|---|---:|---|
| 同空间短消息 | True | 0 | 1 行 |
| 异空间短消息 | False | 0 | 1 行 |
| 异空间长消息 | False | 10 | 1 行 |

两个镜像各三行，共六行。**不要求“同空间、10 words”组合，提供的基准没有这组记录。** 每行抄录 `samples`、`median_cycles`、`q1_cycles`、`q3_cycles`；保留原始 JSON。JSON 的 `Client Prio`、`Server Prio` 应在所选记录间一致，报告其实际值；当前 CSV 没有导出这两列，所以从 JSON 核对。

此基准测的是 **client→server 的单向 IPC**，并由 seL4bench 校正计时开销；它不是整个 `seL4_Call` 请求—回复的往返时间。默认课程 profile 的单位写为“TCG cycle”。

### 6.4 完成 S1–S3 对照表

以下 A、B 均使用对应行的 `median_cycles`；每对报告 A、B、差值 `B−A` 和比值 `B/A`。若 A 为零，比值填“不适用”并保留原值。

| 任务 | 固定条件 | A | B |
|---|---|---|---|
| S1 地址空间 | fastpath on，0 words | 同 VSpace | 异 VSpace |
| S2 短 / 长消息配置 | fastpath on，异 VSpace | 0 words | 10 words |
| S3a 开关对短消息的影响 | 异 VSpace，0 words | fastpath on | fastpath off |
| S3b 开关对长消息的影响 | 异 VSpace，10 words | fastpath on | fastpath off |

对四行各写一段简短说明：先报告变化，再说明相关机制和证据范围。

- **S1**：结合地址空间切换解释不同配置；TCG 结果只描述该仿真配置，不将差值解释为真实硬件 TLB 成本。
- **S2**：说明消息长度以机器字为单位，以及短 / 长消息可能经过不同传输路径。这是两个基准配置的对比，不是纯复制成本的测量；在 MCS 配置中还可能涉及主动 / 被动服务的区别。
- **S3a/S3b**：描述关闭快路径对两种长度的影响是否相同；结合快路径适用条件作解释。`fastpath=on` 只表示允许使用，不能写成每个样本都命中。

机制说明可引用课程材料及固定版本源码。进一步验证动态路径是可选扩展，不要求为核心任务改内核或插桩。若插桩，应把路径验证运行与原始性能计时分开。

以上配置已对照 manifest 16.0.0 所固定的 sel4bench 提交 `a0f099bf538e14d321c2f9b53a30cd1a64a200f8` 核对，数据定义见 `libsel4benchsupport/include/ipc.h` 和 `apps/sel4bench/src/ipc.c`。

## 7. 论文联系与可选扩展

### 必做：联系一条课程论文论断（不超过一页）

1. 选取一条与所测机制有关的论断，写明论文、章节或图表。例如，“内核调用总成本不等于进入/退出指令成本”，或“IPC 实现路径影响通信开销”。
2. 引用自己的一组结果，写明其操作、平台和比较条件。
3. 说明结果能帮助理解该论断的哪一部分，以及哪些条件与原文不同。现代 Linux / TCG 数据通常不能直接复现 1995 年的周期数，不要求给出简单的“证明 / 推翻”结论。
4. 对本实验不能确定的一点，写出一项具体补充测量：固定什么、改变什么、记录什么。

### 可选扩展：有余力再做

以下任选一项或不做；不替代 L1、L2、S1–S3。若完成，附方法和原始结果，计入相应评分项，总分不超过 100%。

- 使用独立小程序和 `strace` / `perf trace` 分别检查 libc 与 raw `clock_gettime` 路径；原基准包含两种调用，不能只看整次运行的汇总跟踪就把所有系统调用归于 libc。
- 修改 Linux ping-pong 程序，比较两端同核和不同核，记录各自亲和性。
- 选择相同条件的 seL4 普通与 FPU IPC 记录作额外对照。
- 设计预热与 Cache 扰动对照，明确扰动方法与测量范围。

扩展部分写明比较对象、改变的参数、保持不变的条件和结果即可，不另交假设卡。

## 8. 数据解释规则

报告中每个主要结论都应具有如下结构：

```text
论断 → 对应操作的精确定义 → 环境和控制变量 → 原始数据 → 统计摘要 → 机制解释 → 局限性
```

禁止以下做法：

- 将函数名相似当作语义等价；
- 只报告最好的一次或只报告平均值；
- 删除异常值但不公开规则和删除前数据；
- 将 TCG cycle 当作真实 CPU cycle；
- 将宿主 Linux 与 TCG seL4 的数值相除并宣布性能胜负；
- 从 TCG 的相对差异定量推断真实硬件的 Cache/TLB 成本；
- 根据一次微基准推断完整应用性能。

## 9. 验收清单

提交前运行：

```bash
python3 -m py_compile scripts/*.py
make -C linux clean all
./scripts/run-linux.sh results/linux-final
```

并确认：

- 环境信息和精确版本齐全；
- 原始数据行数与报告中的样本数一致；
- 图表坐标、单位和误差表达清楚；
- L1、L2、六组 seL4 记录及 S1–S3 四对计算齐全；
- 按报告模板说明 Linux 两种取时接口、socket 往返与 seL4 单向 IPC、seL4 on/off 的比较范围；
- 报告中的每个数字都能追溯到 CSV 或 JSON。

## 10. 评分标准

| 项目 | 比例 | 验收依据 |
|---|---:|---|
| 方法与配置 | 25% | 运行命令、参数、CPU / profile、版本、操作定义；配对条件一致 |
| 数据与复现 | 25% | 规定图表及计算完整，原始结果可追溯，复现步骤可执行 |
| 结果解释 | 25% | L1/L2/S1–S3 的变化描述准确，解释与代码或课程机制相符 |
| 比较范围 | 15% | 批次均值、单向 / 往返和 TCG 口径准确；不超出证据下结论 |
| 论文联系与表达 | 10% | 论断位置明确，对应自己的结果，差异与后续测量写清楚 |

数据没有明显差异或不符合预期，不因此扣分；按实际结果及说明是否充分评分。

## 11. 参考资料

- [seL4 IPC Tutorial](https://docs.sel4.systems/Tutorials/ipc)
- [The sel4bench Suite](https://docs.sel4.systems/projects/sel4bench/)
- [Linux vDSO manual](https://man7.org/linux/man-pages/man7/vdso.7.html)
- [Linux syscall manual](https://man7.org/linux/man-pages/man2/syscall.2.html)
