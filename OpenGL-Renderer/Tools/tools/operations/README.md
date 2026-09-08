# Operations Module

The operations module provides the main interface for executing operations `operations::Library` and defines a useful set of low level glTF operations.

⚠️ The operations contained in this module should be of a generic nature, don't place operations that are only useful in the Gestaltor editor in the operations module. If they correspond to the operation interface they can be loaded into the Library as a plugin.

## [Library](include/operations/Library.h)

The Library class serves as a container for operations and provides a way to make calls to the operations that it holds.

