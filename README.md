# Keshews

**Keshews** is a lightweight compiled programming language featuring static typing, semantic analysis, LLVM IR code generation, JIT (Just-In-Time) execution via LLVM ORC JIT, and an interactive REPL with persistent runtime state.


## Features

- **Static Type Checking**: Built-in types for `int`, `double`, `string`, and `bool`.
- **Control Flow**: `if`, `elif`, and `else` branching; `for` and `while` loops.
- **LLVM JIT Execution**: Directly compiles and executes scripts in-memory using LLVM ORC JIT (`LLJIT`).
- **Interactive REPL**: Multi-line block input with automatic brace balancing and session variable persistence.
- **Clang/Rust-style Diagnostics**: Rich terminal error reporting with source code snippets, line numbers, and column carets (`^`).

## Dependencies & Prerequisites

To build Keshews from source, you will need:

1. **C++20 Compiler**:
   - Must support `<format>` (`std::format`), e.g., **Clang 17+** (with `libc++`), **Apple Clang 15+** (Xcode 15+), or **GCC 13+** (Linux).
   - *macOS note:* Use Clang on macOS. Homebrew LLVM is compiled with `libc++`, which is ABI-incompatible with GCC's `libstdc++`.
2. **CMake**: Version **3.20** or newer.
3. **LLVM**: Version **15.0** or newer (tested up to LLVM 23).
4. **Build Tools**: `make` or `ninja`.

### Installing Dependencies

#### macOS

Using [Homebrew](https://brew.sh/):

```bash
# Install CMake and LLVM
brew install cmake llvm

# Ensure Xcode Command Line Tools are installed (for Apple Clang)
xcode-select --install
```

> **Note:** Homebrew installs LLVM into `/opt/homebrew/opt/llvm` (Apple Silicon) or `/usr/local/opt/llvm` (Intel). CMake is pre-configured to detect these paths automatically. If needed, you can explicitly point CMake to LLVM:
> ```bash
> export LLVM_DIR="$(brew --prefix llvm)/lib/cmake/llvm"
> ```

#### Linux

##### Ubuntu / Debian (22.04 LTS / 24.04 LTS)

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake clang llvm-dev libzstd-dev zlib1g-dev
```

If your distribution package manager provides an older LLVM version (< 15), install modern LLVM via the official LLVM APT repository:

```bash
wget https://apt.llvm.org/llvm.sh
chmod +x llvm.sh
sudo ./llvm.sh 18
```

##### Fedora / RHEL

```bash
sudo dnf install -y gcc-c++ clang cmake llvm-devel zlib-devel libzstd-devel
```

##### Arch Linux

```bash
sudo pacman -S --needed base-devel cmake clang llvm
```

## Building from Source

1. **Clone the repository:**

   ```bash
   git clone https://github.com/your-username/keshews.git
   cd keshews
   ```

2. **Configure with CMake:**

   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   ```

   *(On macOS or if you have multiple compilers, explicitly select Clang):*
   ```bash
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
   ```

3. **Compile:**

   ```bash
   cmake --build build -j
   ```

   The compiled executable will be located at `build/keshews`.


## Usage

### 1. Execute a `.kesh` File

Run any source file directly using the JIT engine:

```bash
./build/keshews examples/loops.kesh
```

### 2. Interactive REPL

Start the REPL by running the binary with no arguments:

```bash
./build/keshews
```

```text
keshews (REPL)
Type 'exit' or 'quit' to quit.
kesh> int a = 10
kesh> int b = 20
kesh> print(a+b)
30
kesh> exit
```

- **Multi-line blocks**: If you open a block with `{`, the REPL prompts with `...` until matching closing braces `}` are entered.
- **State persistence**: Variables declared in earlier REPL inputs persist across subsequent inputs.
- To exit the REPL, type `exit` or `quit` (or press `Ctrl+D`).

## Language Syntax Guide

### Data Types

Keshews supports four primitive types:
- `int` (64-bit integer)
- `double` (64-bit IEEE 754 floating point)
- `string` (character strings)
- `bool` (`true` or `false`)

### Variable Declaration & Assignment

```kesh
int x = 42
double pi = 3.14159
string msg = "Hello, Keshews!"
bool active = true

// Reassignment
x = 100
```

### Conditionals (`if`, `elif`, `else`)

```kesh
int a = 20
int b = 15

if (a > b) {
    print(b)
} elif (a == b) {
    print("equal")
} else {
    print(a)
}
```

### Loops

#### For Loop
```kesh
int n = 5
for (int i = 0; i < n; i = i + 1) {
    print(i)
}
```

>NOTE: i++ won't work as unary operators aren't implemented yet.

#### While Loop
```kesh
int count = 0
while (count < 3) {
    print(count)
    count = count + 1
}
```

### Built-in Output (`print`)

`print(...)` prints any supported expression followed by a newline:

```kesh
print(10 + 2)
print("Welcome to Keshews")
print(true)
```

