## 1. 类的定义与核心变量说明

本项目采用分层面向对象设计，将评测门面、仓库控制器、数据模型与算法引擎解耦：

- **`SomeObj`（`SomeObj.h` / `SomeObj.cpp`）**
  - **职责**：对接 `main.cpp` 的命令门面类，保持评测接口签名不变。
  - **变量与接口**：不包含持久化成员变量，内部仅将各命令参数转发给 `Repository` 类的对应业务方法执行。
- **`Repository`（`Repository.h` / `Repository.cpp`）**
  - **职责**：仓库顶层控制器，负责校验工作目录状态、管理 `.gitlite` 路径结构及调度所有 Gitlite 命令逻辑。
  - **静态变量**：定义 `.gitlite` 根目录、`HEAD`、`index`、`commits/`、`blobs/`、`refs/heads/`、`refs/tags/` 以及远程相关（`remotes/`、`refs/remotes/`）的标准路径常量。
  - **辅助接口**：包含 `getCurrentBranch()`、`getCurrentCommitId()` 用于解析当前 `HEAD` 状态；包含 `resolveRevision()` 统一处理标签名与 Commit ID 前缀的解析；包含 `checkoutSnapshot()` 用于复用快照同步逻辑；包含 `getBranchRefPath()` 统一处理本地与远程分支的路径解析。
- **`Commit`（`Commit.h` / `Commit.cpp`）**
  - **职责**：抽象单次提交的元数据与文件快照，提供日志格式化与对象持久化接口。
  - **实例变量**：`std::string message`（提交信息）、`std::time_t timestamp`（Unix 时间戳）、`std::vector<std::string> parents`（父提交 ID 列表，支持 0 至 2 个父提交）、`std::map<std::string, std::string> blobs`（文件名到 Blob SHA-1 的有序映射）及 `std::string id`（当前提交的 40 位 SHA-1 哈希值）。
  - **核心接口**：提供 `std::string info() const` 方法，将原本直接打印到控制台的逻辑内聚为返回格式化字符串，方便 `log`、`global-log` 和 `show` 命令复用。
- **`Blob`（`Blob.h` / `Blob.cpp`）**
  - **职责**：封装单个工作文件的版本快照，按原始字节存取文件内容并计算 SHA-1。
  - **实例变量**：`std::string id`（40 位 SHA-1 ID）与 `std::string content`（二进制安全的原始文件字节流）。
- **`StagingArea`（`StagingArea.h` / `StagingArea.cpp`）**
  - **职责**：管理暂存区的待添加与待删除状态，支持读取、修改、清空与落盘。
  - **实例变量**：`std::map<std::string, std::string> added`（暂存待添加的文件名到 Blob ID 映射）与 `std::set<std::string> removed`（暂存待删除的文件名集合）。
- **`IgnoreMatcher`（`IgnoreMatcher.h` / `IgnoreMatcher.cpp`）**
  - **职责**：解析工作目录根目录下的 `.gitliteignore` 文件，判断指定未跟踪文件是否被忽略。
  - **实例变量**：`std::vector<Rule> rules`（按行保存解析后的通配规则、以及是否带有 `!` 反选标记）。
- **`DiffEngine`（`DiffEngine.h` / `DiffEngine.cpp`）**
  - **职责**：实现 Bonus-2（`diff` 与 `show`）的行级差异对比与格式化输出。
  - **变量与接口**：无实例状态，提供静态方法负责按 `\n` 切分行、运行 LCS 动态规划、合并上下文 Hunk 并生成标准 diff 文本。

## 2. 类的工作原理、核心算法与边界情况处理

### 2.1 `.gitliteignore` 规则解析与匹配（`IgnoreMatcher`）

- **语法解析**：逐行读取规则，跳过空行与未转义的 `#` 注释行，处理行首 `!` 反选标记与前导 `/`，并识别 `\` 转义字符（如 `\#`、`\!`、`\?`、`\*`）。
- **通配符匹配与优先级**：使用动态规划匹配普通字符、`?`（任意单字符）和 `*`（任意长度字符序列）；对同一文件顺序评估所有规则并以最后一条匹配的规则为准。
- **跟踪状态豁免**：已被当前提交跟踪或已暂存待添加的文件直接豁免忽略规则；若当前提交跟踪的文件被暂存待删除后又在工作区重建同名文件，在 `status` 中仍强制作为 `Untracked Files` 显示。

