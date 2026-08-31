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

	struct Source diagnostics = { "<test>", source };
	bool ok = analyze_program(diagnostics, &program);
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

static void test_undefined_name(struct TestContext* context)
{
	check(context, !analyze_source("proc main\n{\nrax = MISSING\n}\n"));
}

static void test_undefined_label(struct TestContext* context)
{
	check(context, !analyze_source("proc main\n{\ngoto nowhere\n}\n"));
}

static void test_undefined_call(struct TestContext* context)
{
	check(context, !analyze_source("proc main\n{\nnope(rax)\n}\n"));
}

static void test_call_arg_count(struct TestContext* context)
{
	check(context, !analyze_source("proc f(a: rdi)\n{\nsyscall\n}\nproc main\n{\nf(rax, rbx)\n}\n"));
}

static void test_assign_to_const(struct TestContext* context)
{
	check(context, !analyze_source("const K = 5\nproc main\n{\nK = 1\n}\n"));
}

static void test_const_expr_ok(struct TestContext* context)
{
	check(context, analyze_source("const A = 1\nconst B = A + 2 * 3\nproc main\n{\nsyscall\n}\n"));
}

static void test_const_expr_rejects_register(struct TestContext* context)
{
	check(context, !analyze_source("const X = rax + 1\nproc main\n{\nsyscall\n}\n"));
}

static void test_const_expr_rejects_data(struct TestContext* context)
{
	check(context, !analyze_source("data d = \"x\"\nconst X = d\nproc main\n{\nsyscall\n}\n"));
}

static void test_deref_needs_register(struct TestContext* context)
{
	check(context, !analyze_source("proc main\n{\nrax = ^MISSING\n}\n"));
	check(context, analyze_source("proc main\n{\nrax = ^rsi\n}\n"));
}

static void test_stack_size_constant(struct TestContext* context)
{
	check(context, analyze_source(
		"struct P\n{\nx\ny\n}\nconst N = 4\n"
		"proc main\n{\nstack a[P.size]\nstack b[N * 2]\nsyscall\n}\n"));
	check(context, !analyze_source("proc main\n{\nstack a[rax]\n}\n"));
}

static void test_enum_struct_members(struct TestContext* context)
{
	check(context, analyze_source(
		"enum Color\n{\nRed,\nGreen\n}\nstruct Point\n{\nx\ny\n}\n"
		"proc main\n{\nrax = Color.Green\nrbx = Point.y\nrcx = Point.size\n}\n"));
	check(context, !analyze_source(
		"enum Color\n{\nRed\n}\nproc main\n{\nrax = Color.Nope\n}\n"));
	check(context, !analyze_source(
		"struct Point\n{\nx\n}\nproc main\n{\nrax = Point.z\n}\n"));
}

static void test_references_resolve(struct TestContext* context)
{
	// registers, params, consts, data (+ .len), stack buffers, labels, calls
	check(context, analyze_source(
		"const N = 1\n"
		"data msg = \"hi\"\n"
		"proc helper(value: rdi)\n{\nrax = value\n}\n"
		"proc main\n{\n"
		"stack buf[8]\n"
		"rax = N\n"
		"rsi = msg\n"
		"rdx = msg.len\n"
		"rdi = buf + 1\n"
		"helper(rax)\n"
		"loop:\n"
		"goto loop\n"
		"}\n"));
}

void run_sema_tests(struct TestContext* context)
{
	test_valid_program(context);
	test_no_entry_is_ok(context);
	test_defined_entry(context);
	test_undefined_entry(context);
	test_duplicate_const(context);
	test_duplicate_across_kinds(context);
	test_undefined_name(context);
	test_undefined_label(context);
	test_undefined_call(context);
	test_call_arg_count(context);
	test_assign_to_const(context);
	test_const_expr_ok(context);
	test_const_expr_rejects_register(context);
	test_const_expr_rejects_data(context);
	test_deref_needs_register(context);
	test_stack_size_constant(context);
	test_enum_struct_members(context);
	test_references_resolve(context);
}
