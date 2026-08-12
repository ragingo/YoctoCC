#include "Parser/Parser.hpp"

#include "Logger.hpp"
#include "Node/Keywords.hpp"
#include "Node/Node.hpp"
#include "Parser/Common.hpp"
#include "Parser/Util.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include "Utility.hpp"
#include <cassert>
#include <utility>

using namespace std::string_view_literals;

namespace {

std::string makeUniqueName() {
    static int count = 0;
    return std::format(".L..{}", count++);
}

} // namespace

namespace yoctocc {

bool Parser::isFunction(Token* token) {
    if (token::is(token, ";")) {
        return false;
    }
    auto dummy = std::make_shared<Type>(TypeKind::UNKNOWN);
    auto type = declarator(token, dummy);
    return type->kind == TypeKind::FUNCTION;
}

// program = (typedef | function-definition | global-variable)*
std::unique_ptr<Object> Parser::parse(Token* token) {
    assert(token);
    _globals = nullptr;
    while (token->kind != TokenKind::TERMINATOR) {
        VariableAttribute attr{};
        auto baseType = declSpec(token, &attr);

        if (attr.isTypeDef) {
            token = parseTypeDef(token, baseType);
            continue;
        }

        if (isFunction(token)) {
            token = parseFunction(token, baseType, attr);
            continue;
        }

        token = parseGlobalVariable(token, baseType, attr);
    }
    return std::move(_globals);
}

Object* Parser::createLocalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, true);
    Object* raw = var.get();
    var->next = std::move(_locals);
    _parseScope.pushVariableScope(name)->variable = raw;
    _locals = std::move(var);
    return raw;
}

Object* Parser::createTemporaryLocalVariable(const std::shared_ptr<Type>& type) {
    auto var = makeVariable("", type, true);
    Object* raw = var.get();
    var->next = std::move(_locals);
    _locals = std::move(var);
    return raw;
}

Object* Parser::createGlobalVariable(const std::string& name, const std::shared_ptr<Type>& type) {
    auto var = makeVariable(name, type, false);
    Object* raw = var.get();
    var->next = std::move(_globals);
    var->isStatic = true;
    var->isDefinition = true;
    _parseScope.pushVariableScope(name)->variable = raw;
    _globals = std::move(var);
    return raw;
}

Object* Parser::createGlobalAnonymousVariable(const std::shared_ptr<Type>& type) {
    return createGlobalVariable(makeUniqueName(), type);
}

// declaration = declspec (declarator ("=" expr)? ("," declarator ("=" expr)?)*)? ";"
ParseResult Parser::declaration(Token* token, const std::shared_ptr<Type>& baseType, const VariableAttribute* attr) {
    auto head = std::make_unique<Node>(NodeType::UNKNOWN, token);
    Node* current = head.get();

    int i = 0;

    while (!token::is(token, ";")) {
        if (i++ > 0) {
            token = token::skipIf(token, ",");
        }

        auto varType = declarator(token, baseType);
        if (varType->kind == TypeKind::VOID) {
            Log::error("Variable cannot be of type void"sv, token);
            return {};
        }

        if (attr && attr->isStatic) {
            auto var = createGlobalAnonymousVariable(varType);
            _parseScope.pushVariableScope(token::getIdentifier(varType->name))->variable = var;
            if (token::is(token, "=")) {
                token = token->next.get();
                globalVariableInitializer(token, var);
            }
            continue;
        }

        auto varName = token::getIdentifier(varType->name);
        auto var = createLocalVariable(varName, varType);

        if (attr && attr->alignment) {
            var->alignment = attr->alignment;
        }

        if (token::is(token, "=")) {
            auto [node, rest] = parseVariableInitializer(token->next.get(), var);
            token = rest;
            current->next = createUnaryNode(NodeType::EXPRESSION_STATEMENT, token, std::move(node));
            current = current->next.get();
        }

        if (var->type->size < 0) {
            Log::error("Variable has incomplete type"sv, token);
            return {};
        }

        if (var->type->kind == TypeKind::VOID) {
            Log::error("Variable cannot be of type void"sv, token);
            return {};
        }
    }

    auto node = createBlockNode(token, std::move(head->next));
    return {std::move(node), token->next.get()};
}

ParseResult Parser::parseVariableInitializer(Token* token, Object* variable) {
    auto rest = token;
    auto initializer = parseInitializer(rest, variable->type);
    InitDesignator initDesignator{nullptr, 0, nullptr, variable};
    auto left = std::make_unique<Node>(NodeType::MEMORY_CLEAR, token);
    left->variable = variable;
    auto right = createVariableInitializerNode(token, initializer.get(), &initDesignator, variable->type);
    auto binary = createBinaryNode(NodeType::COMMA, token, std::move(left), std::move(right));
    return {std::move(binary), rest};
}

