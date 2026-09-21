# 教师准备与授课说明

## 1. 推荐实施方式

建议两周、2–3 人一组：

- 第一周完成 Linux 实验、理解 raw syscall/vDSO/IPC 的区别；
- 第二周完成 seL4 六组统计、S1–S3 对照和论文联系；
- 课堂或答辩时随机改变一个参数，要求小组重新运行并解释。

不再要求运行前提交假设卡。按学生指导书验收 L1、L2、S1–S3 和一条论文联系；报告模板提供各表的行、列和计算方向。路径跟踪、插桩、跨核比较为可选扩展，不作为核心任务的额外门槛。解释充分的失败结果或“当前配置无法确定”，可获得对应项目的完整分数。

seL4 六组记录固定为普通 `seL4_Call`、`client->server`，两个镜像各取同空间 0、异空间 0、异空间 10 words。核对 JSON 的 Client Prio / Server Prio；不要要求当前基准没有的同空间 10 words，也不要混入 FPU 行。该基准给出经过计时开销校正的单向 IPC 统计，不是完整调用往返。短 / 长消息配置还可能改变执行路径，尤其 MCS 下的服务配置，不能把差值解释成纯数据复制成本。

本实验不按“测得谁最快”评分。主要考察实验设计、机制解释、可复现性和对结论边界的认识。

## 2. 已验证基线

本套脚本在以下环境完成过端到端验证：

- x86_64 Ubuntu 24.04；
- GCC 13.3、CMake/Ninja、QEMU 8.2；
- `sel4bench-manifest` tag `16.0.0`，manifest commit `240919f93cc5d3546d5371de2fc4b12b92c077a6`；
- TCG profile、QEMU `max` CPU、fastpath on；
- seL4 IPC-only 镜像成功启动，输出可解析的 JSON；手工继续运行时可见 `All is well in the universe`。

TCG profile 显式关闭 `KernelSupportPCID`、`KernelHugePage`，并将 `KernelFSGSBase` 改为 `msr`。这些设置是为了让镜像在没有 KVM 的 QEMU TCG 中启动，不代表真实硬件的最佳配置。

## 3. 课前烟雾测试

Linux：

```bash
SYSCALL_SAMPLES=3 SYSCALL_ITERATIONS=1000 \
IPC_SAMPLES=3 IPC_ITERATIONS=50 \
./scripts/run-linux.sh results/linux-smoke
```

seL4：

```bash
./sel4/setup.sh
./sel4/build.sh tcg on
SEL4_TIMEOUT=180 ./sel4/run.sh tcg on results/sel4-smoke
```

验收条件：

- `results/sel4-smoke/serial.log` 含 `END JSON OUTPUT`；
- `results.json` 能被 `python3 -m json.tool` 读取；
- `ipc.csv` 至少含 same/different VSpace、length 0/10 的记录；
- Linux 两个 CSV 的每组样本数与命令参数一致。

## 4. 容器与预构建包

### 4.1 构建统一容器

```bash
./environment/build-container.sh
```

建议教师构建一次后通过校内镜像仓库分发，避免学生重复安装依赖。容器不会自动包含 seL4 源码；源码仍由 `sel4/setup.sh` 固定到指定 manifest。

### 4.2 生成学生预构建包

```bash
./sel4/setup.sh
./sel4/build.sh tcg on
./sel4/build.sh tcg off
./teacher/package-prebuilt.sh tcg
```

发放 `teacher/sel4-tcg-prebuilt.tar.gz`。包内包含两组 kernel/initrd、`simulate`、精确 manifest 和 SHA256 校验。学生无需编译 seL4，只需按指导书解压并运行。

建议同时保留至少一台已经完成源码同步和构建的实验机，以应对学生平台上的 Docker、代理或 QEMU 兼容问题。

课前准备预构建包，以及从已验证运行中整理的原始样例数据（含环境、版本与复现命令）。遇到安装阻塞时，先让学生用样例完成可比性与机制分析；标明数据来源，不能声称是本人测量。环境搭建另行验收，不用安装难度代替研究评分。本次胶片修订没有生成新的预构建包或性能数据。

## 5. 关于公平比较

课程默认采用“两条内部对照证据链”，而不要求跨系统绝对排名：

- Linux 内部：empty/function/vDSO/raw syscall/socket message size；
- seL4 内部：same/different VSpace、message length、fastpath on/off。

如果希望做跨系统绝对比较，需要额外提供：

1. 相同 CPU 和频率状态；
2. Linux 与 seL4 都裸机运行，或都位于等价 KVM 配置；
3. 语义相近的 client/server 往返协议；
4. 相同计时来源及其序列化说明；
5. 对 Linux 安全缓解、seL4 配置和虚拟化开销的完整记录。

即使满足这些条件，也应把结果表述为“在指定配置下的某条路径”，不能外推为整个操作系统的应用性能。

## 6. 建议评分标准

| 项目 | 比例 | 主要观察点 |
|---|---:|---|
| 方法与配置 | 25% | 运行命令、参数、CPU / profile、版本、操作定义；配对条件一致 |
| 数据与复现 | 25% | L1/L2 图表、seL4 六组记录与四对计算、原始文件及复现步骤 |
| 结果解释 | 25% | 描述实际变化，结合代码或课程机制解释，未验证部分标作推测 |
| 比较范围 | 15% | 批次均值、单向 / 往返及 TCG 口径准确，不越过证据范围 |
| 论文联系与表达 | 10% | 一条论断的出处、自己的数据、条件差异与后续测量 |

可选扩展计入相应项目，总分不超过 100%；核心任务完整可获满分。
不建议为具体周期数设置标准答案。不同处理器、缓解配置和后台干扰都会改变绝对值。应检查证据完整性和解释是否与配置一致；TCG 中连相对变化的方向也不保证代表真实硬件，不应预设其为硬件效应的标准答案。

## 7. 答辩核查项目

- 请学生从 `summary.csv` 定位 L1 表中的一项数值，并说明批次与迭代的区别。
- 请学生结合 `bench_ipc.c` 画出一次 socket 往返的四个收发步骤，指出亲和性设置。
- 请学生从 seL4 JSON 定位一条记录的函数、方向、VSpace、长度与双方优先级。
- 抽查 S3a 或 S3b 的两条原始记录及差值、比值计算。
- 请学生说明哪项机制解释来自代码，哪项仍是推测，以及提出的补充测量能补上什么证据。

## 8. 常见故障

### 缺少 Python `ply`

必须通过 `sel4/setup.sh` 创建的 venv 构建；该脚本安装 seL4 自带的 `python-deps` 元包。

### 缺少 `protoc`

安装发行版的 `protobuf-compiler`，或使用提供的 Dockerfile。

### QEMU 报 PCID/FSGSBASE/huge page 不支持

说明使用了默认硬件配置启动 TCG。重新执行：

```bash
./sel4/build.sh tcg on
./sel4/run.sh tcg on
```

### 串口出现许多 `Failed to allocate object`

自动脚本以 JSON 结束标志为终止条件，因此应检查 `END JSON OUTPUT`、可解析的 `results.json` 和非空的 `ipc.csv`。手工继续运行时才要求看到 `All is well in the universe`。不要用简单的 `grep Failed` 判定实验失败。

### `sched_setaffinity` 返回 Invalid argument

学生指定了容器或作业系统不允许的 CPU。取消 `CPU` 环境变量，脚本会从 `Cpus_allowed_list` 选择第一个可用核。
