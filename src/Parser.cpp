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
        std::shared_ptr<VariableScope> next;
        std::shared_ptr<Object> variable;
    };

    struct Scope {
        std::shared_ptr<VariableScope> variable;
        std::shared_ptr<Scope> next;
    };

    std::shared_ptr<Scope> currentScope = std::make_shared<Scope>();

    void enterScope() {
        auto scope = std::make_shared<Scope>();
        scope->next = currentScope;
        currentScope = scope;
    }

    void leaveScope() {
        assert(currentScope);
        currentScope = currentScope->next;
    }

    void pushVariableScope(const std::string& name, const std::shared_ptr<Object>& variable) {
        auto variableScope = std::make_shared<VariableScope>();
        variableScope->name = name;
        variableScope->variable = variable;
        variableScope->next = currentScope->variable;
        currentScope->variable = variableScope;
    }

    bool consumeToken(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::string_view value) {
        if (token::is(token, value)) {
            result = token->next;
            return true;
        }
        result = token;
        return false;
    }

    inline const std::string& getIdentifier(const std::shared_ptr<Token>& token) {
        assert(token && token->kind == TokenKind::IDENTIFIER);
        return token->originalValue;
    }

    inline int getNumber(const std::shared_ptr<Token>& token) {
        assert(token && token->kind == TokenKind::DIGIT);
        return token->numberValue;
    }

    const std::shared_ptr<Type> declSpec(std::shared_ptr<Token>& result, const std::shared_ptr<Token>& token) {
        if (token::is(token, "char")) {
            result = token->next;
            return type::charType();
        }
        result = token::skipIf(token, "int");
        return type::intType();
    }

    const std::shared_ptr<Type> typeSuffix(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type);

    // declarator = "*"* ident type-suffix
    const std::shared_ptr<Type> declarator(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type) {
        while (consumeToken(result, token, "*")) {
            type = type::pointerTo(type);
        }

        if (token->kind == TokenKind::IDENTIFIER) {
            auto name = token;
            auto nextToken = token->next;
            type = typeSuffix(result, nextToken, type);
            type->name = name;
        } else {
            Log::error("Expected an identifier"sv, token->location);
        }

        return type;
    }

    // func-params = (param ("," param)*)? ")"
    // param       = declspec declarator
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

    // type-suffix = "(" func-params
    //             | "[" num "]" type-suffix
    //             | ε
    const std::shared_ptr<Type> typeSuffix(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token, std::shared_ptr<Type>& type) {
        if (token::is(token, "(")) {
            auto nextToken = token->next;
            return functionParameters(result, nextToken, type);
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

    bool isFunction(std::shared_ptr<Token>& token) {
        if (token::is(token, ";")) {
            return false;
        }
        auto dummy = std::make_shared<Type>(TypeKind::UNKNOWN);
        auto start = token;
        auto type = declarator(start, start, dummy);
        return type->kind == TypeKind::FUNCTION;
    }

    std::string makeUniqueName() {
        static int count = 0;
        return std::format(".L..{}", count++);
    }
}

namespace yoctocc {

// program = (function-definition | global-variable)*
std::shared_ptr<Object> Parser::parse(std::shared_ptr<Token>& token) {
    assert(token);
    _globals = nullptr;
    while (token->kind != TokenKind::TERMINATOR) {
        auto baseType = declSpec(token, token);
        if (isFunction(token)) {
            token = parseFunction(token, baseType);
            continue;
        }
        token = parseGlobalVariable(token, baseType);
    }
    return _globals;
}

std::shared_ptr<Object> Parser::findVariable(std::shared_ptr<Token>& token) {
    for (auto scope = currentScope; scope; scope = scope->next) {
        for (auto variableScope = scope->variable; variableScope; variableScope = variableScope->next) {
            if (variableScope->name == token->originalValue) {
                return variableScope->variable;
            }
        }
    }
    return nullptr;
}

std::shared_ptr<Object> Parser::createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, true);
    var->next = _locals;
    pushVariableScope(name, var);
    _locals = var;
    return var;
}

std::shared_ptr<Object> Parser::createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, false);
    var->next = _globals;
    pushVariableScope(name, var);
    _globals = var;
    return var;
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
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

