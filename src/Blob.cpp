#include "../include/Utils.h"
#include "../include/Blob.h"

#include <stdexcept>

// 私有辅助函数: 根据 content 计算 SHA-1 哈希
void Blob::computeId() {
    std::string hash_str = "blob " + std::to_string(content.size()) + '\0' + content;
    id = Utils::sha1(hash_str);
}

// 构造函数: 从工作区读取到的原始内容实例化, 并自动计算 ID
Blob::Blob(const std::string& rawContent) {
    content = rawContent;
    computeId();
}

// ID 访问方法
const std::string& Blob::getId() const {
    return id;
}

// 内容访问方法
const std::string& Blob::getContent() const {
    return content;
}

// 将当前 Blob 的内容持久化到指定的完整路径中 (如 .gitlite/blobs/<id>)
// 如果目标文件已存在, 则直接返回, 实现内容寻址去重
void Blob::save(const std::string& fullPath) const {
    // 内容寻址去重
    if (Utils::isFile(fullPath)) {
        return;
    }

    // 新建目标文件
    Utils::writeContents(fullPath, content);
};

// 从指定的完整路径读取 Blob 并实例化
Blob Blob::read(const std::string& fullPath) {
    // 若文件不存在, 抛出异常
    if (!Utils::isFile(fullPath)) {
        throw std::invalid_argument("File cannot be opened.");
    }

    // 否则读取目标文件并返回实例
    std::string raw_str = Utils::readContentsAsString(fullPath);
    return Blob(raw_str);
}