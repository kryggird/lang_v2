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

class Block;

using ValueType = int;
using VariableType = std::string;
using BlockType = int;
using Variables = std::unordered_map<VariableType, std::unordered_map<BlockType, int>>;

class Context {
public:
    using VariableCounters = std::unordered_map<VariableType, int>;

    void set(BlockType block, VariableType variable, ValueType value) {
        variable_list[variable][block] = value;
    };

    void set(BlockType block, VariableType variable) {
        auto& counter = this->variable_counters[variable];
        this->set(block, variable, counter);
        ++counter;
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
    VariableCounters variable_counters {};
};

bool is_expression(AstNode ast) {
    return true;
}

class Block {
public:
    Block (BlockType t_block_id): m_block_id {t_block_id} {};

    std::vector<Instruction> instructions {};

    VariableType push(Context& context, AstNode ast) {
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
    VariableType token_to_variable(Context& context, AstNode ast) {
        if (ast->name != "Identifier") {
            throw std::runtime_error("Cannot assign to rvalue!");
        }

        context.set(this->block_id(), ast->token);
        auto variable_id = context.get(this->block_id(), ast->token);

        std::ostringstream buffer {};
        buffer << ast->token << variable_id;

        return buffer.str();

    }

    VariableType push_constant(Context& context, AstNode ast) {
        if (ast->name != "Number") {
            throw std::runtime_error("Error converting!");
        }
        context.set(this->block_id(), "_");
        auto variable_id = context.get(this->block_id(), "_");
        std::ostringstream buffer {};
        buffer << "_" << variable_id;

        VariableType variable_name = buffer.str();
        instructions.push_back(Assignment {variable_name, ast->token});
        return variable_name;
    }

    VariableType push_identifier(Context& context, AstNode ast) {
        auto variable_id = context.get(this->block_id(), ast->token);

        std::ostringstream buffer {};
        buffer << ast->token << variable_id;

        return buffer.str();
    }

    VariableType push_operator(Context& context, AstNode ast) {
        // TODO Check ast name
        auto lhs_name = push(context, ast->nodes[0]);
        auto rhs_name = push(context, ast->nodes[1]);
        context.set(this->block_id(), "_");

        auto variable_id = context.get(this->block_id(), "_");
        std::ostringstream buffer {};
        buffer << "_" << variable_id;

        VariableType variable_name = buffer.str();
        instructions.push_back(ThreeAddressCode {variable_name, lhs_name, ast->name, rhs_name});
        return variable_name;
    }

    VariableType push_assigment(Context& context, AstNode ast) {
        VariableType rvalue_name = push(context, ast->nodes[1]);
        auto lvalue_name = token_to_variable(context, ast->nodes[0]);

        instructions.push_back(Assignment {lvalue_name, rvalue_name});
        return lvalue_name;
    }

    BlockType block_id() {
        return this->m_block_id;
    }

    const static std::unordered_set<std::string> operators;
    BlockType m_block_id;
};

const std::unordered_set<std::string> Block::operators = std::unordered_set<std::string> {"Addition", "Subtraction", "Multiplication", "Division"};

class Program {
public:
    void push(AstNode ast) {
        if (ast->name == "BasicBlock") {
            return this->push_basic_block(ast);
        } else if (ast->name == "IfBlock") {
            return this->push_if_block(ast);
        } else if (ast->name == "WhileBlock") {
            throw std::runtime_error("'while' not implemented yet!");
        } else {
            this->current_block()->push(context, ast);
        }
    }

    std::vector<Block> blocks {Block {0}};
private:
    void push_basic_block(AstNode ast) {
        for (auto& child: ast->nodes) {
            this->push(child);
        }
    }

    void push_if_block(AstNode ast) {
        this->current_block()->push(context, ast->nodes[0]);

        auto if_block = add_block();
        // TODO handle Phi functions!
        this->push(ast->nodes[1]);

        if (ast->nodes.size() > 2) {
            auto else_block = add_block();
            // TODO handle Phi functions!
            this->push(ast->nodes[2]);
        }

        add_block();
    }

    BlockType add_block() {
        ++m_current_block;
        blocks.push_back(Block {m_current_block});
        return m_current_block;
    }

    Block* current_block() {
        return &(this->blocks[this->m_current_block]);
    }

    Context context {};
    BlockType m_current_block = 0;
};

#endif //LANG_V2_CFG_H
