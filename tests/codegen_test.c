#include <stdio.h>
#include <string.h>

#include "parser/parser.h"
#include "codegen/nasm.h"
#include "tests.h"

static void test_generate_consts_and_data(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const N = 5\ndata msg = \"hi\"\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "%define N 5") != NULL);
	check(context, strstr(buffer, "section .data") != NULL);
	check(context, strstr(buffer, "msg: db `hi`") != NULL);
	check(context, strstr(buffer, ".len equ $ - msg") != NULL);
	check(context, strstr(buffer, "section .text") != NULL);
	check(context, strstr(buffer, "global _start") != NULL);

	free_program(&program);
}

static void test_generate_text(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nrax = SYS_WRITE\nrsi = message\nrdx = message.len\n^rdi = rax\nloop:\nsyscall\ngoto loop\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "_start:") != NULL);
	check(context, strstr(buffer, "mov rax, SYS_WRITE") != NULL);
	check(context, strstr(buffer, "mov rsi, message") != NULL);
	check(context, strstr(buffer, "mov rdx, message.len") != NULL);
	check(context, strstr(buffer, "mov [rdi], rax") != NULL);
	check(context, strstr(buffer, "loop:") != NULL);
	check(context, strstr(buffer, "syscall") != NULL);
	check(context, strstr(buffer, "jmp loop") != NULL);

	free_program(&program);
}

static void test_generate_if(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nif rcx != 0\ngoto loop\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "cmp rcx, 0") != NULL);
	check(context, strstr(buffer, "je .if_end_0") != NULL);
	check(context, strstr(buffer, "jmp loop") != NULL);
	check(context, strstr(buffer, ".if_end_0:") != NULL);

	free_program(&program);
}

static void test_generate_call(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc print_number(value: rdi)\n{\nsyscall\n}\nproc main\n{\nprint_number(r12)\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rdi, r12") != NULL);
	check(context, strstr(buffer, "call print_number") != NULL);
	check(context, strstr(buffer, "\tret\n") != NULL);

	free_program(&program);
}

static void test_generate_param_substitution(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc print_number(value: rdi)\n{\nrax = value\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "mov rax, rdi") != NULL);
	check(context, strstr(buffer, "value") == NULL);

	free_program(&program);
}

static void test_generate_divide(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax /= rbx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "cqo") != NULL);
	check(context, strstr(buffer, "idiv rbx") != NULL);

	free_program(&program);
}

static void test_generate_stack_frame(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc work\n{\nstack buffer[32]\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	FILE* out = tmpfile();
	generate_nasm(&program, out);
	fflush(out);
	rewind(out);

	char buffer[1024];
	size_t read = fread(buffer, 1, sizeof(buffer) - 1, out);
	buffer[read] = '\0';
	fclose(out);

	check(context, strstr(buffer, "push rbp") != NULL);
	check(context, strstr(buffer, "mov rbp, rsp") != NULL);
	check(context, strstr(buffer, "sub rsp, 32") != NULL);
	check(context, strstr(buffer, "leave") != NULL);

	free_program(&program);
}

void run_codegen_tests(struct TestContext* context)
{
	test_generate_consts_and_data(context);
	test_generate_text(context);
	test_generate_if(context);
	test_generate_call(context);
	test_generate_param_substitution(context);
	test_generate_divide(context);
	test_generate_stack_frame(context);
}
