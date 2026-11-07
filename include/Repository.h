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

    // 元数据目录名, 即 ".gitlite"
    static std::string getGitliteDir();

    // add [file name]
    void add(const std::string& filename);

    // commit [message]
    void commit(const std::string& message);

    // rm [file name]
    void rm(const std::string& filename);

    // log 
    void log();

    // 解析 revision 的辅助函数
    std::string resolveRevision(const std::string& rev);

    // 初始化仓库
    void init();

    // tag
    void listTags();

    // tag [tag name]
    void tag(const std::string& name);

    // tag [tag name] [revision]
    void tag(const std::string& name, const std::string& rev);

    // global-log
    void globalLog();

    // status
    void status();

    // branch [branch name]
    void branch(const std::string& name);

    // rm-branch [branch name]
    void rmBranch(const std::string& name);

    // checkout -- [file name]
    void checkoutFile(const std::string name);

    // checkout [revision] -- [file name]
    void checkoutFileInCommit(const std::string& rev, const std::string& fn);

    // checkout [branch name]
    void checkoutBranch(const std::string& name); 

    // reset [revision]
    void reset(const std::string& rev);

private:
    // 判断 tag 的名字是否合法的私有辅助函数
    bool isValidName(const std::string& tag);

    // 获取当前分支名
    std::string getCurrentBranch();

    // 获取当前 HEAD 指向的 commit ID
    std::string getCurrentCommitId();

    // 获取当前 Commit 对象
    Commit getCurrentCommit();

    // 清理字符串最后可能带有的换行符
    std::string cleanString(std::string s);

    // 用于 checkout 和 reset 的把工作区和暂存区彻底变成目标提交的模样
    // 的私有辅助函数
    void checkoutSnapshot(const Commit& target);
};

#endif // REPOSITORY_H
