#include <string.h>
#include <stdbool.h>

#include "lexer/lexer.h"
#include "tests.h"

static bool token_matches(struct Token token, enum TokenType type, const char* text)
{
	size_t length = strlen(text);
	return token.type == type
		&& token.length == length
		&& memcmp(token.start, text, length) == 0;
}

static void test_identifiers_and_integers(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("rax 42");
	check(context, token_matches(scan_token(&lexer), TOKEN_IDENTIFIER, "rax"));
	check(context, token_matches(scan_token(&lexer), TOKEN_INTEGER, "42"));
	check(context, scan_token(&lexer).type == TOKEN_EOF);
}

static void test_operators(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("= == += != /");
	check(context, scan_token(&lexer).type == TOKEN_EQUAL);
	check(context, scan_token(&lexer).type == TOKEN_EQUAL_EQUAL);
	check(context, scan_token(&lexer).type == TOKEN_PLUS_EQUAL);
	check(context, scan_token(&lexer).type == TOKEN_BANG_EQUAL);
	check(context, scan_token(&lexer).type == TOKEN_SLASH);
}

static void test_literals(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("\"hi\\n\" '0'");
	check(context, token_matches(scan_token(&lexer), TOKEN_STRING, "\"hi\\n\""));
	check(context, token_matches(scan_token(&lexer), TOKEN_CHAR, "'0'"));
}

static void test_keywords(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const proc mainly");
	check(context, scan_token(&lexer).type == TOKEN_CONST);
	check(context, scan_token(&lexer).type == TOKEN_PROC);
	check(context, scan_token(&lexer).type == TOKEN_IDENTIFIER);
}

static void test_size_keywords(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("byte word dword qword");
	check(context, scan_token(&lexer).type == TOKEN_BYTE);
	check(context, scan_token(&lexer).type == TOKEN_WORD);
	check(context, scan_token(&lexer).type == TOKEN_DWORD);
	check(context, scan_token(&lexer).type == TOKEN_QWORD);
}

static void test_line_counting(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("a\nb\nc");
	check(context, scan_token(&lexer).line == 1);
	check(context, scan_token(&lexer).line == 2);
	check(context, scan_token(&lexer).line == 3);
}

static void test_comments(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("rax // line comment\n/* block\ncomment */ rbx");
	check(context, token_matches(scan_token(&lexer), TOKEN_IDENTIFIER, "rax"));

	struct Token second = scan_token(&lexer);
	check(context, token_matches(second, TOKEN_IDENTIFIER, "rbx"));
	check(context, second.line == 3);
	check(context, scan_token(&lexer).type == TOKEN_EOF);
}

void run_lexer_tests(struct TestContext* context)
{
	test_identifiers_and_integers(context);
	test_operators(context);
	test_literals(context);
	test_keywords(context);
	test_size_keywords(context);
	test_line_counting(context);
	test_comments(context);
}
