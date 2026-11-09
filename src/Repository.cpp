#include "../include/Repository.h"
#include "../include/Utils.h"
#include "../include/Commit.h"
#include "../include/IgnoreMatcher.h"
#include "../include/StagingArea.h"
#include "../include/Blob.h"
#include "../include/DiffEngine.h"

#include <ctime>
#include <iostream>
#include <clocale>
#include <algorithm>
#include <queue>
#include <utility>
#include <set>
#include <optional>
#include <dirent.h>

// 定义标准路径
const std::string Repository::GITLITE_DIR = ".gitlite";
const std::string Repository::HEAD_FILE = ".gitlite/HEAD";
const std::string Repository::INDEX_FILE = ".gitlite/index";
const std::string Repository::COMMITS_DIR = ".gitlite/commits";
const std::string Repository::BLOBS_DIR = ".gitlite/blobs";
const std::string Repository::REFS_DIR = ".gitlite/refs";
const std::string Repository::HEADS_DIR = ".gitlite/refs/heads";
const std::string Repository::TAGS_DIR = ".gitlite/refs/tags";
const std::string Repository::REMOTES_DIR = ".gitlite/remotes";
const std::string Repository::REMOTE_REFS_DIR = ".gitlite/refs/remotes";

// 初始化仓库
void Repository::init() {
    // 仓库已经初始化, 直接退出
    if (Utils::isDirectory(GITLITE_DIR)) {
        Utils::exitWithMessage("A Gitlite version-control system already exists in the current directory.");
    }

    // 创建目录
    Utils::createDirectories(COMMITS_DIR);
    Utils::createDirectories(BLOBS_DIR);
    Utils::createDirectories(HEADS_DIR);
    Utils::createDirectories(TAGS_DIR);
    Utils::createDirectories(REMOTES_DIR);
    Utils::createDirectories(REMOTE_REFS_DIR);

    // 创建并保存初始提交
    Commit initial("initial commit", 0, {}, {});
    initial.save(Utils::join(COMMITS_DIR, initial.getId()));

    // 创建 master 分支
    Utils::writeContents(Utils::join(HEADS_DIR, "master"), initial.getId());

    // 设置 HEAD
    Utils::writeContents(HEAD_FILE, "master");
}

// 获取当前分支名
std::string Repository::getCurrentBranch() {
    return Utils::readContentsAsString(HEAD_FILE);
}

// 获取当前 HEAD 指向的 commit ID
std::string Repository::getCurrentCommitId() {
    // 兼容远程
    std::string branch = cleanString(getCurrentBranch());
    std::string branch_path = getBranchRefPath(branch);
    return cleanString(Utils::readContentsAsString(branch_path));
}

// 获取当前 Commit 对象
Commit Repository::getCurrentCommit() {
    std::string commit_id = getCurrentCommitId();
    return Commit::read(Utils::join(COMMITS_DIR, commit_id));
}

