The compiler generates very inefficient code. Partly because of the BTB calling convention and partly because we push and pop when generating expressions.

This operator overload is expensive to call. The floats in the mat4 structs are pushed to the stack. Then we allocate space for arguments. Then we pop floats into the argument space.
Then we call the function. When we return we put the value on the stack in the called function stack space. Then the caller pushed the values in the return space to the stack.
Then pop it into the variable. We severely need to improve this.

```
struct mat4 { v: f32[16]; }

operator *(a: mat4, b: mat4) -> mat4 {
    t: mat4
    for col : 0..4 {
        for row : 0..4 {
            t.v[col * 4 + row] = a.v[0*4 + row] * b.v[col * 4 + 0] +
                a.v[1*4 + row] * b.v[col * 4 + 1] +
                a.v[2*4 + row] * b.v[col * 4 + 2] +
                a.v[3*4 + row] * b.v[col * 4 + 3]
        }
    }
    return t
}

var := mat_a * mat_b
```

# Improvements
- Reduce value move/push/pop overhead for large structs like mat4.
- Support return pointers in the BTB calling convention.
- Support const reference arguments.

# How to achieve
We will work on this in pieces. Large changes at once is difficult because all tests will break and it will take time until you have something working.

# Core changes
## Develop a new BTB calling convention
Here are some ideas.

- If a function returns a value larger than a certain size (e.g. >16 bytes), insert a hidden pointer argument at the beginning.
- Caller allocates return memory and passes it in.
- Callee writes directly to this pointer.
- Function returns void at machine level, but language syntax stays the same.
- const ref arguments. How to change TypeId for this?
- New bytecode to handle this?
- Do we need to change bytecode?

```c
// Source
res: mat4 = a * b

// Compiler lowers to
_tmp: mat4
call mat4_mul(&_tmp, &a, &b)
copy _tmp to res
```

EXTRA THING: we can't return structs with many elements at the moment because when we push the fields we will end up overwriting the return space. Using a return PTR will give us some more options.