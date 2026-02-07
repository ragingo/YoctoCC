#include "Parser.hpp"

#include <cassert>
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Type.hpp"

using namespace std::string_view_literals;

namespace {
    using namespace yoctocc;

    struct VariableScope {
        std::string name;
        std::unique_ptr<VariableScope> next;
        Object* variable = nullptr;
    };

    struct Scope {
        std::unique_ptr<VariableScope> variable;
        std::unique_ptr<Scope> next;
    };

    std::unique_ptr<Scope> currentScope = std::make_unique<Scope>();

    void enterScope() {
        auto scope = std::make_unique<Scope>();
        scope->next = std::move(currentScope);
        currentScope = std::move(scope);
    }

    void leaveScope() {
        assert(currentScope);
        currentScope = std::move(currentScope->next);
    }

    void pushVariableScope(const std::string& name, Object* variable) {
        auto variableScope = std::make_unique<VariableScope>();
        variableScope->name = name;
        variableScope->variable = variable;
        variableScope->next = std::move(currentScope->variable);
        currentScope->variable = std::move(variableScope);
    }

    bool consumeToken(Token*& token, std::string_view value) {
        if (token::is(token, value)) {
            token = token->next.get();
            return true;
        }
        return false;
    }

    inline const std::string& getIdentifier(const Token* token) {
        assert(token && token->kind == TokenKind::IDENTIFIER);
        return token->originalValue;
    }

    inline int getNumber(const Token* token) {
        assert(token && token->kind == TokenKind::DIGIT);
        return token->numberValue;
    }

    const std::shared_ptr<Type> declSpec(Token*& token) {
        if (token::is(token, "char")) {
            token = token->next.get();
            return type::charType();
        }
        token = token::skipIf(token, "int");
        return type::intType();
    }

    const std::shared_ptr<Type> typeSuffix(Token*& token, std::shared_ptr<Type>& type);

    // declarator = "*"* ident type-suffix
    const std::shared_ptr<Type> declarator(Token*& token, std::shared_ptr<Type>& type) {
        while (consumeToken(token, "*")) {
            type = type::pointerTo(type);
        }

        if (token->kind == TokenKind::IDENTIFIER) {
            auto name = token;
            token = token->next.get();
            type = typeSuffix(token, type);
            type->name = name;
        } else {
            Log::error("Expected an identifier"sv, token);
        }

        return type;
    }

    // func-params = (param ("," param)*)? ")"
    // param       = declspec declarator
    const std::shared_ptr<Type> functionParameters(Token*& token, std::shared_ptr<Type>& type) {
        std::shared_ptr<Type> head;
        auto current = &head;

        while (!token::is(token, ")")) {
            if (head) {
                token = token::skipIf(token, ",");
            }
            auto paramType = declSpec(token);
            paramType = declarator(token, paramType);
            *current = paramType;
            current = &paramType->next;
        }

        type = type::functionType(type);
        type->parameters = head;
        token = token->next.get();

        return type;
    }

    // type-suffix = "(" func-params
    //             | "[" num "]" type-suffix
    //             | ε
    const std::shared_ptr<Type> typeSuffix(Token*& token, std::shared_ptr<Type>& type) {
        if (token::is(token, "(")) {
            token = token->next.get();
            return functionParameters(token, type);
        }

        if (token::is(token, "[")) {
            int size = getNumber(token->next.get());
            token = token->next.get()->next.get();
            token = token::skipIf(token, "]");
            type = typeSuffix(token, type);
            return type::arrayOf(type, size);
        }

        return type;
    }

    bool isFunction(Token* token) {
        if (token::is(token, ";")) {
            return false;
        }
        auto dummy = std::make_shared<Type>(TypeKind::UNKNOWN);
        auto type = declarator(token, dummy);
        return type->kind == TypeKind::FUNCTION;
    }

    std::string makeUniqueName() {
        static int count = 0;
        return std::format(".L..{}", count++);
    }
}

namespace yoctocc {

// program = (function-definition | global-variable)*
std::unique_ptr<Object> Parser::parse(Token* token) {
    assert(token);
    _globals = nullptr;
    while (token->kind != TokenKind::TERMINATOR) {
        auto baseType = declSpec(token);
        if (isFunction(token)) {
            token = parseFunction(token, baseType);
            continue;
        }
        token = parseGlobalVariable(token, baseType);
    }
    return std::move(_globals);
}

Object* Parser::findVariable(const Token* token) {
    for (Scope* scope = currentScope.get(); scope; scope = scope->next.get()) {
        for (VariableScope* vs = scope->variable.get(); vs; vs = vs->next.get()) {
            if (vs->name == token->originalValue) {
                return vs->variable;
            }
        }
    }
    return nullptr;
}

Object* Parser::createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, true);
    Object* raw = var.get();
    var->next = std::move(_locals);
    pushVariableScope(name, raw);
    _locals = std::move(var);
    return raw;
}

