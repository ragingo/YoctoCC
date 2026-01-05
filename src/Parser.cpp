#include "Parser.hpp"

#include <cassert>
#include "Logger.hpp"
#include "Node/Node.hpp"
#include "Token.hpp"
#include "Type.hpp"

namespace {
    using namespace yoctocc;

    bool consumeToken(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::string_view value) {
        if (token::is(token, value)) {
            result = token->next;
            return true;
        }
        result = token;
        return false;
    }

    inline const std::string& getIdentifier(const std::shared_ptr<Token>& token) {
        assert(token && token->type == TokenType::IDENTIFIER);
        return token->originalValue;
    }

    inline int getNumber(const std::shared_ptr<Token>& token) {
        assert(token && token->type == TokenType::DIGIT);
        return token->numberValue;
    }

    const std::shared_ptr<Type> declSpec(std::shared_ptr<Token>& result, const std::shared_ptr<Token>& token) {
        result = token::skipIf(token, "int");
        return type::intType();
    }

    const std::shared_ptr<Type> typeSuffix(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type);

    const std::shared_ptr<Type> declarator(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type) {
        while (consumeToken(result, token, "*")) {
            type = type::pointerTo(type);
        }

        if (token->type == TokenType::IDENTIFIER) {
            auto name = token;
            type = typeSuffix(result, token->next, type);
            type->name = name;
        } else {
            using namespace std::literals;
            Log::error(token->location, "Expected an identifier"sv);
        }

        return type;
    }

    const std::shared_ptr<Type> functionParameters(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type) {
        std::shared_ptr<Type> head;
        auto current = &head;

        while (!token::is(token, ")")) {
            if (head) {
                token = token::skipIf(token, ",");
            }
            auto paramType = declSpec(token, token);
            paramType = declarator(token, token, paramType);
            *current = paramType;
            current = &paramType->next;
        }

        type = type::functionType(type);
        type->parameters = head;
        result = token->next;

        return type;
    }

    const std::shared_ptr<Type> typeSuffix(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type) {
        if (token::is(token, "(")) {
            return functionParameters(result, token->next, type);
        }

        if (token::is(token, "[")) {
            int size = getNumber(token->next);
            token = token::skipIf(token->next->next, "]");
            type = typeSuffix(result, token, type);
            return type::arrayOf(type, size);
        }

        result = token;
        return type;
    }
}

namespace yoctocc {

std::shared_ptr<Object> Parser::parse(std::shared_ptr<Token>& token) {
    assert(token);
    _globals = nullptr;
    while (token->type != TokenType::TERMINATOR) {
        auto baseType = declSpec(token, token);
        token = parseFunction(token, baseType);
    }
    return _globals;
}

std::shared_ptr<Object> Parser::findLocalVariable(std::shared_ptr<Token>& token) {
    for (auto var = _locals; var; var = var->next) {
        if (var->name == token->originalValue) {
            return var;
        }
    }
    return nullptr;
}

std::shared_ptr<Object> Parser::createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, true);
    var->next = _locals;
    _locals = var;
    return var;
}

std::shared_ptr<Object> Parser::createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, false);
    var->next = _globals;
    _globals = var;
    return var;
}

std::shared_ptr<Node> Parser::declaration(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto type = declSpec(token, token);
    auto head = std::make_shared<Node>();
    auto current = head;

    int i = 0;

    while (!token::is(token, ";")) {
        if (i++ > 0) {
            token = token::skipIf(token, ",");
        }
        auto varType = declarator(token, token, type);
        auto varName = getIdentifier(type->name);
        auto var = createLocalVariable(varName, varType);

        if (!token::is(token, "=")) {
            continue;
        }

        auto lhs = createVariableNode(token, var);
        auto rhs = parseAssignment(token, token->next);
        auto node = createBinaryNode(NodeType::ASSIGN, token, lhs, rhs);
        current = current->next = createUnaryNode(NodeType::EXPRESSION_STATEMENT, token, node);
    }

    auto node = createBlockNode(token, head->next);
    result = token->next;
    return node;
}

std::shared_ptr<Node> Parser::parseExpression(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    return parseAssignment(result, token);
}

