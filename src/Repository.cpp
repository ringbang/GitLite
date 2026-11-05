#include "../include/Repository.h"
#include "../include/Utils.h"
#include "../include/Commit.h"
#include "../include/IgnoreMatcher.h"
#include "../include/StagingArea.h"
#include "../include/Blob.h"

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

// main.cpp 的 checkCWD 依赖此方法, 请保留
std::string Repository::getGitliteDir() {
    return ".gitlite";
}
