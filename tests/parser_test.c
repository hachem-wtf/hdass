#include <string.h>
#include <stdbool.h>

#include "parser/parser.h"
#include "tests.h"

static bool text_is(struct Token token, const char* text)
{
	size_t length = strlen(text);
	return token.length == length && memcmp(token.start, text, length) == 0;
}

static void test_parse_consts(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const A = 1\nconst B = 60\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.const_count == 2);
	check(context, text_is(program.consts[0].name, "A"));
	check(context, text_is(program.consts[0].value, "1"));
	check(context, text_is(program.consts[1].name, "B"));
	check(context, text_is(program.consts[1].value, "60"));

	free_program(&program);
}

static void test_parse_data(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("data msg = \"hi\"\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.data_count == 1);
	check(context, text_is(program.data_decls[0].name, "msg"));
	check(context, text_is(program.data_decls[0].value, "\"hi\""));

	free_program(&program);
}

static void test_parse_proc_params(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc f(a: rdi, b: rsi)\n{\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.proc_count == 1);
	check(context, text_is(program.procs[0].name, "f"));
	check(context, program.procs[0].param_count == 2);
	check(context, text_is(program.procs[0].params[0].name, "a"));
	check(context, text_is(program.procs[0].params[0].reg, "rdi"));
	check(context, text_is(program.procs[0].params[1].name, "b"));
	check(context, text_is(program.procs[0].params[1].reg, "rsi"));

	free_program(&program);
}

static void test_parse_proc_body_skipped(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax = 1\nsyscall\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.proc_count == 1);
	check(context, program.procs[0].param_count == 0);

	free_program(&program);
}

static void test_parse_errors(struct TestContext* context)
{
	struct Program program;

	struct Lexer missing_name = create_lexer("const = 1\n");
	check(context, !parse_program(&missing_name, &program));
	free_program(&program);

	struct Lexer wrong_value = create_lexer("data x = 5\n");
	check(context, !parse_program(&wrong_value, &program));
	free_program(&program);

	struct Lexer unterminated = create_lexer("proc main\n{\nrax = 1\n");
	check(context, !parse_program(&unterminated, &program));
	free_program(&program);
}

void run_parser_tests(struct TestContext* context)
{
	test_parse_consts(context);
	test_parse_data(context);
	test_parse_proc_params(context);
	test_parse_proc_body_skipped(context);
	test_parse_errors(context);
}
