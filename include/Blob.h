#ifndef BLOB_H
#define BLOB_H

#include <string>

// 文件快照对象: 原文字节读写、SHA-1 计算与持久化
class Blob {
public:
    // 默认构造, 供后续从磁盘读取 / 反序列化时使用
    Blob() = default;

    // 构造函数: 从工作区读取到的原始内容实例化, 并自动计算 id
    explicit Blob(const std::string& rawContent);

    // 将当前 Blob 的内容持久化到指定的完整路径中 (如 .gitlite/blobs/<id>)
    // 如果目标文件已存在, 则直接返回, 实现内容寻址去重
    void save(const std::string& fullPath) const;

    // 从指定的完整路径读取 Blob 并实例化
    static Blob read(const std::string& fullPath);

    // 访问器
    const std::string& getId() const;
    const std::string& getContent() const;

private:
    std::string id;
    std::string content;

    // 私有辅助函数: 根据 content 计算 SHA-1 哈希
    void computeId();
};

#endif // BLOB_H