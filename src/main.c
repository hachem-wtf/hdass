#include <stdio.h>
#include <stddef.h>

#include "io/file.h"
#include "cli/args.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

static void print_expr(struct Expr* expr)
{
	switch (expr->kind)
	{
		case EXPR_PRIMARY:
			printf("%.*s", (int)expr->primary.token.length, expr->primary.token.start);
			break;
		case EXPR_BINARY:
			print_expr(expr->binary.left);
			printf(" %.*s ", (int)expr->binary.op.length, expr->binary.op.start);
			print_expr(expr->binary.right);
			break;
		case EXPR_MEMBER:
			print_expr(expr->member.object);
			printf(".%.*s", (int)expr->member.member.length, expr->member.member.start);
			break;
	}
}

static void print_statement(struct Statement* statement, const char* indent)
{
	switch (statement->kind)
	{
		case STATEMENT_ASSIGN:
		{
			struct AssignStatement assign = statement->assign;
			printf("%s%s%.*s %.*s ", indent,
				assign.target_deref ? "^" : "",
				(int)assign.target.length, assign.target.start,
				(int)assign.op.length, assign.op.start);
			print_expr(assign.value);
			printf("\n");
			break;
		}
		case STATEMENT_LABEL:
			printf("%s%.*s:\n", indent, (int)statement->label.name.length, statement->label.name.start);
			break;
		case STATEMENT_GOTO:
			printf("%sgoto %.*s\n", indent, (int)statement->jump.label.length, statement->jump.label.start);
			break;
		case STATEMENT_SYSCALL:
			printf("%ssyscall\n", indent);
			break;
		case STATEMENT_IF:
			printf("%sif ", indent);
			print_expr(statement->branch.left);
			printf(" %.*s ", (int)statement->branch.comparison.length, statement->branch.comparison.start);
			print_expr(statement->branch.right);
			printf("\n");
			print_statement(statement->branch.body, "    ");
			break;
	}
}

int main(int argc, char** argv)
{
	struct Args args;

	enum ParseResult result = parse_args(argc, argv, &args);
	if (result == PARSE_EXIT)
		return 0;
	if (result == PARSE_ERROR)
		return 1;

	struct File source;
	if (!read_file(args.input_path, &source))
		return 1;

	struct Lexer lexer = create_lexer(source.data);

	struct Program program;
	if (!parse_program(&lexer, &program))
	{
		free_program(&program);
		free_file(&source);
		return 1;
	}

	for (size_t i = 0; i < program.const_count; i += 1)
	{
		struct ConstDecl decl = program.consts[i];
		printf("const %.*s = %.*s\n",
			(int)decl.name.length, decl.name.start,
			(int)decl.value.length, decl.value.start);
	}

	for (size_t i = 0; i < program.data_count; i += 1)
	{
		struct DataDecl decl = program.data_decls[i];
		printf("data %.*s = %.*s\n",
			(int)decl.name.length, decl.name.start,
			(int)decl.value.length, decl.value.start);
	}

	for (size_t i = 0; i < program.proc_count; i += 1)
	{
		struct ProcDecl proc = program.procs[i];
		printf("proc %.*s(", (int)proc.name.length, proc.name.start);
		for (size_t p = 0; p < proc.param_count; p += 1)
		{
			struct Param param = proc.params[p];
			printf("%s%.*s: %.*s", p == 0 ? "" : ", ",
				(int)param.name.length, param.name.start,
				(int)param.reg.length, param.reg.start);
		}
		printf(")\n");

		for (size_t s = 0; s < proc.body_count; s += 1)
			print_statement(&proc.body[s], "  ");
	}

	free_program(&program);
	free_file(&source);
}
