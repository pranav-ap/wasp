# Variables

Variables are declared using `let`, while constants use `const`. Types can be explicitly annotated. Otherwise they are inferred by default.

```python
# Mutable variable
let x: int = 34    

# Immutable constant
const x: int = 34
```

# Type System 

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

## Lists 

```python
let x: [int] = [1, 2, 3]
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

## Maps

```python
let x: { str => str } = { "a" => "b", "c" => "d" }

# Inferred types
let x: { => } = { "a" => "b", "c" => "d" } 

# Key access
x."a" 
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
type Sizes = 1...3:1 
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
# CLASS VARIANT
# accepts any of the listed classes 
type X = int | str | Castle 

# TRAIT VARIANT
# accepts a class that implements either or both
type X = Fortifiable | Livable

# REQUIRED TRAITS 
# accepts any class that implements both 
type X = Fortifiable & Livable

# TRAIT NEGATION 
# Class can be Uploadable but not Executable
type SafeUpload = Uploadable & not Executable

# SUBTRACTION
# Remove a Trait from a Class
type CastleClassWithoutLivableTrait = Castle - Livable
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

## Operators 

```python
a <=> b 
# returns -1, 0 or +1

# Append & Remove 
a = [1, 2, 3] ++ [4, 5]  
# [1, 2, 3, 4, 5]
b = a -- [1, 2]   
# [3, 4, 5]

padding = [0] * 5  
# [0, 0, 0, 0, 0]

# Zip Operator
names = ["Alice", "Bob"]
ages  = [25, 30]
combined = names <> ages 
# [("Alice", 25), ("Bob", 30)]

# Merge Maps
base_config = { "debug": true, "port": 8080 }
overrides = { "port": 9000 }
final_config = base_config << overrides 
# { "debug": true, "port": 9000 }

# Add Key Value Pairs
a = { "id": 1, "name": "John" } ++ { "email": "a@b.com" } 
# { "id": 1, "name": "John", "email": "a@b.com" }

# Remove Keys
user = { "id": 1, "name": "John", "email": "a@b.com" }
public_profile = user -- ["id", "email"]
# { "name": "John" }
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

# squares of even numbers only
evens = [x * x for x in 0..10 if x % 2 == 0]
# Result: [0, 4, 16, 36, 64, 100]

# Create a map of numbers to their squares
squares = { x: x*x for x in 1..5 }
# Result: { 1: 1, 2: 4, 3: 9, 4: 16, 5: 25 }
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

## Data 

```python
class Worker
    private shared record 
        _total: int
        
    shared record 
        total: int
    
    private record
        _performance_rating: float
        _secret_code: int

    MAX_HOURS: const int

    name: str 
    salary: float 

    time record 
        hours_per_week_used: int 
        hours_per_week_total: int 

    projects [record]
        id: int 
        title: str 
```

## Functions 

###  Lifecycle 

```python
impl Worker
    fun default()
        my.time.hours_per_week_used = 0
        my.time.hours_per_week_total = 40
        my._performance_rating = 1.0

    fun initialize(name: str, salary: float, secret: int)
        my.name = name.capitalize() 
        my.salary = salary 
        my.secret_code = secret 
        our.total = our.total + 1 
    
    fun initialize(source: Worker)
        my.name = "${source.name} (Copy)"
        my.salary = source.salary
        # We DON'T copy the secret_code for security
        my.secret_code = generate_new_code()
        
    fun delete()
        our.total = our.total - 1
        print("Worker deleted. Total remaining: {our.total}")
        
    fun enter()
        pass

    fun exit()
        pass 
```

### Functions 

Class functions are called like `Worker.get_total_workers()` and they cannot access `my`. Only instance functions can access `my`. 

```python
impl Worker 
    fun log_hours(hours: int)
        my.time.hours_per_week_used += hours 
        
    fun our::get_total_workers() => int
        return our.total
```

### Accessors 

```python
impl Worker
    get annual_salary => float
        return my.salary * 12 

    set annual_salary (s: float)
        my.salary = s / 12 

    get secret_code => int
        return my._secret_code

    private set secret_code (code: int)
        if code > 1000 then
            my._secret_code = code
```

### Compute

```python
impl Worker
    compute annual_salary => float
        return my.salary * 12 
```

### Meta Functions 

```python
impl Worker
    # Returns a string representation (debugging/logging)
    fun repr() => str
        return "Worker - ${my.name}"
    
    # Returns a key-value map
    fun to_map() => { str => }
        return { "name" => my.name, "salary" => my.salary }
        
    # Returns a raw JSON string
    fun to_json() => str
        return JSON.encode(my.to_map())
    
    # Creates a Worker from a JSON string
    fun our::from_json(data: str) => Worker
        let m = JSON.decode(data)
        return new Worker(m."name", m."salary")