### 2.2 时间戳格式化与本地化输出
- **时间显示**：所有涉及日志输出的命令均调用 `std::localtime` 将 `time_t` 转换为本地时间结构体 `std::tm`，再通过 `std::strftime` 格式化为 `EEE MMM dd HH:mm:ss yyyy Z`。
- **强制英文环境**：在输出前调用 `std::setlocale(LC_TIME, "C")`，防止评测机的中文 `locale` 导致星期和月份输出变成中文，确保输出格式唯一性。

### 2.3 Revision 解析算法（`resolveRevision`）

- **解析优先级**：接收标签名、完整提交 ID 或前缀字符串；首先在 `refs/tags/` 中查找是否存在同名标签，若存在则直接返回对应提交 ID。
- **前缀匹配与平局决胜**：若非标签名，则遍历 `commits/` 目录下的所有提交文件名进行前缀匹配；当存在多个提交匹配同一前缀时，返回完整提交 ID 字典序最小者，无匹配则报错 `No commit with that id exists.`。

### 2.4 标签管理（`tag`）
- **合法性校验**：标签名必须严格匹配正则 `[A-Za-z0-9][A-Za-z0-9._-]*`，否则报 `Incorrect operands.`。
- **三种形态**：
  - **无参调用**：读取 `refs/tags/` 目录下所有文件名（自带字典序），解析内容并输出 `[tag name] [commit ID]`。
  - **单参调用**：在 `refs/tags/` 下新建文件，写入当前 `HEAD` 指向的提交 ID。
  - **双参调用**：调用 `resolveRevision` 解析目标 `revision`，将其指向的提交 ID 写入新标签文件。

### 2.5 全局日志的图遍历与排序（`global-log`）
- **可达性收集**：从 `refs/heads/`、`refs/tags/` 以及 `refs/remotes/`（通过 `<dirent.h>` 遍历子目录）读取所有起点 ID，借助 `std::queue` 进行广度优先搜索（BFS）遍历。使用 `std::set` 记录已访问节点，防止在合并分支构成的图中死循环，同时避免了深度过深时的递归栈溢出风险。
- **确定性排序**：收集所有可达的 Commit 对象后，使用 `std::sort` 进行严格自定义排序：优先按 `timestamp` 降序排列（从新到旧）；若时间戳相同，则按 Commit ID 的字典序升序排列。

### 2.6 状态查询的三方交叉比对（`status`）
- **Branches**：读取 `refs/heads/`、`refs/remotes/`（通过 `<dirent.h>` 遍历目录）和 `HEAD`，将所有分支名合并并按字典序输出，当前分支前加 `*`（星号不参与排序）。
- **Staged/Removed Files**：直接加载 `index` 映射和集合，利用 `std::map` 和 `std::set` 的自带字典序特性依次输出。
- **Modifications Not Staged**：
  - 遍历 `StagingArea.added` 和 `HEAD.blobs` 的并集（使用 `std::map` 去重并保序）。
  - **删除判定**：若工作区文件不存在，且（在 `HEAD` 中未暂存删除 或 被暂存添加），标记为 `(deleted)`。
  - **修改判定**：若工作区文件存在，读取其内容并计算 SHA-1，与 `added` 或 `HEAD` 中的期望 ID 比对，不一致则标记为 `(modified)`。
- **Untracked Files**：遍历工作区普通文件，若未被 `HEAD` 跟踪、未暂存待添加、且未被 `IgnoreMatcher` 忽略，则输出；特例处理“被暂存删除但在工作区重建的同名文件”，强制视为未跟踪。

### 2.7 分支管理与安全性检查（`branch` / `rm-branch`）
- **名字校验与命名空间互斥**：标签、分支、远程共享合法性校验。`branch` 与 `add-remote` 在创建时会互相检查对方的命名空间，防止同名冲突。
- **检查顺序**：`rm-branch` 严格按照“存在性检查 -> 当前分支保护检查”的顺序执行，防止因删除当前分支导致 `HEAD` 悬空。
- **引用删除**：物理删除 `refs/heads/<name>`，不涉及工作区及对象库的修改。

### 2.8 快照检出与未跟踪文件冲突检测（`checkoutSnapshot`）

- **代码复用**：统一服务于 `checkout [branch]`、`reset [revision]` 以及 `merge` 的快进（Fast-Forward）模式。
- **冲突预检**：在修改任何工作文件前，先遍历目标提交跟踪的所有文件；若某文件在工作区中存在、未被当前提交跟踪、未在暂存区待添加且未被 `.gitliteignore` 忽略，则立即报错 `There is an untracked file in the way...` 并终止，保证失败操作的原子性。
- **同步流程**：先删除当前提交有而目标提交没有的文件，再将目标提交包含的文件内容由 Blob 对象库中读出并覆写至工作区，最后清空暂存区并落盘。

