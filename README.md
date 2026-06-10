#  Wasp Programming Language

Wasp is a modern, statically-typed compiled programming language that combines the simplicity of Python with a powerful type system with generics, traits, and algebraic types.

## ✨ Features

- **Statically typed** with type inference
- **Generics** with type constraints
- **Traits** for polymorphism
- **Algebraic types** - variants and intersections
- **First-class functions** and closures
- **Classes** with support for traits
- **Primitive types** (int, float, str, bool)
- **Collections** (list, set, map)

## 📦 Installation

### Building from Source

```bash
# Clone the repository
git clone https://github.com/pranav-ap/wasp.git
cd wasp

# Create build directory and configure
cmake -B build -G Ninja

# Build the project
cmake --build build

# Create global symlink
sudo ln -s $(pwd)/build/src/wasp /usr/local/bin/wasp
```

### Running Tests

```bash
# Build and run all tests
cmake --build build && ctest --test-dir build -L "unit" --output-on-failure
```

## 🚀 Quick Start

### Hello World

```python
import core.io expose *

print("Hello, Wasp!")
```

### Variables and Types

```python
let x = 42           # Type inference
let y: int = 100     # Explicit type

let name = "Wasp"
let is_cool = true

const id = 5
```

### Functions

```python
# Regular function
fun add(a: int, b: int) => int
    return a + b

# Generic function
template
    T: int | float | str
fun greet(value: T)
    print(value)

# Function calls
let result = add(5, 3)
greet("Hello")
greet(42)
```

### Classes and Traits

```python
trait Printable
    fun repr() => str
        required

trait Truthy
    fun is_truthy() => bool
        required

class Foo is Printable & Truthy
    score: int

    fun repr() => str
        return "Foo: {self.score}"

    fun is_truthy() => bool
        return self.score > 10

    share fun shout(text: str)
        print(text)


let f = Foo(20)
print(f) # Calls repr()
print(f.is_truthy())  # true
Foo.shout("Hi!")
```

### Generic Classes

```python
template
    T: any
class Box
    value: T

    fun get_value() => T
        return self.value

let b = Box<int>(42)
print(b.get_value())
```

### Collections

```python
# List
let numbers = [1, 2, 3, 4, 5]
numbers.add(6)
print(numbers.size())  # 6

# Set
let unique = {1, 2, 3, 3, 4}  # {1, 2, 3, 4}

# Map
let scores = {"alice": 95, "bob": 87}
scores.set("charlie", 92)

# Tuple
let point = (10, 20)
print(point.get(0))  # 10
```

### Control Flow

```python
if x > 0 then
    print("positive")
elif x < 0 then
    print("negative")
else
    print("zero")

# Ternary condition
let max = if a > b then a else b

# Loops
let i = 0
while i < 5 do
    print(i)
    i = i + 1

for value in [1, 2, 3] do
    print(value)
```

### Keywords

| Keyword | Purpose |
|---------|---------|
| `fun` | Function definition |
| `class` | Class definition |
| `trait` | Trait definition |
| `template` | Generic parameters |
| `let` | Variable declaration |
| `const` | Immutable variable |
| `share` | Static/class method |
| `self` | Reference to current instance |
| `our` | Reference to current class |
| `native` | Says a std library function is implemented in C++ |
| `required` | Says class must implement a trait method  |


## 🔧 Development

### Project Structure

```
wasp/
├── libs/
│   ├── doctor/          # Error reporting and diagnostics
│   ├── lexer/           # Tokenization
│   ├── parser/          # Parsing
│   ├── ast/             # Abstract Syntax Tree
│   ├── semantics/       # Type checking and resolution
│   ├── compiler/        # Bytecode generation
│   ├── object_system/   # Runtime object model
│   ├── bytecode_system/ # Bytecode format and operations
│   ├── vm/              # Virtual machine
│   └── ...
├── src/                 # Main executable
├── tests/               # Unit tests
├── code/                # Example Wasp programs
└── CMakeLists.txt       # Build configuration
```

## 📝 Examples

More examples can be found in the `code/` directory.
They will be updated over time.
