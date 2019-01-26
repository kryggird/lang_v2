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

const std::string EMPTY_VARIABLE = "";
const std::string SEPARATOR = "_";

class Block;

struct SSAVariable {
    std::string name;
    int counter;

    std::string to_string() const {
        std::ostringstream buffer;
        buffer << name << SEPARATOR << counter;
        return buffer.str();
    }

    bool operator==(const SSAVariable& rhs) const {
        return (this->name == rhs.name) && (this->counter == rhs.counter);
    }

    friend std::ostream& operator<< (std::ostream& stream, const SSAVariable& variable) {
        stream << variable.to_string();
        return stream;
    }
};

namespace std {
    template<>
    struct hash<SSAVariable> {
        std::size_t operator()(const SSAVariable& variable) const {
            size_t result = 17;
            result = result * 31 + hash<string>()(variable.name);
            result = result * 31 + hash<int>()(variable.counter);
            return result;
        }
    };
}

using BlockPtr = std::shared_ptr<Block>;

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

using ValueType = SSAVariable;
using VariableType = std::string;
using BlockType = Block*;
using Variables = std::unordered_map<VariableType, std::unordered_map<BlockType, int>>;

struct Phi {
    BlockType block; // FIXME Use a shared_ptr
    SSAVariable variable_name;
    std::unordered_set<SSAVariable> operands {};
    bool is_complete = false; // FIXME Ugly as fuck!!!

    bool is_trivial() {
        auto contains_self = this->operands.count(this->variable_name) > 0;
        return (operands.size() - contains_self) == 0;
    }

    friend std::ostream& operator<< (std::ostream& stream, const Phi& phi) {
        stream << phi.variable_name << " <- Phi(";
        for (auto& op: phi.operands) {
            stream << op;
        }
        stream << ")";
        return stream;
    }
};

class Context {
public:
    using VariableCounters = std::unordered_map<VariableType, int>;

    void set(BlockType block, VariableType variable, ValueType value) {
        this->set(block, variable, value.counter);
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
            return SSAVariable {variable, variable_list[variable][block]};
        } else {
            // TODO use a shared ptr?
            return get_recursive(block, variable);
        }
    }

    ValueType get_recursive(BlockType block, VariableType variable);

    void seal_block(BlockType block);

    bool is_sealed(BlockType block) {
        return (this->sealed_blocks.count(block) > 0);
    }

private:
    void set(BlockType block, VariableType variable, int counter) {
        // TODO Check against variable counters before setting?
        variable_list[variable][block] = counter;
    };

    Variables variable_list {};
    VariableCounters variable_counters {};
    std::unordered_set<Block*> sealed_blocks {};
};

class Block {
public:
    Block(std::unordered_set<BlockPtr> t_predecessors = {}): predecessors {t_predecessors} {};

    std::vector<Phi> phis {}; // Phis are always at the beginning of a basic block.
    std::vector<Instruction> instructions {};
    std::unordered_set<BlockPtr> predecessors {};

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

    friend std::ostream& operator<< (std::ostream& stream, const Block& block) {
        for (auto& phi: block.phis) {
            stream << phi << "\n";
        }
        for (auto& expr: block.instructions) {
            stream << expr << "\n";
        }
        return stream;
    }

private:
    VariableType token_to_variable(Context& context, AstNode ast) {
        if (ast->name != "Identifier") {
            throw std::runtime_error("Cannot assign to rvalue!");
        }

        context.set(this->block_id(), ast->token);
        auto variable_name = context.get(this->block_id(), ast->token).to_string();
        return variable_name;
    }

    VariableType push_constant(Context& context, AstNode ast) {
        if (ast->name != "Number") {
            throw std::runtime_error("Error converting!");
        }
        context.set(this->block_id(), EMPTY_VARIABLE);
        auto variable_name = context.get(this->block_id(), EMPTY_VARIABLE).to_string();
        instructions.push_back(Assignment {variable_name, ast->token});
        return variable_name;
    }

    VariableType push_identifier(Context& context, AstNode ast) {
        auto variable_name = context.get(this->block_id(), ast->token);

        return variable_name.to_string();
    }

    VariableType push_operator(Context& context, AstNode ast) {
        // TODO Check ast name
        auto lhs_name = push(context, ast->nodes[0]);
        auto rhs_name = push(context, ast->nodes[1]);
        context.set(this->block_id(), EMPTY_VARIABLE);

        auto variable_name = context.get(this->block_id(), EMPTY_VARIABLE).to_string();
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
        return this;
    }

    const static std::unordered_set<std::string> operators;
};

const std::unordered_set<std::string> Block::operators = std::unordered_set<std::string> {"Addition", "Subtraction", "Multiplication", "Division"};

ValueType Context::get_recursive(BlockType block, VariableType variable) {
    if (!is_sealed(block)) {
        set(block, variable);
        auto variable_name = get(block, variable);
        auto phi = Phi{block, variable_name}; // FIXME use a weak_ptr

        block->phis.push_back(phi);

        return variable_name;
    } else if (block->predecessors.size() == 1) {
        // TODO check not null?
        auto predecessor = *(block->predecessors.begin());
        auto variable_name = get(predecessor.get(), variable);
        set(block, variable_name.name, variable_name.counter);
        return variable_name;
    } else {
        set(block, variable);
        auto variable_name = get(block, variable);
        auto phi = Phi{block, variable_name}; // FIXME use a weak_ptr

        for (auto pred: block->predecessors) {
            auto variable_name = this->get(pred.get(), variable);
            phi.operands.insert(variable_name);
        }
        // TODO Remove trivial phis...

        block->phis.push_back(phi);
        return variable_name;
    }
    throw std::runtime_error("Not implemented!");
}

void Context::seal_block(BlockType block) {
    for (auto& phi: block->phis) {
        if (!phi.is_complete) {
            for (auto pred: block->predecessors) {
                auto variable_name = this->get(pred.get(), phi.variable_name.to_string());
                phi.operands.insert(variable_name);
            }
            phi.is_complete = true;
        }
    }
    this->sealed_blocks.insert(block);
}

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

    std::vector<BlockPtr> blocks {std::make_shared<Block>()};
private:
    void push_basic_block(AstNode ast) {
        for (auto& child: ast->nodes) {
            this->push(child);
        }
    }

    void push_if_block(AstNode ast) {
        this->current_block()->push(context, ast->nodes[0]);
        auto last_block = current_block();

        auto if_block = add_block({last_block});
        this->context.seal_block(if_block.get());
        this->push(ast->nodes[1]);
        std::unordered_set<BlockPtr> predecessors {if_block};

        if (ast->nodes.size() > 2) {
            auto else_block = add_block({last_block});
            this->context.seal_block(else_block.get());
            this->push(ast->nodes[2]);
            predecessors.insert(else_block);
        }

        add_block(predecessors);
        this->context.seal_block(this->current_block().get());
    }

    BlockPtr add_block(std::unordered_set<BlockPtr> predecessors = {}) {
        ++m_current_block;
        auto block = std::make_shared<Block>(predecessors);

        blocks.push_back(block);
        return block;
    }

    BlockPtr current_block() {
        return this->blocks[this->m_current_block];
    }

    Context context {};
    int m_current_block = 0;
};

#endif //LANG_V2_CFG_H
