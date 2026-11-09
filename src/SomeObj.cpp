#include "../include/SomeObj.h"
#include "../include/Repository.h"

SomeObj::SomeObj() {}

// ---------------- Subtask 1: init / add / commit / rm ----------------

void SomeObj::init() {
    // TODO: 创建 .gitlite 目录、initial commit 与 master 分支
    Repository repo;
    repo.init();
}

void SomeObj::add(const std::string& f) {
    // TODO: 将文件的当前内容加入暂存区
    Repository repo;
    repo.add(f);
}

void SomeObj::commit(const std::string& m) {
    // TODO: 将暂存区保存为新提交，并推进当前分支
    Repository repo;
    repo.commit(m);
}

void SomeObj::rm(const std::string& f) {
    // TODO: 将文件取消暂存, 或标记为待删除
    Repository repo;
    repo.rm(f);
}

// ---------------- Subtask 2: log / global-log / tag ----------------

void SomeObj::log() {
    // TODO: 从当前提交沿第一个父提交打印日志
    Repository repo;
    repo.log();
}

void SomeObj::globalLog() {
    // TODO: 从所有本地分支、远程跟踪分支和标签出发，输出全部可达提交并按规范排序
    Repository repo;
    repo.globalLog();
}

void SomeObj::listTags() {
    // TODO: 按字典序打印所有标签及其目标提交 id
    Repository repo;
    repo.listTags();
}

void SomeObj::tag(const std::string& n) {
    // TODO: 为当前提交创建轻量标签
    Repository repo;
    repo.tag(n);
}

void SomeObj::tag(const std::string& n, const std::string& rev) {
    // TODO: 为指定提交或标签所指提交创建轻量标签
    Repository repo;
    repo.tag(n, rev);
}

// ---------------- Subtask 3: status / branch / rm-branch ----------------

void SomeObj::status() {
    // TODO: 显示分支、暂存区、未暂存修改与未跟踪文件
    Repository repo;
    repo.status();
}

void SomeObj::branch(const std::string& b) {
    // TODO: 新建指向当前提交的分支, 不切换
    Repository repo;
    repo.branch(b);
}

void SomeObj::rmBranch(const std::string& b) {
    // TODO: 删除分支指针, 不删除其提交
    Repository repo;
    repo.rmBranch(b);
}

// ---------------- Subtask 4: checkout / reset ----------------

void SomeObj::checkoutBranch(const std::string& b) {
    // TODO: 切换到指定分支
    Repository repo;
    repo.checkoutBranch(b);
}

void SomeObj::checkoutFile(const std::string& f) {
    // TODO: 用当前提交中的版本覆盖工作目录中的文件
    Repository repo;
    repo.checkoutFile(f);
}

void SomeObj::checkoutFileInCommit(const std::string& rev, const std::string& f) {
    // TODO: 用指定 revision 的版本覆盖工作文件, 支持标签与 id 缩写
    Repository repo;
    repo.checkoutFileInCommit(rev, f);
}

void SomeObj::reset(const std::string& rev) {
    // TODO: 解析 revision, 检出指定提交并移动当前分支头
    Repository repo;
    repo.reset(rev);
}

// ---------------- Subtask 5: merge ----------------

void SomeObj::merge(const std::string& b) {
    // TODO: 将指定分支合并到当前分支
    Repository repo;
    repo.merge(b);
}

// ---------------- Bonus-1: add-remote / rm-remote / push / fetch / pull ----------------

void SomeObj::addRemote(const std::string& n, const std::string& p) {
    // TODO: 保存远程仓库地址
    Repository repo;
    repo.addRemote(n, p);
}

void SomeObj::rmRemote(const std::string& n) {
    // TODO: 删除远程仓库信息
    Repository repo;
    repo.rmRemote(n);
}

void SomeObj::push(const std::string& n, const std::string& b) {
    // TODO: 将当前分支的提交推送到远程分支
    Repository repo;
    repo.push(n, b);
}

void SomeObj::fetch(const std::string& n, const std::string& b) {
    // TODO: 将远程分支的提交与 Blob 复制到本地
    Repository repo;
    repo.fetch(n, b);
}

void SomeObj::pull(const std::string& n, const std::string& b) {
    // TODO: fetch 后将取回的分支合并到当前分支
    Repository repo;
    repo.pull(n, b);
}

// ---------------- Bonus-2: diff / show ----------------

void SomeObj::diff() {
    // TODO: 比较当前提交与工作目录并输出差异, 无差异时不输出
    Repository repo;
    repo.diff();
}

void SomeObj::diffWithCommit(const std::string& rev) {
    // TODO: 解析 revision, 比较指定提交与工作目录, 无差异时不输出
    Repository repo;
    repo.diffWithCommit(rev);
}

void SomeObj::diffBetween(const std::string& rev1, const std::string& rev2) {
    // TODO: 解析两个 revision 并比较对应提交, 无差异时不输出
    Repository repo;
    repo.diffBetween(rev1, rev2);
}

void SomeObj::show() {
    // TODO: 显示当前提交及其相对第一父提交的差异
    Repository repo;
    repo.show();
}

void SomeObj::show(const std::string& rev) {
    // TODO: 显示指定提交或标签及其相对第一父提交的差异
    Repository repo;
    repo.show(rev);
}
