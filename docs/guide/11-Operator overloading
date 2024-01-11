# Operator overloading
Operations such as addition, equals, and bit shift can be overloaded with an operator function. Any occurances of the operation where the types match the overload will call the function instead of performing the operation.

The most common use case of operator overloading is vector math.

```c++
struct Vector {
    x: f32;
    y: f32;
}
operator +(a: Vector, b: Vector) -> Vector {
    return Vector{
        a.x + b.x,
        a.y + b.y
    }
}
operator *(a: Vector, factor: f32) -> Vector {
    return Vector{
        a.x * factor,
        a.y * factor
    }
}

pos := Vector{1,1}
vel := Vector{40,10}
result := pos + vel * 0.016

```