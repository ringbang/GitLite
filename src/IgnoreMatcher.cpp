#include "../include/IgnoreMatcher.h"
#include "../include/Utils.h"

#include <sstream>
#include <string>
#include <vector>

// 在当前工作目录中, 若存在 .gitliteignore 文件, 则加载之
void IgnoreMatcher::LoadFromWorkingDirectory() {
    // 清空原本的规则
    rules.clear();

    // 如果不存在 .gitliteignore 文件, 则直接返回
    if (!Utils::isFile(".gitliteignore")) {
        return;
    }

    // 初始化
    std::string content = Utils::readContentsAsString(".gitliteignore");
    std::istringstream iss(content);
    std::string line = "";

    // 开始读取并解析文件内容作为忽略规则
    while (std::getline(iss, line)) {
        // 去除 Windows 换行可能残留的 '\r', 但不裁剪空格
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // 空行和以 # 开头的注释不产生效果
        if (line.empty() || line[0] == '#') {
            continue;
        }

        Rule rule{false, {}};
        size_t i = 0;
        size_t length = line.size(); 

        // 行首未转义的 ! 表示反选
        if (line[0] == '!') {
            rule.negated = true;
            i = 1;
        }

        // 规则开头的 / 表示从根目录开始匹配, 在本项目中无特殊效果
        if (i < length && line[i] == '/') {
            i++;
        }

        // 正式开始解析转义字符和通配符
        while (i < length) {
            // 转义得到的字面量
            if (line[i] == '\\' && i + 1 < length) {
                rule.pattern.push_back({TokenType::LITERAL, line[i + 1]});
                i += 2;
                continue;
            }
            // 通配符 *
            if (line[i] == '*') {
                rule.pattern.push_back({TokenType::STAR, 0});
                i++;
                continue;
            }
            // 通配符 ?
            if (line[i] == '?') {
                rule.pattern.push_back({TokenType::QUESTION, 0});
                i++;
                continue;
            }
            // 其他字符
            rule.pattern.push_back({TokenType::LITERAL, line[i]});
            i++;
        }
        rules.push_back(std::move(rule));
    }
}

// 判断某一文件名是否符合某一条特定的忽略规则
bool IgnoreMatcher::matchTokens(const std::vector<Token>& pat, const std::string& text) {
    // 二维布尔数组 dp[i][j] 表示模式 pat 的前 i 个 Token 
    // 能否完整匹配文件名 text 的前 j 个字符
    size_t m = pat.size(), n = text.size();
    std::vector<std::vector<bool>> dp(m + 1, std::vector<bool>(n + 1, false));
    dp[0][0] = true;  // 空模式匹配空文件名

    // 第一个 for 循环处理 dp[i][0]: 当文件名长度为 0 时, 
    // 只有模式前缀连续全是 * (可以匹配长度为零的空串), dp[i][0] 
    // 才能继承 dp[i - 1][0] 的 true 值
    for (size_t i = 1; i <= m; ++i) {
        if (pat[i - 1].type == TokenType::STAR) {
            dp[i][0] = dp[i - 1][0];
        }
    }

    // 状态转移
    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            if (pat[i - 1].type == TokenType::STAR) {
                // 当前 Token 为 *: dp[i][j] = dp[i - 1][j] || dp[i][j - 1],
                // 其中 dp[i - 1][j] 代表让当前 * 匹配 0 个字符 (直接丢弃 *), 
                // dp[i][j - 1] 代表让当前 * 吸收文件名的第 j 个字符并继续保留 * 
                // 匹配后续字符
                dp[i][j] = dp[i - 1][j] || dp[i][j - 1];
            } else if (pat[i - 1].type == TokenType::QUESTION) {
                // 当前 Token 为 ?: 必须恰好匹配任意 1 个字符,
                // 因此直接继承左上角状态 dp[i][j] = dp[i - 1][j - 1]
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                // 当前 Token 为 LITERAL: 除前序状态 dp[i - 1][j - 1] 
                // 为真外, 还要求当前字符严格相等 pat[i - 1].ch == text[j - 1]
                dp[i][j] = dp[i - 1][j - 1] && (pat[i - 1].ch == text[j - 1]);
            }
        }
    }
    return dp[m][n];
}

// 判断某一文件是否符合 .gitliteignore 的忽略规则
bool IgnoreMatcher::isIgnored(const std::string& filename) const {
    // 规范要求 “多条规则匹配同一文件时, 以文件中最后一条匹配规则为准”
    // 默认初始状态 ignored = false; 随后按行号从上到下遍历所有 rules,
    // 只要某条规则匹配成功, 就用该规则的语义覆盖 ignored (普通规则将其
    // 设为 true, 带 ! 的反选规则 !rule.negated 将其重新设回 false). 
    // 循环结束时保留的值, 自然就是最后一条匹配规则的决定
    bool ignored = false;
    for (const auto& rule: rules) {
        if (matchTokens(rule.pattern, filename)) {
            ignored = !rule.negated;
        }
    }
    return ignored;
}