Object* Parser::createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, false);
    Object* raw = var.get();
    var->next = std::move(_globals);
    pushVariableScope(name, raw);
    _globals = std::move(var);
    return raw;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
ParseResult Parser::declaration(Token* token) {
    auto type = declSpec(token);
    auto head = std::make_unique<Node>(NodeType::UNKNOWN, token);
    Node* current = head.get();

    int i = 0;

    while (!token::is(token, ";")) {
        if (i++ > 0) {
            token = token::skipIf(token, ",");
        }
        auto varType = declarator(token, type);
        auto varName = getIdentifier(varType->name);
        auto var = createLocalVariable(varName, varType);

        if (!token::is(token, "=")) {
            continue;
        }

        auto lhs = createVariableNode(token, var);
        auto [rhs, rest] = parseAssignment(token->next.get());
        token = rest;
        auto assignNode = createBinaryNode(NodeType::ASSIGN, token, std::move(lhs), std::move(rhs));
        current->next = createUnaryNode(NodeType::EXPRESSION_STATEMENT, token, std::move(assignNode));
        current = current->next.get();
    }

    auto node = createBlockNode(token, std::move(head->next));
    return {std::move(node), token->next.get()};
}

// expr = assign ("," expr)?
ParseResult Parser::parseExpression(Token* token) {
    auto [node, rest] = parseAssignment(token);

    if (token::is(rest, ",")) {
        auto [right, rest2] = parseExpression(rest->next.get());
        return {createBinaryNode(NodeType::COMMA, rest, std::move(node), std::move(right)), rest2};
    }

    return {std::move(node), rest};
}

