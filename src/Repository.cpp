#include "../include/Repository.h"
#include "../include/Utils.h"
#include "../include/Commit.h"

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

// main.cpp 的 checkCWD 依赖此方法, 请保留
std::string Repository::getGitliteDir() {
    return ".gitlite";
}