```

## Usage 

```python
let sam = new Worker('sam', 4000.0, 1234)

sam.log_hours(8)
sam.annual_salary = 60000.0 

# remove reference from symbol table
# `sam` becomes inaccessible after this unless re-defined
delete sam

with Worker() as w do
    pass

with Worker() as w1, Worker() as w2, Project() as p do
    w1.log_hours(5)
    w2.log_hours(3)
    p.assign_worker(w1)
    # exit() is called for p, then w2, then w1 here
```

# Traits

*Purpose*
Traits define functions and declare accessors. 

*Initialization*
The trait `default()` functions do not get called automatically. Make sure the class `default()` and `initialize()` manages the initialization logic correctly. 

*Access Modifiers*
Public members of a trait must remain public in the class. The private members can be made public. 

*When are members in conflict?*

- Two variables are in conflict if they share the same name 
- Two functions are in conflict if they share the same name and parameter types 

- `my.Fortifiable::health` is available in the class `default()` and `initialize()` 
- `my.Fortifiable::run()` is available in the class definition
- `w.Fortifiable::run()` is available outside the class definition

```python
trait Worker
    # privates may be made public in class 
    # publics must remain so in class 

    # declare required accessors
    id: [g ps] int 
    salary: [pg s] float 

impl Worker 
    fun default() 
        # calls accessors 
        my.id = 0
        my.salary = 0

    compute health => float  
        return my.salary * 100 
        
    # class can optionally override this
    fun action ()
        print('Chop! Chop!')
    
    # class must override this 
    fun shout_at_boss () => str
        pass  
    
    # class must override this 
    private fun say () => str 
        pass 


trait Player 
    id: [g ps] int 
    goals: [pg s] int 
    health: [pg s] int

impl Player 
    fun default() 
        my.id = 0
        my.goals = 0
        # `set health` gets called here     
        my.health = 150 
        
    fun action ()
        print('Kick!')
    
    fun transfer()
        print('Transfer...')
    

class Person is Worker, Player 
    private record
        _work_id: int 
        _salary: float
        _game_id: int 
        _goals: int 
        _health: int

impl Person
    fun default()
        # Setup local class state first
        my._health = 0 
        
        # Can trigger trait defaults 
        # Use your own logic 
        my.Worker::default()
        my.Player::default()

    fun initialize(start_health: int)
        my.health = start_health

impl Person
    # Person gets a Worker id
    get Worker::id => int 
        return my._work_id
    
    set Worker::id (id: int)
        my._work_id = id
    
    # Person gets a Player id
    get Player::id => int 
        return my._game_id
    
    set Player::id (id: int)
        my._game_id = id
    
    # Person only needs one health variable
    # Notice there is no :: here 
    # the most permissive visibility must be applied 
    merge get health => int
        return my._health
        
    merge set health (h: int)
        my._health = h
    
    # salary and goals are not in conflict 
    # but you must still use :: 
    get Worker::salary => float 
        return my._salary

    get Player::goals => int 
        return my._goals

impl Person
    # overrides shout_at_boss() from Worker
    # original cannot be accessed anymore 
    fun Worker::shout_at_boss () => str
        # so we need super
        me.super.Worker::shout_at_boss () 
        return 'I Quit!'
    
    # overrides action() from Worker
    fun Worker::action () => str
        return 'Screw!'
        
    fun action () => str
        print('Wake Up')
        my.Worker::action()
        my.Player::action() 
    

let p = new Person()

# both okay 
p.Worker::shout_at_boss()
p.shout_at_boss()

# calls class Worker::action
p.Worker::action()
# calls Player::action 
p.Player::action()
# calls class action
p.action()
```

In the above example, there are three bags of `action()`. Overloading occurs within the bags, not across. 

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
            utils.wasp
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

# my refers to the current folder 

## company/main.wasp 
import my.hr 
from my.hr import Payroll

## company/ops/Payroll.wasp 
from my.Machine import Machine

# our refers to the set of sibling folders of current folder

## company/ops/Machine.wasp 
import our.hr
from our.hr import Payroll
```

# Operator Overloading

Operators can be overloaded to define custom behavior between types.

```python
operator left: Castle < right: Castle => bool
    return left.cost < right.cost
```

# Generics

```python
class Box[T]
    item: T

let b = new Box[int]()

fun wrap[T](item: T) => Box[T]
    return new Box(item)

wrap[int](12)

class Armory[T: Fortifiable] 
    items: [T]
```

# CMD Tool 

```shell
> wasp run main.wasp
> wasp run 
```