// 清理字符串最后可能带有的换行符
std::string Repository::cleanString(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

// 用于 checkout 和 reset 的把工作区和暂存区彻底变成目标提交的模样
// 的私有辅助函数
void Repository::checkoutSnapshot(const Commit& target) {
    // 初始化
    Commit head = getCurrentCommit();
    IgnoreMatcher matcher;
    matcher.LoadFromWorkingDirectory();
    StagingArea staging;
    staging.load(INDEX_FILE); 

    // 1. 全局冲突预检
    const auto& target_blobs = target.getBlobs();
    for (const auto& [fn, id]: target_blobs) {
        // 如果工作区有这个文件, 但它既不在当前 HEAD 中, 
        // 也不在暂存区中, 且未被 ignore, 则不能覆写
        if (Utils::isFile(fn) && !head.getBlobs().count(fn) &&
            !staging.isAdded(fn) && !matcher.isIgnored(fn)) {
                Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
            }
    }

    // 2. 删除: 当前分支有, 但目标分支没有的文件
    const auto& head_blobs = head.getBlobs();
    for (const auto& [fn, id]: head_blobs) {
        if (!target_blobs.count(fn) && Utils::isFile(fn)) {
            std::remove(fn.c_str());
        }
    }

    // 3. 覆盖 / 生成: 目标分支的所有文件
    for (const auto& [fn, id]: target_blobs) {
        // 利用 Blob 来覆写
        Blob blob = Blob::read(Utils::join(BLOBS_DIR, id));
        Utils::writeContents(fn, blob.getContent());
    }

    // 4. 操作成功后, 清理暂存区
    staging.clear();
    staging.save(INDEX_FILE);
}

// 用于 merge 的收集某提交的所有父提交的私有辅助函数
std::set<std::string> Repository::getAncestors(const std::string& start_id) {
    std::set<std::string> visited;
    std::queue<std::string> q;
    q.push(start_id);
    visited.insert(start_id);
    while (!q.empty()) {
        auto curr = q.front(); 
        q.pop();
        Commit c = Commit::read(Utils::join(COMMITS_DIR, curr));
        for (const auto& p : c.getParents()) {
            if (!visited.count(p)) {
                visited.insert(p);
                q.push(p);
            }
        }
    }
    return visited;
}

// add [file name]
void Repository::add(const std::string& filename) {
    // 检查文件是否存在, 若不存在直接退出
    if (!Utils::isFile(filename)) {
        Utils::exitWithMessage("File does not exist.");
    }

    // 加载暂存区和当前提交
    StagingArea staging;
    staging.load(INDEX_FILE);
    Commit head = getCurrentCommit();

    // 当文件没被提交追踪且不暂存添加时, 加载 .gitliteignore 并匹配
    if (!head.getBlobs().count(filename) && !staging.isAdded(filename)) {
        IgnoreMatcher matcher;
        matcher.LoadFromWorkingDirectory();
        if (matcher.isIgnored(filename)) {
            return;
        }
    }

    // 创建当前文件对应的 Blob 并保存
    std::string content = Utils::readContentsAsString(filename);
    Blob newBlob(content);
    newBlob.save(Utils::join(BLOBS_DIR, newBlob.getId()));

    // 查看当前提交有没有 file; 有的话, 内容有没有区别
    bool inHead = head.getBlobs().count(filename) > 0;
    std::string headBlobId = inHead ? head.getBlobs().at(filename) : "";
    // 当前提交已有 file 且无差异, 取消暂存状态
    if (inHead && headBlobId == newBlob.getId()) {
        staging.unstage(filename);
    }
    // 否则设置为暂存添加状态
    else {
        staging.addFile(filename, newBlob.getId());
    }

    // 保存暂存区状态
    staging.save(INDEX_FILE);
}

// commit [message]
void Repository::commit(const std::string& message) {
    // 前置失败检查: message 为空
    if (message.empty()) {
        Utils::exitWithMessage("Please enter a commit message.");
    }

    // 前置失败检查: 暂存区为空
    StagingArea staging;
    staging.load(INDEX_FILE);
    if (staging.isEmpty()) {
        Utils::exitWithMessage("No changes added to the commit.");
    }

    // 生成新快照: 获取当前提交作为父提交
    Commit head = getCurrentCommit();
    std::map<std::string, std::string> newBlobs = head.getBlobs();

    // 生成新快照: 应用 added 修改
    for (const auto& added_info: staging.getAddedMap()) {
        newBlobs[added_info.first] = added_info.second;
    }

    // 生成新快照: 应用 removed 修改
    for (const auto& removed_name: staging.getRemovedSet()) {
        newBlobs.erase(removed_name);
    }

    // 构建新 Commit 对象
    std::time_t now = std::time(nullptr);
    std::vector<std::string> par = {head.getId()};
    Commit newCommit(message, now, par, newBlobs);

    // 保存新 Commit
    newCommit.save(Utils::join(COMMITS_DIR, newCommit.getId()));

    // 移动当前分支的指针, 即更新当前分支的最新提交
    std::string branch = getCurrentBranch();
    std::string branch_path = getBranchRefPath(branch); 
    Utils::writeContents(branch_path, newCommit.getId());

    // 刷新状态
    staging.clear();
    staging.save(INDEX_FILE);
}

// rm [file name]
void Repository::rm(const std::string& filename) {
    // 加载当前的暂存区和提交跟踪, 判断文件是否暂存待添加, 是否被跟踪
    StagingArea staging;
    staging.load(INDEX_FILE);
    Commit head = getCurrentCommit();
    bool inAdded = (staging.isAdded(filename) > 0);
    bool inHead = (head.getBlobs().count(filename) > 0);

    // 情况 1: 文件既没有暂存待添加, 也没有被当前提交跟踪, 直接退出
    if (!inAdded && !inHead) {
        Utils::exitWithMessage("No reason to remove the file.");
    }

    // 情况 2: 文件已经暂存待添加但尚未被当前提交跟踪, rm 只取消暂存,
    // 工作文件仍然保留
    else if (inAdded && !inHead) {
        staging.unstage(filename);
    }

    // 情况 3: 如果当前提交跟踪该文件, rm 将它标记为待删除, 
    // 并在工作文件仍然存在时将其删除
    else {
        staging.removeFile(filename);
        if (Utils::isFile(filename)) {
            Utils::restrictedDelete(filename);
        }
    }

    // 刷新状态
    staging.save(INDEX_FILE);
}

// log
void Repository::log() {
    // 强制输出英文月份和星期
    std::setlocale(LC_TIME, "C");
    
    std::string curr_id = getCurrentCommitId();
    
    // 只要 ID 不为空, 就继续遍历
    while (!curr_id.empty()) {
        Commit curr_commit = Commit::read(Utils::join(COMMITS_DIR, curr_id));
        std::cout << curr_commit.info();
        
        // 获取父节点, 如果有, 更新 curr_id; 如果没有, 置空结束循环
        const auto& pars = curr_commit.getParents();
        if (!pars.empty()) {
            curr_id = pars[0];
        } else {
            curr_id = "";
        }
    }
}

// 解析 revision 的辅助函数
std::string Repository::resolveRevision(const std::string& rev) {
    // 优先把 revision 作为 tag 解析, 看是否有相应的文件
    std::string tag_path = Utils::join(TAGS_DIR, rev);
    if (Utils::isFile(tag_path)) {
        std::string id = Utils::readContentsAsString(tag_path);
        if (!id.empty() && id.back() == '\n') {
            id.pop_back();
        }
        return id;
    }

    // 其次把 revision 作为 commit ID 解析, 看是否有相应的 ID
    // (开头匹配也可)
    auto commit_ids = Utils::plainFilenamesIn(COMMITS_DIR);
    for (const auto& id: commit_ids) {
        if (id.find(rev) == 0) {
            return id;
        }
    }

    // 如果没找到, 退出
    Utils::exitWithMessage("No commit with that id exists.");
    return "";
}

// 判断 tag 的名字是否合法的私有辅助函数
bool Repository::isValidName(const std::string& tag) {
    // 必须非空且以字母或数字开头
    if (tag.empty() || !std::isalnum(tag[0])) {
        return false;
    }
    // 不能含字母, 数字, _, . 和 - 以外的字符
    for (char c: tag) {
        if (!std::isalnum(c) && c != '_' && c != '.' && c != '-') {
            return false;
        }
    }
    return true;
}

// tag
void Repository::listTags() {
    auto tags = Utils::plainFilenamesIn(TAGS_DIR);
    // 先判断是否有 tag 存在, 若否则不输出任何内容
    if (tags.empty()) {
        return;
    }

    // 否则, 按 tag 字典序输出所有 tag 及其对应的 commit ID
    for (const auto& tag: tags) {
        std::string id = Utils::readContentsAsString(Utils::join(TAGS_DIR, tag));
        if (!id.empty() && id.back() == '\n') {
            id.pop_back();
        }
        std::cout << tag << ' ' << id << '\n';
    }
}

// tag [tag name]
void Repository::tag(const std::string& name) {
    // 判断 tag name 是否合法, 不合法直接退出
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 如果 tag 已经创建, 也退出
    std::string tag_path = Utils::join(TAGS_DIR, name);
    if (Utils::isFile(tag_path)) {
        Utils::exitWithMessage("A tag with that name already exists.");
    }

    // 创建 tag, 加载当前头提交并创建指向 head ID 的 tag 文件
    std::string head_id = getCurrentCommitId();
    Utils::writeContents(tag_path, head_id);
}

// tag [tag name] [revision]
void Repository::tag(const std::string& name, const std::string& rev) {
    // 判断 tag name 是否合法, 不合法直接退出
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 如果 tag 已经创建, 也退出
    std::string tag_path = Utils::join(TAGS_DIR, name);
    if (Utils::isFile(tag_path)) {
        Utils::exitWithMessage("A tag with that name already exists.");
    }

    // 解析 rev 代表的 commit ID 并令 name 指向之
    std::string id = resolveRevision(rev);
    Utils::writeContents(tag_path, id);
};

// global-log
void Repository::globalLog() {
    // 1. 找到所有的起点提交, 包括每个 branch, tag 和 remote
    std::vector<std::string> start_ids;
    auto branches = Utils::plainFilenamesIn(HEADS_DIR);
    for (const auto& b: branches) {
        std::string id = Utils::readContentsAsString(Utils::join(HEADS_DIR, b));
        start_ids.push_back(cleanString(id));
    }

    auto tags = Utils::plainFilenamesIn(TAGS_DIR);
    for (const auto& t: tags) {
        std::string id = Utils::readContentsAsString(Utils::join(TAGS_DIR, t));
        start_ids.push_back(cleanString(id));
    }

    DIR* dir = opendir(REMOTE_REFS_DIR.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string r = entry->d_name;
            // 过滤掉 "." 和 ".." 以及普通文件
            if (r == "." || r == ".." || !Utils::isDirectory(Utils::join(REMOTE_REFS_DIR, r))) {
                continue;
            }
            auto remote_branches = Utils::plainFilenamesIn(Utils::join(REMOTE_REFS_DIR, r));
            for (const auto& b : remote_branches) {
                std::string id = Utils::readContentsAsString(Utils::join(REMOTE_REFS_DIR, r, b));
                start_ids.push_back(cleanString(id));
            }
        }
        closedir(dir);
    }

    // 2. 沿每个提交的所有父引用遍历 (BFS)
    std::set<std::string> visited;  // 已经记录过的 commit
    std::queue<std::string> q;

    // 将每个起点提交去重后加入队列
    for (const auto& id: start_ids) {
        if (visited.find(id) == visited.end()) {
            visited.insert(id);
            q.push(id);
        }
    }

    // 开始遍历, 记录所有 commit
    std::vector<Commit> all_commits;
    while (!q.empty()) {
        std::string curr_id = q.front();
        q.pop();

        // 读取并保存该 commit
        Commit commit = Commit::read(Utils::join(COMMITS_DIR, curr_id));
        all_commits.push_back(commit);

        // 把该 commit 的所有父提交去重并加入队列
        for (const auto& par: commit.getParents()) {
            if (visited.find(par) == visited.end()) {
                visited.insert(par);
                q.push(par);
            }
        }
    }

    // 3. 按照规则将所有 commit 排序并输出
    std::sort(all_commits.begin(), all_commits.end(), [](const Commit& c, const Commit& d) {
        if (c.getTimestamp() != d.getTimestamp()) {
            return (c.getTimestamp() > d.getTimestamp());
        }
        else {
            return (c.getId() < d.getId());
        }
    });

    // 4. 输出所有 commit 的信息
    for (const auto& commit: all_commits) {
        std::cout << commit.info();
    }
}

