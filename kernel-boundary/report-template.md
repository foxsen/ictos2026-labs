# 跨越保护边界实验报告

按 L1、L2、S1–S3 和论文联系填写。运行前记录环境与参数，运行后填写结果；不另交假设卡。所有数值注明原始文件及对应记录。

## 1. 基本信息与环境

- 姓名 / 学号，小组成员与分工：
- 实验代码版本及本地修改：
- CPU、Linux 内核、编译器、是否使用容器：
- Linux 固定 CPU、频率策略、安全缓解状态（引用 environment.txt）：
- seL4 profile、manifest、预构建包名称及校验结果，或源码构建命令：
- 两个 seL4 镜像的其他配置是否相同：

## 2. 运行参数与原始文件

| 项目 | 默认值 | 实际值 / 文件位置 |
|---|---|---|
| Linux 调用 | 101 批 × 10000 次；固定同一个允许的 CPU | |
| Linux socket | 每长度 51 批 × 1000 次往返；双进程同核 | |
| 消息长度 | 1、64、1024、4096 B | |
| seL4 | tcg；fastpath on / off 各一次完整基准运行 | |
| Linux 输出 | syscall.csv、ipc.csv、summary.csv、environment.txt、stderr | |
| seL4 on 输出 | serial.log、results.json、ipc.csv、pinned-manifest.xml | |
| seL4 off 输出 | serial.log、results.json、ipc.csv、pinned-manifest.xml | |

记录预热方式和任何参数修改。默认 Linux 调用预热为每类 `iterations/10+1` 次，socket 每种长度预热 50 次往返。异常运行保留日志；如重新运行，说明原因和采用哪一份数据。

## 3. L1：Linux 调用耗时

填入 `summary.csv` 的对应统计。单位为 **ns/操作，统计对象为批次均值**，不扣除基线。

| benchmark | 批次数 | median | mean | P95 |
|---|---:|---:|---:|---:|
| empty_loop | | | | |
| ordinary_function | | | | |
| raw_getpid | | | | |
| clock_gettime_vdso_candidate | | | | |
| raw_clock_gettime | | | | |

- 原始文件及记录定位：
- 两种 clock_gettime 的 median 差值（raw − libc）及比值（raw / libc）：
- 对照 `bench_syscall.c`，说明普通函数、raw syscall 与 libc 取时接口的调用方式：
- 结合数据说明差异。没有动态路径验证时，将 libc 调用标作“vDSO 候选路径”：

## 4. L2：socket 消息长度

插入“消息字节数—median ns/往返”曲线，并填表。一次操作包括客户请求与服务回复的完整往返。

| 消息长度（B） | 批次数 | median ns/往返 | P95 ns/往返 |
|---:|---:|---:|---:|
| 1 | | | |
| 64 | | | |
| 1024 | | | |
| 4096 | | | |

- 图与原始数据位置：
- median 差值（4096 B − 1 B）及比值（4096 B / 1 B）：
- 曲线趋势：
- 对照 `bench_ipc.c`，说明一次往返的收发过程，以及消息传输、系统调用和调度对耗时的影响；无法单独量化的成本明确标出：

## 5. seL4：六组记录与 S1–S3 对照

### 5.1 六组统计

仅选 `One way IPC microbenchmarks` 中普通 `seL4_Call`、`client->server` 的记录。核对 JSON 中双方优先级一致；不选 `(FPU)` 行。单位为 **TCG cycle，单向 IPC，经基准计时开销校正**。

- JSON 中 Client Prio / Server Prio 的实际值：
- 结果来源与所选记录定位：

| fastpath | same_vspace | IPC length（words） | samples | median | Q1 | Q3 |
|---|---|---:|---:|---:|---:|---:|
| on | True | 0 | | | | |
| on | False | 0 | | | | |
| on | False | 10 | | | | |
| off | True | 0 | | | | |
| off | False | 0 | | | | |
| off | False | 10 | | | | |

### 5.2 配对计算

A、B 取对应记录的 median；A 为零时比值填“不适用”，保留原始值。

| 对照 | A 配置 | B 配置 | A | B | B−A | B/A |
|---|---|---|---:|---:|---:|---:|
| S1 | on / 同空间 / 0 | on / 异空间 / 0 | | | | |
| S2 | on / 异空间 / 0 | on / 异空间 / 10 | | | | |
| S3a | on / 异空间 / 0 | off / 异空间 / 0 | | | | |
| S3b | on / 异空间 / 10 | off / 异空间 / 10 | | | | |

### 5.3 结果说明

每项先描述变化，再结合机制解释；未验证的解释注明是推测。

- S1：同 / 异地址空间的变化，以及 TCG 下允许解释的范围：
- S2：短 / 长消息基准配置的变化；不将差值直接等同于纯复制成本：
- S3a/S3b：关闭快路径对两种长度的影响，结合快路径适用条件说明；区分“开关开启”和“实际命中”：

## 6. 三项比较范围说明

每行写明可以怎样比较以及限制，不用只填“可比 / 不可比”。

| 对象 | 共同点与关键区别 | 允许报告的比较 |
|---|---|---|
| Linux libc / raw clock_gettime | 相同取时任务，调用路径不同 | |
| Linux socket / TCG seL4 IPC | 纳秒 / 仿真 cycle；完整往返 / 单向 IPC；语义不同 | |
| seL4 fastpath on / off | 相同版本与 profile、配对消息配置 | |

补充说明：Linux 的 P95 描述批次均值波动；TCG 结果不用于定量推断真实 CPU Cache/TLB 成本，也不用于跨系统绝对排名。

## 7. 论文联系（不超过一页）

- 论文、章节或图表位置：
- 用自己的话概括一条与实验相关的论断：
- 引用本报告的一组数据，写明其操作和环境：
- 数据能帮助理解的部分，以及与论文实验条件的差异：
- 尚不能确定的一点；补充测量应固定什么、改变什么、记录什么：

## 8. 可选扩展（未做可略）

- 比较对象、参数与固定条件：
- 方法、原始数据位置与结果：
- 说明路径验证与正式计时是否分开：

## 9. 结论与复现

用三至五条结论归纳实际观察，每条注明适用的环境或配置。

附图表生成脚本或可编辑表格，列出环境准备、基准运行、汇总及制图命令：

```bash
# 从 labs/kernel-boundary 目录开始。
# 如使用预构建包，先列出包名、解压及校验步骤。
./scripts/run-linux.sh
./sel4/run.sh tcg on
./sel4/run.sh tcg off
# 继续填写生成报告图表所需命令或表格操作。
```

## 提交检查

- [ ] L1 五行、L2 四个长度和曲线齐全。
- [ ] seL4 六组统计、四对计算和解释齐全。
- [ ] 原始数据、环境、版本、命令与代码修改已附。
- [ ] 论文联系有具体出处和自己的数据。
- [ ] 图表单位、单向 / 往返、批次均值与 TCG 口径正确。
