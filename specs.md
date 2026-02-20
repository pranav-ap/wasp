# Variables and Constants

Variables are declared using `let`, while constants use `const`. Types can be explicitly annotated. Otherwise they are inferred by default.

```python
# Mutable variable
let x: int = 34    

# Immutable constant
const x: int = 34  

# list length is constant, but elements are mutable
let x: const [int] = [1, 2, 3] 

# list elements are constant, but list length is mutable
let x: [const int] = [1, 2, 3]  

# Both list length and elements are constant
let x: const [const int] = [1, 2, 3] 
const x: [int] = [1, 2, 3]  
```

# Type System 

## Lists 

```python
let x: [int] = [1, 2, 3]
```

## Ranges

Ranges can be defined for iterative or slice operations.

```python
# Range Syntaxes
1..10      # Standard range
1...10     # Inclusive range
1..10:2    # Range with step (2)
1..        # Open-ended range
1..:2      # Open-ended with step
```

## Tuples

Fixed-size collections of potentially mixed types.

```python
let x: (str, str) = ("1", "hello")
```

## Sets

Sets are defined using curly braces `{}`.

```python
a = {1, 2, 3, 4}
b = {3, 4, 5, 6}

a & b  # Intersection: {3, 4}
a | b  # Union: {1, 2, 3, 4, 5, 6}
a - b  # Difference: {1, 2}
```

## Maps

```python
let x: { str => str } = { "a" => "b", "c" => "d" }

# Inferred types
let x: { => } = { "a" => "b", "c" => "d" } 

# Key access
x."a" 
```

## Strings

```python
x = 'Some text'
```

Multi-line strings are trimmed by default. You can access properties on strings directly.

```python
x = '''
    Some text
    '''
```

## Type Alias 

```python
type length = int
type length = int | str
type names = (str, str)
```

## Literal Type 

```python
type Sizes = 1 | 2 | 3 
type WindowStates = "open" | "closed" | "minimized"
```

## String Template Type 

```python
type Lang = "en" | "es"
type Category = "user" | "admin"

# Calculates: "en_user" | "en_admin" | "es_user" | "es_admin"
type LocaleID = `${Lang}_${Category}`

# Valid
let x: LocaleID = "en_user"   
# Compile-time Error: "fr" is not in Lang
let y: LocaleID = "fr_admin"  
```

## Type Logic 

```python
trait Fortifiable
    name: str
    health: int 
    
trait Livable
    name: str
    housing_capacity: int

class Castle is Fortifiable, Livable
    pass 

class Farm is Livable
    pass


# CLASS VARIANT
# accepts any of the listed classes 
type X = int | str | Castle 

# TRAIT VARIANT
# accepts a class that implements either or both
type X = Fortifiable | Livable

# REQUIRED TRAITS 
# accepts any class that implements both 
type X = Fortifiable & Livable

# COMMON CLASS MEMBERS 
# common members must match in type as well as name 
type X = CAT & DOG 

# TRAIT NEGATION 
# Class can be Uploadable but not Executable
type SafeUpload = Uploadable & not Executable

# SUBTRACTION
# Remove a Trait from a Class
type CastleClassWithoutLivableTrait = Castle - Livable
```

## Omit & Pick

```python
class Worker
    id: int
    name: str
    salary: float
    secret_code: int

# 1. Calculates a type containing ONLY 'id' and 'name'. 
# 2. Private members can be picked as values are not being accessed here
type WorkerIdentity = Worker.pick('id', 'name')

# 1. Calculates a type with everything EXCEPT the sensitive fields
# 2. Accessing these fields directly, or passing this type to a function that expects a full Worker, throws a compile-time error.
type PublicWorker = Worker.omit('salary', 'secret_code')


fun display_badge(w: WorkerIdentity)
    pass
```

## Distinct

```python
distinct type USD = float
distinct type EUR = float

let price: USD = 10.50
let tax: EUR = 2.00

# Compile-time Error: Cannot add USD to EUR
let total = price + tax 

# The developer is forced to explicitly cast or convert
let total = price + convert_to_usd(tax)
```
## Enums

Enums can be nested, establishing clear hierarchical namespaces.

```python
enum Animal
    Dog  
    enum Bird 
        Crow
        Pigeon 

Animal.Dog 
Animal.Bird.Crow 
```

## Optional Type