// status
void Repository::status() {
    // 1. Branches
    // 加载当前已有的所有 branches 和目前活跃的 branch
    std::vector<std::string> all_branches;
    
    auto local_branches = Utils::plainFilenamesIn(HEADS_DIR);
    for (const auto& b: local_branches) {
        all_branches.push_back(b);
    }
    
    DIR* dir = opendir(REMOTE_REFS_DIR.c_str());
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string r = entry->d_name;
            if (r == "." || r == ".." || !Utils::isDirectory(Utils::join(REMOTE_REFS_DIR, r))) {
                continue;
            }
            auto remote_branches = Utils::plainFilenamesIn(Utils::join(REMOTE_REFS_DIR, r));
            for (const auto& b : remote_branches) {
                all_branches.push_back(r + "/" + b);
            }
        }
        closedir(dir);
    }

    // 字典序排序
    std::sort(all_branches.begin(), all_branches.end());
    std::string curr_branch = cleanString(Utils::readContentsAsString(HEAD_FILE));

    // 将所有 branches 按规则输出
    std::cout << "=== Branches ===" << '\n';
    for (const auto& b: all_branches) {
        if (b == curr_branch) {
            std::cout << "*";
        }
        std::cout << b << '\n';
    }
    std::cout << '\n';

    // 2. Staged Files & Removed Files
    // 加载当前暂存区, 按字典序排列后输出 added 和 removed
    // std::map 和 std::set 自带字典序
    StagingArea staging;
    staging.load(INDEX_FILE);
    auto added_map = staging.getAddedMap();
    auto removed_set = staging.getRemovedSet();

    std::cout << "=== Staged Files ===" << '\n';
    for (const auto& [name, id]: added_map) {
        std::cout << name << '\n';
    }
    std::cout << '\n' << "=== Removed Files ===" << '\n';;
    for (const auto& name: removed_set) {
        std::cout << name << '\n';
    }
    std::cout << '\n';

    // 3. Modifications Not Staged For Commit
    // 加载当前头提交, 初始化 std::map 用于存储文件名和修改状态, 自动去重
    // 'M' -> Modified, 'D' -> Deleted
    Commit head = getCurrentCommit();
    const auto head_blobs = head.getBlobs();
    std::map<std::string, char> stages;

    // 读取暂存待添加文件, 比对期望状态 (e_id) 和实际状态 (是否存在, r_id)
    for (const auto& [fn, e_id]: added_map) {
        // (1) 工作区不存在且被暂存添加 -> deleted
        if (!Utils::isFile(fn)) {
            stages[fn] = 'D';
            continue;
        }
        // (2) 工作区存在, 但实际状态与期望状态不符 -> modified
        Blob actual(Utils::readContentsAsString(fn));
        if (actual.getId() != e_id) {
            stages[fn] = 'M';
        }
    }

    // 读取提交追踪文件, 比对期望状态 (e_id) 和实际状态 (是否存在, r_id)
    for (const auto& [fn, e_id]: head_blobs) {
        // (1) 如果文件被暂存, 说明其已经被处理过了, 跳过
        if (added_map.count(fn) || removed_set.count(fn)) {
            continue;
        }
        // (2) 工作区不存在, 被头提交追踪且未暂存待删除 -> deleted
        if (!Utils::isFile(fn)) {
            stages[fn] = 'D';
            continue;
        }
        // (3) 工作区存在, 但实际状态与期望状态不符 -> modified
        Blob actual(Utils::readContentsAsString(fn));
        if (actual.getId() != e_id) {
            stages[fn] = 'M';
        }
    }

    // 将文件名按照字典序排序后输出 (后缀不参与排序)
    std::cout << "=== Modifications Not Staged For Commit ===" << '\n';
    for (const auto& [name, stage]: stages) {
        std::cout << name << ' ' << ((stage == 'M') ? "(modified)" : "(deleted)") << '\n';
    }
    std::cout << '\n';

    // 4. Untracked Files
    // 遍历工作区并筛选文件
    IgnoreMatcher matcher;
    matcher.LoadFromWorkingDirectory();
    std::cout << "=== Untracked Files ===" << '\n';
    for (const auto& fn: Utils::plainFilenamesIn(".")) {
        // 过滤内部隐藏文件
        if (fn == ".gitlite") {
            continue;
        }
        // (1) 特例处理: 如果在 HEAD 里, 且被暂存删除了 (在 removed 里), 
        // 但工作区又出现了, 则必须算作未跟踪
        if (head_blobs.count(fn) > 0 && removed_set.count(fn) > 0) {
            std::cout << fn << '\n';  // 自带字典序排序
            continue;
        }
        // (2) 未被 HEAD 跟踪, 未被暂存, 未被忽略
        if (!head_blobs.count(fn) && !staging.isAdded(fn) && !matcher.isIgnored(fn)) {
            std::cout << fn << '\n';
        }
    }
}

