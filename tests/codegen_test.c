#include <stdio.h>
#include <string.h>

#include "parser/parser.h"
#include "codegen/nasm.h"
#include "tests.h"

static void generate_to_buffer(struct Program* program, char* buffer, size_t size)
{
	FILE* out = tmpfile();
	if (out == NULL)
	{
		buffer[0] = '\0';
		return;
	}

	generate_nasm(program, out);
	fflush(out);
	rewind(out);

	size_t read = fread(buffer, 1, size - 1, out);
	buffer[read] = '\0';
	fclose(out);
}

static void test_generate_consts_and_data(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("const N = 5\ndata msg = \"hi\"\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "%define N (5)") != NULL);
	check(context, strstr(buffer, "section .data") != NULL);
	check(context, strstr(buffer, "msg: db `hi`") != NULL);
	check(context, strstr(buffer, ".len equ $ - msg") != NULL);
	check(context, strstr(buffer, "section .text") != NULL);

	free_program(&program);
}

static void test_generate_text(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nrax = SYS_WRITE\nrsi = message\nrdx = message.len\n^rdi = rax\nloop:\nsyscall\ngoto loop\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "main:") != NULL);
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

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "cmp rcx, 0") != NULL);
	check(context, strstr(buffer, "je .if_end_0") != NULL);
	check(context, strstr(buffer, "jmp loop") != NULL);
	check(context, strstr(buffer, ".if_end_0:") != NULL);

	free_program(&program);
}

static void test_generate_if_else(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nif rax > 3\n{\nrdi = 1\n}\nelse\n{\nrdi = 0\n}\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "jle .if_else_0") != NULL);
	check(context, strstr(buffer, "mov rdi, 1") != NULL);
	check(context, strstr(buffer, "jmp .if_end_0") != NULL);
	check(context, strstr(buffer, ".if_else_0:") != NULL);
	check(context, strstr(buffer, "mov rdi, 0") != NULL);
	check(context, strstr(buffer, ".if_end_0:") != NULL);

	free_program(&program);
}

static void test_generate_call(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc print_number(value: rdi)\n{\nsyscall\n}\nproc main\n{\nprint_number(r12)\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

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

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov rax, rdi") != NULL);
	check(context, strstr(buffer, "value") == NULL);

	free_program(&program);
}

static void test_generate_divide(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax /= rbx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "cqo") != NULL);
	check(context, strstr(buffer, "idiv rbx") != NULL);

	free_program(&program);
}

