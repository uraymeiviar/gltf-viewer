### Set value
Set the value at the specified path. Be sure you know what you are doing, as the operationcannot check the validity of the value.

 `dangerous.setValue` 
-  `path` (string) ^(.*)$
-  `value` () 


### Remove value
Remove the value at the specified path. Be sure you know what you are doing, as the operationcannot check the validity of removing the value.

 `dangerous.removeValue` 
-  `path` (string) ^(.*)$


### For Each
Apply given operation with given parameter set to each property defined via pattern. Parameters are passed to each call of the operation.

 `dangerous.forEach` 
-  `operation` (string) 
-  `parameters` () 
-  `pathPattern` (string) 


### 


 `set_gltf_value` 


### 


 `set_gltf_pointer` 


### Move
Move pointer value

 `move_gltf_pointer` 


### Insert
Insert pointer value into an array

 `insert_gltf_pointer` 


### 


 `remove_gltf_pointer` 


### Remove
Remove pointer value from an array

 `remove_gltf_pointer_from_array` 


### Move
Move pointer value between arrays

 `move_gltf_pointer_from_array` 


### Remove
Remove property from the glTF

 `remove` 


### Add Node
Adds another node

 `add_node` 
-  `path` (string) ^((/gltf/nodes/\d+)|(/gltf/scenes/\d+))$
-  `pathSrc` (string) ^(/gltf/nodes/\d+)$


### Add New Node
Adds a newly created node

 `add_new_node` 
-  `path` (string) ^((/gltf/nodes/\d+)|(/gltf/scenes/\d+))$


### Add New Scene
Adds a newly created scene.

 `create_scene` 


### Add New Mesh
Adds a newly generated mesh.

 `gltf.createMesh` 
-  `innerRadius` (number) 
-  `outerRadius` (number) 
-  `path` (string) ^(/gltf/nodes/\d+)$
-  `scale` (number) 
-  `sectors` (number) 
-  `segments` (number) 
-  `shape` () 


### Add New Animation
Adds a newly generated animation.

 `gltf.createAnimation` 
-  `bounce` (boolean) 
-  `degrees` (number) 
-  `duration` (number) 
-  `path` (string) ^(/gltf/nodes/\d+)$
-  `rotationAxis` (array) 
-  `start` (number) 
-  `translation` (array) 


### Remove Node Reference
Removes the references to the specified node

 `remove_node_reference` 
-  `path` (string) ^(/gltf/nodes/\d+)$


### Clone
Clones a property and adds the clone to the target if set. If cloneModeShallow is set, leafs are referencing the same properties.

 `clone_property` 


### Trim Buffer
Trim Buffer goes through all referencing BufferViews that have the respective Buffer set as target and trims the Buffer to that data

 `trim_buffer` 
-  `path` (string) ^(/gltf/buffers/\d+)$


### Merge Buffers
Merge all buffers listed in the array. Merged buffers get deleted. The newly created buffer is appended to the buffer list.

 `merge_buffers` 
-  `bufferPaths` (array) 


### Clean All
Remove all or optionally filtered unused properties from the glTF

 `clean_gltf` 
-  `filter` (array) 


### Calculate Min Max
Calculate min and max values of an accessor

 `calculate_min_max` 
-  `path` (string) ^(/gltf/accessors/\d+)$


### remove empty nodes
removes all empty nodes from the gltf (also removes all nodes which become empty because their children where deleted).

 `clean_nodes` 


### cleanTexCoordAttributes
Removes all unused texture coordinate attributes.

 `clean_texture_coordinate_attributes` 


### deleteAttribute
Removes an attribute from a primitive if the removal is valid.

 `delete_attribute` 
-  `path` (string) ^(/gltf/meshes/\d+/primitives/\d+/attributes/[A-Za-z0-9_]*)$


### removeProperties
Removes all properties with the given name from the entire gltf.

 `remove_properties_by_name` 


### removeKnownExtensions
Removes all extensions with the given name from the entire gltf.

 `remove_extension` 


### removeUnknownExtensions
Removes all unknown extensions from the entire gltf.

 `remove_unknown_extension` 