std::shared_ptr<Node> Parser::parseAssignment(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto node = parseEquality(token, token);

    if (token::is(token, "=")) {
        node = createBinaryNode(NodeType::ASSIGN, token, node, parseAssignment(token, token->next));
    }

    result = token;
    return node;
}

std::shared_ptr<Node> Parser::parseStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    if (token::is(token, "return")) {
        auto node = createUnaryNode(NodeType::RETURN, token, parseExpression(token, token->next));
        if (token->originalValue == ";") {
            token = token->next;
        }
        result = token;
        return node;
    }

    if (token::is(token, "if")) {
        auto node = std::make_shared<Node>(NodeType::IF);
        token = token::skipIf(token->next, "(");
        node->condition = parseExpression(token, token);
        token = token::skipIf(token, ")");
        node->then = parseStatement(token, token);
        if (token::is(token, "else")) {
            node->els = parseStatement(token, token->next);
        }
        result = token;
        return node;
    }

    if (token::is(token, "for")) {
        auto node = std::make_shared<Node>(NodeType::FOR);
        token = token::skipIf(token->next, "(");
        node->init = parseExpressionStatement(token, token);

        if (!token::is(token, ";")) {
            node->condition = parseExpression(token, token);
        }
        token = token::skipIf(token, ";");

        if (!token::is(token, ")")) {
            node->inc = parseExpression(token, token);
        }
        token = token::skipIf(token, ")");

        node->then = parseStatement(result, token);
        return node;
    }

    if (token::is(token, "while")) {
        auto node = std::make_shared<Node>(NodeType::FOR);
        token = token::skipIf(token->next, "(");
        node->condition = parseExpression(token, token);
        token = token::skipIf(token, ")");
        node->then = parseStatement(result, token);
        return node;
    }

    if (token::is(token, "{")) {
        return parseCompoundStatement(result, token->next);
    }

    return parseExpressionStatement(result, token);
}

std::shared_ptr<Node> Parser::parseCompoundStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto head = std::make_shared<Node>();
    auto current = head;
    while (token->type != TokenType::TERMINATOR && !token::is(token, "}")) {
        if (token::is(token, "int")) {
            current = current->next = declaration(token, token);
        } else {
            current = current->next = parseStatement(token, token);
        }
        type::addType(current);
    }
    result = token->next;
    auto node = createBlockNode(token, head->next);
    return node;
}

std::shared_ptr<Node> Parser::parseExpressionStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    if (token::is(token, ";")) {
        result = token->next;
        return createBlockNode(token);
    }

    auto node = createUnaryNode(NodeType::EXPRESSION_STATEMENT, token, parseExpression(token, token));
    result = token::skipIf(token, ";");
    return node;
}

std::shared_ptr<Node> Parser::parseEquality(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto node = parseRelational(token, token);

    while (true) {
        if (token::is(token, "==")) {
            node = createBinaryNode(NodeType::EQUAL, token, node, parseRelational(token, token->next));
            continue;
        }
        if (token::is(token, "!=")) {
            node = createBinaryNode(NodeType::NOT_EQUAL, token, node, parseRelational(token, token->next));
            continue;
        }
        result = token;
        return node;
    }
}

std::shared_ptr<Node> Parser::parseRelational(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto node = parseAdditive(token, token);

    while (true) {
        if (token::is(token, "<")) {
            node = createBinaryNode(NodeType::LESS, token, node, parseAdditive(token, token->next));
            continue;
        }
        if (token::is(token, "<=")) {
            node = createBinaryNode(NodeType::LESS_EQUAL, token, node, parseAdditive(token, token->next));
            continue;
        }
        if (token::is(token, ">")) {
            node = createBinaryNode(NodeType::GREATER, token, node, parseAdditive(token, token->next));
            continue;
        }
        if (token::is(token, ">=")) {
            node = createBinaryNode(NodeType::GREATER_EQUAL, token, node, parseAdditive(token, token->next));
            continue;
        }
        result = token;
        return node;
    }
}

std::shared_ptr<Node> Parser::parseAdditive(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto node = parseMultiply(token, token);

    while (true) {
        if (token::is(token, "+")) {
            node = createAddNode(token, node, parseMultiply(token, token->next));
            continue;
        }
        if (token::is(token, "-")) {
            node = createSubNode(token, node, parseMultiply(token, token->next));
            continue;
        }
        result = token;
        return node;
    }
}

