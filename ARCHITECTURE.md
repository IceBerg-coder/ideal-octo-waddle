# Language Architecture Specification: "PyNext" (Project Name TBD)

## 0. Core Philosophy
To replace Python, we must match its **developer velocity** while fixing its **runtime performance** and **concurrency**.
*   **Host Language:** C++20 / C++23.
*   **Backend:** LLVM (Version 18+).
*   **Execution Model:** Ahead-of-Time (AOT) Compiled to native binaries.

## 1. Type System: "Sound Inference"
Python is dynamic; C++ is static. To replace Python without becoming C++, we use **Global Type Inference**.
*   **Design:** Bidirectional Hindley-Milner type inference.
*   **User Experience:** Types are optional in 90% of code. The compiler infers `x = 10` is an `Int64`.
*   **Gradual Typing:** Function boundaries can be optionally typed. `def add(a: int, b) -> int:`
*   **Safety:** Null-safety by default (Option types instead of None).

## 2. Memory Model: "Hybrid Lifetime Management" (Unique Approach)
We implement **Compile-Time Lifetime Analysis with Runtime Fallback**.
*   **Static Path:** The compiler performs lifetime analysis. If it can prove an object's life ends at scope exit, it inserts a `free()`.
*   **Dynamic Path:** If lifetimes are ambiguous, the object is promoted to a **Region-based Heap** or **ARC**.
*   **Benefit:** Local variables are zero-overhead. Complex shared state doesn't require a global "Stop the World" GC.

## 3. Concurrency Model
*   **Primitives:** Lightweight "Fibers" (M:N threading).
*   **No GIL:** Since memory is managed via local regions or atomic ref-counts, threads run truly parallel.
*   **Communication:** Channels (CSP style) preferred over shared memory.

## 4. Syntax: Explicit Block Structure
Moving away from Significant Whitespace to explicit delimiters.
*   **Style:** Ruby/Julia aesthetic (using `end` keywords).
*   **Example:**
    ```ruby
    def fib(n: Int) -> Int
        if n < 2
            return n
        end
        return fib(n-1) + fib(n-2)
    end
    ```

## 5. Compiler Pipeline (LLVM)
1.  **Lexer/Parser:** Handwritten C++ recursive descent parser.
2.  **AST:** Strongly typed Abstract Syntax Tree.
3.  **Sema:** Semantic Analysis & Type Inference pass.
4.  **IR Generation:** Lower AST to **LLVM IR**.
5.  **Optimization:** LLVM `opt` passes.
6.  **CodeGen:** LLVM `llc` to object files.

## 6. Directory Structure
```
/root
  /src
    /lexer       # Tokenization
    /parser      # AST generation
    /sema        # Semantic Analysis
    /codegen     # LLVM IR Binding
    /runtime     # The Hybrid Memory Manager
  /tests
  CMakeLists.txt
```
