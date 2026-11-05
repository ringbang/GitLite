#ifndef IGNORE_MATCHER_H
#define IGNORE_MATCHER_H

#include <string>
#include <vector>

// .gitliteignore 规则解析器与通配符匹配引擎
class IgnoreMatcher {
public:
    IgnoreMatcher() = default;

    // 在当前工作目录中, 若存在 .gitliteignore 文件, 则加载之
    void LoadFromWorkingDirectory();
 
    // 判断文件名是否匹配忽略规则 (仅做纯规则匹配, 调用方需先排除已跟踪 / 已暂存文件)
    bool isIgnored(const std::string& filename) const;

private:
    enum class TokenType {
        LITERAL,     // 字面量, ch 存储其值
        QUESTION,    // ? 匹配任意单个字符, ch = 0
        STAR         // * 匹配任意字符序列, ch = 0
    };
    // 以 Token 这一结构体解析每一行的每个字符 (某种意义上相当于 char), 消歧义
    struct Token {
        TokenType type;
        char ch;
    };
    // 每一条 Rule 即 .gitliteignore 中的单行规则, pattern 为其具体内容
    struct Rule {
        bool negated;  // 是否以 ! 开头表示反选
        std::vector<Token> pattern;
    };

    std::vector<Rule> rules;  // 表示整个 .gitliteignore 的规则
    // 看某个文件名是否与单条特定规则匹配
    static bool matchTokens(const std::vector<Token>& pattern, const std::string& text);
};


#endif // IGNORE_MATCHER_H