// branch [branch name]
void Repository::branch(const std::string& name) {
    // 先检查 name 是否合法, 不合法直接退出
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 再检查是否存在同名 branch 或 remote, 若存在也退出
    std::string branch_path = Utils::join(HEADS_DIR, name);
    std::string remote_path = Utils::join(REMOTES_DIR, name);
    if (Utils::isFile(branch_path) || Utils::isFile(remote_path)) {
        Utils::exitWithMessage("A branch with that name already exists.");
    }

    // 可以创建新 branch
    std::string head_id = cleanString(getCurrentCommitId());
    Utils::writeContents(branch_path, head_id);
};

// rm-branch [branch name]
void Repository::rmBranch(const std::string& name) {
    // 先检查 name 是否合法, 不合法直接退出
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 再检查是否存在该分支, 若不存在直接退出
    std::string branch_path = Utils::join(HEADS_DIR, name);
    if (!Utils::isFile(branch_path)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }

    // 最后检查是否为当前分支, 若是直接退出
    std::string curr_branch = cleanString(getCurrentBranch());
    if (name == curr_branch) {
        Utils::exitWithMessage("Cannot remove the current branch.");
    }

    // 可以删除该 branch
    std::remove(branch_path.c_str());
}

// checkout -- [file name]
void Repository::checkoutFile(const std::string name) {
    // 加载当前提交并看指定文件是否被跟踪,
    // 若未被跟踪, 则直接退出
    Commit head = getCurrentCommit();
    auto blobs = head.getBlobs();
    if (!blobs.count(name)) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }

    // 否则覆写该文件
    std::string id = blobs[name];
    Blob blob = Blob::read(Utils::join(BLOBS_DIR, id));
    Utils::writeContents(name, blob.getContent());
}

// checkout [revision] -- [file name]
void Repository::checkoutFileInCommit(const std::string& rev, const std::string& fn) {
    // 解析目标提交, 其余同 checkoutFile
    std::string target_commit_id = resolveRevision(rev);
    Commit target_commit = Commit::read(Utils::join(COMMITS_DIR, target_commit_id));
    auto blobs = target_commit.getBlobs();
    if (!blobs.count(fn)) {
        Utils::exitWithMessage("File does not exist in that commit.");
    }

    std::string id = blobs[fn];
    Blob blob = Blob::read(Utils::join(BLOBS_DIR, id));
    Utils::writeContents(fn, blob.getContent());
}

