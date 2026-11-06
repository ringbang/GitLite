#include "../include/Commit.h"
#include "../include/Utils.h"

#include <stdexcept>
#include <sstream>
#include <iostream>

// 建议类 Commit 的实现文件, 可按需使用或删除

// 序列化核心逻辑 (供 computeId 和 save 复用)
std::string Commit::serialize() const {
    std::string hash_str = std::to_string(timestamp) + '\n' + 
                           std::to_string(message.size()) + '\n' + message + 
                           '\n' + std::to_string(parents.size()) + '\n';
    for (size_t i = 0; i < parents.size(); i++) {
        hash_str += std::to_string(parents[i].size()) + '\n' +
                    parents[i] + '\n';
    }
    hash_str += std::to_string(blobs.size()) + '\n';
    for (const auto& blob: blobs) {
        hash_str += std::to_string(blob.first.size()) + '\n' +
                    blob.first + '\n' + std::to_string(blob.second.size()) + 
                    '\n' + blob.second + '\n';
    }
    return hash_str;                           
}

// 私有辅助函数: 将当前对象序列化为不含自身 id 的字符串, 并计算 SHA-1
void Commit::computeId() {
    std::string content = serialize();
    std::string hash_str = "commit " + std::to_string(content.size()) + 
                           '\0' + content;
    id = Utils::sha1(hash_str);
}

// 完整构造, 自动计算 ID
Commit::Commit(const std::string& msg, std::time_t time, 
               const std::vector<std::string>& pars, 
               const std::map<std::string, std::string>& bl):
    message(msg), timestamp(time), parents(pars), blobs(bl) {
        computeId();
    }

// 将当前 Commit 序列化并持久化到指定路径 (如 .gitlite/commits/<id>)
void Commit::save(const std::string& fullPath) const {
    // 去重
    if (Utils::isFile(fullPath)) {
        return;
    }
    // 保存
    Utils::writeContents(fullPath, serialize());
}

// 从指定路径读取 Commit 并实例化
Commit Commit::read(const std::string& fullPath) {
    // 若文件不存在则抛出异常
    if (!Utils::isFile(fullPath)) {
        throw std::invalid_argument("File cannot be opened.");
    }

    // 否则逐行解析并创建新对象, 对每一类信息的读取都设置异常检查
    std::string content = Utils::readContentsAsString(fullPath);
    std::istringstream iss(content);

    // 读取 timestamp
    std::time_t time;
    if (!(iss >> time)) {
        throw std::runtime_error("Corrupted commit file: missing timestamp.");
    }
    iss.get();

    // 读取 message 长度
    size_t msg_len = 0;
    if (!(iss >> msg_len)) {
        throw std::runtime_error("Corrupted commit file: missing message length.");
    }
    iss.get();

    // 读取 message 内容
    std::string msg(msg_len, ' ');
    if (!iss.read(&msg[0], msg_len)) {
        throw std::runtime_error("Corrupted commit file: missing message.");
    }
    iss.get();

    // 读取 parents 长度
    size_t pars_size = 0;
    if (!(iss >> pars_size)) {
        throw std::runtime_error("Corrupted commit file: missing parents size.");
    }
    iss.get();

    // 读取 parents 每个元素的长度和内容
    std::vector<std::string> pars(pars_size, "");
    for (size_t i = 0; i < pars_size; i++) {
        size_t par_len = 0;
        if (!(iss >> par_len)) {
            throw std::runtime_error("Corrupted commit file: missing length of commit " + std::to_string(i + 1) + ".");
        }
        iss.get();
        std::string par(par_len, ' ');
        if (!iss.read(&par[0], par_len)) {
            throw std::runtime_error("Corrupted commit file: missing content of commit " + std::to_string(i + 1) + ".");
        }
        iss.get();
        pars[i] = par;
    }

    // 读取 blobs 长度
    size_t blobs_size = 0;
    if (!(iss >> blobs_size)) {
        throw std::runtime_error("Corrupted commit file: missing blobs size.");
    }
    iss.get();

    // 读取 blobs 每个元素的长度和内容
    std::map<std::string, std::string> bl;
    for (size_t i = 0; i < blobs_size; i++) {
        size_t name_len = 0;
        if (!(iss >> name_len)) {
            throw std::runtime_error("Corrupted commit file: missing length of name of blob " + std::to_string(i + 1) + ".");
        }
        iss.get();
        std::string name(name_len, ' ');
        if (!iss.read(&name[0], name_len)) {
            throw std::runtime_error("Corrupted commit file: missing name of blob " + std::to_string(i + 1) + ".");
        }
        iss.get();
        size_t id_len = 0;
        if (!(iss >> id_len)) {
            throw std::runtime_error("Corrupted commit file: missing length of ID of blob " + std::to_string(i + 1) + ".");
        }
        iss.get();
        std::string id(id_len, ' ');
        if (!iss.read(&id[0], id_len)) {
            throw std::runtime_error("Corrupted commit file: missing ID of blob " + std::to_string(i + 1) + ".");
        }
        iss.get();
        bl[name] = id;
    }

    return Commit(msg, time, pars, bl);
}

// 输出用于打印的相关信息的辅助函数 info
std::string Commit::info() const {
    std::ostringstream oss;

    // 前 2 行: === 和 commit + id
    oss << "===" << '\n';
    oss << "commit " << id << '\n';
    
    // 可选的第 3 行: 合并产生的提交
    if (parents.size() == 2) {
        oss << "Merge: " << parents[0].substr(0, 7) <<
            " " << parents[1].substr(0, 7) << '\n';
    }

    // 第 4 行: 转换 timestamp 为时间点
    std::tm* tm = std::localtime(&timestamp);
    char time_buf[100];
    // %a (星期), %b (月份), %d (日), %H:%M:%S, %Y (年), %z (时区)
    std::strftime(time_buf, sizeof(time_buf), "%a %b %d %H:%M:%S %Y %z", tm);
    oss << "Date: " << time_buf << '\n';

    // 第 5 行: message
    oss << message << "\n\n";

    return oss.str();
}

// 访问器
std::string Commit::getId() const {
    return id;
}
std::string Commit::getMessage() const {
    return message;
}
std::time_t Commit::getTimestamp() const {
    return timestamp;
}
const std::vector<std::string>& Commit::getParents() const {
    return parents;
}
const std::map<std::string, std::string>& Commit::getBlobs() const {
    return blobs;
}