// assign = equality ("=" assign)?
ParseResult Parser::parseAssignment(Token* token) {
    auto [node, rest] = parseEquality(token);

    if (token::is(rest, "=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        return {createBinaryNode(NodeType::ASSIGN, start, std::move(node), std::move(right)), rest2};
    }

    return {std::move(node), rest};
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//      | "while" "(" expr ")" stmt
//      | "{" compound-stmt
//      | expr-stmt
ParseResult Parser::parseStatement(Token* token) {
    if (token::is(token, "return")) {
        auto start = token;
        auto [expr, rest] = parseExpression(token->next.get());
        rest = token::skipIf(rest, ";");
        return {createUnaryNode(NodeType::RETURN, start, std::move(expr)), rest};
    }

    if (token::is(token, "if")) {
        auto node = std::make_unique<Node>(NodeType::IF, token);
        token = token::skipIf(token->next.get(), "(");

        auto [cond, afterCond] = parseExpression(token);
        node->condition = std::move(cond);
        token = token::skipIf(afterCond, ")");

        auto [thenStmt, afterThen] = parseStatement(token);
        node->then = std::move(thenStmt);
        token = afterThen;

        if (token::is(token, "else")) {
            auto [elseStmt, afterElse] = parseStatement(token->next.get());
            node->els = std::move(elseStmt);
            token = afterElse;
        }
        return {std::move(node), token};
    }

    if (token::is(token, "for")) {
        auto node = std::make_unique<Node>(NodeType::FOR, token);
        token = token::skipIf(token->next.get(), "(");

        auto [initStmt, afterInit] = parseExpressionStatement(token);
        node->init = std::move(initStmt);
        token = afterInit;

        if (!token::is(token, ";")) {
            auto [cond, afterCond] = parseExpression(token);
            node->condition = std::move(cond);
            token = afterCond;
        }
        token = token::skipIf(token, ";");

        if (!token::is(token, ")")) {
            auto [inc, afterInc] = parseExpression(token);
            node->inc = std::move(inc);
            token = afterInc;
        }
        token = token::skipIf(token, ")");

        auto [body, afterBody] = parseStatement(token);
        node->then = std::move(body);
        return {std::move(node), afterBody};
    }

    if (token::is(token, "while")) {
        auto node = std::make_unique<Node>(NodeType::FOR, token);
        token = token::skipIf(token->next.get(), "(");

        auto [cond, afterCond] = parseExpression(token);
        node->condition = std::move(cond);
        token = token::skipIf(afterCond, ")");

        auto [body, afterBody] = parseStatement(token);
        node->then = std::move(body);
        return {std::move(node), afterBody};
    }

    if (token::is(token, "{")) {
        return parseCompoundStatement(token->next.get());
    }

    return parseExpressionStatement(token);
}

// compound-stmt = (declaration | stmt)* "}"
ParseResult Parser::parseCompoundStatement(Token* token) {
    auto head = std::make_unique<Node>(NodeType::UNKNOWN, token);
    Node* current = head.get();

    enterScope();

    while (token->kind != TokenKind::TERMINATOR && !token::is(token, "}")) {
        if (type::isTypeName(token)) {
            auto [decl, rest] = declaration(token);
            current->next = std::move(decl);
            token = rest;
        } else {
            auto [stmt, rest] = parseStatement(token);
            current->next = std::move(stmt);
            token = rest;
        }
        current = current->next.get();
        type::addType(current);
    }

    leaveScope();

    auto node = createBlockNode(token, std::move(head->next));
    return {std::move(node), token->next.get()};
}

// expr-stmt = expr? ";"
ParseResult Parser::parseExpressionStatement(Token* token) {
    if (token::is(token, ";")) {
        return {createBlockNode(token), token->next.get()};
    }

    auto [expr, rest] = parseExpression(token);
    return {createUnaryNode(NodeType::EXPRESSION_STATEMENT, token, std::move(expr)), token::skipIf(rest, ";")};
}

// equality = relational ("==" relational | "!=" relational)*
ParseResult Parser::parseEquality(Token* token) {
    auto [node, rest] = parseRelational(token);
    token = rest;

    while (true) {
        if (token::is(token, "==")) {
            auto start = token;
            auto [right, r] = parseRelational(token->next.get());
            node = createBinaryNode(NodeType::EQUAL, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, "!=")) {
            auto start = token;
            auto [right, r] = parseRelational(token->next.get());
            node = createBinaryNode(NodeType::NOT_EQUAL, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        return {std::move(node), token};
    }
}

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
ParseResult Parser::parseRelational(Token* token) {
    auto [node, rest] = parseAdditive(token);
    token = rest;

    while (true) {
        if (token::is(token, "<")) {
            auto start = token;
            auto [right, r] = parseAdditive(token->next.get());
            node = createBinaryNode(NodeType::LESS, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, "<=")) {
            auto start = token;
            auto [right, r] = parseAdditive(token->next.get());
            node = createBinaryNode(NodeType::LESS_EQUAL, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, ">")) {
            auto start = token;
            auto [right, r] = parseAdditive(token->next.get());
            node = createBinaryNode(NodeType::GREATER, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, ">=")) {
            auto start = token;
            auto [right, r] = parseAdditive(token->next.get());
            node = createBinaryNode(NodeType::GREATER_EQUAL, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        return {std::move(node), token};
    }
}

// add = mul ("+" mul | "-" mul)*
ParseResult Parser::parseAdditive(Token* token) {
    auto [node, rest] = parseMultiply(token);
    token = rest;

    while (true) {
        if (token::is(token, "+")) {
            auto start = token;
            auto [right, r] = parseMultiply(token->next.get());
            node = createAddNode(start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, "-")) {
            auto start = token;
            auto [right, r] = parseMultiply(token->next.get());
            node = createSubNode(start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        return {std::move(node), token};
    }
}

// mul = unary ("*" unary | "/" unary)*
ParseResult Parser::parseMultiply(Token* token) {
    auto [node, rest] = parseUnary(token);
    token = rest;

    while (true) {
        if (token::is(token, "*")) {
            auto start = token;
            auto [right, r] = parseUnary(token->next.get());
            node = createBinaryNode(NodeType::MUL, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, "/")) {
            auto start = token;
            auto [right, r] = parseUnary(token->next.get());
            node = createBinaryNode(NodeType::DIV, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        return {std::move(node), token};
    }
}

// unary = ("+" | "-" | "*" | "&") unary
//       | postfix
ParseResult Parser::parseUnary(Token* token) {
    if (token::is(token, "+")) {
        return parsePrimary(token->next.get());
    }
    if (token::is(token, "-")) {
        auto start = token;
        auto [operand, rest] = parseUnary(token->next.get());
        return {createUnaryNode(NodeType::NEGATE, start, std::move(operand)), rest};
    }
    if (token::is(token, "&")) {
        auto start = token;
        auto [operand, rest] = parseUnary(token->next.get());
        return {createUnaryNode(NodeType::ADDRESS, start, std::move(operand)), rest};
    }
    if (token::is(token, "*")) {
        auto start = token;
        auto [operand, rest] = parseUnary(token->next.get());
        return {createUnaryNode(NodeType::DEREFERENCE, start, std::move(operand)), rest};
    }
    return parsePostfix(token);
}

// postfix = primary ("[" expr "]")*
ParseResult Parser::parsePostfix(Token* token) {
    auto [node, rest] = parsePrimary(token);
    token = rest;

    while (token::is(token, "[")) {
        auto start = token;
        auto [index, r] = parseExpression(token->next.get());
        node = createAddNode(start, std::move(node), std::move(index));
        node = createUnaryNode(NodeType::DEREFERENCE, start, std::move(node));
        token = token::skipIf(r, "]");
    }
    return {std::move(node), token};
}

// funcall = ident "(" (assign ("," assign)*)? ")"
ParseResult Parser::parseFunctionCall(Token* token) {
    auto start = token;
    token = token->next.get()->next.get(); // 関数名と"("をスキップ
    auto head = std::make_unique<Node>(NodeType::UNKNOWN, token);
    Node* current = head.get();

    while (!token::is(token, ")")) {
        if (current != head.get()) {
            token = token::skipIf(token, ",");
        }
        auto [arg, rest] = parseAssignment(token);
        current->next = std::move(arg);
        current = current->next.get();
        token = rest;
    }

    token = token::skipIf(token, ")");

    auto node = std::make_unique<Node>(NodeType::FUNCTION_CALL, start);
    node->functionName = getIdentifier(start);
    node->arguments = std::move(head->next);

    return {std::move(node), token};
}

Token* Parser::parseFunction(Token* token, std::shared_ptr<Type>& baseType) {
    auto funcType = declarator(token, baseType);
    auto name = getIdentifier(funcType->name);
    auto func = makeFunction(name, funcType);
    _locals.reset();

    enterScope();

    token = token::skipIf(token, "{");
    applyParamLVars(funcType->parameters);
    func->parameters = _locals.get();

    auto [body, rest] = parseCompoundStatement(token);
    func->body = std::move(body);
    token = rest;

    func->locals = std::move(_locals);
    func->next = std::move(_globals);
    _globals = std::move(func);

    leaveScope();

    return token;
}

Token* Parser::parseGlobalVariable(Token* token, std::shared_ptr<Type>& baseType) {
    bool isFirst = true;

    while (!consumeToken(token, ";")) {
        if (!isFirst) {
            token = token::skipIf(token, ",");
        }
        isFirst = false;
        auto varType = declarator(token, baseType);
        auto varName = getIdentifier(varType->name);
        createGlobalVariable(varName, varType);
    }

    return token;
}

// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")"
//         | "sizeof" unary
//         | ident func-args?
//         | str
//         | num
ParseResult Parser::parsePrimary(Token* token) {
    if (token::is(token, "(") && token::is(token->next.get(), "{")) {
        auto node = std::make_unique<Node>(NodeType::STATEMENT_EXPRESSION, token);
        auto [block, rest] = parseCompoundStatement(token->next.get()->next.get());
        node->body = std::move(block->body);
        return {std::move(node), token::skipIf(rest, ")")};
    }

    if (token::is(token, "(")) {
        auto [expr, rest] = parseExpression(token->next.get());
        return {std::move(expr), token::skipIf(rest, ")")};
    }

    if (token::is(token, "sizeof")) {
        auto [operand, rest] = parseUnary(token->next.get());
        type::addType(operand.get());
        return {createNumberNode(token, operand->type->size), rest};
    }

    if (token->kind == TokenKind::IDENTIFIER) {
        // function
        if (token::is(token->next.get(), "(")) {
            return parseFunctionCall(token);
        }

        // variable
        auto var = findVariable(token);
        if (!var) {
            Log::error(std::format("Undefined variable: {}", token->originalValue), token);
            return {nullptr, token};
        }
        return {createVariableNode(token, var), token->next.get()};
    }

    if (token->kind == TokenKind::STRING) {
        auto var = createGlobalVariable(makeUniqueName(), token->type);
        var->initialData = token->originalValue;
        return {createVariableNode(token, var), token->next.get()};
    }

    if (token->kind == TokenKind::DIGIT) {
        return {createNumberNode(token, token->numberValue), token->next.get()};
    }

    Log::error("Expected an expression"sv, token);
    return {nullptr, token};
}

void Parser::applyParamLVars(const std::shared_ptr<Type>& parameter) {
    if (parameter) {
        applyParamLVars(parameter->next);
        createLocalVariable(getIdentifier(parameter->name), parameter);
    }
}

} // namespace yoctocc