```python
# Declares an optional variable.
let x: int? = 34

# Extracts the value from an optional.
let y = x ?

# Null-coalescing: returns 10 if x is none.
x ? 10

# Assigns 10 to x if x is none.
x ?= 10

# Safe-chaining: returns none if x is none.
x?.foo.age
```

You can pattern match optional types,

```python
let name: str? = "Gemini"

when name
    is some n where n.length > 10 then
        print("That is a long name: ${n}")
    is some n then
        print("Short name: ${n}")
    is none then
        pass
```

You can write more complicated expressions. For example, you can match a list of optional types,

```python
let coordinates: [int?] = [10, none]

when coordinates
    is [some x, some y] then 
        print("Point at ${x}, ${y}")
    is [some x, none] then 
        print("X-axis only at ${x}")
    is [none, some y] then 
        print("Y-axis only at ${y}")
    else
        pass 
```

Use `when` in expressions. 

```python
let status = when user_age
    is some a where a >= 18 then "Adult"
    is some a then "Minor"
    else "Unknown"
```

# Control Flow

### Conditionals 

```python
# Statement
if x == 25 then
    pass
elif x == 30 then
    pass
else
    pass

# Expression
x = if a > 3 then call() else 0 

# If-Let Binding
x = if let x int = expr then x + 5 else 0
```

### Looping

```python
while expr do a = a + 4
until expr do a = a + 4
unless expr do a = a + 4

while let x = expr do a = x + 4
for x: int in [1, 2, 3] do x
```

### Pattern Matching

```python
let result = when x + 1 as value
    is [x, y: int] where x < 0 then 
        x + y 
    is i: int then i + 1
    is s: str then s
    is e: ZeroDivisionException then 0 
    else 0 

let x: bool = when expr is [x, y] where x > 43 then 10 else 0
```

# Functions

Functions are declared with `fun`. Function types are expressed with arrow syntax `(args) => ret`.

Functions support implicit argument passing (`...`), keyword arguments, and a chain operator (`~`). 

Constants can be passed as arguments using the `const` keyword, ensuring that their values cannot be modified within the function.

```python
fun sub (a: int, b: int) => int
    return a - b

    
fun remove(a: int, b: const int) => int
    if a > b then
        return a

    # same as sub(a, b) 
    return sub(...)   


# call
remove(12, 23)
# named call
remove(a=12, b=23)

# Chaining 
# . drops the result in place
# ... spreads the result
foo() ~ bar(., 35) ~ boom(...)
```

# Class

## Schema

Class members are public by default. You can use `private` and `shared` to control visibility and mutability. `const` can be used for class-level constants.

```python
class Worker
    shared record 
        total: int
    
    private record
        _performance_rating: float
        _secret_code: int

    MAX_HOURS: const int = 60 

    name: str 
    salary: float 

    time record 
        hours_per_week_used: int 
        hours_per_week_total: int 

    projects [record]
        id: int 
        title: str 
```

## Implementation

Behavior is attached to classes using `impl` blocks. `us` references static/shared context, and `me` references instance context. 

###  Lifecycle 

```python
impl Worker lifecycle
    fun default()
        me.time.hours_per_week_used = 0
        me.time.hours_per_week_total = 40
        me._performance_rating = 1.0

    fun initialize(name: str, salary: float, secret: int)
        me.name = name.capitalize() 
        me.salary = salary 
        me.secret_code = secret 
        us.total = us.total + 1 
    
    fun initialize(source: Worker)
        me.name = "${source.name} (Copy)"
        me.salary = source.salary
        # We DON'T copy the secret_code for security
        me.secret_code = generate_new_code()
        
    fun delete()
        us.total = us.total - 1
        print("Worker deleted. Total remaining: {us.total}")
        
    fun enter()
        pass

    fun exit()
        pass 
```

### Accessors 

You get to decide whether to use explicit getter/setter functions or property accessors.

The `get` and `set` keywords allow you to define functions that can compute values on the fly or perform validation when setting values.

You can mark setters and getters as private if you wish. 

```python
impl Worker accessors
    get annual_salary => float
        return me.salary * 12 

    set annual_salary (s: float)
        me.salary = s / 12 

    get secret_code => int
        return me._secret_code

    private set secret_code (code: int)
        if code > 1000 then
            me._secret_code = code
```

### Instance Functions 

```python
impl Worker me 
    fun log_hours(hours: int)
        me.time.hours_per_week_used += hours 
```

### Class Functions  

Class functions are called like `Worker.get_total_workers()` and they cannot access `me`. 

