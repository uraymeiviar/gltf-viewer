# Response

```c++
template<class T, class E>
class Response {
    // ...
};
```

## Concept

The response type aims to make error handling as convenient as possible without using exceptions. It's basically a binary sum type over a success and an error type.

One possible approach to implement functions which may fail is to return the error code explaining the error and move the actual returned value into an out parameter. This is not only ugly, it also requires the value's type to implement the default constructor and the assignment operator, which in turn may interfere with it's move semantics and making it more complex than necessary. It also introduces mutability of variables where it is not necessary.

The response type allows a function to return the error code or the value, depending on whether the function succeeded or failed in a single type in a concise way. This only requires the type to be copy and/or move constructible, which is usually trivial. Neither the success type nor the error type need to be default constructible or to implement the assignment operator.

## Usage

A code snippet demonstrating the usage of the response type.

```c++
// An enumeration of error codes.
enum class AppError
{
    IsZero,
    IsNegative,
    NaN,
};

// A function which may return an `int`, but in error cases
// returns an `AppError` enumerant.
Response<int, AppError> divide(int nom, int den)
{
    if (den == 0)
    {
        // Return an error.
        return AppError::IsZero;
    }
    // Return an success value.
    return nom / den;
}
```

The response can be queried for being a success or an error:

```c++
auto x = divide(6, 3);
assert(x);
int value = *x;
```

When an error is returned:

```c++
auto x = divide(6, 0);
assert(!x);
assert(x.get_error() == AppError::IsZero);
```

### Boolean response

This is though to be a drop-in replacement for `bool` where it is used to indicate success to make the error handling more uniform. *This does not apply to predicates.*

Functions returning `bool` usually don't return any data, just whether the computation was successful or not. In other words, the are either some error value or some success constant which does not hold any information.

Therefore we define the type `SuccessResponse`. Using that, we define `BoolResponse` to be `Response<SuccessResponse E>` where `E` is the error type describing the failure.

```c++
BoolResponse<AppError> check(int n)
{
	if (n == 0)
	{
		return AppError::IsZero;
	}
	else
	{
		return core::SuccessResponse{};
	}
}
```



### Combinators

The response type provides some combinator functions, further simplifying error propagation.

**`and_then`**: Takes a function, which in turn receives the response's success value if the response is a success, mapping it to another response. If the response is an error, the function is not called.

```c++
auto x = divide(6, 0)
    .and_then([](int x) { return divide(x, 2); });

assert(!x);
assert(x.get_error() == AppError::IsZero);
```

**`or_else`**: Takes another response, returning it if this response itself is an error. Otherwise, this response is returned.

```c++
auto x = divide(24, 0)
    .or_else();

assert(x);
assert(*x == 5);
```

**`or_default`**: Takes a value of the success type, returning it if this response itself is an error. Otherwise, the success value of this response is returned.

```c++
int x = divide(24, 0)
    .or_default(-1);

assert(x == -1);
```
