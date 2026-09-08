// Copyright © 2021 UX3D GmbH. All rights reserved. 🏝
#pragma once

constexpr auto boxGltf = R"(
{
    "asset": {
        "generator": "COLLADA2GLTF",
        "version": "2.0"
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
            ],
            "camera": 1
        },
        {
            "mesh": 0,
            "camera": 0
        },
        {
        }
    ],
    "cameras": [
		{
			"name": "Camera_1",
			"perspective": {
				"yfov": 1.000000000000001,
				"zfar": 688.0,
				"znear": 0.07
			},
			"type": "perspective"
		},
		{
			"name": "Camera_2",
			"orthographic": {
				"xmag": 1.0,
				"ymag": 1.0,
				"zfar": 100.0,
				"znear": 0.0
			},
			"type": "orthographic"
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
                    "material": 0
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
            "pbrMetallicRoughness": {
                "baseColorFactor": [
                    0.800000011920929,
                    0.0,
                    0.0,
                    1.0
                ],
                "metallicFactor": 0.0
            },
            "name": "Red"
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
        },
        {
            "buffer": 1,
            "byteOffset": 0,
            "byteLength": 100,
            "target": 34962
        }
    ],
    "buffers": [
        {
            "byteLength": 648,
            "uri": "Box0.bin"
        },
        {
            "byteLength": 1000,
            "uri": "DummyData.bin"
        }
    ]
}
)";
