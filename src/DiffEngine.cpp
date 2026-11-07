#include "../include/DiffEngine.h"

#include <string>
#include <vector>
#include <optional>
#include <utility>
#include <algorithm>

// 辅助函数: 按 '\n' 切分字符串, 并记录每行是否有换行符
std::vector<DiffEngine::Line> DiffEngine::splitLines(const std::string& content) {
    std::vector<DiffEngine::Line> lines;
    // 若 content 为空, 直接返回
    if (content.empty()) {
        return lines;
    }
    
    // 否则 lines 非空, 逐字符扫描 content
    lines.push_back({"", false});
    for (size_t i = 0; i < content.size(); i++) {
        // 普通字符直接加入最后一个 line 的内容
        if (content[i] != '\n') {
            lines.back().text.push_back(content[i]);
        }
        // 遇到换行符置 has_newline 为 true, 并根据其后还有没有字符来确定
        // 是否需要增加新的 line
        else {
            lines.back().has_newline = true;
            if (i < content.size() - 1) {
                lines.push_back({"", false});
            }
        }
    }
    return lines;
}

// 辅助函数: 利用 LCS 生成编辑序列 (' ' 表示未变, '-' 表示删除, '+' 表示新增)
// 编辑序列的长度与行数相关, 每一项绑定一个 Line
std::vector<std::pair<char, DiffEngine::Line>> DiffEngine::computeEdits(
    const std::vector<Line>& a, 
    const std::vector<Line>& b
) {
    int m = a.size(), n = b.size();
    
    // 1. 构建 LCS 的二维 DP 表
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1, 0));
    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            // 行内容和换行状态都必须相同, 才算同一行
            if (a[i - 1].text == b[j - 1].text && a[i-1].has_newline == b[j - 1].has_newline) {
                dp[i][j] = dp[i - 1][j - 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i - 1][j], dp[i][j - 1]);
            }
        }
    }

    // 2. 回溯生成编辑序列
    std::vector<std::pair<char, Line>> edits;
    int i = m, j = n;
    while (i > 0 || j > 0) {
        if (i > 0 && j > 0 && a[i - 1].text == b[j - 1].text && a[i - 1].has_newline == b[j - 1].has_newline) {
            // 匹配: 保留该行
            edits.push_back({' ', a[i-1]});
            i--;
            j--;
        } else if (i > 0 && (j == 0 || dp[i - 1][j] > dp[i][j - 1])) {
            // 删除旧行 (平局时严格优先删除旧行)
            edits.push_back({'-', a[i - 1]});
            i--;
        } else {
            // 插入新行
            edits.push_back({'+', b[j - 1]});
            j--;
        }
    }

    // 3. 回溯是倒序的, 翻转成正序
    std::reverse(edits.begin(), edits.end());
    return edits;
}

// 辅助函数: 利用 LCS 生成编辑序列 (' ' 表示未变, '-' 表示删除, '+' 表示新增)
// 编辑序列的长度与行数相关, 每一项绑定一个 Line
std::string DiffEngine::formatHunks(const std::vector<std::pair<char, Line>>& edits) {
    std::string result;
    int n = edits.size();

    // 1. 标记哪些行需要被输出 (即变更行及前后 3 行)
    std::vector<bool> include(n, false);
    for (int i = 0; i < n; ++i) {
        if (edits[i].first == '+' || edits[i].first == '-') {
            int start = std::max(0, i - 3);
            int end = std::min(n - 1, i + 3);
            for (int j = start; j <= end; ++j) {
                include[j] = true;
            }
        }
    }

    // 2. 根据 include 数组划分 Hunk 的范围
    struct HunkRange {
        int start;
        int end;
    };
    std::vector<HunkRange> hunks;
    for (int i = 0; i < n; i++) {
        if (include[i]) {
            if (hunks.empty() || i > hunks.back().end + 1) {
                hunks.push_back({i, i});
            } else {
                hunks.back().end = i;
            }
        }
    }

    // 3. 计算全局行号并输出 Hunk
    int old_line = 1;  // 当前在旧文件的第几行
    int new_line = 1;  // 当前在新文件的第几行
    int last_processed = 0;

    for (const auto& h : hunks) {
        // 补齐被跳过 (不在这个 Hunk 里) 的行的行号计算
        for (int i = last_processed; i < h.start; ++i) {
            if (edits[i].first != '+') old_line++;
            if (edits[i].first != '-') new_line++;
        }

        int old_start = old_line;
        int new_start = new_line;
        int old_count = 0, new_count = 0;
        std::string body;

        for (int i = h.start; i <= h.end; ++i) {
            char type = edits[i].first;
            const Line& line = edits[i].second;

            body += type;
            body += line.text;
            if (!line.has_newline) {
                body += "\n\\ No newline at end of file\n";
            } else {
                body += "\n";
            }

            if (type != '+') {
                old_count++;
                old_line++;
            }
            if (type != '-') {
                new_count++;
                new_line++;
            }
        }

        // 拼装 Hunk 头
        result += "@@ -";
        if (old_count == 0) {
            result += "0,0";
        } else {
            result += std::to_string(old_start) + "," + std::to_string(old_count);
        }

        result += " +";
        if (new_count == 0) {
            result += "0,0";
        } else {
            result += std::to_string(new_start) + "," + std::to_string(new_count);
        }
        result += " @@\n";
        result += body;

        last_processed = h.end + 1;
    }

    return result;
}

// 核心入口: 生成单个文件的完整 diff 文本
// old_content / new_content: 文件的原始字节, std::nullopt 表示文件不存在
// filename: 用于打印文件头 (如 diff --gitlite a/f.txt b/f.txt)
std::string DiffEngine::generateDiff(
    const std::optional<std::string>& old_content,
    const std::optional<std::string>& new_content,
    const std::string& filename
) {
    // 1. 如果新旧文件内容完全一致 (包括两个都是 nullopt, 或者都是同一个字符串), 则无需输出
    if (old_content == new_content) {
        return "";
    }

    // 2. 拼接文件头
    std::string result = "diff --gitlite a/" + filename + " b/" + filename + "\n";
    
    if (old_content.has_value()) {
        result += "--- a/" + filename + "\n";
    } else {
        result += "--- /dev/null\n";
    }

    if (new_content.has_value()) {
        result += "+++ b/" + filename + "\n";
    } else {
        result += "+++ /dev/null\n";
    }

    // 3. 切分新旧行
    std::vector<Line> old_lines, new_lines;
    if (old_content.has_value()) {
        old_lines = splitLines(old_content.value());
    }
    if (new_content.has_value()) {
        new_lines = splitLines(new_content.value());
    }

    // 4. 计算编辑序列
    auto edits = computeEdits(old_lines, new_lines);

    // 5. 生成 Hunk 主体并拼接
    result += formatHunks(edits);

    return result;
}