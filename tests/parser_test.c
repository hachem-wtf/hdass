#include <string.h>
#include <stdbool.h>

#include "parser/parser.h"
#include "tests.h"

static bool text_is(struct Token token, const char* text)
{
	size_t length = strlen(text);
	return token.length == length && memcmp(token.start, text, length) == 0;
}

static bool primary_is(struct Expr* expr, const char* text)
{
	return expr->kind == EXPR_PRIMARY && text_is(expr->primary.token, text);
}

static void test_parse_consts(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const A = 1\nconst B = 60\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.const_count == 2);
	check(context, text_is(program.consts[0].name, "A"));
	check(context, primary_is(program.consts[0].value, "1"));
	check(context, text_is(program.consts[1].name, "B"));
	check(context, primary_is(program.consts[1].value, "60"));

	free_program(&program);
}

static void test_parse_const_expr(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const A = 1\nconst B = A + 2 * 3\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));

	struct Expr* value = program.consts[1].value;
	check(context, value->kind == EXPR_BINARY);
	check(context, text_is(value->binary.op, "+"));
	check(context, primary_is(value->binary.left, "A"));
	check(context, value->binary.right->kind == EXPR_BINARY);

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

static void test_parse_proc_body(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax = 1\nrcx -= 1\n^rsi = rdx\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.proc_count == 1);
	check(context, program.procs[0].body_count == 3);

	struct Statement first = program.procs[0].body[0];
	check(context, first.kind == STATEMENT_ASSIGN);
	check(context, !first.assign.target_deref);
	check(context, text_is(first.assign.target, "rax"));
	check(context, text_is(first.assign.op, "="));
	check(context, primary_is(first.assign.value, "1"));

	struct Statement second = program.procs[0].body[1];
	check(context, text_is(second.assign.target, "rcx"));
	check(context, text_is(second.assign.op, "-="));

	struct Statement third = program.procs[0].body[2];
	check(context, third.assign.target_deref);
	check(context, text_is(third.assign.target, "rsi"));
	check(context, primary_is(third.assign.value, "rdx"));

	free_program(&program);
}

static void test_parse_simple_statements(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nloop:\nsyscall\ngoto loop\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.procs[0].body_count == 3);

	struct Statement label = program.procs[0].body[0];
	check(context, label.kind == STATEMENT_LABEL);
	check(context, text_is(label.label.name, "loop"));

	check(context, program.procs[0].body[1].kind == STATEMENT_SYSCALL);

	struct Statement jump = program.procs[0].body[2];
	check(context, jump.kind == STATEMENT_GOTO);
	check(context, text_is(jump.jump.label, "loop"));

	free_program(&program);
}

static void test_parse_expressions(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrsi = buffer + 31\nrdx = message.len\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.procs[0].body_count == 2);

	struct Expr* sum = program.procs[0].body[0].assign.value;
	check(context, sum->kind == EXPR_BINARY);
	check(context, text_is(sum->binary.op, "+"));
	check(context, primary_is(sum->binary.left, "buffer"));
	check(context, primary_is(sum->binary.right, "31"));

	struct Expr* member = program.procs[0].body[1].assign.value;
	check(context, member->kind == EXPR_MEMBER);
	check(context, primary_is(member->member.object, "message"));
	check(context, text_is(member->member.member, "len"));

	free_program(&program);
}

static void test_parse_if(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nif rax != 0\ngoto loop\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.procs[0].body_count == 1);

	struct Statement branch = program.procs[0].body[0];
	check(context, branch.kind == STATEMENT_IF);
	check(context, primary_is(branch.branch.left, "rax"));
	check(context, text_is(branch.branch.comparison, "!="));
	check(context, primary_is(branch.branch.right, "0"));
	check(context, branch.branch.body->kind == STATEMENT_GOTO);
	check(context, text_is(branch.branch.body->jump.label, "loop"));

	free_program(&program);
}

static void test_parse_call(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nprint_number(r12)\nf(a, b)\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.procs[0].body_count == 2);

	struct CallStatement first = program.procs[0].body[0].call;
	check(context, program.procs[0].body[0].kind == STATEMENT_CALL);
	check(context, text_is(first.name, "print_number"));
	check(context, first.arg_count == 1);
	check(context, primary_is(first.args[0], "r12"));

	struct CallStatement second = program.procs[0].body[1].call;
	check(context, second.arg_count == 2);
	check(context, primary_is(second.args[0], "a"));
	check(context, primary_is(second.args[1], "b"));

	free_program(&program);
}

static void test_parse_stack(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nstack buffer[32]\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.procs[0].body_count == 1);

	struct Statement statement = program.procs[0].body[0];
	check(context, statement.kind == STATEMENT_STACK);
	check(context, text_is(statement.stack.name, "buffer"));
	check(context, text_is(statement.stack.size, "32"));

	free_program(&program);
}

static void test_parse_sized_deref(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\n^byte rsi = rdx\n^rdi = rax\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.procs[0].body_count == 2);

	struct AssignStatement sized = program.procs[0].body[0].assign;
	check(context, sized.target_deref);
	check(context, sized.store_size == STORE_SIZE_BYTE);
	check(context, text_is(sized.target, "rsi"));

	struct AssignStatement plain = program.procs[0].body[1].assign;
	check(context, plain.target_deref);
	check(context, plain.store_size == STORE_SIZE_NONE);

	free_program(&program);
}

static void test_parse_directives(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("[bits: 32]\n[entry: kmain]\n[enable: logical_registers]\nproc kmain\n{\nsyscall\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));
	check(context, program.config.bits == 32);
	check(context, program.config.has_entry);
	check(context, text_is(program.config.entry, "kmain"));
	check(context, program.config.logical_registers);

	free_program(&program);
}

static void test_parse_bad_directive(struct TestContext* context)
{
	struct Lexer bad_bits = create_lexer("[bits: 16]\nproc main\n{\nsyscall\n}\n");
	struct Program program;
	check(context, !parse_program(&bad_bits, &program));
	free_program(&program);

	struct Lexer bad_key = create_lexer("[target: nasm]\nproc main\n{\nsyscall\n}\n");
	check(context, !parse_program(&bad_key, &program));
	free_program(&program);
}

static void test_parse_register_size_suffix(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nr1 = r2.8\n}\n");
	struct Program program;

	check(context, parse_program(&lexer, &program));

	struct Expr* value = program.procs[0].body[0].assign.value;
	check(context, value->kind == EXPR_MEMBER);
	check(context, value->member.member.type == TOKEN_INTEGER);
	check(context, text_is(value->member.member, "8"));
	check(context, value->member.object->kind == EXPR_PRIMARY);
	check(context, text_is(value->member.object->primary.token, "r2"));

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
	test_parse_const_expr(context);
	test_parse_data(context);
	test_parse_proc_params(context);
	test_parse_proc_body(context);
	test_parse_simple_statements(context);
	test_parse_expressions(context);
	test_parse_if(context);
	test_parse_call(context);
	test_parse_stack(context);
	test_parse_sized_deref(context);
	test_parse_directives(context);
	test_parse_bad_directive(context);
	test_parse_register_size_suffix(context);
	test_parse_errors(context);
}
