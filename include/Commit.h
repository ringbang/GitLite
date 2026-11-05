#ifndef COMMIT_H
#define COMMIT_H

#include <string>
#include <vector>
#include <map>
#include <ctime>

// 建议类: 抽象一次提交 (消息、时间戳、父提交、文件快照), 可自由设计或删除
// tag 是指向提交的仓库引用, 不属于 Commit 对象自身
class Commit {
public:
    // 默认构造, 供从磁盘读取时使用
    Commit() = default;

    // 完整构造, 自动计算 ID
    Commit(const std::string& msg, std::time_t time, 
           const std::vector<std::string>& pars, 
           const std::map<std::string, std::string>& bl);

    // 将当前 Commit 序列化并持久化到指定路径 (如 .gitlite/commits/<id>)
    void save(const std::string& fullPath) const;

    // 从指定路径读取 Commit 并实例化
    static Commit read(const std::string& fullPath);

    // 访问器
    std::string getId() const;
    std::string getMessage() const;
    std::time_t getTimestamp() const;
    const std::vector<std::string>& getParents() const;
    const std::map<std::string, std::string>& getBlobs() const;

private:
    std::string message;
    std::time_t timestamp;
    std::vector<std::string> parents;
    std::map<std::string, std::string> blobs;
    std::string id;

    // 私有辅助函数: 将当前对象序列化为不含自身 id 的字符串, 并计算 SHA-1
    void computeId();
    // 序列化核心逻辑 (供 computeId 和 save 复用)
    std::string serialize() const; 
};

#endif // COMMIT_H