# Enums
An enum is a type that contains members of names and values. The value of an enum type is the same as an unsigned or signed integer with a few differences with type conversions and allowed operations. The value of the members automatically increment starting from 0.
```c++
enum Flavour {
    STRAWBERRY,    // 0, starts at zero
    CHOCOLATE,     // 1, auto incremented
    CHICKEN = 82,  // user defined
    TASTELESS,     // 83, auto incremented
}

a: Flavour = Flavour.CHICKEN;
b := CHOCOLATE;                 // if you want to be brief

```
An enum is a i32 by default but can be changed like this (only integers allowed):
```c++
enum Hi : u8 { }
```

## Annotations
When defining enums, you may want them to have some special behaviour such as ensuring each enum member is unique or that the value of all members are manually set or that you don't have to specify all enum values in a switch case. Annotations allow you to do this.
```c++
// All members must be explicitly specified, no automatic increment
enum @specified EnumA {
    zero = 0,
    one = 1,
    two, // error
}
// Each member is incremented by a power of two instead of by one
enum @bitfield EnumB {
    bit0, // 1
    bit1, // 2
    bit2, // 4
    bit3, // 8
}
// Each member must have a unique value, two members cannot be 1.
enum @unique EnumC {
    unique0,
    unique1,
    unique0_again = 0, // error
}
// Members must be accessed like this 'EnumD.member'.
enum @enclosed EnumD {
    cat,
    dog,
}
EnumD.cat
cat // error, cat not defined, it's not available without refering to EnumD first

// Used with switch, it allows you to skip some members. See more details in the switch section.
enum @lenient_switch EnumE {
    yes,
    no,
    maybe,
}
```

# Switch
Switch is a replacement for many if-statements. You can write the expression once and specify what should happen when it's equals to what. Currently, switch only works with integers and enums.

```c++
expr: i32 = 2
// With if
if expr == 0 { /* do stuff */ }
else if expr == 1 { /* do other stuff */ }
else if expr == 2 { /* do other and other stuff */ }
else { /* otherwise do that */ }
// With switch
switch expr {
    case 0:
        // do stuff
    case 1:
        // do other stuff
    case 2:
        // do other and other stuff
    case: // default case
        // otherwise do that
}
```

Each case has it's own scope which means that a variable or struct defined in one case won't interfere with variables in another. When the program has executed all statements in one case, it will not continue executing in the next statement. There is an implicit break at the end of each case. If you want the program to fallthrough to the next case, use the @fall annotation.

```c++
#import "Logger"
switch 1 {
    case 0:
        a := 3
        // return/break out of switch
    case 1:
        a++ // error, a is not defined
        log("Case 1")
        @fall // continue executing the next switch
    case 2:
        log("Case 2")
    
    case 3: // cases next to each will NOT automatically fall through
    case 3: @fall // you must specify a fall like this, the reason
                  // being that I value consistency and minmizing
                  // confusion, this becomes apparent with macros
                  // which may be empty without you realizing it
    case 4:
        log("Case 3 or 4")
}
```

**NOTE**: `break` inside cases is not implemented. Not officially at least.

When enums are just in a switch, all members of the enum must be handled. If you do not want this you can either specify a default case or the @lenient_switch annotation when defining the enum.
```c++
enum Firm {
    very_firm,
    little_firm,
    maybe_firm,
}
switch maybe_firm {
    case very_firm: // blah
    case little_firm: // blah
    // error, 'maybe_firm' isn't handled

    case: // default case is one way to fix the error
}
// This annotation is the other alternative
enum @lenient_switch Soft {
    very_soft,
    little_soft,
    maybe_soft,
}
switch maybe_soft {
    case very_soft: // blah
    case little_soft: // blah
    // no error even though 'maybe_soft' isn't handled
}
```