std::unique_ptr<Initializer> Parser::parseInitializer(Token*& token, std::shared_ptr<Type>& type) {
    auto initializer = createInitializer(type, true);
    initializer->token = token;
    parseInitializer2(token, initializer);

    if ((type::is(type, TypeKind::STRUCT) || type::is(type, TypeKind::UNION)) && type->isFlexibleArray) {
        auto newType = type::copyStructType(type);
        auto member = newType->members.get();
        while (member->next) {
            member = member->next.get();
        }
        member->type = initializer->children[member->index]->type;
        newType->size += member->type->size;
        type = newType;
        return initializer;
    }

    type = initializer->type;
    return initializer;
}

// string-initializer = string-literal
void Parser::stringInitializer(Token*& token, std::unique_ptr<Initializer>& initializer) {
    if (initializer->isFlexibleArray) {
        initializer = createInitializer(type::arrayOf(initializer->type->base, token->type->arraySize));
    }

    int length = std::min(initializer->type->arraySize, token->type->arraySize);

    for (int i = 0; i < length; i++) {
        initializer->children[i]->expression = createNumberNode(token, *(token->originalValue.data() + i));
    }

    token = token->next.get();
}

int Parser::countElements(Token* token, std::shared_ptr<Type> type) {
    auto dummyInitializer = createInitializer(type);
    int i = 0;
    for (; !token::consumeEnd(token); i++) {
        if (i > 0) {
            token = token::skipIf(token, ",");
        }
        parseInitializer2(token, dummyInitializer);
    }
    return i;
}

// array-initializer1 = "{" initializer ("," initializer)* ","? "}"
void Parser::arrayInitializer1(Token*& token, std::unique_ptr<Initializer>& initializer) {
    token = token::skipIf(token, "{");

    if (initializer->isFlexibleArray) {
        int count = countElements(token, initializer->type->base);
        initializer = createInitializer(type::arrayOf(initializer->type->base, count));
    }

    for (int i = 0; !token::consumeEnd(token); i++) {
        if (i > 0) {
            token = token::skipIf(token, ",");
        }
        if (i < initializer->type->arraySize) {
            parseInitializer2(token, initializer->children[i]);
        } else {
            skipExcessElement(token);
        }
    }
}

// array-initializer2 = initializer ("," initializer)*
void Parser::arrayInitializer2(Token*& token, std::unique_ptr<Initializer>& initializer) {
    if (initializer->isFlexibleArray) {
        int count = countElements(token, initializer->type->base);
        initializer = createInitializer(type::arrayOf(initializer->type->base, count));
    }

    for (int i = 0; i < initializer->type->arraySize && !token::isEnd(token); i++) {
        if (i > 0) {
            token = token::skipIf(token, ",");
        }
        parseInitializer2(token, initializer->children[i]);
    }
}

// struct-initializer1 = "{" initializer ("," initializer)* ","? "}"
void Parser::structInitializer1(Token*& token, std::unique_ptr<Initializer>& initializer) {
    token = token::skipIf(token, "{");

    auto members = initializer->type->members.get();

    while (!token::consumeEnd(token)) {
        if (members != initializer->type->members.get()) {
            token = token::skipIf(token, ",");
        }

        if (members) {
            parseInitializer2(token, initializer->children[members->index]);
            members = members->next.get();
        } else {
            skipExcessElement(token);
        }
    }
}

// struct-initializer2 = initializer ("," initializer)*
void Parser::structInitializer2(Token*& token, std::unique_ptr<Initializer>& initializer) {
    bool isFirst = true;

    for (auto member = initializer->type->members.get(); member; member = member->next.get()) {
        if (token::isEnd(token)) {
            break;
        }
        if (!isFirst) {
            token = token::skipIf(token, ",");
        }
        isFirst = false;
        parseInitializer2(token, initializer->children[member->index]);
    }
}

void Parser::unionInitializer(Token*& token, std::unique_ptr<Initializer>& initializer) {
    if (token::is(token, "{")) {
        token = token->next.get();
        parseInitializer2(token, initializer->children[0]);
        token::consume(token, ",");
        token = token::skipIf(token, "}");
    } else {
        parseInitializer2(token, initializer->children[0]);
    }
}

// initializer = string-initializer | array-initializer
//             | struct-initializer | union-initializer
//             | assign
void Parser::parseInitializer2(Token*& token, std::unique_ptr<Initializer>& initializer) {
    if (initializer->type->kind == TypeKind::ARRAY) {
        if (token->kind == TokenKind::STRING) {
            stringInitializer(token, initializer);
        } else {
            if (token::is(token, "{")) {
                arrayInitializer1(token, initializer);
            } else {
                arrayInitializer2(token, initializer);
            }
        }
        return;
    }

    if (initializer->type->kind == TypeKind::STRUCT) {
        if (token::is(token, "{")) {
            structInitializer1(token, initializer);
            return;
        }

        auto [node, rest] = parseAssignment(token);
        type::addType(node.get());
        if (node->type->kind == TypeKind::STRUCT) {
            initializer->expression = std::move(node);
            token = rest;
            return;
        }

        structInitializer2(token, initializer);
        return;
    }

    if (initializer->type->kind == TypeKind::UNION) {
        unionInitializer(token, initializer);
        return;
    }

    if (token::is(token, "{")) {
        token = token->next.get();
        parseInitializer2(token, initializer);
        token = token::skipIf(token, "}");
        return;
    }

    auto [node, rest] = parseAssignment(token);
    initializer->expression = std::move(node);
    token = rest;
}

