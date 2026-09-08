# glTF module

The glTF module gives a user the tools to create a glTF object (from existing assets or from scratch), modify it and save it to a file.
* The tool should be used to repair broken glTF assets so obviously, we allow invalid glTFs to be loaded.
* We don't want to introduce changes to a glTF asset that the user doesn't request:
  * A property can be empty, can incidentally have its default value and a property can not exist – we parse, handle and serialize all these states individually.
  * When dealing with invalid assets, we try to keep as much of the information to assist the user in recovering the correct state.

## Usage

The `Gltf` class is the entry point for all interactions with the module
```C++
Gltf myGltf;                                            // create a new glTF asset
Scene& scene = myGltf.create<Scene>();                  // with one scene
Node& node = myGltf.create<Node>();                     // and one node
scene.addNode(node);                                    // that is contained by the scene.
myGltf.merge(otherGltf);                                // merge another asset into mine
```
The `GltfParsing` class is used for JSON parsing/serializing.

## Coding Convenitons

### General

* setters always return `GltfResult`

* Gltf components have no default or copy constructors, but always need an owning Gltf and a unique ID.
  ```C++
  Thing(const Thing&) = delete;
  Thing(const Gltf& _rootGltf, uint32_t _id);
  Thing(const Gltf& _rootGltf, uint32_t _id, const Thing& _source);
  ```

* Primitive members have simple getters and setters
  ```C++
  int32_t m_int;
  int32_t getInt() const { return m_int; }
  GltfResult setInt(int32_t _int);
  ```
  
* Complexer members don't need setters – the caller should change their contents through the non-const getter
  ```C++
  Thing m_thing;
  Thing& getThing() { return m_thing; }
  const Thing& getThing() const { return m_thing; }
  ```
* The header of a Gltf component A may only include the header of another Gltf component B if component A **owns** an instance of component B. In all other cases, only pointers and references should be passed around so no include is required.

* Every Gltf component should have a `public const Gltf&` member so the owning Gltf can always be deduced. In the case of a subcomponent like `AnimationChannel` that is owned by another component, it might be necessary to keep a reference to the owning component instead (owning Gltf can still be deduced implicitly).

* Apart from the readonly root Gltf reference, there are no public members in Gltf components. All access is handled through accessor methods.
    
* Every Gltf component has a `bool isValid() const` function and a `core::Vector<core::string> getInvalidProperties() const` function to clarify if all members are valid withing the glTF specification.

### Indices/Pointers

* An index to another glTF object is represented by an actual pointer in our classes

* Whenever a pointer to another Gltf component is set in a component, it must be checked that both components are owned by the same Gltf object.

* Pointer members can be set, but only through a reference of an actual object. If we want a nullpointer, we explicitly use the `unset` method
  ```C++
  Thing* m_thing;
  Thing* getThing() { return m_thing; }
  const Thing* getThing() const { return m_thing; }
  GltfResult setThing(Thing& _thing);
  void unsetThing() { m_thing = nullptr; }
  ```
  
### Optionals

* If a property of a glTF object is not an index and not required, we will use an `std::optional` to store it.

* Optionals can be created, accessed and destroyed.
  ```C++
  std::optional<Thing> m_thing;
  std::optional<Thing>& getThing() { return m_thing; }
  const std::optional<Thing>& getThing() const { return m_thing; }
  GltfResult createThing();
  void destroyThing() { m_thing.reset(); }
  ```
  
* If an optional holds a primitive type, creating and destroying just become setting and unsetting and no non-const getter is required
  ```C++
  std::optional<uint32_t> m_int;
  const std::optional<uint32_t>& getInt() const { return m_int; }
  GltfResult setInt(uint32_t _int);
  void unsetInt() { m_int.reset(); }
  ```
  
* If an optional value can be resolved to something meaningful (usually a default value) even if there is no value, we add a resolve method, the rest stays the same. Note that this can not return a reference.
  ```C++
  uint32_t resolveInt() { return m_int.value_or(17u); }
  ```
  
### Serialize Gltf to Json

* When a required property of an object is in an invalid state (which should be a rare occasion because we control the Gltf structure), we **don't serialize that property**, but serialize the overall object anyway (without that property) so we keep as much information as we can about the object.

## Module Structure

* the root folder contains the `Gltf` class and all its components like `Node` and `Scene`
  * subfolders like `animation` and `material` are used for subcomponents to avoid cluttering the root folder – the actuall `Material` and `Animation` classes are still located in the root folder though

* `json` contains all functionality for creating a `Gltf` object from its json representation and vice versa

* `merge` contains all functionality for merging two `Gltf` objects