// checkout [branch name]
void Repository::checkoutBranch(const std::string& name) {
    // 兼容远程分支名
    std::string branch_path = getBranchRefPath(name);
    if (!Utils::isFile(branch_path)) {
        Utils::exitWithMessage("No such branch exists.");
    }

    // 指定分支就是当前分支
    if (name == cleanString(getCurrentBranch())) {
        Utils::exitWithMessage("No need to checkout the current branch.");
    }

    // 加载目标分支并尝试覆写工作区
    std::string target_id = cleanString(Utils::readContentsAsString(branch_path));
    Commit target_commit = Commit::read(Utils::join(COMMITS_DIR, target_id));
    checkoutSnapshot(target_commit);

    // 如果成功, 修改 HEAD, 指向新分支
    Utils::writeContents(HEAD_FILE, name);
}

// reset [revision]
void Repository::reset(const std::string& rev) {
    // 解析 revision 并执行快照覆盖
    std::string target_id = resolveRevision(rev);
    Commit target_commit = Commit::read(Utils::join(COMMITS_DIR, target_id));
    checkoutSnapshot(target_commit);

    // 移动当前分支指针
    std::string branch = cleanString(getCurrentBranch());
    std::string branch_path = getBranchRefPath(branch);
    Utils::writeContents(branch_path, target_id);
}

// 用于 pull 的带 Commit 的私有 merge 函数
void Repository::mergeWithCommit(const std::string& target_id, const std::string& target_name) {
    StagingArea staging;
    staging.load(INDEX_FILE);
    std::string head_id = getCurrentCommitId();
    const auto& head_ancestors = getAncestors(head_id);
    std::string branch_id = target_id; // 直接使用传入的 target_id
    std::string split_id = "";

    // 1. BFS 搜索最近分割点
    std::set<std::string> visited;
    std::queue<std::string> q;
    q.push(branch_id);
    visited.insert(branch_id);
    while (!q.empty()) {
        auto curr = q.front();
        q.pop();
        if (head_ancestors.count(curr)) {
            split_id = curr;
            break;
        }
        Commit c = Commit::read(Utils::join(COMMITS_DIR, curr));
        for (const auto& p : c.getParents()) {
            if (!visited.count(p)) {
                visited.insert(p);
                q.push(p);
            }
        }
    }

    // 理论上 split_id 不可能为空, 但以防万一进行防御
    if (split_id.empty()) {
        Utils::exitWithMessage("No common ancestor found.");
    }

    // 2. 特殊关系处理
    // (1) 给定分支是当前分支的祖先
    if (split_id == branch_id) {
        Utils::exitWithMessage("Given branch is an ancestor of the current branch.");
    }

    // (2) 当前分支是给定分支的祖先, 执行快进
    if (split_id == head_id) {
        Commit target_commit = Commit::read(Utils::join(COMMITS_DIR, branch_id));
        checkoutSnapshot(target_commit);

        // 移动当前分支指针并输出相关信息
        std::string curr_branch = cleanString(getCurrentBranch());
        Utils::writeContents(Utils::join(HEADS_DIR, curr_branch), branch_id);
        std::cout << "Current branch fast-forwarded." << '\n';
        return;
    }

    // 3. 计算普通合并结果
    // (1) 预备工作: 加载 3 方 Commit 对象, 获取其追踪的文件名并集,
    // 为合并设立 3 个区: 待覆写的 to_write, 待删除的 to_remove 和
    // 有冲突的 conflicts 
    Commit base_commit = Commit::read(Utils::join(COMMITS_DIR, split_id));
    Commit ours_commit = Commit::read(Utils::join(COMMITS_DIR, head_id));
    Commit theirs_commit = Commit::read(Utils::join(COMMITS_DIR, branch_id));
    std::set<std::string> all_filenames;
    std::map<std::string, std::string> to_write;
    std::vector<std::string> to_remove, conflicts;
    for (const auto& [fn, id]: base_commit.getBlobs()) {
        all_filenames.insert(fn);
    }
    for (const auto& [fn, id]: ours_commit.getBlobs()) {
        all_filenames.insert(fn);
    }
    for (const auto& [fn, id]: theirs_commit.getBlobs()) {
        all_filenames.insert(fn);
    }
    
    // (2) 根据文件合并规则, 决定是否为文件分区, 以及分到哪里
    for (const auto& fn: all_filenames) {
        // 求出每个文件名在 3 个 Commit 对应的 ID 分别是什么, 找不到的话置空
        std::string base_id = (base_commit.getBlobs().count(fn)) ? base_commit.getBlobs().at(fn) : "";
        std::string ours_id = (ours_commit.getBlobs().count(fn)) ? ours_commit.getBlobs().at(fn) : "";
        std::string theirs_id = (theirs_commit.getBlobs().count(fn)) ? theirs_commit.getBlobs().at(fn) : "";

        if (base_id.empty() && theirs_id.empty() && !ours_id.empty()) {
            // 规则 4: 保持原样
            continue;
        }

        if (ours_id == theirs_id) {
            // 规则 3: 双方一致 (都没改, 或者都改成了同样的, 或者都没了), 保持原样
            continue; 
        }

        if (ours_id == base_id) {
            // 当前没改 (或者当前没有), 但给定分支改了 (或者给定分支新建了)
            // 对应规则 1, 5 和 6
            if (theirs_id.empty()) {
                // 规则 6: 给定分支删了 fn
                to_remove.push_back(fn);
            }
            else {
                // 规则 1, 5: 给定分支改了或新建了 fn
                to_write[fn] = theirs_id;
            }
            continue;
        }

        if (theirs_id == base_id) {
            // 给定分支没改 fn (或者给定没有), 但当前分支改了 (或者当前分支新建了)
            // 规则 2, 7: 保持原样
            continue;
        }

        // 双方都改了且不一样, 或者一边改了一边删了, 或者双方新建了但不一样
        // 规则 8: 冲突
        conflicts.push_back(fn);
    }

    // (3) 未跟踪文件预检
    // 只针对 to_write, to_remove 和 conflicts 里的文件进行检查
    IgnoreMatcher matcher;
    matcher.LoadFromWorkingDirectory();

    // 把所有会修改的文件放到一个 set 里方便统一检查
    std::set<std::string> paths_to_check;
    for (const auto& [fn, id] : to_write) {
        paths_to_check.insert(fn);
    }
    for (const auto& fn : to_remove) {
        paths_to_check.insert(fn);
    }
    for (const auto& fn : conflicts) {
        paths_to_check.insert(fn);
    }

    for (const auto& fn : paths_to_check) {
        // 如果工作区有这个文件, 且它未被当前提交追踪、未暂存添加、未被 ignore, 
        // 则直接退出
        if (Utils::isFile(fn) && !ours_commit.getBlobs().count(fn) 
            && !staging.isAdded(fn) && !matcher.isIgnored(fn)) {
            Utils::exitWithMessage("There is an untracked file in the way; delete it, or add and commit it first.");
        }
    }

    // 4. 执行普通合并: 修改工作区与暂存区
    // (1) 删除
    for (const auto& fn: to_remove) {
        if (Utils::isFile(fn)) {
            std::remove(fn.c_str());
        }
        staging.removeFile(fn);
    }

    // (2) 写入
    for (const auto& [fn, id]: to_write) {
        Blob blob = Blob::read(Utils::join(BLOBS_DIR, id));
        Utils::writeContents(fn, blob.getContent());
        staging.addFile(fn, id);
    }

    // (3) 冲突处理
    for (const auto& fn: conflicts) {
        // 读取 fn 在双方 Commit 的内容
        std::string ours_content = "", theirs_content = "", conflict_content = "";
        if (ours_commit.getBlobs().count(fn)) {
            Blob blob = Blob::read(Utils::join(BLOBS_DIR, ours_commit.getBlobs().at(fn)));
            ours_content = blob.getContent();
            if (!ours_content.empty() && ours_content.back() != '\n') {
                ours_content.push_back('\n');
            }
        }
        if (theirs_commit.getBlobs().count(fn)) {
            Blob blob = Blob::read(Utils::join(BLOBS_DIR, theirs_commit.getBlobs().at(fn)));
            theirs_content = blob.getContent();
            if (!theirs_content.empty() && theirs_content.back() != '\n') {
                theirs_content.push_back('\n');
            }
        }

        // 按给定格式拼接
        conflict_content = "<<<<<<< HEAD\n" + ours_content + 
                           "=======\n" + theirs_content + ">>>>>>>\n";

        // 将新文件写入工作区并暂存
        Blob blob(conflict_content);
        blob.save(Utils::join(BLOBS_DIR, blob.getId()));
        Utils::writeContents(fn, conflict_content);
        staging.addFile(fn, blob.getId());
    }

    // 保存暂存区
    staging.save(INDEX_FILE);

    // 5. 生成合并提交
    std::map<std::string, std::string> new_blobs = ours_commit.getBlobs();
    for (const auto& [fn, id]: staging.getAddedMap()) {
        new_blobs[fn] = id;
    }
    for (const auto& fn: staging.getRemovedSet()) {
        new_blobs.erase(fn);
    }

    std::time_t now = std::time(nullptr);
    std::vector<std::string> pars = {head_id, branch_id};
    std::string msg = "Merged " + target_name + " into " + cleanString(getCurrentBranch()) + ".";
    Commit merge_commit(msg, now, pars, new_blobs);
    merge_commit.save(Utils::join(COMMITS_DIR, merge_commit.getId()));

    // 移动当前分支的指针
    Utils::writeContents(Utils::join(HEADS_DIR, cleanString(getCurrentBranch())), merge_commit.getId());

    // 清空暂存区
    staging.clear();
    staging.save(INDEX_FILE);

    // 6. 输出冲突提示
    if (!conflicts.empty()) {
        std::cout << "Encountered a merge conflict." << '\n';
    }
}