void Parser::skipExcessElement(Token*& token) {
    if (token::is(token, "{")) {
        token = token->next.get();
        skipExcessElement(token);
        token = token::skipIf(token, "}");
        return;
    }

    auto [_, rest] = parseAssignment(token);
    token = rest;
}

void Parser::globalVariableInitializer(Token*& token, Object* variable) {
    auto initializer = parseInitializer(token, variable->type);
    auto relocation = std::make_unique<Relocation>();
    std::vector<char> buf(variable->type->size);
    writeGlobalVariableData(relocation.get(), initializer.get(), variable->type, buf, 0);
    variable->initialData = std::vector(buf.begin(), buf.end());
    variable->relocations = std::move(relocation->next);
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

int64_t Parser::constExpression(Token*& token) {
    auto [node, rest] = parseConditional(token);
    token = rest;
    return eval(node.get());
}

// ex) a += b
// => tmp = &a, *tmp = *tmp + b
std::unique_ptr<Node> Parser::toAssign(std::unique_ptr<Node>&& binary) {
    type::addType(binary->left.get());
    type::addType(binary->right.get());
    auto token = binary->token;
    auto pointerType = type::pointerTo(binary->left->type);
    auto object = createTemporaryLocalVariable(pointerType);
    auto expression1 = createBinaryNode(NodeType::ASSIGN,
                                        token,
                                        createVariableNode(token, object),
                                        createUnaryNode(NodeType::ADDRESS, token, std::move(binary->left)));
    auto expression2 = createBinaryNode(
        NodeType::ASSIGN,
        token,
        createUnaryNode(NodeType::DEREFERENCE, token, createVariableNode(token, object)),
        createBinaryNode(binary->nodeType,
                         token,
                         createUnaryNode(NodeType::DEREFERENCE, token, createVariableNode(token, object)),
                         std::move(binary->right)));
    return createBinaryNode(NodeType::COMMA, token, std::move(expression1), std::move(expression2));
}

// ex) a++
// => (typedef a)((a += 1) - 1)
std::unique_ptr<Node> Parser::createIncDecNode(const Token* token, std::unique_ptr<Node> node, bool isInc) {
    type::addType(node.get());
    auto nodeType = node->type;
    auto number = createNumberNode(token, isInc ? 1 : -1);
    auto add = createAddNode(token, std::move(node), std::move(number));
    auto assign = toAssign(std::move(add));
    auto add2 = createAddNode(token, std::move(assign), createNumberNode(token, isInc ? -1 : 1));
    auto cast = createCastNode(std::move(add2), nodeType);
    return cast;
}

// bitand = equality ("&" equality)*
ParseResult Parser::createBitAndNode(Token* token) {
    auto [left, rest] = parseEquality(token);
    while (token::is(rest, "&")) {
        auto start = rest;
        auto [right, rest2] = parseEquality(rest->next.get());
        left = createBinaryNode(NodeType::BIT_AND, start, std::move(left), std::move(right));
        rest = rest2;
    }
    return {std::move(left), rest};
}

// bitor = bitxor ("|" bitxor)*
ParseResult Parser::createBitOrNode(Token* token) {
    auto [left, rest] = createBitXorNode(token);
    while (token::is(rest, "|")) {
        auto start = rest;
        auto [right, rest2] = createBitXorNode(rest->next.get());
        left = createBinaryNode(NodeType::BIT_OR, start, std::move(left), std::move(right));
        rest = rest2;
    }
    return {std::move(left), rest};
}

// bitxor = bitand ("^" bitand)*
ParseResult Parser::createBitXorNode(Token* token) {
    auto [left, rest] = createBitAndNode(token);
    while (token::is(rest, "^")) {
        auto start = rest;
        auto [right, rest2] = createBitAndNode(rest->next.get());
        left = createBinaryNode(NodeType::BIT_XOR, start, std::move(left), std::move(right));
        rest = rest2;
    }
    return {std::move(left), rest};
}

// logand = bitor ("&&" bitor)*
ParseResult Parser::createLogicalAndNode(Token* token) {
    auto [left, rest] = createBitOrNode(token);
    while (token::is(rest, "&&")) {
        auto start = rest;
        auto [right, rest2] = createBitOrNode(rest->next.get());
        left = createBinaryNode(NodeType::LOGICAL_AND, start, std::move(left), std::move(right));
        rest = rest2;
    }
    return {std::move(left), rest};
}

// logor = logand ("||" logand)*
ParseResult Parser::createLogicalOrNode(Token* token) {
    auto [left, rest] = createLogicalAndNode(token);
    while (token::is(rest, "||")) {
        auto start = rest;
        auto [right, rest2] = createLogicalAndNode(rest->next.get());
        left = createBinaryNode(NodeType::LOGICAL_OR, start, std::move(left), std::move(right));
        rest = rest2;
    }
    return {std::move(left), rest};
}

// assign    = conditional (assign-op assign)?
// assign-op = "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "&=" | "|=" | "^="
//           | "<<=" | ">>="
ParseResult Parser::parseAssignment(Token* token) {
    auto [node, rest] = parseConditional(token);

    if (token::is(rest, "=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        return {createBinaryNode(NodeType::ASSIGN, start, std::move(node), std::move(right)), rest2};
    }

    if (token::is(rest, "+=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createAddNode(start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "-=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createSubNode(start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "*=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::MUL, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "/=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::DIV, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "%=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::MOD, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "&=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::BIT_AND, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "|=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::BIT_OR, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "^=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::BIT_XOR, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, "<<=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::SHL, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    if (token::is(rest, ">>=")) {
        auto start = rest;
        auto [right, rest2] = parseAssignment(rest->next.get());
        auto binary = createBinaryNode(NodeType::SHR, start, std::move(node), std::move(right));
        return {toAssign(std::move(binary)), rest2};
    }

    return {std::move(node), rest};
}

// conditional = logor ("?" expr ":" conditional)?
ParseResult Parser::parseConditional(Token* token) {
    auto [conditionalNode, rest] = createLogicalOrNode(token);

    if (!token::is(rest, "?")) {
        return {std::move(conditionalNode), rest};
    }

    auto node = std::make_unique<Node>(NodeType::CONDITIONAL, rest);
    node->condition = std::move(conditionalNode);
    auto [thenNode, afterThen] = parseExpression(rest->next.get());
    node->then = std::move(thenNode);
    afterThen = token::skipIf(afterThen, ":");
    auto [elseNode, afterElse] = parseConditional(afterThen);
    node->els = std::move(elseNode);
    return {std::move(node), afterElse};
}

// stmt = "return" expr? ";"
//      | "if" "(" expr ")" stmt ("else" stmt)?
//      | "switch" "(" expr ")" stmt
//      | "case" const-expr ":" stmt
//      | "default" ":" stmt
//      | "for" "(" expr-stmt expr? ";" expr? ")" stmt
//      | "while" "(" expr ")" stmt
//      | "goto" ident ";"
//      | "break" ";"
//      | "continue" ";"
//      | ident ":" stmt
//      | "{" compound-stmt
//      | expr-stmt
ParseResult Parser::parseStatement(Token* token) {
    if (token::is(token, "return")) {
        assert(_currentFunction);
        auto returnNode = std::make_unique<Node>(NodeType::RETURN, token);

        auto start = token;
        token = token->next.get();
        if (token::consume(token, ";")) {
            return {std::move(returnNode), token};
        }
        token = start;

        auto [expr, rest] = parseExpression(token->next.get());
        rest = token::skipIf(rest, ";");
        type::addType(expr.get());
        auto lhsNode = createCastNode(std::move(expr), _currentFunction->type->returnType);
        returnNode->left = std::move(lhsNode);
        return {std::move(returnNode), rest};
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

    if (token::is(token, "switch")) {
        auto node = std::make_unique<Node>(NodeType::SWITCH, token);
        token = token::skipIf(token->next.get(), "(");

        auto [cond, afterCond] = parseExpression(token);
        node->condition = std::move(cond);
        token = token::skipIf(afterCond, ")");

        auto currentSwitch = _currentSwitch;
        _currentSwitch = node.get();

        auto breakLabel = _breakLabel;
        _breakLabel = node->breakLabel = makeUniqueName();

        auto [body, afterBody] = parseStatement(token);
        node->then = std::move(body);

        _currentSwitch = currentSwitch;
        _breakLabel = breakLabel;
        return {std::move(node), afterBody};
    }

    if (token::is(token, "case")) {
        if (!_currentSwitch) {
            Log::error("case statement not within a switch"sv, token);
            return {};
        }

        auto node = std::make_unique<Node>(NodeType::CASE, token);
        token = token->next.get();
        node->value = constExpression(token);
        token = token::skipIf(token, ":");
        node->label = makeUniqueName();

        auto [stmt, rest] = parseStatement(token);
        node->left = std::move(stmt);

        node->cases = _currentSwitch->cases;
        _currentSwitch->cases = node.get();

        return {std::move(node), rest};
    }

    if (token::is(token, "default")) {
        if (!_currentSwitch) {
            Log::error("default statement not within a switch"sv, token);
            return {};
        }

        auto node = std::make_unique<Node>(NodeType::CASE, token);
        token = token::skipIf(token->next.get(), ":");
        node->label = makeUniqueName();

        auto [stmt, rest] = parseStatement(token);
        node->left = std::move(stmt);

        _currentSwitch->defaultCase = node.get();

        return {std::move(node), rest};
    }

    if (token::is(token, "for")) {
        auto node = std::make_unique<Node>(NodeType::FOR, token);
        token = token::skipIf(token->next.get(), "(");

        _parseScope.enterScope();

        auto breakLabel = _breakLabel;
        auto continueLabel = _continueLabel;
        _breakLabel = node->breakLabel = makeUniqueName();
        _continueLabel = node->continueLabel = makeUniqueName();

        if (type::isTypeName(token)) {
            auto baseType = declSpec(token, nullptr);
            auto [initDecl, afterDecl] = declaration(token, baseType, nullptr);
            node->init = std::move(initDecl);
            token = afterDecl;
        } else {
            auto [initStmt, afterInit] = parseExpressionStatement(token);
            node->init = std::move(initStmt);
            token = afterInit;
        }

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

        _parseScope.leaveScope();

        _breakLabel = breakLabel;
        _continueLabel = continueLabel;
        return {std::move(node), afterBody};
    }

    if (token::is(token, "while")) {
        auto node = std::make_unique<Node>(NodeType::FOR, token);
        token = token::skipIf(token->next.get(), "(");

        auto [cond, afterCond] = parseExpression(token);
        node->condition = std::move(cond);
        token = token::skipIf(afterCond, ")");

        auto breakLabel = _breakLabel;
        auto continueLabel = _continueLabel;
        _breakLabel = node->breakLabel = makeUniqueName();
        _continueLabel = node->continueLabel = makeUniqueName();

        auto [body, afterBody] = parseStatement(token);
        node->then = std::move(body);

        _breakLabel = breakLabel;
        _continueLabel = continueLabel;

        return {std::move(node), afterBody};
    }

    if (token::is(token, "goto")) {
        auto node = std::make_unique<Node>(NodeType::GOTO, token);
        node->label = token::getIdentifier(token->next.get());
        node->gotoNext = _gotos;
        _gotos = node.get();
        return {std::move(node), token::skipIf(token->next.get()->next.get(), ";")};
    }

    if (token::is(token, "break")) {
        if (_breakLabel.empty()) {
            Log::error("break statement not within a loop"sv, token);
            return {};
        }
        auto node = std::make_unique<Node>(NodeType::GOTO, token);
        node->uniqueLabel = _breakLabel;
        return {std::move(node), token::skipIf(token->next.get(), ";")};
    }

    if (token::is(token, "continue")) {
        if (_continueLabel.empty()) {
            Log::error("continue statement not within a loop"sv, token);
            return {};
        }
        auto node = std::make_unique<Node>(NodeType::GOTO, token);
        node->uniqueLabel = _continueLabel;
        return {std::move(node), token::skipIf(token->next.get(), ";")};
    }

    if (token->kind == TokenKind::IDENTIFIER && token->next && token::is(token->next.get(), ":")) {
        auto node = std::make_unique<Node>(NodeType::LABEL, token);
        node->label = token::getIdentifier(token);
        node->uniqueLabel = makeUniqueName();
        node->gotoNext = _labels;
        _labels = node.get();
        auto [statement, rest] = parseStatement(token->next.get()->next.get());
        node->left = std::move(statement);
        return {std::move(node), rest};
    }

    if (token::is(token, "{")) {
        return parseCompoundStatement(token->next.get());
    }

    return parseExpressionStatement(token);
}

// compound-stmt = (typedef | declaration | stmt)* "}"
ParseResult Parser::parseCompoundStatement(Token* token) {
    auto head = std::make_unique<Node>(NodeType::UNKNOWN, token);
    Node* current = head.get();

    _parseScope.enterScope();

    while (token->kind != TokenKind::TERMINATOR && !token::is(token, "}")) {
        if (parser::isTypeName(token, _parseScope) && !token::is(token->next.get(), ":")) {
            VariableAttribute attr{};
            auto baseType = declSpec(token, &attr);

            if (attr.isTypeDef) {
                token = parseTypeDef(token, baseType);
                continue;
            }

            if (isFunction(token)) {
                token = parseFunction(token, baseType, attr);
                continue;
            }

            if (attr.isExtern) {
                token = parseGlobalVariable(token, baseType, attr);
                continue;
            }

            auto [decl, rest] = declaration(token, baseType, &attr);
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

    _parseScope.leaveScope();

    auto node = createBlockNode(head->token, std::move(head->next));
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

// relational = shift ("<" shift | "<=" shift | ">" shift | ">=" shift)*
ParseResult Parser::parseRelational(Token* token) {
    auto [node, rest] = parseShift(token);
    token = rest;

    while (true) {
        if (token::is(token, "<")) {
            auto start = token;
            auto [right, rest2] = parseShift(token->next.get());
            node = createBinaryNode(NodeType::LESS, start, std::move(node), std::move(right));
            token = rest2;
            continue;
        }
        if (token::is(token, "<=")) {
            auto start = token;
            auto [right, rest2] = parseShift(token->next.get());
            node = createBinaryNode(NodeType::LESS_EQUAL, start, std::move(node), std::move(right));
            token = rest2;
            continue;
        }
        if (token::is(token, ">")) {
            auto start = token;
            auto [right, rest2] = parseShift(token->next.get());
            node = createBinaryNode(NodeType::GREATER, start, std::move(node), std::move(right));
            token = rest2;
            continue;
        }
        if (token::is(token, ">=")) {
            auto start = token;
            auto [right, rest2] = parseShift(token->next.get());
            node = createBinaryNode(NodeType::GREATER_EQUAL, start, std::move(node), std::move(right));
            token = rest2;
            continue;
        }
        return {std::move(node), token};
    }
}

// shift = add ("<<" add | ">>" add)*
ParseResult Parser::parseShift(Token* token) {
    auto [node, rest] = parseAdditive(token);
    token = rest;

    while (true) {
        if (token::is(token, "<<")) {
            auto start = token;
            auto [right, rest2] = parseAdditive(token->next.get());
            node = createBinaryNode(NodeType::SHL, start, std::move(node), std::move(right));
            token = rest2;
            continue;
        }
        if (token::is(token, ">>")) {
            auto start = token;
            auto [right, rest2] = parseAdditive(token->next.get());
            node = createBinaryNode(NodeType::SHR, start, std::move(node), std::move(right));
            token = rest2;
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

// mul = cast ("*" cast | "/" cast | "%" cast)*
ParseResult Parser::parseMultiply(Token* token) {
    auto [node, rest] = parseCast(token);
    token = rest;

    while (true) {
        if (token::is(token, "*")) {
            auto start = token;
            auto [right, r] = parseCast(token->next.get());
            node = createBinaryNode(NodeType::MUL, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, "/")) {
            auto start = token;
            auto [right, r] = parseCast(token->next.get());
            node = createBinaryNode(NodeType::DIV, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        if (token::is(token, "%")) {
            auto start = token;
            auto [right, r] = parseCast(token->next.get());
            node = createBinaryNode(NodeType::MOD, start, std::move(node), std::move(right));
            token = r;
            continue;
        }
        return {std::move(node), token};
    }
}

// cast = "(" type-name ")" cast | unary
ParseResult Parser::parseCast(Token* token) {
    if (token::is(token, "(") && parser::isTypeName(token->next.get(), _parseScope)) {
        auto start = token;
        auto next = token->next.get();
        auto type = typeName(next);
        token = token::skipIf(next, ")");

        // compound literal
        if (token::is(token, "{")) {
            return parseUnary(start);
        }

        auto [expr, rest] = parseCast(token);
        auto node = createCastNode(std::move(expr), type);
        node->token = start;
        return {std::move(node), rest};
    }
    return parseUnary(token);
}

// unary = ("+" | "-" | "*" | "&" | "!" | "~") cast
//       | ("++" | "--") unary
//       | postfix
ParseResult Parser::parseUnary(Token* token) {
    if (token::is(token, "+")) {
        return parseCast(token->next.get());
    }
    if (token::is(token, "-")) {
        auto start = token;
        auto [operand, rest] = parseCast(token->next.get());
        return {createUnaryNode(NodeType::NEGATE, start, std::move(operand)), rest};
    }
    if (token::is(token, "&")) {
        auto start = token;
        auto [operand, rest] = parseCast(token->next.get());
        return {createUnaryNode(NodeType::ADDRESS, start, std::move(operand)), rest};
    }
    if (token::is(token, "*")) {
        auto start = token;
        auto [operand, rest] = parseCast(token->next.get());
        return {createUnaryNode(NodeType::DEREFERENCE, start, std::move(operand)), rest};
    }
    if (token::is(token, "!")) {
        auto start = token;
        auto [operand, rest] = parseCast(token->next.get());
        return {createUnaryNode(NodeType::NOT, start, std::move(operand)), rest};
    }
    if (token::is(token, "~")) {
        auto start = token;
        auto [operand, rest] = parseCast(token->next.get());
        return {createUnaryNode(NodeType::BIT_NOT, start, std::move(operand)), rest};
    }
    if (token::is(token, "++")) {
        auto start = token;
        auto [operand, rest] = parseUnary(token->next.get());
        auto binary = createAddNode(start, std::move(operand), createNumberNode(token, 1));
        return {toAssign(std::move(binary)), rest};
    }
    if (token::is(token, "--")) {
        auto start = token;
        auto [operand, rest] = parseUnary(token->next.get());
        auto binary = createSubNode(start, std::move(operand), createNumberNode(token, 1));
        return {toAssign(std::move(binary)), rest};
    }
    return parsePostfix(token);
}

// postfix = "(" type-name ")" "{" initializer-list "}"
//         | primary ("[" expr "]" | "." ident | "->" ident | "++" | "--")*
ParseResult Parser::parsePostfix(Token* token) {
    if (token::is(token, "(") && parser::isTypeName(token->next.get(), _parseScope)) {
        // compound literal
        auto start = token;
        token = token->next.get();
        auto type = typeName(token);
        token = token::skipIf(token, ")");

        if (!_parseScope.currentScope()->next) {
            auto var = createGlobalAnonymousVariable(type);
            globalVariableInitializer(token, var);
            return {createVariableNode(start, var), token};
        }

        auto var = createLocalVariable("", type);
        auto [left, rest] = parseVariableInitializer(token, var);
        auto right = createVariableNode(token, var);
        return {createBinaryNode(NodeType::COMMA, start, std::move(left), std::move(right)), rest};
    }

    auto [node, rest] = parsePrimary(token);
    token = rest;

    while (true) {
        if (token::is(token, "[")) {
            auto start = token;
            auto [index, rest] = parseExpression(token->next.get());
            token = token::skipIf(rest, "]");
            node = createAddNode(start, std::move(node), std::move(index));
            node = createUnaryNode(NodeType::DEREFERENCE, start, std::move(node));
            continue;
        }
        if (token::is(token, ".")) {
            node = createStructRefNode(token->next.get(), std::move(node));
            token = token->next->next.get();
            continue;
        }
        if (token::is(token, "->")) {
            node = createUnaryNode(NodeType::DEREFERENCE, token, std::move(node));
            node = createStructRefNode(token->next.get(), std::move(node));
            token = token->next->next.get();
            continue;
        }
        if (token::is(token, "++")) {
            auto start = token;
            node = createIncDecNode(start, std::move(node), true);
            token = token->next.get();
            continue;
        }
        if (token::is(token, "--")) {
            auto start = token;
            node = createIncDecNode(start, std::move(node), false);
            token = token->next.get();
            continue;
        }
        return {std::move(node), token};
    }
}

// funcall = ident "(" (assign ("," assign)*)? ")"
ParseResult Parser::parseFunctionCall(Token* token) {
    auto start = token;
    token = token->next->next.get(); // 関数名と"("をスキップ

    auto varScope = _parseScope.findVariable(start);
    if (!varScope) {
        Log::error("implicit declaration of function", start);
        return {};
    }
    if (!varScope->variable || !varScope->variable->isFunction) {
        Log::error("not a function", start);
        return {};
    }

    auto type = varScope->variable->type;
    auto parameterType = type->parameters;

    auto head = std::make_unique<Node>(NodeType::UNKNOWN, token);
    Node* current = head.get();

    while (!token::is(token, ")")) {
        if (current != head.get()) {
            token = token::skipIf(token, ",");
        }
        auto [arg, rest] = parseAssignment(token);
        type::addType(arg.get());

        if (parameterType) {
            if (parameterType->kind == TypeKind::STRUCT || parameterType->kind == TypeKind::UNION) {
                Log::error("Passing struct/union is not supported yet"sv, token);
                return {};
            }
            arg = createCastNode(std::move(arg), parameterType);
            parameterType = parameterType->next;
        }

        current->next = std::move(arg);
        current = current->next.get();
        token = rest;
        type::addType(current);
    }

    token = token::skipIf(token, ")");

    auto node = std::make_unique<Node>(NodeType::FUNCTION_CALL, start);
    node->functionName = token::getIdentifier(start);
    node->functionType = type;
    node->type = type->returnType;
    node->arguments = std::move(head->next);

    return {std::move(node), token};
}

Token* Parser::parseTypeDef(Token* token, std::shared_ptr<Type>& baseType) {
    bool isFirst = true;

    while (!token::consume(token, ";")) {
        if (!isFirst) {
            token = token::skipIf(token, ",");
        }
        isFirst = false;
        auto type = declarator(token, baseType);
        auto name = token::getIdentifier(type->name);
        _parseScope.pushVariableScope(name)->typeDef = type;
    }

    return token;
}

Token* Parser::parseFunction(Token* token, std::shared_ptr<Type>& baseType, const VariableAttribute& attr) {
    auto funcType = declarator(token, baseType);
    auto name = token::getIdentifier(funcType->name);
    auto func = makeFunction(name, funcType);
    func->isDefinition = !token::consume(token, ";");
    func->isStatic = attr.isStatic;

    _parseScope.pushVariableScope(name)->variable = func.get();

    if (!func->isDefinition) {
        func->next = std::move(_globals);
        _globals = std::move(func);
        return token;
    }

    _currentFunction = std::move(func);
    _locals.reset();

    _parseScope.enterScope();

    applyParamLVars(funcType->parameters);
    _currentFunction->parameters = _locals.get();

    token = token::skipIf(token, "{");
    auto [body, rest] = parseCompoundStatement(token);
    _currentFunction->body = std::move(body);
    token = rest;

    _currentFunction->locals = std::move(_locals);
    _currentFunction->next = std::move(_globals);
    _globals = std::move(_currentFunction);

    _parseScope.leaveScope();

    resolveGotoLabels();

    return token;
}

Token* Parser::parseGlobalVariable(Token* token, std::shared_ptr<Type>& baseType, const VariableAttribute& attr) {
    bool isFirst = true;

    while (!token::consume(token, ";")) {
        if (!isFirst) {
            token = token::skipIf(token, ",");
        }
        isFirst = false;

        auto varType = declarator(token, baseType);
        auto varName = token::getIdentifier(varType->name);
        auto var = createGlobalVariable(varName, varType);
        var->isDefinition = !attr.isExtern;
        var->isStatic = attr.isStatic;

        if (attr.alignment) {
            var->alignment = attr.alignment;
        }

        if (token::is(token, "=")) {
            token = token->next.get();
            globalVariableInitializer(token, var);
        }
    }

    return token;
}

// primary = "(" "{" stmt+ "}" ")"
//         | "(" expr ")"
//         | "sizeof" "(" type-name ")"
//         | "sizeof" unary
//         | "_Alignof" "(" type-name ")"
//         | "_Alignof" unary
//         | ident func-args?
//         | str
//         | num
ParseResult Parser::parsePrimary(Token* token) {
    if (token::is(token, "(") && token::is(token->next.get(), "{")) {
        auto node = std::make_unique<Node>(NodeType::STATEMENT_EXPRESSION, token);
        auto [block, rest] = parseCompoundStatement(token->next->next.get());
        node->body = std::move(block->body);
        return {std::move(node), token::skipIf(rest, ")")};
    }

    if (token::is(token, "(")) {
        auto [expr, rest] = parseExpression(token->next.get());
        return {std::move(expr), token::skipIf(rest, ")")};
    }

    if (token::is(token, Keyword::SIZEOF) && token::is(token->next.get(), "(") &&
        parser::isTypeName(token->next->next.get(), _parseScope)) {
        auto start = token;
        token = token->next->next.get();
        auto type = typeName(token);
        if (!type) {
            Log::error("Expected a type name after sizeof"sv, token);
            return {};
        }
        token = token::skipIf(token, ")");
        return {createNumberNode(start, type->size), token};
    }

    if (token::is(token, Keyword::SIZEOF)) {
        auto [operand, rest] = parseUnary(token->next.get());
        type::addType(operand.get());
        return {createNumberNode(token, operand->type->size), rest};
    }

    if (token::is(token, Keyword::ALIGNOF)) {
        if (token::is(token->next.get(), "(") && type::isTypeName(token->next->next.get())) {
            token = token->next->next.get();
            auto type = typeName(token);
            auto rest = token::skipIf(token, ")");
            return {createNumberNode(token, type->alignment), rest};
        }
        auto [node, rest] = parseUnary(token->next.get());
        type::addType(node.get());
        return {createNumberNode(token, node->type->alignment), rest};
    }

    if (token->kind == TokenKind::IDENTIFIER) {
        // function
        if (token::is(token->next.get(), "(")) {
            return parseFunctionCall(token);
        }

        // variable
        auto variableScope = _parseScope.findVariable(token);
        if (!variableScope || (!variableScope->variable && !variableScope->enumType)) {
            Log::error(std::format("Undefined variable: {}", token->originalValue), token);
            return {nullptr, token};
        }

        if (variableScope->variable) {
            return {createVariableNode(token, variableScope->variable), token->next.get()};
        }

        if (variableScope->enumType) {
            return {createNumberNode(token, variableScope->enumValue), token->next.get()};
        }

        std::unreachable();
    }

    if (token->kind == TokenKind::STRING) {
        auto var = createGlobalAnonymousVariable(token->type);
        var->initialData = std::vector<char>(token->originalValue.begin(), token->originalValue.end());
        var->initialData.emplace_back('\0');
        return {createVariableNode(token, var), token->next.get()};
    }

    if (token->kind == TokenKind::DIGIT) {
        return {createNumberNode(token, token->numberValue), token->next.get()};
    }

    Log::error("Expected an expression"sv, token, false);
    return {nullptr, token};
}

void Parser::applyParamLVars(const std::shared_ptr<Type>& parameter) {
    if (parameter) {
        applyParamLVars(parameter->next);
        createLocalVariable(token::getIdentifier(parameter->name), parameter);
    }
}

void Parser::resolveGotoLabels() {
    for (auto gotoNode = _gotos; gotoNode; gotoNode = gotoNode->gotoNext) {
        auto labelNode = _labels;
        for (; labelNode; labelNode = labelNode->gotoNext) {
            if (labelNode->label == gotoNode->label) {
                gotoNode->uniqueLabel = labelNode->uniqueLabel;
                break;
            }
        }
        if (gotoNode->uniqueLabel.empty()) {
            Log::error(std::format("Undefined label: {}", gotoNode->label), gotoNode->token->next.get());
        }
    }
    _gotos = _labels = nullptr;
}

} // namespace yoctocc
