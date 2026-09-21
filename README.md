# ICTOS 2026 实验

高级操作系统课程的独立实验仓库。课件见 [foxsen/ictos2026](https://github.com/foxsen/ictos2026)。

## 跨越保护边界

[kernel-boundary/](kernel-boundary/) 提供三个层次的任务：

- **基本要求：Linux 测试与报告。** 测量调用与 socket 请求—回复，整理图表并解释结果。
- **进阶实验：共享内存通信。** 学生实现“共享内存传数据、同步机制传通知”，验证正确性并与统一 socket 对照比较。
- **可选实验：seL4 IPC。** 在独立附录中分析地址空间、消息长度和快路径配置。

## 开始实验

```bash
git clone https://github.com/foxsen/ictos2026-labs.git
cd ictos2026-labs/kernel-boundary
# 先阅读 README.md；基础实验：
./scripts/run-linux.sh
```

基本和进阶部分需要 Linux、C11 编译器、make 和 Python 3。seL4 环境仅用于选做部分。

- [学生指导书](kernel-boundary/README.md)
- [进阶要求与接口说明](kernel-boundary/ADVANCED.md)
- [分层报告模板](kernel-boundary/report-template.md)
- [可选 seL4 指导](kernel-boundary/sel4/README.md)
- [教师准备与学生包生成](kernel-boundary/INSTRUCTOR.md)

进阶实验的共享内存通信函数有意留空。提供的测量框架、socket 对照及检查工具帮助学生验证自己的实现。

## 仓库来源

本仓库从课件仓库提交 `83118d5` 的 `labs/` 目录拆分，保留该目录的三次相关提交。原 `labs/` 的内容现在位于仓库根目录；实验命令在 `kernel-boundary/` 中执行。结果、编译产物及本地缓存不纳入版本管理。