// merge [branch name]
void Repository::merge(const std::string& branch) {
    // 1. 前置失败检查: 暂存区有未提交的修改 (必须最先检查)
    StagingArea staging;
    staging.load(INDEX_FILE);
    if (!staging.isEmpty()) {
        Utils::exitWithMessage("You have uncommitted changes.");
    }

    // 2. 检查分支存在性
    std::string branch_path = Utils::join(HEADS_DIR, branch);
    if (!isValidName(branch) || !Utils::isFile(branch_path)) {
        Utils::exitWithMessage("A branch with that name does not exist.");
    }

    // 3. 不能将分支与自身合并
    if (branch == cleanString(getCurrentBranch())) {
        Utils::exitWithMessage("Cannot merge a branch with itself.");
    }

    // 4. 获取 ID 并调用核心函数
    std::string target_id = cleanString(Utils::readContentsAsString(branch_path));
    mergeWithCommit(target_id, branch);
}

// 复制 Blob 和 Commit 的私有辅助函数
void Repository::copyObjects(const std::string& from_dir, const std::string& to_dir) {
    auto files = Utils::plainFilenamesIn(from_dir);
    for (const auto& f : files) {
        std::string target = Utils::join(to_dir, f);
        if (!Utils::isFile(target)) {
            // 不要覆盖远程已有对象
            std::string content = Utils::readContentsAsString(Utils::join(from_dir, f));
            Utils::writeContents(target, content);
        }
    }
}

// 获取分支引用的完整路径 (兼容本地分支和远程跟踪分支)
std::string Repository::getBranchRefPath(const std::string& name) {
    // 如果包含 '/', 说明是远程分支
    if (name.find('/') != std::string::npos) {
        size_t pos = name.find('/');
        std::string r = name.substr(0, pos);
        std::string b = name.substr(pos + 1);
        return Utils::join(REMOTE_REFS_DIR, r, b);
    }
    // 否则是本地分支
    return Utils::join(HEADS_DIR, name);
}