static void test_generate_multiply(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrax = rbx * rcx\nrbx = rcx * 3 + rdx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov rax, rbx") != NULL);
	check(context, strstr(buffer, "imul rax, rcx") != NULL);
	check(context, strstr(buffer, "imul rbx, 3") != NULL);
	check(context, strstr(buffer, "add rbx, rdx") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_modulo(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nrbx %= rcx\nrax = rsi % rdi\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	// modulo takes idiv's remainder from rdx
	check(context, strstr(buffer, "mov rax, rbx") != NULL);
	check(context, strstr(buffer, "idiv rcx") != NULL);
	check(context, strstr(buffer, "mov rbx, rdx") != NULL);
	check(context, strstr(buffer, "mov rax, rdx") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_divide_nonrax(struct TestContext* context)
{
	// division into a register other than rax routes through rax:rdx
	struct Lexer lexer = create_lexer("proc main\n{\nrbx /= rcx\nrsi = rdi / rcx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov rax, rbx") != NULL);
	check(context, strstr(buffer, "idiv rcx") != NULL);
	check(context, strstr(buffer, "mov rbx, rax") != NULL);
	check(context, strstr(buffer, "mov rsi, rax") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_stack_frame(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc work\n{\nstack buffer[32]\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "push rbp") != NULL);
	check(context, strstr(buffer, "mov rbp, rsp") != NULL);
	check(context, strstr(buffer, "sub rsp, 32") != NULL);
	check(context, strstr(buffer, "leave") != NULL);

	free_program(&program);
}

static void test_generate_address_expr(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc work\n{\nstack buffer[32]\nrsi = buffer + 31\nrdx = buffer + 32 - rsi\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "lea rsi, [rbp - 32]") != NULL);
	check(context, strstr(buffer, "add rsi, 31") != NULL);
	check(context, strstr(buffer, "lea rdx, [rbp - 32]") != NULL);
	check(context, strstr(buffer, "add rdx, 32") != NULL);
	check(context, strstr(buffer, "sub rdx, rsi") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_sized_store(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\n^byte rsi = rdx\n^dword rsi = rax\n^byte rsi = 5\n^rsi = rbx\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov byte [rsi], dl") != NULL);
	check(context, strstr(buffer, "mov dword [rsi], eax") != NULL);
	check(context, strstr(buffer, "mov byte [rsi], 5") != NULL);
	check(context, strstr(buffer, "mov [rsi], rbx") != NULL);

	free_program(&program);
}

static void test_generate_entry_and_bits(struct TestContext* context)
{
	struct Lexer with_entry = create_lexer("[bits: 64]\n[entry: main]\nproc main\n{\nsyscall\n}\nproc helper\n{\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&with_entry, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "bits 64") != NULL);
	check(context, strstr(buffer, "global main") != NULL);
	check(context, strstr(buffer, "main:") != NULL);
	check(context, strstr(buffer, "_start") == NULL);
	// the entry proc does not return; the non-entry helper does
	check(context, strstr(buffer, "helper:\n\tsyscall\n\tret") != NULL);

	free_program(&program);
}

static void test_generate_no_entry(struct TestContext* context)
{
	struct Lexer no_entry = create_lexer("proc main\n{\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&no_entry, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "global") == NULL);
	check(context, strstr(buffer, "main:") != NULL);

	free_program(&program);
}

static void test_generate_logical_registers(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"[enable: logical_registers]\nproc main\n{\nr1 = 5\nr4 = r10\nr6 = r1.8\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov rax, 5") != NULL);    // r1 -> rax
	check(context, strstr(buffer, "mov rdx, r11") != NULL);  // r4 -> rdx, r10 -> r11
	check(context, strstr(buffer, "mov rdi, al") != NULL);   // r6 -> rdi, r1.8 -> al

	free_program(&program);
}

static void test_generate_logical_disabled(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nr1 = 5\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	// without the extension, r1 is passed through untouched
	check(context, strstr(buffer, "mov r1, 5") != NULL);

	free_program(&program);
}

static void test_generate_load(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nrax = ^rsi\nrbx = ^byte rsi\nrcx = ^dword rsi\nrdx = ^rsi + 4\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov rax, [rsi]") != NULL);
	check(context, strstr(buffer, "movzx rbx, byte [rsi]") != NULL);
	check(context, strstr(buffer, "mov ecx, [rsi]") != NULL);
	check(context, strstr(buffer, "mov rdx, [rsi]") != NULL);
	check(context, strstr(buffer, "add rdx, 4") != NULL);
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_enum_struct(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"enum Color\n{\nRed,\nGreen,\nBlue\n}\n"
		"struct Point\n{\nx: qword\ny: qword\nflag: byte\n}\n"
		"proc main\n{\nrax = Color.Blue\nrbx = Point.y\nrcx = Point.flag\nrdx = Point.size\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "mov rax, 2") != NULL);   // Color.Blue -> 2
	check(context, strstr(buffer, "mov rbx, 8") != NULL);   // Point.y -> 8
	check(context, strstr(buffer, "mov rcx, 16") != NULL);  // Point.flag -> 16
	check(context, strstr(buffer, "mov rdx, 17") != NULL);  // Point.size -> 17

	free_program(&program);
}

static void test_generate_floats(struct TestContext* context)
{
	struct Lexer lexer = create_lexer(
		"proc main\n{\nxmm0 = 3.5\nxmm0 *= xmm1\nrax = 4\nxmm2 = rax\nrbx = xmm0\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "__float0: dq 3.5") != NULL);
	check(context, strstr(buffer, "movsd xmm0, [__float0]") != NULL);
	check(context, strstr(buffer, "mulsd xmm0, xmm1") != NULL);   // float arithmetic
	check(context, strstr(buffer, "cvtsi2sd xmm2, rax") != NULL); // int -> float
	check(context, strstr(buffer, "cvttsd2si rbx, xmm0") != NULL); // float -> int
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_float_compare(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\nif xmm0 > 4.0\ngoto done\ndone:\nsyscall\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "ucomisd xmm0, [__float0]") != NULL);
	check(context, strstr(buffer, "jbe .if_end") != NULL); // '>' skips when <=
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

static void test_generate_add_zero_peephole(struct TestContext* context)
{
	// `+= 0` / `-= 0` (e.g. a struct field at offset 0) is dropped; `*= 0` is not
	struct Lexer lexer = create_lexer("proc main\n{\nrax += 0\nrbx -= 0\nrcx *= 0\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "add rax, 0") == NULL);
	check(context, strstr(buffer, "sub rbx, 0") == NULL);
	check(context, strstr(buffer, "imul rcx, 0") != NULL);

	free_program(&program);
}

static void test_generate_float_memory(struct TestContext* context)
{
	struct Lexer lexer = create_lexer("proc main\n{\n^rsi = xmm0\nxmm1 = ^rsi\n}\n");
	struct Program program;
	check(context, parse_program(&lexer, &program));

	char buffer[1024];
	generate_to_buffer(&program, buffer, sizeof(buffer));

	check(context, strstr(buffer, "movsd [rsi], xmm0") != NULL); // float store
	check(context, strstr(buffer, "movsd xmm1, [rsi]") != NULL); // float load
	check(context, strstr(buffer, "; TODO") == NULL);

	free_program(&program);
}

void run_codegen_tests(struct TestContext* context)
{
	test_generate_floats(context);
	test_generate_float_compare(context);
	test_generate_float_memory(context);
	test_generate_add_zero_peephole(context);
	test_generate_enum_struct(context);
	test_generate_load(context);
	test_generate_logical_registers(context);
	test_generate_logical_disabled(context);
	test_generate_entry_and_bits(context);
	test_generate_no_entry(context);
	test_generate_consts_and_data(context);
	test_generate_text(context);
	test_generate_if(context);
	test_generate_if_else(context);
	test_generate_call(context);
	test_generate_param_substitution(context);
	test_generate_divide(context);
	test_generate_multiply(context);
	test_generate_modulo(context);
	test_generate_divide_nonrax(context);
	test_generate_stack_frame(context);
	test_generate_address_expr(context);
	test_generate_sized_store(context);
}
