The compiler cannot currently handle these properly.

```
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
```