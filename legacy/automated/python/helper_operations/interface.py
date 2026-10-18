from GestaltorAutomated import proxy_properties as pp

class gltf_textures__d__extensions_KHR_texture_basisu(pp.ObjectProperty): 
    path_pattern = '/gltf/textures/\d+/extensions/KHR_texture_basisu'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.source = pp.Reference[gltf_images__d_](str(self.path)+'/source', self.handle, gltf_images__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, source = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_textures__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/textures/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_basisu = gltf_textures__d__extensions_KHR_texture_basisu(str(self.path)+'/KHR_texture_basisu', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_textures__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/textures/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_textures__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.sampler = pp.Reference[gltf_samplers__d_](str(self.path)+'/sampler', self.handle, gltf_samplers__d_)
        self.source = pp.Reference[gltf_images__d_](str(self.path)+'/source', self.handle, gltf_images__d_)

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, extensions = None, extras = None, name = None, sampler = None, source = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_textures(pp.PropertyArray[gltf_textures__d_]): 
    path_pattern = '/gltf/textures'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, name = None, sampler = None, source = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_skins__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/skins/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.inverseBindMatrices = pp.Reference[gltf_accessors__d_](str(self.path)+'/inverseBindMatrices', self.handle, gltf_accessors__d_)
        self.joints:list[pp.Reference[gltf_nodes__d_]] = pp.ReferenceArray[gltf_nodes__d_](str(self.path)+'/joints', self.handle, gltf_nodes__d_)
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.skeleton = pp.Reference[gltf_nodes__d_](str(self.path)+'/skeleton', self.handle, gltf_nodes__d_)

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, extensions = None, extras = None, inverseBindMatrices = None, joints = None, name = None, skeleton = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_skins(pp.PropertyArray[gltf_skins__d_]): 
    path_pattern = '/gltf/skins'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, inverseBindMatrices = None, joints = None, name = None, skeleton = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_scenes__d__extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/scenes/\d+/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_scenes__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/scenes/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_xmp_json_ld = gltf_scenes__d__extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_scenes__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/scenes/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_scenes__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.nodes:list[pp.Reference[gltf_nodes__d_]] = pp.ReferenceArray[gltf_nodes__d_](str(self.path)+'/nodes', self.handle, gltf_nodes__d_)

    def add_new_node(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_new_node', local)
        
    def add_node(self, pathSrc = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_node', local)
        
    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, extensions = None, extras = None, name = None, nodes = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_scenes(pp.PropertyArray[gltf_scenes__d_]): 
    path_pattern = '/gltf/scenes'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, name = None, nodes = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_samplers__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/samplers/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.magFilter = pp.GenericValue(str(self.path)+'/magFilter', self.handle )
        self.minFilter = pp.GenericValue(str(self.path)+'/minFilter', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.wrapS = pp.GenericValue(str(self.path)+'/wrapS', self.handle )
        self.wrapT = pp.GenericValue(str(self.path)+'/wrapT', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, extensions = None, extras = None, magFilter = None, minFilter = None, name = None, wrapS = None, wrapT = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_samplers(pp.PropertyArray[gltf_samplers__d_]): 
    path_pattern = '/gltf/samplers'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, magFilter = None, minFilter = None, name = None, wrapS = None, wrapT = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_nodes__d__extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/nodes/\d+/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def add_new_node(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_new_node', local)
        
    def add_node(self, pathSrc = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_node', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_nodes__d__extensions_KHR_lights_punctual(pp.ObjectProperty): 
    path_pattern = '/gltf/nodes/\d+/extensions/KHR_lights_punctual'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.light = pp.Reference[gltf_extensions_KHR_lights_punctual_lights__d_](str(self.path)+'/light', self.handle, gltf_extensions_KHR_lights_punctual_lights__d_)

    def add_new_node(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_new_node', local)
        
    def add_node(self, pathSrc = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_node', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, light = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_nodes__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/nodes/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_lights_punctual = gltf_nodes__d__extensions_KHR_lights_punctual(str(self.path)+'/KHR_lights_punctual', self.handle )
        self.KHR_xmp_json_ld = gltf_nodes__d__extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def add_new_node(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_new_node', local)
        
    def add_node(self, pathSrc = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_node', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_nodes__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/nodes/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.camera = pp.Reference[gltf_cameras__d_](str(self.path)+'/camera', self.handle, gltf_cameras__d_)
        self.children:list[pp.Reference[gltf_nodes__d_]] = pp.ReferenceArray[gltf_nodes__d_](str(self.path)+'/children', self.handle, gltf_nodes__d_)
        self.extensions = gltf_nodes__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.matrix = pp.GenericValue(str(self.path)+'/matrix', self.handle )
        self.mesh = pp.Reference[gltf_meshes__d_](str(self.path)+'/mesh', self.handle, gltf_meshes__d_)
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.skin = pp.Reference[gltf_skins__d_](str(self.path)+'/skin', self.handle, gltf_skins__d_)
        self.translation = pp.GenericValue(str(self.path)+'/translation', self.handle )
        self.weights = pp.GenericValue(str(self.path)+'/weights', self.handle )

    def add_new_node(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_new_node', local)
        
    def add_node(self, pathSrc = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'add_node', local)
        
    def apply_node_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_node_transform', local)
        
    def change_transformation_format(self, storageType = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'change_transformation_format', local)
        
    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, camera = None, children = None, extensions = None, extras = None, matrix = None, mesh = None, name = None, rotation = None, scale = None, skin = None, translation = None, weights = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def create_light(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_light', local)
        
    def create_mesh(self, innerRadius = None, outerRadius = None, scale = None, sectors = None, segments = None, shape = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_mesh', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def remove_node_reference(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove_node_reference', local)
        
    def transform_node(self, rotation = None, scale = None, translation = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'transform_node', local)
        

class gltf_nodes(pp.PropertyArray[gltf_nodes__d_]): 
    path_pattern = '/gltf/nodes'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, camera = None, children = None, extensions = None, extras = None, matrix = None, mesh = None, name = None, rotation = None, scale = None, skin = None, translation = None, weights = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_meshes__d__primitives__d__targets__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/targets/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.NORMAL = pp.Reference[gltf_accessors__d_](str(self.path)+'/NORMAL', self.handle, gltf_accessors__d_)
        self.POSITION = pp.Reference[gltf_accessors__d_](str(self.path)+'/POSITION', self.handle, gltf_accessors__d_)
        self.TANGENT = pp.Reference[gltf_accessors__d_](str(self.path)+'/TANGENT', self.handle, gltf_accessors__d_)

    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_meshes__d__primitives__d__targets(pp.PropertyArray[gltf_meshes__d__primitives__d__targets__d_]): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/targets'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, NORMAL = None, POSITION = None, TANGENT = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_meshes__d__primitives__d__extensions_KHR_materials_variants_mappings__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/extensions/KHR_materials_variants/mappings/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.material = pp.Reference[gltf_materials__d_](str(self.path)+'/material', self.handle, gltf_materials__d_)
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.variants:list[pp.Reference[gltf_extensions_KHR_materials_variants_variants__d_]] = pp.ReferenceArray[gltf_extensions_KHR_materials_variants_variants__d_](str(self.path)+'/variants', self.handle, gltf_extensions_KHR_materials_variants_variants__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, material = None, name = None, variants = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_meshes__d__primitives__d__extensions_KHR_materials_variants_mappings(pp.PropertyArray[gltf_meshes__d__primitives__d__extensions_KHR_materials_variants_mappings__d_]): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/extensions/KHR_materials_variants/mappings'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, material = None, name = None, variants = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_meshes__d__primitives__d__extensions_KHR_materials_variants(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/extensions/KHR_materials_variants'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.mappings = gltf_meshes__d__primitives__d__extensions_KHR_materials_variants_mappings(str(self.path)+'/mappings', self.handle, gltf_meshes__d__primitives__d__extensions_KHR_materials_variants_mappings__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, mappings = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_meshes__d__primitives__d__extensions_KHR_draco_mesh_compression(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/extensions/KHR_draco_mesh_compression'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.attributes = pp.GenericValue(str(self.path)+'/attributes', self.handle )
        self.bufferView = pp.Reference[gltf_bufferViews__d_](str(self.path)+'/bufferView', self.handle, gltf_bufferViews__d_)
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_meshes__d__primitives__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_draco_mesh_compression = gltf_meshes__d__primitives__d__extensions_KHR_draco_mesh_compression(str(self.path)+'/KHR_draco_mesh_compression', self.handle )
        self.KHR_materials_variants = gltf_meshes__d__primitives__d__extensions_KHR_materials_variants(str(self.path)+'/KHR_materials_variants', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_meshes__d__primitives__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/primitives/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.attributes = pp.GenericValue(str(self.path)+'/attributes', self.handle )
        self.extensions = gltf_meshes__d__primitives__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.indices = pp.Reference[gltf_accessors__d_](str(self.path)+'/indices', self.handle, gltf_accessors__d_)
        self.material = pp.Reference[gltf_materials__d_](str(self.path)+'/material', self.handle, gltf_materials__d_)
        self.mode = pp.GenericValue(str(self.path)+'/mode', self.handle )
        self.targets = gltf_meshes__d__primitives__d__targets(str(self.path)+'/targets', self.handle, gltf_meshes__d__primitives__d__targets__d_)

    def apply_draco(self, keepUncompressedData = None, quantizationBitsColor = None, quantizationBitsGeneric = None, quantizationBitsNormal = None, quantizationBitsPosition = None, quantizationBitsUv = None, sequentialEncoding = None, targetCompressionLevel = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_draco', local)
        
    def apply_vertex_clustering(self, colorThreshold = None, normalThreshold = None, positionThreshold = None, tangentThreshold = None, texCoordThreshold = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_vertex_clustering', local)
        
    def create_gltf_object(self, attributes = None, extensions = None, extras = None, indices = None, material = None, mode = None, targets = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def create_tangents(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_tangents', local)
        
    def create_texture_coordinates(self, normal = None, proxyShape = None, texCoordIndex = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_texture_coordinates', local)
        
    def reduce_triangle_count(self, quality = None, targetSize = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reduce_triangle_count', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def resolve_draco(self, deleteEncodedData = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'resolve_draco', local)
        

class gltf_meshes__d__primitives(pp.PropertyArray[gltf_meshes__d__primitives__d_]): 
    path_pattern = '/gltf/meshes/\d+/primitives'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, attributes = None, extensions = None, extras = None, indices = None, material = None, mode = None, targets = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_meshes__d__extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_meshes__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_xmp_json_ld = gltf_meshes__d__extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_meshes__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/meshes/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_meshes__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.primitives = gltf_meshes__d__primitives(str(self.path)+'/primitives', self.handle, gltf_meshes__d__primitives__d_)
        self.weights = pp.GenericValue(str(self.path)+'/weights', self.handle )

    def apply_draco(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_draco', local)
        
    def apply_vertex_clustering(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_vertex_clustering', local)
        
    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, extensions = None, extras = None, name = None, primitives = None, weights = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def join_primitives(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'join_primitives', local)
        
    def reduce_triangle_count(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reduce_triangle_count', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def resolve_draco(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'resolve_draco', local)
        
    def split_up_primitives(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'split_up_primitives', local)
        

class gltf_meshes(pp.PropertyArray[gltf_meshes__d_]): 
    path_pattern = '/gltf/meshes'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, name = None, primitives = None, weights = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_materials__d__pbrMetallicRoughness_metallicRoughnessTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness/metallicRoughnessTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__pbrMetallicRoughness_metallicRoughnessTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness/metallicRoughnessTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__pbrMetallicRoughness_metallicRoughnessTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__pbrMetallicRoughness_metallicRoughnessTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness/metallicRoughnessTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__pbrMetallicRoughness_metallicRoughnessTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__pbrMetallicRoughness_baseColorTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness/baseColorTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__pbrMetallicRoughness_baseColorTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness/baseColorTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__pbrMetallicRoughness_baseColorTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__pbrMetallicRoughness_baseColorTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness/baseColorTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__pbrMetallicRoughness_baseColorTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__pbrMetallicRoughness(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/pbrMetallicRoughness'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.baseColorFactor = pp.GenericValue(str(self.path)+'/baseColorFactor', self.handle )
        self.baseColorTexture = gltf_materials__d__pbrMetallicRoughness_baseColorTexture(str(self.path)+'/baseColorTexture', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.metallicFactor = pp.GenericValue(str(self.path)+'/metallicFactor', self.handle )
        self.metallicRoughnessTexture = gltf_materials__d__pbrMetallicRoughness_metallicRoughnessTexture(str(self.path)+'/metallicRoughnessTexture', self.handle )
        self.roughnessFactor = pp.GenericValue(str(self.path)+'/roughnessFactor', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__occlusionTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/occlusionTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__occlusionTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/occlusionTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__occlusionTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__occlusionTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/occlusionTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__occlusionTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.strength = pp.GenericValue(str(self.path)+'/strength', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__normalTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/normalTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__normalTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/normalTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__normalTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__normalTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/normalTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__normalTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_volume_thicknessTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_volume/thicknessTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_volume_thicknessTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_volume/thicknessTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_volume_thicknessTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_volume_thicknessTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_volume/thicknessTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_volume_thicknessTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_volume(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_volume'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.attenuationColor = pp.GenericValue(str(self.path)+'/attenuationColor', self.handle )
        self.attenuationDistance = pp.GenericValue(str(self.path)+'/attenuationDistance', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.thicknessFactor = pp.GenericValue(str(self.path)+'/thicknessFactor', self.handle )
        self.thicknessTexture = gltf_materials__d__extensions_KHR_materials_volume_thicknessTexture(str(self.path)+'/thicknessTexture', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, attenuationColor = None, attenuationDistance = None, extensions = None, extras = None, thicknessFactor = None, thicknessTexture = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_unlit(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_unlit'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_transmission_transmissionTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_transmission/transmissionTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_transmission_transmissionTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_transmission/transmissionTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_transmission_transmissionTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_transmission_transmissionTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_transmission/transmissionTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_transmission_transmissionTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_transmission(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_transmission'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.transmissionFactor = pp.GenericValue(str(self.path)+'/transmissionFactor', self.handle )
        self.transmissionTexture = gltf_materials__d__extensions_KHR_materials_transmission_transmissionTexture(str(self.path)+'/transmissionTexture', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, transmissionFactor = None, transmissionTexture = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular_specularTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular/specularTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular_specularTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular/specularTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_specular_specularTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular_specularTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular/specularTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_specular_specularTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular_specularColorTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular/specularColorTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular_specularColorTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular/specularColorTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_specular_specularColorTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular_specularColorTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular/specularColorTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_specular_specularColorTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_specular(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_specular'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.specularColorFactor = pp.GenericValue(str(self.path)+'/specularColorFactor', self.handle )
        self.specularColorTexture = gltf_materials__d__extensions_KHR_materials_specular_specularColorTexture(str(self.path)+'/specularColorTexture', self.handle )
        self.specularFactor = pp.GenericValue(str(self.path)+'/specularFactor', self.handle )
        self.specularTexture = gltf_materials__d__extensions_KHR_materials_specular_specularTexture(str(self.path)+'/specularTexture', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, specularColorFactor = None, specularColorTexture = None, specularFactor = None, specularTexture = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen_sheenRoughnessTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen_sheenRoughnessTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen/sheenRoughnessTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_sheen_sheenRoughnessTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen_sheenRoughnessTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen/sheenRoughnessTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_sheen_sheenRoughnessTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen_sheenColorTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen/sheenColorTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen_sheenColorTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen/sheenColorTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_sheen_sheenColorTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen_sheenColorTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen/sheenColorTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_sheen_sheenColorTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_sheen(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_sheen'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.sheenColorFactor = pp.GenericValue(str(self.path)+'/sheenColorFactor', self.handle )
        self.sheenColorTexture = gltf_materials__d__extensions_KHR_materials_sheen_sheenColorTexture(str(self.path)+'/sheenColorTexture', self.handle )
        self.sheenRoughnessFactor = pp.GenericValue(str(self.path)+'/sheenRoughnessFactor', self.handle )
        self.sheenRoughnessTexture = gltf_materials__d__extensions_KHR_materials_sheen_sheenRoughnessTexture(str(self.path)+'/sheenRoughnessTexture', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, sheenColorFactor = None, sheenColorTexture = None, sheenRoughnessFactor = None, sheenRoughnessTexture = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_specularGlossinessTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness/specularGlossinessTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_specularGlossinessTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness/specularGlossinessTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_specularGlossinessTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_specularGlossinessTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness/specularGlossinessTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_specularGlossinessTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_diffuseTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness/diffuseTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_diffuseTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness/diffuseTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_diffuseTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_diffuseTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness/diffuseTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_diffuseTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_pbrSpecularGlossiness'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.diffuseFactor = pp.GenericValue(str(self.path)+'/diffuseFactor', self.handle )
        self.diffuseTexture = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_diffuseTexture(str(self.path)+'/diffuseTexture', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.glossinessFactor = pp.GenericValue(str(self.path)+'/glossinessFactor', self.handle )
        self.specularFactor = pp.GenericValue(str(self.path)+'/specularFactor', self.handle )
        self.specularGlossinessTexture = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness_specularGlossinessTexture(str(self.path)+'/specularGlossinessTexture', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, diffuseFactor = None, diffuseTexture = None, extensions = None, extras = None, glossinessFactor = None, specularFactor = None, specularGlossinessTexture = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceThicknessTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceThicknessTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence/iridescenceThicknessTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceThicknessTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceThicknessTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence/iridescenceThicknessTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceThicknessTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence/iridescenceTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence/iridescenceTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence/iridescenceTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_iridescence(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_iridescence'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.iridescenceFactor = pp.GenericValue(str(self.path)+'/iridescenceFactor', self.handle )
        self.iridescenceIor = pp.GenericValue(str(self.path)+'/iridescenceIor', self.handle )
        self.iridescenceTexture = gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceTexture(str(self.path)+'/iridescenceTexture', self.handle )
        self.iridescenceThicknessMaximum = pp.GenericValue(str(self.path)+'/iridescenceThicknessMaximum', self.handle )
        self.iridescenceThicknessMinimum = pp.GenericValue(str(self.path)+'/iridescenceThicknessMinimum', self.handle )
        self.iridescenceThicknessTexture = gltf_materials__d__extensions_KHR_materials_iridescence_iridescenceThicknessTexture(str(self.path)+'/iridescenceThicknessTexture', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, iridescenceFactor = None, iridescenceIor = None, iridescenceTexture = None, iridescenceThicknessMaximum = None, iridescenceThicknessMinimum = None, iridescenceThicknessTexture = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_ior(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_ior'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.ior = pp.GenericValue(str(self.path)+'/ior', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, ior = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_emissive_strength(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_emissive_strength'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.emissiveStrength = pp.GenericValue(str(self.path)+'/emissiveStrength', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, emissiveStrength = None, extensions = None, extras = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatRoughnessTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatRoughnessTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatRoughnessTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatRoughnessTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatRoughnessTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatRoughnessTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatNormalTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatNormalTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatNormalTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatNormalTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatNormalTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat/clearcoatNormalTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatNormalTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions_KHR_materials_clearcoat(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions/KHR_materials_clearcoat'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.clearcoatFactor = pp.GenericValue(str(self.path)+'/clearcoatFactor', self.handle )
        self.clearcoatNormalTexture = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatNormalTexture(str(self.path)+'/clearcoatNormalTexture', self.handle )
        self.clearcoatRoughnessFactor = pp.GenericValue(str(self.path)+'/clearcoatRoughnessFactor', self.handle )
        self.clearcoatRoughnessTexture = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatRoughnessTexture(str(self.path)+'/clearcoatRoughnessTexture', self.handle )
        self.clearcoatTexture = gltf_materials__d__extensions_KHR_materials_clearcoat_clearcoatTexture(str(self.path)+'/clearcoatTexture', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, clearcoatFactor = None, clearcoatNormalTexture = None, clearcoatRoughnessFactor = None, clearcoatRoughnessTexture = None, clearcoatTexture = None, extensions = None, extras = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_materials_clearcoat = gltf_materials__d__extensions_KHR_materials_clearcoat(str(self.path)+'/KHR_materials_clearcoat', self.handle )
        self.KHR_materials_emissive_strength = gltf_materials__d__extensions_KHR_materials_emissive_strength(str(self.path)+'/KHR_materials_emissive_strength', self.handle )
        self.KHR_materials_ior = gltf_materials__d__extensions_KHR_materials_ior(str(self.path)+'/KHR_materials_ior', self.handle )
        self.KHR_materials_iridescence = gltf_materials__d__extensions_KHR_materials_iridescence(str(self.path)+'/KHR_materials_iridescence', self.handle )
        self.KHR_materials_pbrSpecularGlossiness = gltf_materials__d__extensions_KHR_materials_pbrSpecularGlossiness(str(self.path)+'/KHR_materials_pbrSpecularGlossiness', self.handle )
        self.KHR_materials_sheen = gltf_materials__d__extensions_KHR_materials_sheen(str(self.path)+'/KHR_materials_sheen', self.handle )
        self.KHR_materials_specular = gltf_materials__d__extensions_KHR_materials_specular(str(self.path)+'/KHR_materials_specular', self.handle )
        self.KHR_materials_transmission = gltf_materials__d__extensions_KHR_materials_transmission(str(self.path)+'/KHR_materials_transmission', self.handle )
        self.KHR_materials_unlit = gltf_materials__d__extensions_KHR_materials_unlit(str(self.path)+'/KHR_materials_unlit', self.handle )
        self.KHR_materials_volume = gltf_materials__d__extensions_KHR_materials_volume(str(self.path)+'/KHR_materials_volume', self.handle )
        self.KHR_xmp_json_ld = gltf_materials__d__extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__emissiveTexture_extensions_KHR_texture_transform(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/emissiveTexture/extensions/KHR_texture_transform'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.offset = pp.GenericValue(str(self.path)+'/offset', self.handle )
        self.rotation = pp.GenericValue(str(self.path)+'/rotation', self.handle )
        self.scale = pp.GenericValue(str(self.path)+'/scale', self.handle )
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def apply_texture_transform(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_texture_transform', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, offset = None, rotation = None, scale = None, texCoord = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_materials__d__emissiveTexture_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/emissiveTexture/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_texture_transform = gltf_materials__d__emissiveTexture_extensions_KHR_texture_transform(str(self.path)+'/KHR_texture_transform', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d__emissiveTexture(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+/emissiveTexture'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = gltf_materials__d__emissiveTexture_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.index = pp.Reference[gltf_textures__d_](str(self.path)+'/index', self.handle, gltf_textures__d_)
        self.texCoord = pp.GenericValue(str(self.path)+'/texCoord', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_materials__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/materials/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.alphaCutoff = pp.GenericValue(str(self.path)+'/alphaCutoff', self.handle )
        self.alphaMode = pp.GenericValue(str(self.path)+'/alphaMode', self.handle )
        self.doubleSided = pp.GenericValue(str(self.path)+'/doubleSided', self.handle )
        self.emissiveFactor = pp.GenericValue(str(self.path)+'/emissiveFactor', self.handle )
        self.emissiveTexture = gltf_materials__d__emissiveTexture(str(self.path)+'/emissiveTexture', self.handle )
        self.extensions = gltf_materials__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.normalTexture = gltf_materials__d__normalTexture(str(self.path)+'/normalTexture', self.handle )
        self.occlusionTexture = gltf_materials__d__occlusionTexture(str(self.path)+'/occlusionTexture', self.handle )
        self.pbrMetallicRoughness = gltf_materials__d__pbrMetallicRoughness(str(self.path)+'/pbrMetallicRoughness', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def convert_specular_glossiness_to_metallic_roughness(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_specular_glossiness_to_metallic_roughness', local)
        
    def create_gltf_object(self, alphaCutoff = None, alphaMode = None, doubleSided = None, emissiveFactor = None, emissiveTexture = None, extensions = None, extras = None, name = None, normalTexture = None, occlusionTexture = None, pbrMetallicRoughness = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_materials(pp.PropertyArray[gltf_materials__d_]): 
    path_pattern = '/gltf/materials'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, alphaCutoff = None, alphaMode = None, doubleSided = None, emissiveFactor = None, emissiveTexture = None, extensions = None, extras = None, name = None, normalTexture = None, occlusionTexture = None, pbrMetallicRoughness = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_images__d__extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/images/\d+/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_images__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/images/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_xmp_json_ld = gltf_images__d__extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_images__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/images/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.bufferView = pp.Reference[gltf_bufferViews__d_](str(self.path)+'/bufferView', self.handle, gltf_bufferViews__d_)
        self.extensions = gltf_images__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.mimeType = pp.GenericValue(str(self.path)+'/mimeType', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.uri = pp.GenericValue(str(self.path)+'/uri', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def convert_color_space(self, source = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_color_space', local)
        
    def convert_image(self, height = None, mimeType = None, width = None, quality = None, compression = None, etc1sCompressionLevel = None, etc1sQualityLevel = None, superCompression = None, transferFunction = None, uastcQualityLevel = None, uastcRDO = None, zstdCompressionLevel = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_image', local)
        
    def convert_storage_type(self, storageType = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_storage_type', local)
        
    def create_gltf_object(self, bufferView = None, extensions = None, extras = None, mimeType = None, name = None, uri = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def export_image(self, filepath = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'export_image', local)
        
    def image_blur(self, radius = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'image_blur', local)
        
    def image_hsv(self, brightness = None, hue = None, saturation = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'image_hsv', local)
        
    def image_invert(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'image_invert', local)
        
    def image_laplace(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'image_laplace', local)
        
    def image_sharpen(self, intensity = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'image_sharpen', local)
        
    def reload_file(self, filepath = None, resourceDirectory = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reload_file', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def transform_image(self, filter = None, offset = None, rotation = None, scale = None, wrapS = None, wrapT = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'transform_image', local)
        

class gltf_images(pp.PropertyArray[gltf_images__d_]): 
    path_pattern = '/gltf/images'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def convert_storage_type_for_each(self, storageType = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_storage_type_for_each', local)
        
    def create_gltf_component(self, bufferView = None, extensions = None, extras = None, mimeType = None, name = None, uri = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_extensions_KHR_xmp_json_ld_packets__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_xmp_json_ld/packets/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.context = pp.GenericValue(str(self.path)+'/context', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, context = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_xmp_json_ld_packets(pp.PropertyArray[gltf_extensions_KHR_xmp_json_ld_packets__d_]): 
    path_pattern = '/gltf/extensions/KHR_xmp_json_ld/packets'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, context = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packets = gltf_extensions_KHR_xmp_json_ld_packets(str(self.path)+'/packets', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_materials_variants_variants__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_materials_variants/variants/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, name = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_materials_variants_variants(pp.PropertyArray[gltf_extensions_KHR_materials_variants_variants__d_]): 
    path_pattern = '/gltf/extensions/KHR_materials_variants/variants'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, name = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_materials_variants(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_materials_variants'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.variants = gltf_extensions_KHR_materials_variants_variants(str(self.path)+'/variants', self.handle, gltf_extensions_KHR_materials_variants_variants__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_lights_punctual_lights__d__spot(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_lights_punctual/lights/\d+/spot'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.innerConeAngle = pp.GenericValue(str(self.path)+'/innerConeAngle', self.handle )
        self.outerConeAngle = pp.GenericValue(str(self.path)+'/outerConeAngle', self.handle )

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, innerConeAngle = None, outerConeAngle = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_lights_punctual_lights__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_lights_punctual/lights/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.color = pp.GenericValue(str(self.path)+'/color', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.intensity = pp.GenericValue(str(self.path)+'/intensity', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.range = pp.GenericValue(str(self.path)+'/range', self.handle )
        self.spot = gltf_extensions_KHR_lights_punctual_lights__d__spot(str(self.path)+'/spot', self.handle )
        self.type = pp.GenericValue(str(self.path)+'/type', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, color = None, extensions = None, extras = None, intensity = None, name = None, range = None, spot = None, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_lights_punctual_lights(pp.PropertyArray[gltf_extensions_KHR_lights_punctual_lights__d_]): 
    path_pattern = '/gltf/extensions/KHR_lights_punctual/lights'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, color = None, extensions = None, extras = None, intensity = None, name = None, range = None, spot = None, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions_KHR_lights_punctual(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions/KHR_lights_punctual'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.lights = gltf_extensions_KHR_lights_punctual_lights(str(self.path)+'/lights', self.handle, gltf_extensions_KHR_lights_punctual_lights__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_lights_punctual = gltf_extensions_KHR_lights_punctual(str(self.path)+'/KHR_lights_punctual', self.handle )
        self.KHR_materials_variants = gltf_extensions_KHR_materials_variants(str(self.path)+'/KHR_materials_variants', self.handle )
        self.KHR_xmp_json_ld = gltf_extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )


class gltf_cameras__d__perspective(pp.ObjectProperty): 
    path_pattern = '/gltf/cameras/\d+/perspective'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.aspectRatio = pp.GenericValue(str(self.path)+'/aspectRatio', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.yfov = pp.GenericValue(str(self.path)+'/yfov', self.handle )
        self.zfar = pp.GenericValue(str(self.path)+'/zfar', self.handle )
        self.znear = pp.GenericValue(str(self.path)+'/znear', self.handle )


class gltf_cameras__d__orthographic(pp.ObjectProperty): 
    path_pattern = '/gltf/cameras/\d+/orthographic'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.xmag = pp.GenericValue(str(self.path)+'/xmag', self.handle )
        self.ymag = pp.GenericValue(str(self.path)+'/ymag', self.handle )
        self.zfar = pp.GenericValue(str(self.path)+'/zfar', self.handle )
        self.znear = pp.GenericValue(str(self.path)+'/znear', self.handle )


class gltf_cameras__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/cameras/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.orthographic = gltf_cameras__d__orthographic(str(self.path)+'/orthographic', self.handle )
        self.perspective = gltf_cameras__d__perspective(str(self.path)+'/perspective', self.handle )
        self.type = pp.GenericValue(str(self.path)+'/type', self.handle )

    def change_camera_type(self, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'change_camera_type', local)
        
    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, extensions = None, extras = None, name = None, orthographic = None, perspective = None, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_cameras(pp.PropertyArray[gltf_cameras__d_]): 
    path_pattern = '/gltf/cameras'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, name = None, orthographic = None, perspective = None, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_buffers__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/buffers/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.byteLength = pp.GenericValue(str(self.path)+'/byteLength', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.uri = pp.GenericValue(str(self.path)+'/uri', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def convert_storage_type(self, storageType = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_storage_type', local)
        
    def create_gltf_object(self, byteLength = None, extensions = None, extras = None, name = None, uri = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def trim_buffer(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'trim_buffer', local)
        

class gltf_buffers(pp.PropertyArray[gltf_buffers__d_]): 
    path_pattern = '/gltf/buffers'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def convert_storage_type_for_each(self, storageType = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_storage_type_for_each', local)
        
    def create_gltf_component(self, byteLength = None, extensions = None, extras = None, name = None, uri = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_bufferViews__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/bufferViews/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.buffer = pp.Reference[gltf_buffers__d_](str(self.path)+'/buffer', self.handle, gltf_buffers__d_)
        self.byteLength = pp.GenericValue(str(self.path)+'/byteLength', self.handle )
        self.byteOffset = pp.GenericValue(str(self.path)+'/byteOffset', self.handle )
        self.byteStride = pp.GenericValue(str(self.path)+'/byteStride', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.target = pp.GenericValue(str(self.path)+'/target', self.handle )

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, buffer = None, byteLength = None, byteOffset = None, byteStride = None, extensions = None, extras = None, name = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def delete_bufferview(self, skipIfReferenced = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'delete_bufferview', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_bufferViews(pp.PropertyArray[gltf_bufferViews__d_]): 
    path_pattern = '/gltf/bufferViews'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, buffer = None, byteLength = None, byteOffset = None, byteStride = None, extensions = None, extras = None, name = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_asset_extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/asset/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_asset_extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/asset/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_xmp_json_ld = gltf_asset_extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_asset(pp.ObjectProperty): 
    path_pattern = '/gltf/asset'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.copyright = pp.GenericValue(str(self.path)+'/copyright', self.handle )
        self.extensions = gltf_asset_extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.generator = pp.GenericValue(str(self.path)+'/generator', self.handle )
        self.minVersion = pp.GenericValue(str(self.path)+'/minVersion', self.handle )
        self.version = pp.GenericValue(str(self.path)+'/version', self.handle )


class gltf_animations__d__samplers__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/animations/\d+/samplers/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.input = pp.Reference[gltf_accessors__d_](str(self.path)+'/input', self.handle, gltf_accessors__d_)
        self.interpolation = pp.GenericValue(str(self.path)+'/interpolation', self.handle )
        self.output = pp.Reference[gltf_accessors__d_](str(self.path)+'/output', self.handle, gltf_accessors__d_)

    def create_gltf_object(self, extensions = None, extras = None, input = None, interpolation = None, output = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_animations__d__samplers(pp.PropertyArray[gltf_animations__d__samplers__d_]): 
    path_pattern = '/gltf/animations/\d+/samplers'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, input = None, interpolation = None, output = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_animations__d__extensions_KHR_xmp_json_ld(pp.ObjectProperty): 
    path_pattern = '/gltf/animations/\d+/extensions/KHR_xmp_json_ld'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.packet = pp.Reference[gltf_extensions_KHR_xmp_json_ld_packets__d_](str(self.path)+'/packet', self.handle, gltf_extensions_KHR_xmp_json_ld_packets__d_)

    def create_gltf_component(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        
    def create_gltf_object(self, extensions = None, extras = None, packet = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def insert_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'insert_gltf_pointer', local)
        
    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        
    def set_gltf_pointer(self, pointer = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_pointer', local)
        
    def set_gltf_value(self, value = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'set_gltf_value', local)
        

class gltf_animations__d__extensions(pp.ObjectProperty): 
    path_pattern = '/gltf/animations/\d+/extensions'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.KHR_xmp_json_ld = gltf_animations__d__extensions_KHR_xmp_json_ld(str(self.path)+'/KHR_xmp_json_ld', self.handle )

    def reset(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reset', local)
        

class gltf_animations__d__channels__d__target(pp.ObjectProperty): 
    path_pattern = '/gltf/animations/\d+/channels/\d+/target'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.node = pp.Reference[gltf_nodes__d_](str(self.path)+'/node', self.handle, gltf_nodes__d_)
        self.path = pp.GenericValue(str(self.path)+'/path', self.handle )


class gltf_animations__d__channels__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/animations/\d+/channels/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.sampler = pp.Reference[gltf_animations__d__samplers__d_](str(self.path)+'/sampler', self.handle, gltf_animations__d__samplers__d_)
        self.target = gltf_animations__d__channels__d__target(str(self.path)+'/target', self.handle )

    def create_gltf_object(self, extensions = None, extras = None, sampler = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_animations__d__channels(pp.PropertyArray[gltf_animations__d__channels__d_]): 
    path_pattern = '/gltf/animations/\d+/channels'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, extensions = None, extras = None, sampler = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_animations__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/animations/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.channels = gltf_animations__d__channels(str(self.path)+'/channels', self.handle, gltf_animations__d__channels__d_)
        self.extensions = gltf_animations__d__extensions(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.samplers = gltf_animations__d__samplers(str(self.path)+'/samplers', self.handle, gltf_animations__d__samplers__d_)

    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, channels = None, extensions = None, extras = None, name = None, samplers = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def optimize_keyframes(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'optimize_keyframes', local)
        
    def reassign_skinned_animation(self, sourceSkin = None, targetSkin = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'reassign_skinned_animation', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        

class gltf_animations(pp.PropertyArray[gltf_animations__d_]): 
    path_pattern = '/gltf/animations'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, channels = None, extensions = None, extras = None, name = None, samplers = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf_accessors__d__sparse_values(pp.ObjectProperty): 
    path_pattern = '/gltf/accessors/\d+/sparse/values'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.bufferView = pp.Reference[gltf_bufferViews__d_](str(self.path)+'/bufferView', self.handle, gltf_bufferViews__d_)
        self.byteOffset = pp.GenericValue(str(self.path)+'/byteOffset', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )


class gltf_accessors__d__sparse_indices(pp.ObjectProperty): 
    path_pattern = '/gltf/accessors/\d+/sparse/indices'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.bufferView = pp.Reference[gltf_bufferViews__d_](str(self.path)+'/bufferView', self.handle, gltf_bufferViews__d_)
        self.byteOffset = pp.GenericValue(str(self.path)+'/byteOffset', self.handle )
        self.componentType = pp.GenericValue(str(self.path)+'/componentType', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )


class gltf_accessors__d__sparse(pp.ObjectProperty): 
    path_pattern = '/gltf/accessors/\d+/sparse'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.count = pp.GenericValue(str(self.path)+'/count', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.indices = gltf_accessors__d__sparse_indices(str(self.path)+'/indices', self.handle )
        self.values = gltf_accessors__d__sparse_values(str(self.path)+'/values', self.handle )


class gltf_accessors__d_(pp.ObjectProperty): 
    path_pattern = '/gltf/accessors/\d+'

    def __init__(self, path, handle): 
        self.path = path 
        self.handle = handle 
        pp.ObjectProperty.__init__(self, path, handle) 

        self.bufferView = pp.Reference[gltf_bufferViews__d_](str(self.path)+'/bufferView', self.handle, gltf_bufferViews__d_)
        self.byteOffset = pp.GenericValue(str(self.path)+'/byteOffset', self.handle )
        self.componentType = pp.GenericValue(str(self.path)+'/componentType', self.handle )
        self.count = pp.GenericValue(str(self.path)+'/count', self.handle )
        self.extensions = pp.GenericValue(str(self.path)+'/extensions', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.max = pp.GenericValue(str(self.path)+'/max', self.handle )
        self.min = pp.GenericValue(str(self.path)+'/min', self.handle )
        self.name = pp.GenericValue(str(self.path)+'/name', self.handle )
        self.normalized = pp.GenericValue(str(self.path)+'/normalized', self.handle )
        self.sparse = gltf_accessors__d__sparse(str(self.path)+'/sparse', self.handle )
        self.type = pp.GenericValue(str(self.path)+'/type', self.handle )

    def calculate_min_max(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'calculate_min_max', local)
        
    def clamp_accessor_values(self, max = None, min = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clamp_accessor_values', local)
        
    def clone_property(self, cloneModeShallow = None, target = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clone_property', local)
        
    def create_gltf_object(self, bufferView = None, byteOffset = None, componentType = None, count = None, extensions = None, extras = None, max = None, min = None, name = None, normalized = None, sparse = None, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_object', local)
        
    def deinterleave_accessor(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'deinterleave_accessor', local)
        
    def delete_accessor(self, skipIfReferenced = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'delete_accessor', local)
        
    def export_accessor(self, filepath = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'export_accessor', local)
        
    def normalize_weights(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'normalize_weights', local)
        
    def remove(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove', local)
        
    def resolve_mesh_quantization(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'resolve_mesh_quantization', local)
        
    def resolve_sparse_accessor(self):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'resolve_sparse_accessor', local)
        

class gltf_accessors(pp.PropertyArray[gltf_accessors__d_]): 
    path_pattern = '/gltf/accessors'

    def __init__(self, path, handle, type): 
        self.path = path 
        self.handle = handle 
        self.type = type 


    def create_gltf_component(self, bufferView = None, byteOffset = None, componentType = None, count = None, extensions = None, extras = None, max = None, min = None, name = None, normalized = None, sparse = None, type = None):
        path = self.path
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_gltf_component', local)
        

class gltf(pp.ObjectProperty): 
    path_pattern = '/gltf'

    def __init__(self, handle): 
        self.path = "/gltf" 
        self.handle = handle 

        self.accessors = gltf_accessors(str(self.path)+'/accessors', self.handle, gltf_accessors__d_)
        self.animations = gltf_animations(str(self.path)+'/animations', self.handle, gltf_animations__d_)
        self.asset = gltf_asset(str(self.path)+'/asset', self.handle )
        self.bufferViews = gltf_bufferViews(str(self.path)+'/bufferViews', self.handle, gltf_bufferViews__d_)
        self.buffers = gltf_buffers(str(self.path)+'/buffers', self.handle, gltf_buffers__d_)
        self.cameras = gltf_cameras(str(self.path)+'/cameras', self.handle, gltf_cameras__d_)
        self.extensions = gltf_extensions(str(self.path)+'/extensions', self.handle )
        self.extensionsRequired = pp.GenericValue(str(self.path)+'/extensionsRequired', self.handle )
        self.extensionsUsed = pp.GenericValue(str(self.path)+'/extensionsUsed', self.handle )
        self.extras = pp.GenericValue(str(self.path)+'/extras', self.handle )
        self.images = gltf_images(str(self.path)+'/images', self.handle, gltf_images__d_)
        self.materials = gltf_materials(str(self.path)+'/materials', self.handle, gltf_materials__d_)
        self.meshes = gltf_meshes(str(self.path)+'/meshes', self.handle, gltf_meshes__d_)
        self.nodes = gltf_nodes(str(self.path)+'/nodes', self.handle, gltf_nodes__d_)
        self.samplers = gltf_samplers(str(self.path)+'/samplers', self.handle, gltf_samplers__d_)
        self.scene = pp.Reference[gltf_scenes__d_](str(self.path)+'/scene', self.handle, gltf_scenes__d_)
        self.scenes = gltf_scenes(str(self.path)+'/scenes', self.handle, gltf_scenes__d_)
        self.skins = gltf_skins(str(self.path)+'/skins', self.handle, gltf_skins__d_)
        self.textures = gltf_textures(str(self.path)+'/textures', self.handle, gltf_textures__d_)

    def apply_draco_for_each(self, keepUncompressedData = None, quantizationBitsColor = None, quantizationBitsGeneric = None, quantizationBitsNormal = None, quantizationBitsPosition = None, quantizationBitsUv = None, sequentialEncoding = None, targetCompressionLevel = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'apply_draco_for_each', local)
        
    def blend_images(self, imageA = None, imageB = None, weight = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'blend_images', local)
        
    def change_transformation_format_for_each(self, storageType = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'change_transformation_format_for_each', local)
        
    def clean_gltf(self, filter = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clean_gltf', local)
        
    def clean_nodes(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clean_nodes', local)
        
    def clean_texture_coordinate_attributes(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'clean_texture_coordinate_attributes', local)
        
    def convert_image_for_each(self, height = None, mimeType = None, width = None, quality = None, compression = None, etc1sCompressionLevel = None, etc1sQualityLevel = None, superCompression = None, uastcQualityLevel = None, uastcRDO = None, zstdCompressionLevel = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'convert_image_for_each', local)
        
    def create_animation(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_animation', local)
        
    def create_camera_from_view(self, rotation = None, sceneIdx = None, translation = None, yFov = None, zFar = None, zNear = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_camera_from_view', local)
        
    def create_empty_gltf(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_empty_gltf', local)
        
    def create_scene(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'create_scene', local)
        
    def export_usdz(self, filepath = None, overwrite = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'export_usdz', local)
        
    def import_asset(self, filepath = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'import_asset', local)
        
    def import_image(self, filepath = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'import_image', local)
        
    def import_texture(self, filepath = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'import_texture', local)
        
    def join_meshes(self, meshPaths = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'join_meshes', local)
        
    def join_primitives(self, paths = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'join_primitives', local)
        
    def load_file(self, filepath = None, resourceDirectory = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'load_file', local)
        
    def merge_buffers(self, bufferPaths = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'merge_buffers', local)
        
    def merge_gltf(self, filepath = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'merge_gltf', local)
        
    def move_gltf_pointer(self, pointer = None, sourcePath = None, targetPath = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'move_gltf_pointer', local)
        
    def optimize_index_type(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'optimize_index_type', local)
        
    def remove_extension(self, property = None, properties = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove_extension', local)
        
    def remove_properties_by_name(self, property = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove_properties_by_name', local)
        
    def remove_redundant_draco(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove_redundant_draco', local)
        
    def remove_unknown_extension(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'remove_unknown_extension', local)
        
    def resolve_draco_for_each(self, deleteEncodedData = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'resolve_draco_for_each', local)
        
    def resolve_mesh_quantization_for_each(self):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'resolve_mesh_quantization_for_each', local)
        
    def save_file(self, cleanupData = None, filepath = None, overwrite = None, sizeLimit = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'save_file', local)
        
    def save_properties(self, filepath = None, overwrite = None, paths = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'save_properties', local)
        
    def update_extension_flags(self, extensions = None):
        local = locals()
        local.pop('self', None)
        local = {k: v for k, v in local.items() if v != None}
        pp.call(self, self.handle, 'update_extension_flags', local)
        

