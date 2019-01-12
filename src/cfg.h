#ifndef LANG_V2_CFG_H
#define LANG_V2_CFG_H

#include <string>
#include <sstream>
#include <variant>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include <peglib.h>

// TODO Centralise type aliases?
using AstNode = std::shared_ptr<peg::Ast>;

struct ThreeAddressCode {
    std::string target;
    std::string lhs;
    std::string op;
    std::string rhs;

    friend std::ostream& operator<< (std::ostream& stream, const ThreeAddressCode& inst) {
        stream << inst.target << " <- " << inst.lhs << " " << inst.op << " " << inst.rhs;
        return stream;
    }
};

struct Assignment {
    std::string target;
    std::string value;

    friend std::ostream& operator<< (std::ostream& stream, const Assignment& inst) {
        stream << inst.target << " <- " << inst.value;
        return stream;
    }
};

using Instruction = std::variant<Assignment, ThreeAddressCode>;

std::ostream& operator<< (std::ostream& stream, const Instruction& inst) {
    std::visit([&stream](auto&& arg) { stream << arg; }, inst);
    return stream;
}

class Counter {
public:
    std::string operator()() {
        std::ostringstream buffer {};
        buffer << "l" << counter;
        ++counter;

        return buffer.str();
    }
private:
    int counter = 0;
};

class Tmp {
public:
    using ValueType = int;
    using VariableType = std::string;
    using BlockType = int;
    using Variables = std::unordered_map<VariableType, std::unordered_map<BlockType, int>>;

    void set(BlockType block, VariableType variable, ValueType value) {
        variable_list[variable][block] = value;
    };

    void set(BlockType block, VariableType variable) {
        bool var_exists = variable_list.count(variable) && variable_list[variable].count(block);
        auto value = var_exists ? this->get(block, variable) : 0;
        this->set(block, variable, ++value);
    }

    ValueType get(BlockType block, VariableType variable) {
        if (!variable_list.count(variable)) {
            throw std::runtime_error("Reading unknown variable!");
        }

        if (variable_list[variable].count(block)) {
            return variable_list[variable][block];
        } else {
            throw std::runtime_error("Not implemented!");
        }
    }
private:
    Variables variable_list {};
};

bool is_expression(AstNode ast) {
    return true;
}

class Block {
public:
    std::vector<Instruction> instructions {};

    std::string push(Tmp& context, AstNode ast) {
        if (ast->name == "Number") {
            return this->push_constant(context, ast);
        } else if (ast->name == "Identifier") {
            return this->push_identifier(context, ast);
        } else if (operators.count(ast->name)) {
            return this->push_operator(context, ast);
        } else if (ast->name == "Assignment"){
            return this->push_assigment(context, ast);
        } else {
            throw std::runtime_error("Not implemented!");
        }
    }
private:
    std::string token_to_variable(Tmp& context, AstNode ast) {
        if (ast->name != "Identifier") {
            throw std::runtime_error("Cannot assign to rvalue!");
        }

        context.set(block_id, ast->token);
        auto variable_id = context.get(block_id, ast->token);

        std::ostringstream buffer {};
        buffer << ast->token << variable_id;

        return buffer.str();

    }

    std::string push_constant(Tmp& context, AstNode ast) {
        if (ast->name != "Number") {
            throw std::runtime_error("Error converting!");
        }
        context.set(this->block_id, "_");
        auto variable_id = context.get(this->block_id, "_");
        std::ostringstream buffer {};
        buffer << "_" << variable_id;

        std::string variable_name = buffer.str();
        instructions.push_back(Assignment {variable_name, ast->token});
        return variable_name;
    }

    std::string push_identifier(Tmp& context, AstNode ast) {
        auto variable_id = context.get(block_id, ast->token);

        std::ostringstream buffer {};
        buffer << ast->token << variable_id;

        return buffer.str();
    }

    std::string push_operator(Tmp& context, AstNode ast) {
        // TODO Check ast name
        std::string lhs_name = push(context, ast->nodes[0]);
        std::string rhs_name = push(context, ast->nodes[1]);
        context.set(this->block_id, "_");

        auto variable_id = context.get(this->block_id, "_");
        std::ostringstream buffer {};
        buffer << "_" << variable_id;

        std::string variable_name = buffer.str();
        instructions.push_back(ThreeAddressCode {variable_name, lhs_name, ast->name, rhs_name});
        return variable_name;
    }

    std::string push_assigment(Tmp& context, AstNode ast) {
        std::string rvalue_name = push(context, ast->nodes[1]);
        auto lvalue_name = token_to_variable(context, ast->nodes[0]);

        instructions.push_back(Assignment {lvalue_name, rvalue_name});
        return lvalue_name;
    }

    const static std::unordered_set<std::string> operators;
    int block_id = 0;
};

const std::unordered_set<std::string> Block::operators = std::unordered_set<std::string> {"Addition", "Subtraction", "Multiplication", "Division"};

#endif //LANG_V2_CFG_H