### 2.9 版本检出与重置（`checkout` 与 `reset`）
- **文件级检出（`checkout -- [file]` / `checkout [revision] -- [file]`）**：仅从当前或目标提交中读取特定文件的 Blob 并覆写工作区，不修改分支指针、HEAD 或暂存区。
- **分支切换（`checkout [branch]`）**：通过 `getBranchRefPath` 验证分支存在且非当前分支后，调用 `checkoutSnapshot` 同步工作区与暂存区，成功后将新的分支名（支持带 `/` 的远程跟踪分支名）写入 `HEAD`。
- **版本重置（`reset [revision]`）**：解析目标提交后，调用 `checkoutSnapshot` 同步工作区，但不切换分支，而是将当前分支所指向的文件引用覆写为目标提交 ID。

### 2.10 分支合并（`merge`）与分割点查找

- **前置检查**：严格按优先级检查“暂存区未提交修改 -> 分支存在性 -> 禁止自身合并”。
- **分割点（Split Point）算法**：先从当前分支头提交出发，沿所有父引用遍历收集全部祖先集合（含自身）；随后从给定分支头提交出发执行广度优先搜索（BFS，按父引用存储顺序入队），首个命中当前分支祖先集合的提交即为分割点。
- **特殊历史处理**：若分割点为给定分支头，输出 `Given branch is an ancestor of the current branch.` 直接结束；若分割点为当前分支头，则调用 `checkoutSnapshot` 执行快进合并并移动当前分支指针。
- **普通合并的三方比对**：取出 `base`（分割点）、`ours`（当前分支）和 `theirs`（给定分支）的快照并集。对每个文件，比较其在这三个提交中的 Blob ID：
  - 若 `ours_id == theirs_id`，或 `theirs_id == base_id`，说明当前分支已是最新或双方未冲突修改，保留工作区原样。
  - 若 `ours_id == base_id` 且 `theirs_id` 非空，将该文件加入待覆写列表。
  - 若 `ours_id == base_id` 且 `theirs_id` 为空，将该文件加入待删除列表。
  - 其余情况（双方修改不一致、一方修改另一方删除等）均视为冲突。
- **未跟踪文件预检与执行**：严格只对待覆写、待删除和冲突列表中的路径进行未跟踪文件冲突预检。随后执行工作区的文件删除、覆写。
- **冲突文件生成与提交**：对于冲突文件，补齐换行符后按 `<<<<<<< HEAD`、`=======`、`>>>>>>>` 格式拼接写入工作区并自动暂存。最后生成包含当前分支头与给定分支头作为双父引用的新提交。更新当前分支指针并清空暂存区，若有冲突则输出 `Encountered a merge conflict.`。

### 2.11 远程管理（`remote`、`push`、`fetch`、`pull`）
- **命名空间互斥**：`add-remote` 时检查本地分支目录，`branch` 时检查远程目录，保证普通本地分支名与远程名共享顶层命名空间。
- **路径解析（`getBranchRefPath`）**：根据分支名是否包含 `/`（如 `R1/master`）判断是本地分支还是远程跟踪分支，分别前往 `refs/heads/` 或 `refs/remotes/` 读取或写入 ID。
- **通信机制（`push` / `fetch`）**：
  - `push`：检查远程分支头是否为本地分支头的祖先（防止覆盖别人的提交），将本地缺失的 commit 和 blob 物理复制到远程仓库目录，更新远程分支指针。
  - `fetch`：将远程仓库的 commit 和 blob 复制到本地，在 `refs/remotes/<remote>/<branch>` 下更新远程跟踪分支指针，不修改工作区与暂存区。
- **拉取合并（`pull`）**：先执行 `fetch`，再进行暂存区检查，最后复用核心的 `mergeWithCommit` 函数，将 `refs/remotes/...` 下的分支合入当前分支。

### 2.12 差异对比与展示（`diff` 与 `show`）
- **底层算法（`DiffEngine`）**：
  - **切分与换行检测**：按 `\n` 逐字符切分文本，保留 `\r` 为普通字符。对每一行记录 `has_newline` 标志，若末尾无换行符，则在输出该行后追加 `\ No newline at end of file`。
  - **唯一性 LCS 动态规划**：构建二维 DP 表求最长公共子序列。在回溯生成编辑序列时，遇到平局（删除和插入得到的 LCS 长度相同时），严格优先选择插入（在倒序数组中优先压入 `+`），反转后表现为优先删除旧行，保证输出唯一性。
  - **上下文与 Hunk 聚合**：利用标记数组（`include`）标出所有变更行及其前后各 3 行的范围。扫描时合并相邻或重叠的区间（间隔不超过 6 行），计算并输出标准的 Hunk 头部 `@@ -旧起始,行数 +新起始,行数 @@`。通过 `std::optional` 区分“不存在的空（`/dev/null`）”与“0 字节空文件”。