```python
impl Worker us
    fun get_total_workers() => int
        return us.total
```

### Meta Functions 

Can access `me` and `us`.

```python
impl Worker meta
    # Returns a string representation (debugging/logging)
    fun repr() => str
        return "$Worker - {me.name}"
    
    # Returns a key-value map
    fun to_map() => { str => . }
        return { "name" => me.name, "salary" => me.salary }
        
    # Returns a raw JSON string
    fun to_json() => str
        return JSON.encode(me.to_map())
    
    # Creates a Worker from a JSON string
    fun from_json(data: str) => me
        let m = JSON.decode(data)
        return new Worker(m."name", m."salary")
```

## Usage 

```python
let sam = new Worker('sam', 4000.0, 1234)

sam.log_hours(8)
sam.annual_salary = 60000.0 

delete sam

with Worker() as w
    pass
```

# Traits

Traits provide shared variables and functions with default logic. They must declare all class variables they interact with.

Traits are in *conflict if they have overlapping member names*. The Class must mediate over them by overriding the conflicting members.  

The scope resolution operator `::` allows you to specify which trait's function to call when there are conflicts.

When your class overrides a variable in `Fortifiable`, then `Fortifiable::default()` will not automatically called. So, take care of the logic in your class's `default()`. Call `Fortifiable::default()` if needed.

```python
trait Fortifiable
    private record
        _id: int 

    health: int

impl Fortifiable
    fun default()        
        me.health = 100
    
    fun run()
        # pass here means class must implement it.
        # Class allowed to pass but type checking still done.
        pass 


trait Livable
    health: int

impl Livable 
    fun default()
        me.health = 50

    fun run()
        pass


class Worker is Fortifiable, Livable
    # Conflicting variables are overriden
    health: int 

impl Worker 
    fun default()
        me.health = 75 

    fun run()
        # 1. Function conflict resolved by ::
        me::Fortifiable.run()
        me::Livable.run()
        # 2. Can also provide custom business logic
```

# Annotations

Annotations inject metadata into functions and classes.

```python
@tag('smoke', 'unit')
fun run(a, b, c)
    pass 
```

# Exceptions
 
```python
try
    perform_action()
    throw NetworkError() 
catch e: NetworkError where e.code == 121 then
    log(e)
catch e: Exception then
    log(e)
else
    print('no errors thrown')
finally
    print("Clean Up...") 
```

`try` can be used as an expression. 

```python
let x: int = try perform_action() ? 42
```

# Module

## Entry Point 

The entry point is a `main()`. 
```python
fun main(args: [str])
    print("Wasp is flying!")
```

## Visibility 

The members of a module are private by default. Export them if necessary.

```python
# Calculator.wasp

# _ is a convention
fun _verify_expession()
    pass

# acutally exposed 
export fun verify_expession()
    pass
    
export class Calculator
    pass
```

## Import 

`calc` and `company` are `packages` as they have a `wasp.yaml` file. 

- Any capitalized file within a package are `modules` exposed by the package. 
- The `top` of your code is the `workspace` but for third party code like `calc` it's `libs\calc`
- A folder without a `wasp.yaml` is a namespace

```python
'''
workspace/
    libs/
        calc/
            Calculator.wasp
            main.wasp
            wasp.yaml
    
    utils/
        Utils.wasp
    
    company/
        Worker.wasp
        main.wasp
        wasp.yaml
        hr/
            Payroll.wasp
        ops/
            Payroll.wasp
            Machine.wasp
        wasp.yaml
        
    main.wasp
    wasp.yaml
'''

# third party packages in libs 
import calc
from calc import Calculator

# access local packages with `top`
import top.company
from top.company import Worker, Machine
from top.company.hr import Payroll as hr_Payroll 
from top.company.ops import Payroll as ops_Payroll, Machine

# me refers to the package current code is in 

## company/main.wasp 
import me.hr 
from me.hr import Payroll

## company/ops/Payroll.wasp 
from me.ops.Machine import Machine

# us refers to the set of sibling packages of current package

## main.wasp 
import us.company
from us.company import Worker

```

# Operator Overloading

Operators can be overloaded to define custom behavior between types.

```python
operator left: Castle < right: Castle => bool
    return left.cost < right.cost
```

# Generics

```python
# Missing: Generic Class/Trait
class Box[T]
    item: T

# Missing: Generic Function
fun wrap[T](item: T) => Box[T]
    return new Box(item)
```
