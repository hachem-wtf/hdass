#include <stdbool.h>

#include "parser/parser.h"
#include "sema/sema.h"
#include "tests.h"

static bool analyze_source(const char* source)
{
	struct Lexer lexer = create_lexer(source);
	struct Program program;
	if (!parse_program(&lexer, &program))
	{
		free_program(&program);
		return false;
	}

	bool ok = analyze_program(&program);
	free_program(&program);
	return ok;
}

static void test_valid_program(struct TestContext* context)
{
	check(context, analyze_source("const N = 1\ndata msg = \"hi\"\nproc main\n{\nsyscall\n}\n"));
}

static void test_no_entry_is_ok(struct TestContext* context)
{
	// without an [entry: ...] directive there is no required entry point
	check(context, analyze_source("proc helper\n{\nsyscall\n}\n"));
}

static void test_defined_entry(struct TestContext* context)
{
	check(context, analyze_source("[entry: start]\nproc start\n{\nsyscall\n}\n"));
}

static void test_undefined_entry(struct TestContext* context)
{
	check(context, !analyze_source("[entry: main]\nproc helper\n{\nsyscall\n}\n"));
}

static void test_duplicate_const(struct TestContext* context)
{
	check(context, !analyze_source("const X = 1\nconst X = 2\nproc main\n{\nsyscall\n}\n"));
}

static void test_duplicate_across_kinds(struct TestContext* context)
{
	check(context, !analyze_source("data foo = \"a\"\nproc foo\n{\nsyscall\n}\nproc main\n{\nsyscall\n}\n"));
}

void run_sema_tests(struct TestContext* context)
{
	test_valid_program(context);
	test_no_entry_is_ok(context);
	test_defined_entry(context);
	test_undefined_entry(context);
	test_duplicate_const(context);
	test_duplicate_across_kinds(context);
}
