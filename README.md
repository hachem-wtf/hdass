# hdass
hachem's dumb assembly super-set, pronounced "HD Ass".

The entire point of this project is to provide a middle ground between C and assembly. If we think about why we still write assembly today, it's usually because we need direct control over what the CPU is doing. We want control over the exact instructions being executed, the memory, the stack and everything else that higher-level programming languages normally abstract away. The thing is, not every program written in assembly actually needs that control. Sometimes you want to write something close to the machine without having to manuall deal with every tiny detail yourself. You still want registers, explicit control over memory and a good understanding of what your program is doing, but you don't necessarily need to manually express everything as individual assembly instructions.

This is where hdass comes in. It's not quite high-level enough to be a C-like language, but it's also not low-level enough to be as annoying to write as raw assembly. The goal is to sit somewhere in between, keeping the parts of assembly that make it useful while making the parts that don't need to be painful a little nicer to work with. hdass transpiles into multiple flavours of assembly, such as NASM and MASM, rather than directly producing machine code. The idea is to provide a single language for writing low-level programs while allowing the backend to translate code into the assembler syntax you want to target. You're still ultimately producing assembly, and you're never particularly far away from the code that gets assembled. The goal isn't to hide the machine from you or turn assembly into C. There are already plenty of high-level languages that do that. hdass just fills the gap between the two, where you might want some more convenience whilst writing assembly without taking away the reason you wanted to work close to the machine in the first place.

## Examples
Here's a simple "Hello, World!" world program written using hdass' syntax:
```hdass
const SYS_WRITE = 1
const SYS_EXIT = 60

const STDOUT = 1

data message = "Hello, World!\n"

proc main
{
    rax = SYS_WRITE
    rdi = STDOUT
    rsi = message
    rdx = message.len
    syscall

    rax = SYS_EXIT
    rdi = 0
    syscall
}
```

The program still directly controls the registers used for the system calls. Nothing is hiding what the program is doing. More examples can be found in [examples/](examples/).

## Building
The build is driven by [premake5](https://premake.github.io/). Generate the makefiles and build the compiler:
```bash
premake5 gmake
make config=debug
```
This produces the `hdass` binary at `bin/<config>-<system>/hdass` (for example `bin/debug-macosx/hdass` or `bin/debug-linux/hdass`). The available configurations are `debug`, `release` and `dist`. To run the unit tests:
```bash
make config=debug
./bin/<config>-<system>/tests
```

hdass emits x86-64 assembly, so to actually assemble and run its output you need an x86-64 Linux toolchain. The bundled Docker environment provides a consistent one on any host, including Apple Silicon, where the amd64 image runs under emulation. The image is a Debian base with `nasm`, `ld` (binutils), a C toolchain and `premake5` preinstalled.

Start the container (this builds the image the first time):
```bash
docker compose up -d --build
```

Open a shell inside it. The repository is bind-mounted at `/hdass`, so edits on the host are visible immediately:
```bash
docker compose exec hdass bash
```

From inside the container you can build the compiler and take a program all the way to a running executable:
```bash
premake5 gmake && make config=debug
./bin/debug-linux/hdass examples/hello_world.hdass -o hello.asm
nasm -f elf64 hello.asm -o hello.o
ld hello.o -o hello
./hello
```

When you're finished, stop and remove the container:
```bash
docker compose down
```

## Disclaimer
hdass is extremely experimental. The language, syntax and semantics are still being figured out, so things will probably change, sometimes because there is a better way to do something and sometimes because I decided the old syntax looked stupid.

## License
This project is licensed under the MIT License. See [LICENSE](LICENSE) for more information.