- **顶层调度（`Repository`）**：
  - **比较集合确定**：`diff` 命令仅遍历旧提交（当前或指定）跟踪的文件名；`diffBetween` 和 `show` 命令遍历新旧提交文件名的并集并按字典序输出。
  - **严格参数语义**：`diff [rev1] [rev2]` 严格遵守用户输入的顺序，`rev1` 视为旧版本，`rev2` 视为新版本，绝不根据时间戳擅自交换两者的比较基准，确保输出结果确定。
  - **`show` 的职责**：先输出目标提交的格式化 `info()`，再将目标提交与其第一个父提交进行文件对比（若为初始提交则仅输出 `info` 跳过 diff）。

## 3. 类的持久化实现与序列化方案

### 3.1 `.gitlite` 目录结构布局

所有版本控制元数据均持久化保存在工作目录的 `.gitlite/` 子目录下，具体结构如下：

```
.gitlite/
├── HEAD # 文本文件：保存当前活跃分支名（如 "master" 或 "R1/master"）
├── index # 序列化文件：保存 StagingArea 的 added 和 removed 状态
├── commits/ # 目录：存放所有 Commit 对象，文件名为 40 位 SHA-1 ID
├── blobs/ # 目录：存放所有 Blob 对象，文件名为 40 位 SHA-1 ID
├── remotes/ # 目录：每个文件以远程名命名，内容为远程 .gitlite 的路径
└── refs/
├── heads/ # 目录：存放本地分支
├── tags/ # 目录：存放标签
└── remotes/ # 目录：存放远程跟踪分支（如 R1/master）
```

- **分目录存储优势**：将 `commits/` 与 `blobs/` 物理隔离，使 `resolveRevision` 在执行提交 ID 前缀匹配时只需扫描 `commits/` 目录，避免与 Blob 对象哈希发生混淆，同时利用 `Utils::plainFilenamesIn` 可直接获取字典序排列的引用列表。
- **目录兼容处理**：由于 `Utils::plainFilenamesIn` 仅返回普通文件，在读取 `refs/remotes/` 时需要借助 `<dirent.h>` 库，手动识别并深入子目录（如 `R1`）读取具体分支。

### 3.2 序列化与反序列化机制

- **有序容器保证哈希确定性**：`Commit` 的文件映射与 `StagingArea` 的暂存映射均使用有序的 `std::map` 和 `std::set`，避免 `std::unordered_map` 因迭代顺序不确定导致相同提交内容生成不同的 SHA-1 ID。
- **长度前缀序列化（Length-Prefixed Serialization）**：由于提交消息可能包含空格或换行符，序列化 `Commit` 和 `StagingArea` 时采用“字段长度 + 换行符 + 原始字节内容”的格式（配合 `std::ostringstream` 与 `std::istringstream` 读写），确保反序列化时不会因特殊字符产生解析歧义。
- **对象落盘与读取流程**：
  - **`Blob` 对象**：直接将工作文件的原始字节流作为内容，加上类型标识计算 40 位 SHA-1 后，使用 `Utils::writeContents` 写入 `.gitlite/blobs/<id>`，若同名文件已存在则跳过写入以实现内容寻址去重。
  - **`Commit` 对象**：先将时间戳、父提交列表、消息和 `blobs` 映射按固定顺序序列化为字节流，调用 `Utils::sha1` 计算得到提交 ID，再写入 `.gitlite/commits/<id>`。
  - **`StagingArea` 对象**：每次执行 `add`、`rm`、`commit`、`checkout`、`reset`、`merge` 时，在命令开始时从 `.gitlite/index` 反序列化加载状态，并在命令成功结束前序列化覆写回 `.gitlite/index`。
- **状态持久化与保护**：涉及修改暂存区的命令（如 `add`, `rm`, `commit`）在开始时会调用 `StagingArea::load()` 读取 `index`，只在命令成功执行的末尾调用 `save()` 落盘。若中途遇到规范要求的错误并调用 `Utils::exitWithMessage()` 终止进程，则不会覆写 `index`，保证了失败操作的原子性。