#pragma once
#include <string>

enum TokenType
{
    DIGRAPH,
    LBRACE,
    RBRACE,
    NODE,
    SHAPE,
    LABEL,
    DATA,
    EQUALS,
    ARROW,
    LBRACKET,
    RBRACKET,
    SEMICOLON,
    IDENTIFIER,
    STRING_LITERAL,
    JSON
};
struct Token
{
    TokenType type;
    std::string lexeme;
};