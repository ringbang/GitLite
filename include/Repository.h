#ifndef REPOSITORY_H
#define REPOSITORY_H

#include "Commit.h"

#include <string>

// 建议类: 抽象仓库的持久化状态, 包括 HEAD、分支、标签、暂存区与对象库;
// 可自由扩充或重新设计
// 注意: main.cpp 的 checkCWD 依赖 getGitliteDir(), 请保留该方法
class Repository {
public:
    // 静态常量：定义标准路径 (避免在代码里到处写字符串)
    static const std::string GITLITE_DIR;
    static const std::string HEAD_FILE;
    static const std::string INDEX_FILE;
    static const std::string COMMITS_DIR;
    static const std::string BLOBS_DIR;
    static const std::string REFS_DIR;
    static const std::string HEADS_DIR;
    static const std::string TAGS_DIR;

    // 元数据目录名，即 ".gitlite"
    static std::string getGitliteDir();

    // 获取当前分支名
    std::string getCurrentBranch();

    // 获取当前 HEAD 指向的 commit ID
    std::string getCurrentCommitId();

    // 获取当前 Commit 对象
    Commit getCurrentCommit();

    // add
    void add(const std::string& filename);

    // commit
    void commit(const std::string& message);

    // rm
    void rm(const std::string& filename);

    // log 
    void log();

    // 初始化仓库
    void init();
};

#endif // REPOSITORY_H