// add-remote [remote name] [path]
void Repository::addRemote(const std::string& name, const std::string& path) {
    // 校验名字合法性
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 检查是否冲突: HEADS_DIR 下有无同名分支, REMOTES_DIR 下有无同名远程
    if (Utils::isFile(Utils::join(HEADS_DIR, name)) || Utils::isFile(Utils::join(REMOTES_DIR, name))) {
        Utils::exitWithMessage("A remote with that name already exists.");
    }

    // 不检查路径是否存在, 直接保存
    Utils::writeContents(Utils::join(REMOTES_DIR, name), path);
}

// rm-remote [remote name]
void Repository::rmRemote(const std::string& name) {
    // 校验名字合法性
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 检查 REMOTES_DIR 下有无同名远程, 若有则删除, 若无则报错退出
    if (!Utils::isFile(Utils::join(REMOTES_DIR, name))) {
        Utils::exitWithMessage("A remote with that name does not exist.");
    }
    else {
        std::remove(Utils::join(REMOTES_DIR, name).c_str());
    }
}

// push [remote name] [remote branch name]
void Repository::push(const std::string& remote, const std::string& branch) {
    // 检查远程名和分支名是否合法
    if (!isValidName(remote) || !isValidName(branch)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 检查 REMOTES_DIR 下有没有该远程, 如果没有报错退出
    if (!Utils::isFile(Utils::join(REMOTES_DIR, remote))) {
        Utils::exitWithMessage("Remote directory not found.");
    }

    // 检查 remote 是否真的是一个 GitLite 仓库, 若不是也报错退出
    std::string remote_path = cleanString(Utils::readContentsAsString(Utils::join(REMOTES_DIR, remote)));
    if (!Utils::isDirectory(remote_path) || !Utils::isFile(Utils::join(remote_path, "HEAD"))) {
        Utils::exitWithMessage("Remote directory not found.");
    }

    // 检查历史包含关系 (仅当远程分支已存在时)
    std::string remote_branch_path = Utils::join(remote_path, "refs/heads", branch);
    std::string local_head_id = cleanString(getCurrentCommitId());
    if (Utils::isFile(remote_branch_path)) {
        std::string remote_head_id = cleanString(Utils::readContentsAsString(remote_branch_path));
        // 如果远程头不是本地头的祖先, 说明本地落后了, 报错退出
        if (!getAncestors(local_head_id).count(remote_head_id)) {
            Utils::exitWithMessage("Please pull down remote changes before pushing.");
        }
    }

    // 将本地的 Commit 和 Blob 复制到目标远程仓库
    copyObjects(COMMITS_DIR, Utils::join(remote_path, "commits"));
    copyObjects(BLOBS_DIR, Utils::join(remote_path, "blobs"));

    // 更新远程分支头
    Utils::writeContents(remote_branch_path, local_head_id);
}

// fetch [remote name] [remote branch name]
void Repository::fetch(const std::string& remote, const std::string& branch) {
    // 检查远程名和分支名是否合法
    if (!isValidName(remote) || !isValidName(branch)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 检查 REMOTES_DIR 下有没有该远程, 如果没有报错退出
    if (!Utils::isFile(Utils::join(REMOTES_DIR, remote))) {
        Utils::exitWithMessage("Remote directory not found.");
    }

    // 检查 remote 是否真的是一个 GitLite 仓库, 若不是也报错退出
    std::string remote_path = cleanString(Utils::readContentsAsString(Utils::join(REMOTES_DIR, remote)));
    if (!Utils::isDirectory(remote_path) || !Utils::isFile(Utils::join(remote_path, "HEAD"))) {
        Utils::exitWithMessage("Remote directory not found.");
    }

    // 检查 remote 是否有给定 branch, 若否报错退出
    std::string remote_branch_path = Utils::join(remote_path, "refs/heads", branch);
    if (!Utils::isFile(remote_branch_path)) {
        Utils::exitWithMessage("That remote does not have that branch.");
    }

    // 将目标远程仓库的 Commit 和 Blob 复制到本地
    copyObjects(Utils::join(remote_path, "commits"), COMMITS_DIR);
    copyObjects(Utils::join(remote_path, "blobs"), BLOBS_DIR);

    // 更新本地远程分支头 ID
    std::string remote_head_id = cleanString(Utils::readContentsAsString(remote_branch_path));
    Utils::writeContents(Utils::join(REMOTE_REFS_DIR, remote, branch), remote_head_id);
}

// pull [remote name] [remote branch name]
void Repository::pull(const std::string& remote, const std::string& branch) {
    // 1. 先 fetch (fetch 内部已经包含了所有关于远程目录和分支的失败检查)
    fetch(remote, branch);

    // 2. 检查暂存区
    StagingArea staging;
    staging.load(INDEX_FILE);
    if (!staging.isEmpty()) {
        Utils::exitWithMessage("You have uncommitted changes.");
    }

    // 3. 获取 fetch 下来的远程跟踪分支 ID
    std::string remote_branch_path = Utils::join(REMOTE_REFS_DIR, remote, branch);
    std::string target_id = cleanString(Utils::readContentsAsString(remote_branch_path));
    
    // 4. 构造一个用来展示在 commit message 里的名字
    std::string target_name = remote + "/" + branch;

    // 5. 复用 merge 逻辑
    mergeWithCommit(target_id, target_name);
}

// 用于 diff 和 show 的从 Commit 读取文件内容的私有辅助函数
std::optional<std::string> Repository::readFileFromCommit(const Commit& commit, 
                                                          const std::string& filename) {
    if (!commit.getBlobs().count(filename)) {
        return std::nullopt;
    }
    std::string id = commit.getBlobs().at(filename);
    return Blob::read(Utils::join(BLOBS_DIR, id)).getContent();
}

// diff
void Repository::diff() {
    // 获取当前头提交, 读取其 Blobs, 对每个文件名构建 content
    Commit head = getCurrentCommit();
    for (const auto& [fn, id]: head.getBlobs()) {
        std::optional<std::string> head_content, actual_content;
        Blob blob = Blob::read(Utils::join(BLOBS_DIR, id));
        head_content = blob.getContent();

        // content 不为空选项, 当且仅当工作区有文件 fn
        if (Utils::isFile(fn)) {
            actual_content = Utils::readContentsAsString(fn);
        }
        else {
            actual_content = std::nullopt;
        }

        // 输出编辑信息
        std::cout << DiffEngine::generateDiff(head_content, actual_content, fn);
    }
}

// diff [revision]
void Repository::diffWithCommit(const std::string& rev) {
    // 获取目标提交, 读取其 Blobs, 对每个文件名构建 content
    std::string head_id = resolveRevision(rev);
    Commit head = Commit::read(Utils::join(COMMITS_DIR, head_id));
    for (const auto& [fn, id]: head.getBlobs()) {
        std::optional<std::string> head_content, actual_content;
        Blob blob = Blob::read(Utils::join(BLOBS_DIR, id));
        head_content = blob.getContent();

        // content 不为空选项, 当且仅当工作区有文件 fn
        if (Utils::isFile(fn)) {
            actual_content = Utils::readContentsAsString(fn);
        }
        else {
            actual_content = std::nullopt;
        }

        // 输出编辑信息
        std::cout << DiffEngine::generateDiff(head_content, actual_content, fn);
    }
}

// diff [revision1] [revision2]
void Repository::diffBetween(const std::string& rev1, const std::string& rev2) {
    // 获取 2 个 revision 指向的提交
    std::string old_commit_id = resolveRevision(rev1);
    Commit old_commit = Commit::read(Utils::join(COMMITS_DIR, old_commit_id));
    std::string new_commit_id = resolveRevision(rev2);
    Commit new_commit = Commit::read(Utils::join(COMMITS_DIR, new_commit_id));

    // 构建二者跟踪文件的并集
    std::set<std::string> all_filenames;
    for (const auto& [fn, id]: old_commit.getBlobs()) {
        all_filenames.insert(fn);
    }
    for (const auto& [fn, id]: new_commit.getBlobs()) {
        all_filenames.insert(fn);
    }

    // 对每个文件名, 构建其对应的 content 并输出比较信息
    for (const std::string& fn: all_filenames) {
        std::optional<std::string> old_content, new_content;
        // 如果该文件被旧提交跟踪, 那么 old_content 不为空选项,
        // 读取相应的 Blob; 否则, old_content 为空选项
        // 新提交同理
        old_content = readFileFromCommit(old_commit, fn);
        new_content = readFileFromCommit(new_commit, fn);

        // 输出编辑信息
        std::cout << DiffEngine::generateDiff(old_content, new_content, fn);
    }
}

// show
void Repository::show() {
    // 获取当前头提交并打印 info
    Commit head = getCurrentCommit();
    std::cout << head.info();

    // 初始提交不与父提交比较
    if (head.getParents().empty()) {
        return;
    }

    // 否则获取父提交并构建二者跟踪文件的并集
    std::set<std::string> all_filenames;
    const auto& parent_id = head.getParents()[0];
    Commit parent = Commit::read(Utils::join(COMMITS_DIR, parent_id));
    const auto& head_blobs = head.getBlobs();
    const auto& parent_blobs = parent.getBlobs();
    for (const auto& [fn, id]: head_blobs) {
        all_filenames.insert(fn);
    }
    for (const auto& [fn, id]: parent_blobs) {
        all_filenames.insert(fn);
    }

    // 对每个文件名, 构建其对应的 content 并输出比较信息
    for (const std::string& fn: all_filenames) {
        std::optional<std::string> head_content, parent_content;
        // 如果该文件被当前头提交跟踪, 那么 head_content 不为空选项,
        // 读取相应的 Blob; 否则, head_content 为空选项
        // 父提交同理
        head_content = readFileFromCommit(head, fn);
        parent_content = readFileFromCommit(parent, fn);

        // 输出编辑信息
        std::cout << DiffEngine::generateDiff(parent_content, head_content, fn);
    }
}

// show [revision]
void Repository::show(const std::string& rev) {
    // 获取当前 revision 指向的提交并打印 info
    std::string head_id = resolveRevision(rev);
    Commit head = Commit::read(Utils::join(COMMITS_DIR, head_id));
    std::cout << head.info();

    // 初始提交不与父提交比较
    if (head.getParents().empty()) {
        return;
    }

    // 否则获取父提交并构建二者跟踪文件的并集
    std::set<std::string> all_filenames;
    const auto& parent_id = head.getParents()[0];
    Commit parent = Commit::read(Utils::join(COMMITS_DIR, parent_id));
    const auto& head_blobs = head.getBlobs();
    const auto& parent_blobs = parent.getBlobs();
    for (const auto& [fn, id]: head_blobs) {
        all_filenames.insert(fn);
    }
    for (const auto& [fn, id]: parent_blobs) {
        all_filenames.insert(fn);
    }

    // 对每个文件名, 构建其对应的 content 并输出比较信息
    for (const std::string& fn: all_filenames) {
        std::optional<std::string> head_content, parent_content;
        // 如果该文件被当前头提交跟踪, 那么 head_content 不为空选项,
        // 读取相应的 Blob; 否则, head_content 为空选项
        // 父提交同理
        head_content = readFileFromCommit(head, fn);
        parent_content = readFileFromCommit(parent, fn);

        // 输出编辑信息
        std::cout << DiffEngine::generateDiff(parent_content, head_content, fn);
    }
}

// main.cpp 的 checkCWD 依赖此方法, 请保留
std::string Repository::getGitliteDir() {
    return ".gitlite";
}