// expr = assign
std::shared_ptr<Node> Parser::parseExpression(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    return parseAssignment(result, token);
}

// assign = equality ("=" assign)?
std::shared_ptr<Node> Parser::parseAssignment(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto node = parseEquality(token, token);

    if (token::is(token, "=")) {
        node = createBinaryNode(NodeType::ASSIGN, token, node, parseAssignment(token, token->next));
    }

    result = token;
    return node;
}

// stmt = "return" expr ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//      | "while" "(" expr ")" stmt
//      | "{" compound-stmt
//      | expr-stmt
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

// compound-stmt = (declaration | stmt)* "}"
std::shared_ptr<Node> Parser::parseCompoundStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    auto head = std::make_shared<Node>();
    auto current = head;

    enterScope();

    while (token->kind != TokenKind::TERMINATOR && !token::is(token, "}")) {
        if (type::isTypeName(token)) {
            current = current->next = declaration(token, token);
        } else {
            current = current->next = parseStatement(token, token);
        }
        type::addType(current);
    }

    leaveScope();

    result = token->next;
    auto node = createBlockNode(token, head->next);
    return node;
}

// expr-stmt = expr? ";"
std::shared_ptr<Node> Parser::parseExpressionStatement(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    if (token::is(token, ";")) {
        result = token->next;
        return createBlockNode(token);
    }

    auto node = createUnaryNode(NodeType::EXPRESSION_STATEMENT, token, parseExpression(token, token));
    result = token::skipIf(token, ";");
    return node;
}

// equality = relational ("==" relational | "!=" relational)*
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

// relational = add ("<" add | "<=" add | ">" add | ">=" add)*
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

// add = mul ("+" mul | "-" mul)*
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

// mul = unary ("*" unary | "/" unary)*
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

// unary = ("+" | "-" | "*" | "&") unary
//       | postfix
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

// postfix = primary ("[" expr "]")*
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

// funcall = ident "(" (assign ("," assign)*)? ")"
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

    enterScope();

    token = token::skipIf(token, "{");
    applyParamLVars(funcType->parameters);
    func->parameters = _locals;
    func->body = parseCompoundStatement(token, token);
    func->locals = _locals;
    func->next = _globals;
    _globals = func;

    leaveScope();

    return token;
}

std::shared_ptr<Token> Parser::parseGlobalVariable(std::shared_ptr<Token>& token, std::shared_ptr<Type>& baseType) {
    bool isFirst = true;

    while (!consumeToken(token, token, ";")) {
        if (!isFirst) {
            token = token::skipIf(token, ",");
        }
        isFirst = false;
        auto varType = declarator(token, token, baseType);
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
std::shared_ptr<Node> Parser::parsePrimary(std::shared_ptr<Token>& result, std::shared_ptr<Token>& token) {
    if (token::is(token, "(") && token::is(token->next, "{")) {
        auto node = std::make_shared<Node>(NodeType::STATEMENT_EXPRESSION);
        node->token = token;
        auto next = token->next->next;
        node->body = parseCompoundStatement(token, next)->body;
        result = token::skipIf(token, ")");
        return node;
    }

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

    if (token->kind == TokenKind::IDENTIFIER) {
        // function
        if (token::is(token->next, "(")) {
            return parseFunctionCall(result, token);
        }

        // variable
        auto var = findVariable(token);
        if (!var) {
            Log::error(std::format("Undefined variable: {}", token->originalValue), token->location);
            return nullptr;
        }
        result = token->next;

        auto node = createVariableNode(token, var);
        return node;
    }

    if (token->kind == TokenKind::STRING) {
        auto var = createGlobalVariable(makeUniqueName(), token->type);
        var->initialData = token->originalValue;
        auto node = createVariableNode(token, var);
        result = token->next;
        return node;
    }

    if (token->kind == TokenKind::DIGIT) {
        auto node = createNumberNode(token, token->numberValue);
        result = token->next;
        return node;
    }

    Log::error("Expected an expression"sv, token->location);

    return nullptr;
}

void Parser::applyParamLVars(const std::shared_ptr<Type>& parameter) {
    if (parameter) {
        applyParamLVars(parameter->next);
        createLocalVariable(getIdentifier(parameter->name), parameter);
    }
}

} // namespace yoctocc
