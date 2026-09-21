# 教师准备与授课说明

## 1. 分层目标与实施

- **基本要求：Linux 测试与报告。** 已给可运行程序，要求读懂调用和收发流程，完成 L1、L2 图表、机制解释及复现材料。
- **进阶实验：学生实现共享内存通信并评价。** 给统一接口、socket 对照、计时与检查框架；请求、接收、回复三个函数留空。考查缓冲区所有权、内存顺序、等待 / 唤醒、受控比较与研究结论。
- **可选实验：seL4。** 环境与操作独立，作为选做附录，不替代共享内存实现，不作为 Linux 部分的先决条件。

建议两周、2–3 人一组：第一周做 Linux 基础并提交进阶协议草图；第二周实现、检查、测量和分析。只完成基本部分的学生交 A 部分即可。seL4 按兴趣另行安排。不要求假设卡，也不预设共享内存一定更快。

## 2. 发放学生 starter

```bash
# 本目录执行；输出不能已存在。
./teacher/package-starter.sh
```

生成 teacher/kernel-boundary-starter.tar.gz，包含指导书、分层报告模板、基础程序、进阶框架与待实现骨架、框架测试、可选 seL4 脚本及 SHA256SUMS。打包排除测量结果、编译产物、源码缓存和教师说明，不携带学生答案。学生修改前先校验 SHA256SUMS。

Linux 基本 / 进阶只需 C11 编译器、make、Python 3；不需要 Python 第三方包或 QEMU。进阶最好提供至少两个允许使用的逻辑 CPU，记录是否 SMT 兄弟；只有一个 CPU 时先完成同核，再安排另一台机器补做不同 CPU。

## 3. 课前检查与学生验收

```bash
SYSCALL_SAMPLES=3 SYSCALL_ITERATIONS=1000 \
IPC_SAMPLES=3 IPC_ITERATIONS=50 \
./scripts/run-linux.sh results/linux-smoke
make -C linux test
python3 scripts/run-advanced.py --mode check --transport socket
# 原始 starter 的以下命令应失败，并提示 STUDENT TODO：
python3 scripts/run-advanced.py --mode check --transport shm
```

框架测试验证 socket 同 / 异核、不同长度、CSV 汇总，注入损坏载荷、旧序号和传输错误，并检查超时后整个进程组被终止。它不是学生协议的验收。学生实现后须运行默认 check，再运行 bench；后者也先跑全部检查。不得通过跳过校验或替换共同测量程序来获取结果。

验收时阅读状态机及源码，重点检查缓冲区复用、发布 / 读取次序与丢失唤醒。运行检查不能证明所有交错正确，可随机改变 seed、非标准长度和 CPU 配置。纯忙等不满足此进阶的阻塞通知要求；可在完成阻塞版本后研究混合自旋。

计时与正确性扰动分开：检查逐次变化并验证数据，计时采用固定载荷、递增序号、每批最后一次回复验证。两种传输共用 8 B 序号头、进程私有缓冲区和 echo 服务。要求学生用同框架的 socket 作对照，不能复用基础 L2 数据。报告仍需说明实际复制次数，不能默认共享内存等于零复制。

## 4. 评分与答辩

建议基本 60 分、进阶 40 分，seL4 作为独立选做反馈。

| 项目 | 分值 | 验收重点 |
|---|---:|---|
| 基础方法与复现 | 20 | CPU、参数、环境、命令与原始数据 |
| 基础数据与统计 | 20 | L1/L2、曲线、差值 / 比值与批次均值口径 |
| 基础机制与范围 | 20 | 源码调用方式、socket 往返、解释依据与局限 |
| 进阶实现与正确性 | 20 | 实现、状态机、内存顺序、检查记录、错误处理 |
| 进阶对照与评价 | 20 | 相同操作下同 / 异核对照、长度变化、成本与论文联系 |

不按速度或倍率评分；正确的较慢实现、范围有限但证据完整的结论同样可得满分。未完成代码不应因写出预期曲线而获得正确性分。

答辩可让学生定位原始数据中的一项统计，画出 socket 和自己的 SHM 一次往返，解释一个缓冲区何时可复用，以及通知先到时为何不会死锁。学生应能说明哪个成本由代码确认、哪个仍是推测；不要求从总延迟里强行分离所有成本。

## 5. 可选 seL4 准备

具体学生步骤见 sel4/README.md。教师可以提供预构建包：

```bash
./sel4/setup.sh
./sel4/build.sh tcg on
./sel4/build.sh tcg off
./teacher/package-prebuilt.sh tcg
```

统一容器仍可通过 ./environment/build-container.sh 构建，主要服务可选 seL4 环境。学生拿到预构建包后不必重新编译内核。可另发带来源和环境的示例结果，不能让学生把样例冒充本人测量。

历史基线曾在 x86_64 Ubuntu 24.04、GCC 13.3、QEMU 8.2 下完成 TCG fastpath-on 启动及 JSON 解析；manifest 16.0.0 / 240919f93cc5d3546d5371de2fc4b12b92c077a6。本轮改造没有重新验证 seL4 镜像。课前仍应对所发 on/off 镜像实际运行，核对 END JSON OUTPUT、可解析 JSON 及六组指定记录。

仅取普通 seL4_Call、client->server，同空间 0、异空间 0、异空间 10 words，on/off 各三组；从 JSON 核对优先级。数据是校正计时开销后的单向 IPC。短 / 长消息还可能改变基准路径，不能视为纯复制成本。TCG 不支持把结果定量归因为真实 CPU Cache/TLB，也不与 Linux 纳秒作绝对排名。

## 6. 常见故障（seL4 为可选环境）

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
