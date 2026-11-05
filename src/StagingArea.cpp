#include "../include/StagingArea.h"
#include "../include/Utils.h"

#include <fstream>
#include <sstream>
#include <stdexcept>

// 暂存添加 / 修改: 加入 added, 同时取消其 removed 状态
void StagingArea::addFile(const std::string& filename, const std::string& blob_id) {
    added[filename] = blob_id;
    removed.erase(filename);
}

// 暂存删除: 加入 removed, 同时取消其 added 状态
void StagingArea::removeFile(const std::string& filename) {
    removed.insert(filename);
    added.erase(filename);
}

// 取消暂存 (例如 add 一个与当前提交完全相同的文件时调用)
void StagingArea::unstage(const std::string& filename) {
    removed.erase(filename);
    added.erase(filename);
}

// 清空暂存区 (commit 之后调用)
void StagingArea::clear() {
    removed.clear();
    added.clear();
}

// 获取数据供 Repository 遍历 (返回 const 引用避免拷贝)
const std::map<std::string, std::string>& StagingArea::getAddedMap() const {
    return added;
}
const std::set<std::string>& StagingArea::getRemovedSet() const {
    return removed;
}

// 状态查询
bool StagingArea::isAdded(const std::string& filename) const {
    return (added.count(filename) > 0);
};
bool StagingArea::isRemoved(const std::string& filename) const {
    return (removed.count(filename) > 0);
};
bool StagingArea::isEmpty() const {
    return (added.empty() && removed.empty());
};

// 获取暂存的文件对应的 Blob ID 
const std::string& StagingArea::getBlobId(const std::string& filename) const {
    // 调用前确保 isAdded 为 true
    if (!isAdded(filename)) {
        throw std::invalid_argument("File is not added.");
    }

    return added.at(filename);
};

// 将当前状态序列化并写入指定的完整路径 (.gitlite/index)
void StagingArea::save(const std::string& index_path) const {
    // 使用 std::ostringstream oss;
    std::ostringstream oss;
    // 先写入 added 的大小: oss << added.size() << '\n';
    // 遍历 added (因为 std::map 自带字典序, 序列化结果是确定的):
    // 写入文件名长度和内容: oss << filename.size() << '\n' << filename;
    // 写入 Blob ID 长度和内容: oss << blob_id.size() << '\n' << blob_id;
    oss << added.size() << '\n';
    for (const auto& added_file: added) {
        oss << added_file.first.size() << '\n' << added_file.first << '\n';
        oss << added_file.second.size() << '\n' << added_file.second << '\n';
    }
    // 接着写入 removed 的大小: oss << removed.size() << '\n';
    // 遍历 removed:
    // 写入文件名长度和内容: oss << filename.size() << '\n' << filename;
    oss << removed.size() << '\n';
    for (const auto& removed_file: removed) {
        oss << removed_file.size() << '\n' << removed_file << '\n';
    }
    // 最后, 用 Utils::writeContents(index_path, oss.str()); 写入磁盘
    Utils::writeContents(index_path, oss.str());
}

// 从指定的完整路径 (.gitlite/index) 加载状态
void StagingArea::load(const std::string& index_path) {
    // 先调用 clear() 清空当前状态
    clear();
    // 检查 index_path 是否存在 (Utils::isFile), 如果不存在直接返回 (说明是空的暂存区)
    if (!Utils::isFile(index_path)) {
        return;
    }
    // std::istringstream iss(Utils::readContentsAsString(index_path));
    std::istringstream iss(Utils::readContentsAsString(index_path));
    // 读取 added 的数量 (注意读完之后要 iss.get() 吃掉换行符)
    // 用一个 for 循环读取 added:
    // 读长度 size_t name_length; iss >> name_length; iss.get(); (吃换行)
    // 读文件名 std::string filename(name_length, ' '); iss.read(&filename[0], name_length);
    // 同样方法读 Blob ID
    // 存入 added
    size_t added_size = 0, removed_size = 0;
    iss >> added_size;
    iss.get();
    for (size_t i = 0; i < added_size; i++) {
        size_t name_length = 0, id_length = 0;
        iss >> name_length;
        iss.get();
        std::string filename(name_length, ' ');
        iss.read(&filename[0], name_length);
        iss.get();
        iss >> id_length;
        iss.get();
        std::string id(id_length, ' ');
        iss.read(&id[0], id_length);
        iss.get();
        added[filename] = id;
    }
    // 用同样的逻辑读取 removed 的数量并循环读入 removed
    iss >> removed_size;
    iss.get();
    for (size_t i = 0; i < removed_size; i++) {
        size_t length = 0;
        iss >> length;
        iss.get();
        std::string filename(length, ' ');
        iss.read(&filename[0], length);
        iss.get();
        removed.insert(filename);
    }
}