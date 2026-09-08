// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝

#include "reactor/Reactor.h"

using namespace ux3d;
using namespace ux3d::reactor;

constexpr auto box = R"(
{
    "asset": {
        "generator": "COLLADA2GLTF",
        "version": "2.0",
        "extensions": {
            "KHR_xmp_json_ld": {
                "packet": 0
            }
        }
    },
    "scene": 0,
    "scenes": [
        {
            "nodes": [
                0
            ]
        }
    ],
    "nodes": [
        {
            "name": "first",
            "children": [
                1
            ],
            "matrix": [
                1.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                -1.0,
                0.0,
                0.0,
                1.0,
                0.0,
                0.0,
                0.0,
                0.0,
                0.0,
                1.0
            ]
        },
        {
            "mesh": 0,
            "extensions": {
                "KHR_lights_punctual": {
                    "light": 0
                }
            }
        }
    ],
    "meshes": [
        {
            "primitives": [
                {
                    "attributes": {
                        "NORMAL": 1,
                        "POSITION": 2
                    },
                    "indices": 0,
                    "mode": 4,
                    "material": 0,
                    "extensions": {
                        "KHR_draco_mesh_compression": {
                            "attributes": {
                                "NORMAL": 0,
                                "POSITION": 1,
                                "TEXCOORD_0": 2
                            },
                            "bufferView": 0
                        },
                        "KHR_materials_variants": {
                            "mappings": [
                                {
                                    "material": 0,
                                    "variants": [
                                        0
                                    ]
                                },
                                {
                                    "material": 1,
                                    "variants": [
                                        1
                                    ]
                                }
                            ]
                        }
                    }
                }
            ],
            "name": "Mesh"
        }
    ],
    "accessors": [
        {
            "bufferView": 0,
            "byteOffset": 0,
            "componentType": 5123,
            "count": 36,
            "max": [
                23
            ],
            "min": [
                0
            ],
            "type": "SCALAR"
        },
        {
            "bufferView": 1,
            "byteOffset": 0,
            "componentType": 5126,
            "count": 24,
            "max": [
                1.0,
                1.0,
                1.0
            ],
            "min": [
                -1.0,
                -1.0,
                -1.0
            ],
            "type": "VEC3"
        },
        {
            "bufferView": 1,
            "byteOffset": 288,
            "componentType": 5126,
            "count": 24,
            "max": [
                0.5,
                0.5,
                0.5
            ],
            "min": [
                -0.5,
                -0.5,
                -0.5
            ],
            "type": "VEC3"
        }
    ],
    "materials": [
        {
            "emissiveFactor": [
                0.02199999988079071,
                0.017999999225139618,
                0.017999999225139618
            ],
            "pbrMetallicRoughness": {
                "baseColorFactor": [
                    0.800000011920929,
                    0.0,
                    0.0,
                    1.0
                ],
                "baseColorTexture": {
                    "extensions": {
                        "KHR_texture_transform": {
                            "offset": [
                                0.3,
                                0.2
                            ],
                            "rotation": 0.1,
                            "scale": [
                                0.8,
                                1.0
                            ]
                        }
                    },
                    "index": 0
                },
                "metallicFactor": 0.0
            },
            "name": "Red",
            "extensions": {
                "KHR_materials_clearcoat": {
                    "clearcoatFactor": 0.1
                },
                "KHR_materials_ior": {
                    "ior": 1.1
                },
                "KHR_materials_sheen": {
                    "sheenRoughnessFactor": 0.3
                },
                "KHR_materials_specular": {
                    "specularFactor": 0.4
                },
                "KHR_materials_transmission": {
                    "transmissionFactor": 0.2
                },
                "KHR_materials_volume": {
                    "thicknessFactor": 0.5
                },
                "KHR_materials_emissive_strength": {
                    "emissiveStrength": 1.2
                },
                "KHR_materials_iridescence": {
                    "iridescenceFactor": 0.3
                }
            }
        },
        {
            "pbrMetallicRoughness": {
                "baseColorFactor": [
                    0.0,
                    1.0,
                    0.0,
                    1.0
                ],
                "metallicFactor": 0.3
            },
            "name": "Green"
        }
    ],
    "textures": [
        {
            "source": 0
        },
        {
            "extensions": {
                "KHR_texture_basisu": {
                    "source": 1
                }
            }
        }
    ],
    "images": [
        {
            "mimeType": "image/png",
            "uri": "testImage.png"
        },
        {
            "mimeType": "image/ktx2",
            "uri": "testImage_BasisU.ktx2"
        }
    ],
    "bufferViews": [
        {
            "buffer": 0,
            "byteOffset": 576,
            "byteLength": 72,
            "target": 34963
        },
        {
            "buffer": 0,
            "byteOffset": 0,
            "byteLength": 576,
            "byteStride": 12,
            "target": 34962
        }
    ],
    "buffers": [
        {
            "byteLength": 648,
            "uri": "Box0.bin"
        }
    ],
    "extensionsRequired": [
        "KHR_draco_mesh_compression"
    ],
    "extensionsUsed": [
        "KHR_lights_punctual",
        "KHR_xmp_json_ld",
        "KHR_materials_variants",
        "KHR_materials_ior",
        "KHR_materials_clearcoat",
        "KHR_materials_sheen",
        "KHR_materials_specular",
        "KHR_materials_transmission",
        "KHR_materials_volume",
        "KHR_materials_emissive_strength",
        "KHR_materials_iridescence",
        "KHR_texture_transform",
        "KHR_texture_basisu",
        "KHR_draco_mesh_compression"
    ],
    "extensions": {
        "KHR_xmp_json_ld": {
            "packets": [
                {
                    "@context": {
                        "dc": "http://purl.org/dc/elements/1.1/",
                        "rdf": "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
                    },
                    "@id": "",
                    "dc:contributor": {
                        "@set": [
                            "Creator1Name",
                            "Creator2Email@email.com",
                            "Creator3Name<Email@email.com>"
                        ]
                    }
                }
            ]
        },
        "KHR_lights_punctual": {
            "lights": [
                {
                    "color": [
                        1.0,
                        0.0,
                        0.0
                    ],
                    "name": "Red Light",
                    "type": "point"
                }
            ]
        },
        "KHR_materials_variants": {
            "variants": [
                {
                    "name": "red"
                },
                {
                    "name": "green"
                }
            ]
        }
    }
}
)";
