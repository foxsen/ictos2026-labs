# 可选实验：seL4 IPC

本项不属于 Linux 基本要求，也不替代共享内存进阶实现。只有选择本项才需要 QEMU、seL4 镜像或源码构建环境。以下命令均在 `kernel-boundary/` 目录执行。

## 环境与运行

### 1. 教师提供预构建包时

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

### 2. 从源码构建时

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

### 3. 选出六组指定结果

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

### 4. 完成 S1–S3 对照表

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


## 可选附录提交

按主报告模板的可选附录填写：两份环境与 manifest、日志/JSON/CSV、六组统计和 S1–S3 计算。Linux 与 seL4 单位及语义不同，不做绝对性能排名。TCG 的相对变化也不用于定量推断真实硬件的 Cache/TLB 成本。

源码构建依赖见 `environment/Dockerfile`；可直接使用 `./environment/build-container.sh` 和 `./environment/container-shell.sh` 提供的完整环境。
