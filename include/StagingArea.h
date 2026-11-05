#ifndef STAGING_AREA_H
#define STAGING_AREA_H

#include <string>
#include <map>
#include <set>

// 暂存区 (Index): 管理待添加映射与待删除集合的存取
class StagingArea {
public:
    StagingArea() = default;

    // 从指定的完整路径 (.gitlite/index) 加载状态
    void load(const std::string& index_path);

    // 将当前状态序列化并写入指定的完整路径 (.gitlite/index)
    void save(const std::string& index_path) const;

    // 暂存添加 / 修改: 加入 added, 同时取消其 removed 状态
    void addFile(const std::string& filename, const std::string& blob_id);

    // 暂存删除: 加入 removed, 同时取消其 added 状态
    void removeFile(const std::string& filename);

    // 取消暂存 (例如 add 一个与当前提交完全相同的文件时调用)
    void unstage(const std::string& filename);

    // 清空暂存区 (commit 之后调用)
    void clear();

    // 状态查询
    bool isAdded(const std::string& filename) const;
    bool isRemoved(const std::string& filename) const;
    bool isEmpty() const;
    
    // 获取数据供 Repository 遍历 (返回 const 引用避免拷贝)
    const std::map<std::string, std::string>& getAddedMap() const;
    const std::set<std::string>& getRemovedSet() const;
    // 获取暂存的文件对应的 Blob ID (调用前确保 isAdded 为 true)
    const std::string& getBlobId(const std::string& filename) const;

private:
    std::map<std::string, std::string> added;
    std::set<std::string> removed;
};

#endif // STAGING_AREA_H