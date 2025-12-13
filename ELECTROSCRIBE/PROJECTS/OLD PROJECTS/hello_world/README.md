# Hello World

The simplest possible HEIDIC program - a basic console "Hello, World!" example.

## What This Demonstrates

- Basic `main()` function
- Built-in `print()` function for console output
- Minimal program structure (no extern functions, no graphics, no ECS)

## Usage

Compile and run:
```bash
heidic_v2 compile hello_world.hd
# Then build and run the generated C++ code
```

## Code

```heidic
fn main(): void {
    print("Hello, World!\n");
}
```

That's it! Just 3 lines of code. The `print()` function is built into HEIDIC and generates to `std::cout` in C++.