std::shared_ptr<Node> Parser::parseMultiply(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto node = parseUnary(token, token);

    while (true) {
        if (token::is(token, "*")) {
            node = createBinaryNode(NodeType::MUL, token, node, parseUnary(token, token->next));
            continue;
        }
        if (token::is(token, "/")) {
            node = createBinaryNode(NodeType::DIV, token, node, parseUnary(token, token->next));
            continue;
        }
        result = token;
        return node;
    }
}

std::shared_ptr<Node> Parser::parseUnary(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    if (token::is(token, "+")) {
        return parsePrimary(result, token->next);
    }
    if (token::is(token, "-")) {
        auto start = token;
        return createUnaryNode(NodeType::NEGATE, start, parseUnary(result, token->next));
    }
    if (token::is(token, "&")) {
        auto start = token;
        return createUnaryNode(NodeType::ADDRESS, start, parseUnary(result, token->next));
    }
    if (token::is(token, "*")) {
        auto start = token;
        return createUnaryNode(NodeType::DEREFERENCE, start, parseUnary(result, token->next));
    }
    return parsePostfix(result, token);
}

std::shared_ptr<Node> Parser::parsePostfix(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    std::shared_ptr<Token> rest = token;
    auto node = parsePrimary(rest, token);

    while (token::is(rest, "[")) {
        auto start = rest;
        std::shared_ptr<Token> nextToken = rest->next;
        auto index = parseExpression(rest, nextToken);
        node = createAddNode(start, node, index);
        node = createUnaryNode(NodeType::DEREFERENCE, start, node);
        rest = token::skipIf(rest, "]");
    }
    result = rest;
    return node;
}

std::shared_ptr<Node> Parser::parseFunctionCall(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto start = token;
    token = token->next->next; // 関数名と"("をスキップ
    auto head = std::make_shared<Node>();
    auto current = head;

    while (!token::is(token, ")")) {
        if (current != head) {
            token = token::skipIf(token, ",");
        }
        current = current->next = parseAssignment(token, token);
    }

    result = token::skipIf(token, ")");

    auto node = std::make_shared<Node>(NodeType::FUNCTION_CALL);
    node->functionName = getIdentifier(start);
    node->arguments = head->next;

    return node;
}

std::shared_ptr<Token> Parser::parseFunction(std::shared_ptr<Token>& token, std::shared_ptr<Type>& baseType) {
    auto funcType = declarator(token, token, baseType);
    auto name = getIdentifier(funcType->name);
    auto func = makeFunction(name, funcType);
    _locals.reset();
    token = token::skipIf(token, "{");
    applyParamLVars(funcType->parameters);
    func->parameters = _locals;
    func->body = parseCompoundStatement(token, token);
    func->locals = _locals;
    func->next = _globals;
    _globals = func;
    return token;
}

std::shared_ptr<Node> Parser::parsePrimary(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    if (token::is(token, "(")) {
        auto node = parseExpression(token, token->next);
        result = token::skipIf(token, ")");
        return node;
    }

    if (token::is(token, "sizeof")) {
        auto node = parseUnary(result, token->next);
        type::addType(node);
        return createNumberNode(token, node->type->size);
    }

    if (token->type == TokenType::IDENTIFIER) {
        // function
        if (token::is(token->next, "(")) {
            return parseFunctionCall(result, token);
        }

        // variable
        auto var = findLocalVariable(token);
        if (!var) {
            Log::error(token->location, std::format("Undefined variable: {}", token->originalValue));
            return nullptr;
        }
        result = token->next;

        auto node = createVariableNode(token, var);
        return node;
    }

    if (token->type == TokenType::DIGIT) {
        auto node = createNumberNode(token, token->numberValue);
        result = token->next;
        return node;
    }

    using namespace std::literals;
    Log::error(token->location, "Expected an expression"sv);

    return nullptr;
}

void Parser::applyParamLVars(const std::shared_ptr<Type>& parameter) {
    if (parameter) {
        applyParamLVars(parameter->next);
        createLocalVariable(getIdentifier(parameter->name), parameter);
    }
}

} // namespace yoctocc
