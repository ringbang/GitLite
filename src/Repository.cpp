#include "../include/Repository.h"
#include "../include/Utils.h"
#include "../include/Commit.h"
#include "../include/IgnoreMatcher.h"
#include "../include/StagingArea.h"
#include "../include/Blob.h"

#include <ctime>
#include <iostream>
#include <clocale>

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
    
    // 只要 ID 不为空，就继续遍历
    while (!curr_id.empty()) {
        Commit curr_commit = Commit::read(Utils::join(COMMITS_DIR, curr_id));
        curr_commit.print();
        
        // 获取父节点, 如果有, 更新 curr_id; 如果没有, 置空结束循环
        const auto& pars = curr_commit.getParents();
        if (!pars.empty()) {
            curr_id = pars[0];
        } else {
            curr_id = "";
        }
    }
}

// main.cpp 的 checkCWD 依赖此方法, 请保留
std::string Repository::getGitliteDir() {
    return ".gitlite";
}
