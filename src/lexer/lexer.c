#include <stdbool.h>

#include "lexer/lexer.h"

static bool is_alpha(char character)
{
	return (character >= 'a' && character <= 'z') || 
		   (character >= 'A' && character <= 'Z') || 
		    character == '_';
}

static bool is_digit(char character)
{
	return character >= '0' && character <= '9';
}

static char peek(struct Lexer* lexer)
{
	return *lexer->current;
}

static char advance(struct Lexer* lexer)
{
	char c = *lexer->current;
	lexer->current += 1;
	return c;
}

static void skip_whitespace(struct Lexer* lexer)
{
	for (;;)
	{
		char c = peek(lexer);
		if (c == ' ' || c == '\t' || c == '\r')
			advance(lexer);
		else if (c == '\n')
		{
			lexer->line += 1;
			advance(lexer);
		}
		else
			return;
	}
}

static struct Token make_token(struct Lexer* lexer, enum TokenType type, const char* start)
{
	struct Token token;
	token.type = type;
	token.start = start;
	token.length = (size_t)(lexer->current - start);
	token.line = lexer->line;
	return token;
}

struct Lexer create_lexer(const char* source)
{
	struct Lexer lexer;
	lexer.source = source;
	lexer.current = source;
	lexer.line = 1;
	return lexer;
}

struct Token scan_token(struct Lexer* lexer)
{
	skip_whitespace(lexer);

	const char* start = lexer->current;
	if (peek(lexer) == '\0')
		return make_token(lexer, TOKEN_EOF, start);
	char character = advance(lexer);

	if (is_alpha(character))
	{
		while (is_alpha(peek(lexer)) || is_digit(peek(lexer)))
			advance(lexer);
		return make_token(lexer, TOKEN_IDENTIFIER, start);
	}

	if (is_digit(character))
	{
		while (is_digit(peek(lexer)))
			advance(lexer);
		return make_token(lexer, TOKEN_INTEGER, start);
	}

	switch (character)
	{
		case '=': return make_token(lexer, TOKEN_EQUAL,         start);
		case '+': return make_token(lexer, TOKEN_PLUS,          start);
		case '-': return make_token(lexer, TOKEN_MINUS,         start);
		case '*': return make_token(lexer, TOKEN_STAR,          start);
		case '/': return make_token(lexer, TOKEN_SLASH,         start);
		case '^': return make_token(lexer, TOKEN_CARET,         start);
		case '.': return make_token(lexer, TOKEN_DOT,           start);
		case ',': return make_token(lexer, TOKEN_COMMA,         start);
		case ':': return make_token(lexer, TOKEN_COLON,         start);
		case '(': return make_token(lexer, TOKEN_LEFT_PAREN,    start);
		case ')': return make_token(lexer, TOKEN_RIGHT_PAREN,   start);
		case '[': return make_token(lexer, TOKEN_LEFT_BRACKET,  start);
		case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET, start);
		case '{': return make_token(lexer, TOKEN_LEFT_BRACE,    start);
		case '}': return make_token(lexer, TOKEN_RIGHT_BRACE,   start);
	}

	return make_token(lexer, TOKEN_UNKNOWN, start);
}

const char* token_type_name(enum TokenType type)
{
	switch (type)
	{
		case TOKEN_EOF:           return "eof";
		case TOKEN_IDENTIFIER:    return "identifier";
		case TOKEN_INTEGER:       return "integer";
		case TOKEN_EQUAL:         return "equal";
		case TOKEN_PLUS:          return "plus";
		case TOKEN_MINUS:         return "minus";
		case TOKEN_STAR:          return "star";
		case TOKEN_SLASH:         return "slash";
		case TOKEN_CARET:         return "caret";
		case TOKEN_DOT:           return "dot";
		case TOKEN_COMMA:         return "comma";
		case TOKEN_COLON:         return "colon";
		case TOKEN_LEFT_PAREN:    return "left_paren";
		case TOKEN_RIGHT_PAREN:   return "right_paren";
		case TOKEN_LEFT_BRACKET:  return "left_bracket";
		case TOKEN_RIGHT_BRACKET: return "right_bracket";
		case TOKEN_LEFT_BRACE:    return "left_brace";
		case TOKEN_RIGHT_BRACE:   return "right_brace";
		case TOKEN_UNKNOWN:       return "unknown";
	}

	return "unknown";
}