### Convert Primitives to Meshes
Splits all primitives in the provided mesh into individual meshes. For each mesh a new node is generated which are added 

 `split_up_primitives` 
-  `path` (string) ^(/gltf/meshes/\d+)$


### Join Meshes
Join multiple Meshes into a new Mesh

 `join_meshes` 
-  `meshPaths` (array) 


### Transform Node
Apply translation, rotation, and scale to current node

 `transform_node` 
-  `path` (string) ^(/gltf/nodes/\d+)$
-  `rotation` (array) 
-  `scale` (array) 
-  `translation` (array) 


### Create Empty glTF
Create minimal and valid glTF

 `create_empty_gltf` 


### Load glTF or GLB File
Load a glTF or GLB file from a filesystem path

 `file.load` 
-  `filepath` (string) 


### Save as glTF or GLB File
Save the current glTF as a .gltf or .glb file

 `file.save` 
-  `filepath` (string) ^.*\.gltf|.*\.glb$
-  `overwrite` (boolean) 


### Save selection as glTF or GLB File
Save the current selection as a .gltf or .glb file

 `file.saveSelection` 
-  `filepath` (string) ^.*\.gltf|.*\.glb$
-  `overwrite` (boolean) 
-  `paths` (array) 


### Export Accessor
Export accessor to readable text file

 `file.exportAccessor` 
-  `filepath` (string) ^.*\.txt$
-  `path` (string) ^((/gltf/accessors/\d+))$


### Convert Storage Type
Convert storage type of an image (Embedded, BufferView, File) or buffer (Embedded, File)

 `convert_storage_type` 


### Convert Storage Type for All Images or Buffers
Convert storage type for all images (Embedded, BufferView, File) or buffers (Embedded, File)

 `convert_storage_type_for_each` 


### Apply Draco Mesh Compression
Apply draco mesh compression on a primitive with given compression settings

 `apply_draco` 
-  `path` (string) ^(/gltf/meshes/\d+/primitives/\d+)$
-  `quantizationBitsColor` (number) 
-  `quantizationBitsGeneric` (number) 
-  `quantizationBitsNormal` (number) 
-  `quantizationBitsPosition` (number) 
-  `quantizationBitsUv` (number) 
-  `sequentialEncoding` (boolean) 
-  `targetCompressionLevel` (number) 


### Apply Draco Mesh Compression for Each Primitive
Apply draco mesh compression to each primitive with given compression settings

 `apply_draco_for_each` 
-  `quantizationBitsColor` (number) 
-  `quantizationBitsGeneric` (number) 
-  `quantizationBitsNormal` (number) 
-  `quantizationBitsPosition` (number) 
-  `quantizationBitsUv` (number) 
-  `sequentialEncoding` (boolean) 
-  `targetCompressionLevel` (number) 


### Resolve Draco Mesh Compression
Decodes the compressed primitive and removes the draco mesh compression extension from it

 `resolve_draco` 
-  `path` (string) ^(/gltf/meshes/\d+/primitives/\d+)$


### Resolve Draco Mesh Compression for Each Primitive
Decodes all compressed primitives and removes the draco mesh compression extension from them

 `resolve_draco_for_each` 


### Deinterleave Accessor
The data of the Accessor is reordered so that it is tightly packed

 `deinterleave_accessor` 
-  `path` (string) ^(/gltf/accessors/\d+)$


### Convert Image
Change resolution and compression of an image

 `convert_image` 


### Convert Image for Each
Change resolution and compression of all images

 `convert_image_for_each` 


### Change TRS
Change Translation, Rotation, Scale format of the node to individual components (TRS) or to a single transformation matrix

 `change_transformation_format` 
-  `path` (string) ^(/gltf/nodes/\d+)$
-  `storageType` (string) ^TRS|Matrix$


### Change TRS For All
Change transformation format (TRS or Matrix) for all nodes in the glTF

 `change_transformation_format_for_each` 
-  `storageType` (string) ^TRS|Matrix$


### Join Primitives
Join primitives of a mesh

 `join_primitives` 


### Narrow Index Type
Minimize byte length of indices

 `optimize_index_type` 


### Merge glTF
A second glTF referenced by a file path is merged into the current one

 `merge_gltf` 
-  `filepath` (string) 


