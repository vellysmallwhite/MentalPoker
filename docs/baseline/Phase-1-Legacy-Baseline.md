# 阶段 1：Legacy 工程基线报告

## 1. 结论

截至 2026-08-24，仓库已经具备可重复构建和自动判定的四节点 v1
基线。固定的 release 配置连续运行三轮均通过，ASan/UBSan 配置运行一轮
通过。这个结果只证明现有 legacy 流程在受控环境中可构建并能完成一次
可观察的旧协议手牌流程，不是密码学、安全性、共识正确性或真钱可用性的
证明。

阶段 1 不新增 v1 协议能力，也不承诺 v1 wire/state compatibility。v2 的安全
边界、目标架构和后续阶段顺序不属于本基线报告的交付范围。

## 2. 可复现环境

| 项目 | 固定值或实测值 |
|---|---|
| Builder/runtime | Ubuntu 24.04，镜像 digest `sha256:33ceb71981b602c1a7443a53469e4dba065f7503eab3078a2d7a57a2ab987517` |
| APT snapshot | `20260810T000000Z` |
| C++ | C++20，GCC 13.3.0 |
| CMake / Ninja | CMake 3.28.3 / Ninja 1.11.1 |
| Boost / GMP | Boost 1.83.0 / GMP 6.3.0 |
| Python | 3.12.3（项目约束为 3.12） |
| Rust | 1.98.0 minimal；阶段 1 尚无 Rust target |
| 容器权限 | player 和 lobby 均以 UID/GID `10001:10001` 非 root 运行 |

Dockerfile 同时固定基础镜像 digest 和 Ubuntu snapshot。APT 仅在安装
`ca-certificates` 的 bootstrap 步骤暂时关闭 TLS peer verification，随即删除该
配置；后续依赖安装恢复证书校验并继续由 Ubuntu 仓库签名校验保护。

## 3. 基线门禁及证据

### 3.1 构建与静态入口

- `ci-release` 和 `asan-ubsan` 使用相同的 `legacy_v1_smoke` CMake target；
- 真实依赖由 CMake 发现并链接，包括 Boost.System、Boost.Thread、
  `Threads::Threads`、GMP/GMPXX 和 jsoncpp；
- CTest 的 `legacy_v1_missing_environment` 检查缺少必需环境变量时必须
  fail closed，并验证诊断文本；
- runtime 镜像执行 `ldd`，任何 `not found` 都使镜像构建失败；
- Compose 配置不挂载源码，构建上下文由 `.dockerignore` 限定为构建文件和
  `src/`。

### 3.2 四节点 smoke

每轮启动一个 legacy lobby 和四个 player，逐个等待入桌，再要求每个 player
都出现以下阶段标记：

1. 成功加入房间；
2. 进入加密阶段；
3. 完成牌堆共识；
4. 生成本地手牌；
5. 完成最终赢家共识。

runner 对超时、容器提前退出、缺少阶段标记、密钥/人数文件打开失败、异常
终止，以及 ASan、LeakSanitizer、UBSan 诊断返回非零。2026-08-24 的本地
Docker Desktop（Linux arm64）实测结果为：

| 配置 | 轮次 | 结果 | 耗时 |
|---|---:|---|---:|
| `ci-release` | 1 | pass | 53.491 s |
| `ci-release` | 2 | pass | 53.411 s |
| `ci-release` | 3 | pass | 53.137 s |
| `asan-ubsan` | 1 | pass | 53.595 s |

可复现命令：

```sh
cmake --preset ci-release
cmake --build --preset ci-release
ctest --preset ci-release
python3 -m unittest discover -s tests/legacy -p 'test_*.py'
python3 tests/legacy/run_smoke.py --preset ci-release --repeat 3 --timeout 180
python3 tests/legacy/run_smoke.py --preset asan-ubsan --repeat 1 --timeout 180 \
  --artifact-dir artifacts/legacy-smoke-sanitized
```

原始日志可能含手牌或大整数，不作为 CI artifact 保存。runner 仅保存脱敏日志
和 `summary.json`；`artifacts/` 被 Git 忽略。

## 4. 为稳定基线所做的最小兼容修复

- 为 C++20 编译补齐标准库和系统头文件；
- showdown READY 增加幂等保护和一次有界回送，避免到达顺序导致永久等待；
- 同一 peer 的发送被串行化，避免多个异步写交叠破坏 JSON frame；
- 接收端在多条以 NUL 分隔的消息被 TCP 合并时复用同一 stream buffer，避免
  读取第一帧后丢失后续完整帧；
- 非 JSON object 的 wire frame 被显式拒绝，避免 jsoncpp 类型异常终止进程。

这些修复保留现有 NUL 分隔 JSON wire format 和 legacy state 语义。它们仅用于
建立可重复迁移基线，不能替代 v2 的签名命令、ABCI、确定性 FSM 或规范编码。

## 5. 已知编译告警

当前目标保留警告但尚未以 `-Werror` 构建，主要包括：

- signed/unsigned 比较；
- 构造函数成员初始化顺序；
- aggregate 缺失字段初始化；
- 未覆盖 enum 分支和未使用参数/局部变量。

这些告警在阶段 1 不隐藏，也不解释为已解决。后续迁移新组件时应使新代码
满足更严格门禁；legacy 文件最终按阶段 14 删除。

## 6. v1 已知边界与非目标

- 使用共享 `/src/key`、旧 SRA 相关密码学和未审计实现；
- 使用自定义、未签名的共识消息和 winner consensus，不具备 CometBFT 的安全
  属性；
- 没有持久化 application state、崩溃恢复、状态同步或确定性 `app_hash`；
- 旧 `GameEngine` 不是完整且可验证的无限注德州 FSM；
- 没有 v2 身份签名、sequence replay protection、threshold DKG、可验证洗牌、
  HPKE 私密 share 或授权 reveal；
- 四节点测试只验证正常路径，不覆盖 Byzantine、分区、资源耗尽或秘密恢复；
- 旧十节点脚本不是阶段 1 门禁，仍是未验证的 legacy utility。

因此该基线仅用于持续迁移和回归定位，禁止用于真钱、代币或其他高价值场景。

## 7. 退出条件状态

| 退出条件 | 状态 |
|---|---|
| 干净 checkout 可用 documented preset 构建 | 满足 |
| 本地与 Ubuntu CI 使用同一 CMake target | 满足 |
| 四节点 smoke 连续运行并稳定判定 | 满足，release 3/3 |
| ASan/UBSan 能构建、启动并完成 smoke | 满足，1/1 |
| 构建不依赖未声明的工作区文件 | 满足；构建上下文显式限定 |
| 不改变 v1 wire/state 语义 | 满足；仅做构建与 liveness 兼容修复 |

阶段 2 可以此报告和 CI workflow 为入口开始实现规范类型、编码和身份层；不得
把本报告中的 legacy target 提升为 v2 production path。
