#ifndef DIFF_ENGINE_H
#define DIFF_ENGINE_H

#include <string>
#include <vector>
#include <optional>
#include <utility>

// Bonus-2 专用: 按行拆分、LCS 算法与 Unified Diff 格式化
class DiffEngine {
public:
    // 核心入口: 生成单个文件的完整 diff 文本
    // old_content / new_content: 文件的原始字节, std::nullopt 表示文件不存在
    // filename: 用于打印文件头 (如 diff --gitlite a/f.txt b/f.txt)
    static std::string generateDiff(
        const std::optional<std::string>& old_content,
        const std::optional<std::string>& new_content,
        const std::string& filename
    );

private:
    // 内部数据结构: 记录每一行的内容和是否以换行符结尾
    struct Line {
        std::string text;
        bool has_newline;
    };

    // 辅助函数: 按 '\n' 切分字符串, 并记录每行是否有换行符
    static std::vector<Line> splitLines(const std::string& content);

    // 辅助函数: 利用 LCS 生成编辑序列 (' ' 表示未变, '-' 表示删除, '+' 表示新增)
    // 编辑序列的长度与行数相关, 每一项绑定一个 Line
    static std::vector<std::pair<char, Line>> computeEdits(
        const std::vector<Line>& a, 
        const std::vector<Line>& b
    );

    // 辅助函数: 将编辑序列聚合成 Hunk, 并格式化输出
    static std::string formatHunks(
        const std::vector<std::pair<char, Line>>& edits
    );
};

#endif // DIFF_ENGINE_H