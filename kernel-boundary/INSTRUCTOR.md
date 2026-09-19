# 教师准备与授课说明

## 1. 推荐实施方式

建议两周、2–3 人一组：

- 第一周完成 Linux 实验、理解 raw syscall/vDSO/IPC 的区别；
- 第二周完成 seL4 fastpath 对照、可比性审计和论文论断复核；
- 课堂或答辩时随机改变一个参数，要求小组重新运行并解释。

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
| 实验设计与控制变量 | 25% | 操作定义、亲和性、预热、样本与环境记录 |
| 数据与可复现性 | 25% | 原始数据、manifest、脚本、可重复运行 |
| 机制解释 | 25% | trap、调度、VSpace、消息长度、fastpath |
| 可比性与局限 | 15% | 能否识别语义和环境不等价 |
| 论文论断复核与表达 | 10% | 论断定位、证据边界、图表清晰度 |

不建议为具体周期数设置标准答案。不同处理器、缓解配置和后台干扰都会改变绝对值。可检查的应是方向性关系、证据完整性和解释是否与配置一致。

## 7. 答辩抽查题

- 把 `clock_gettime` 改为 raw syscall 后，为什么变化不能归因于“函数调用开销”？
- 为什么 socket RTT 至少包含四次数据传输系统调用以及调度影响？
- 把 Linux ping-pong 两端放在不同 CPU 后，哪些新变量进入了实验？
- seL4 length 10 为何可能离开最短 fastpath？
- TCG 输出为何不能视为真实 CPU cycles？
- 哪一项原始证据能够证明学生实际使用了指定 seL4 版本？

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
