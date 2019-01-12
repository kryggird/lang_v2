#include <algorithm>
#include <iostream>
#include <memory>
#include <unordered_set>
#include <stack>
#include <string>

#include <peglib.h>

#include "cfg.h"

using AstNode = std::shared_ptr<peg::Ast>;

AstNode prune(const std::unordered_set<std::string>& blacklist, AstNode root, AstNode parent = nullptr) {
    if (root == nullptr) {
        return nullptr;
    }
    AstNode result = std::make_shared<peg::Ast>(*root);
    result->nodes.clear();

    std::vector<AstNode> still_to_visit;
    std::copy(root->nodes.crbegin(), root->nodes.crend(), std::back_inserter(still_to_visit));

    while(!still_to_visit.empty()) {
        auto child = still_to_visit.back();
        still_to_visit.pop_back();
        if (blacklist.count(child->name)) {
            std::copy(child->nodes.crbegin(), child->nodes.crend(), std::back_inserter(still_to_visit));
        } else {
            AstNode pruned = prune(blacklist, child, root);
            pruned->parent = parent;
            result->nodes.push_back(pruned);
        }
    }
    return result;
}


int main() {
    auto grammar = R"(
        BasicBlock      <- SingleLineBlock (ExpressionEnd SingleLineBlock)*

        SingleLineBlock <- IfBlock / WhileBlock / Expression
        IfBlock         <- 'if' Expression ExpressionEnd BasicBlock (ExpressionEnd 'else' ExpressionEnd BasicBlock)? ExpressionEnd 'end'
        WhileBlock      <- 'while' Expression ExpressionEnd BasicBlock ExpressionEnd 'end'
        Expression      <- Assignment / Additive

        Additive        <- Addition / Subtraction / Multiplicative
        Multiplicative  <- Multiplication / Division / Primary
        Primary         <- '(' Additive ')' / Number / Identifier

        Assignment      <- 'var' Identifier '=' Additive
        Addition        <- Multiplicative '+' Additive
        Subtraction     <- Multiplicative '-' Additive
        Multiplication  <- Primary '*' Multiplicative
        Division        <- Primary '/' Multiplicative

        Number          <- < [0-9]+ >
        Identifier      <- < [a-zA-Z] [a-zA-Z0-9]* >

        ~ExpressionEnd  <- < [;\n] >
        %whitespace     <- [ \t]*
    )";

//    grammar = R"(
//        Identifier      <- < [a-zA-Z]+ >
//        %whitespace     <- [ \t\r\n]*
//    )";

    auto parser = peg::parser {grammar};
    parser.enable_ast();

    auto make_ast = parser["Identifier"].action;
    parser["Identifier"] = [make_ast](peg::SemanticValues& sv, peg::any& dt) {
        static std::unordered_set<std::string> reserved = {
            "while", "if", "else", "end", "var"
        };

        if (reserved.count(sv.token())) {
            throw peg::parse_error("Reserved keyword!");
        } else {
            auto result = make_ast(sv, dt);
            return result;
        }
    };

    AstNode ast;
//    auto text = R"(my)";
//    auto text = R"(var my = 7 + 5)";
//    auto text = R"(4 + 5 * 7; 5 + 4)";
    auto text = R"(if 5; 7; else; 8; 9 + 5; end)";
//    auto text = R"(while 1; 7; end)";

    std::unordered_set<std::string> blacklist = {"SingleLineBlock", "Expression", "Additive", "Multiplicative", "Primary"};
    parser.parse(text, ast);
    ast = prune(blacklist, ast);

    std::cout << peg::ast_to_s(ast) << "\n";

    Program program {};
    program.push(ast);

    for (auto& block: program.blocks) {
        std::cout << "--- new block ---\n";
        for (auto& expr: block.instructions) {
            std::cout << expr << "\n";
        }
    }

    return 0;
}