#include "../include/Repository.h"
#include "../include/Utils.h"
#include "../include/Commit.h"
#include "../include/IgnoreMatcher.h"
#include "../include/StagingArea.h"
#include "../include/Blob.h"

#include <ctime>
#include <iostream>
#include <clocale>
#include <algorithm>
#include <queue>
#include <utility>

// 定义标准路径
const std::string Repository::GITLITE_DIR = ".gitlite";
const std::string Repository::HEAD_FILE = ".gitlite/HEAD";
const std::string Repository::INDEX_FILE = ".gitlite/index";
const std::string Repository::COMMITS_DIR = ".gitlite/commits";
const std::string Repository::BLOBS_DIR = ".gitlite/blobs";
const std::string Repository::REFS_DIR = ".gitlite/refs";
const std::string Repository::HEADS_DIR = ".gitlite/refs/heads";
const std::string Repository::TAGS_DIR = ".gitlite/refs/tags";

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
    std::string branch = getCurrentBranch();
    std::string branch_path = Utils::join(HEADS_DIR, branch);
    return Utils::readContentsAsString(branch_path);
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

// add
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

// commit
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
    std::string branch_path = Utils::join(HEADS_DIR, branch);
    Utils::writeContents(branch_path, newCommit.getId());

    // 刷新状态
    staging.clear();
    staging.save(INDEX_FILE);
}

// rm
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
    // 1. 找到所有的起点提交, 包括每个 branch 和 tag
    std::vector<std::string> start_ids;
    auto branches = Utils::plainFilenamesIn(HEADS_DIR);
    for (const auto& b: branches) {
        std::string id = Utils::readContentsAsString(Utils::join(HEADS_DIR, b));
        start_ids.push_back(id);
    }

    auto tags = Utils::plainFilenamesIn(TAGS_DIR);
    for (const auto& t: tags) {
        std::string id = Utils::readContentsAsString(Utils::join(TAGS_DIR, t));
        start_ids.push_back(id);
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

void Repository::status() {
    // 1. Branches
    // 加载当前已有的所有 branches 和目前活跃的 branch
    auto branches = Utils::plainFilenamesIn(HEADS_DIR);
    std::string curr_branch = Utils::readContentsAsString(HEAD_FILE);

    // 将所有 branches 按规则输出 (自带字典序, 当前 branch 是特例)
    std::cout << "=== Branches ===" << '\n';
    for (const auto& b: branches) {
        if (b == curr_branch) {
            std::cout << "*";
        }
        std::cout << b << '\n';
    }
    std::cout << '\n';
    branches.clear();

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

// branch
void Repository::branch(const std::string& name) {
    // 先检查 name 是否合法, 不合法直接退出
    if (!isValidName(name)) {
        Utils::exitWithMessage("Incorrect operands.");
    }

    // 再检查是否存在同名 branch, 若存在也退出
    std::string branch_path = Utils::join(HEADS_DIR, name);
    if (Utils::isFile(branch_path)) {
        Utils::exitWithMessage("A branch with that name already exists.");
    }

    // 可以创建新 branch
    std::string head_id = cleanString(getCurrentCommitId());
    Utils::writeContents(branch_path, head_id);
};

// rm-branch
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

// main.cpp 的 checkCWD 依赖此方法, 请保留
std::string Repository::getGitliteDir() {
    return ".gitlite";
}
