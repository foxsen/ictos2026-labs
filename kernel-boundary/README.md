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

如果 seL4 运行在 QEMU TCG 中，输出的 cycle 数是仿真环境中的观测值。它适合比较同一 TCG 配置下的相对变化，**不得拿来除以宿主 Linux 的纳秒数，也不得据此声称 Linux 或 seL4 更快**。只有在教师提供相同硬件或严格配对的虚拟机环境时，才能进行受限的跨系统绝对比较。

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

最终提交：

- 填写完成的 `report.md`；
- `results/linux/` 中的 CSV、环境信息和 stderr；
- `results/sel4-*/` 中的 serial log、JSON、CSV 和 pinned manifest；
- 你修改过的代码与脚本；
- 一条可以从干净环境重新生成主要结果的命令。

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

### 必答问题 A

1. `clock_gettime_vdso_candidate` 与 `raw_clock_gettime` 是否不同？这个差异能否证明前者一定使用了 vDSO？还需要什么证据？
2. 为什么不能简单用 `raw_getpid - empty_loop` 得到“纯系统调用开销”？
3. mean、median、P95 和 P99 中，哪一个最能代表你的主要观察？为什么？
4. 消息从 1 字节增加到 4096 字节时，socket 往返成本怎样变化？哪些成本与消息长度无关？

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

### 必答问题 B

1. 同一 VSpace 与不同 VSpace 的 `seL4_Call` 结果有什么差异？在 TCG 环境中，这个差异能解释到什么程度？
2. IPC length 从 0 增加到 10 words 后发生了什么？它与 Linux 1/64 字节 socket 消息是否等价？
3. fastpath 关闭后哪些结果变化最大？这一观察是否符合 fastpath 的适用条件？
4. 为什么 `seL4_Call`、`seL4_ReplyRecv` 和 Linux socket round trip 不能直接放在同一张“谁更快”柱状图中？

## 7. 自选扩展

选择一项即可：

- 用 `strace` 或 `perf trace` 验证 libc `clock_gettime` 是否进入内核；
- 改变 Linux CPU 亲和性，让 ping-pong 两端位于同核、同 LLC 不同核或不同 NUMA 节点；
- 在 seL4 中比较 FPU 与非 FPU IPC 路径；
- 比较 warm run 与人为污染 Cache 后的结果；
- 从课程论文中选择一条性能论断，说明本实验支持、反驳或无法判断它。

扩展实验必须先写假设，再运行实验。没有说明控制变量的额外数据不计分。

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
- 至少指出两个不可比较项；
- 报告中的每个数字都能追溯到 CSV 或 JSON。

## 10. 参考资料

- [seL4 IPC Tutorial](https://docs.sel4.systems/Tutorials/ipc)
- [The sel4bench Suite](https://docs.sel4.systems/projects/sel4bench/)
- [Linux vDSO manual](https://man7.org/linux/man-pages/man7/vdso.7.html)
- [Linux syscall manual](https://man7.org/linux/man-pages/man2/syscall.2.html)
