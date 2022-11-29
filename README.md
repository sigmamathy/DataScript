# DataScript

DataScript is a scripting lanugage made for portable data control,
allows user to add data inside a resources file instead of write it in a `.cpp` file directly.
It makes it easier to insert, format or delete data.

Type Formating was used in DataScript in order to store different types of data.
Available types are `int`, `uint`, `long`, `ulong`, `float`, `double` and `string`.
Types can specified using parentheses like `(int:2, string)`, where `int:2` is just a shorthand of `int, int`.

Values are stored as an individual data, the type of the data is the corresponding type defined before.

## Example

In `data.ds` file:
```
[label] (int:2, string)
100, 200, "Hello World"
-1, -2
"ok"
```

In `main.cpp` file:
```cpp
#include "DataScriptReader.hpp"

int main()
{
    DsReader read("data.ds");
    read.Goto("label");
    while (read & DsEof == 0)
    {
        int x, y;
        std::string str;
        read >> x >> y >> str;
        std::cout << x << ", " << y << ", "
            << str << '\n';
    }
    return 0;
}
```

Output:
```
100, 200, Hello World
-1, -2, ok
```
