# Runtime

## Coding Convention

### Custom Allocators
* We use templated custom allocators in the runtime module. All functions or classes that need to dynamically allocate data need to accept a template parameter of an allocator type. The allocators that should at least be supported are `HeapAllocator` and `MemoryArena`. 

* It is not allowed to use the convenience wrappers `HeapVector` or `HeapSlice` in the highly optimized *runtime* component of the middleware. 

* It is also not allowed to specify the allocator of a component with a `using MyAllocator = core::HeapAllocator` or similar constructs.

* The allocator should always be the last argument in a function signature that needs the allocator to dynamically allocate data
