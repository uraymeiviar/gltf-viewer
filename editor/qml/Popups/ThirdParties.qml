import QtQuick 2.14
import QtQuick.Layouts 1.14
import QtQuick.Controls 2.15

import "../Components" as Components

import "../Colors.js" as Colors


Rectangle {
    id: mainRect
    width: 410
    height: 490
    visible: true
    color: Colors.dark1

    ColumnLayout {
        width: parent.width
        spacing: 1

        Components.PrefPanelRowComp {
            height: 40
            rightCustomPadding: 30
            leftCustomPadding: 30
            Label {
                text: "Software"
                verticalAlignment: Text.AlignVCenter
                topPadding: 5
            }

            Item {
                Layout.fillWidth: true
            }

            Label {
                text: "License"
                verticalAlignment: Text.AlignVCenter
                topPadding: 5
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://www.qt.io"><font color=\"' + Colors.light1 + '\">Qt</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://doc.qt.io/qt-6/lgpl.html"><font color=\"' + Colors.light1 + '\">LPGL v3</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://nlohmann.github.io/json/"><font color=\"' + Colors.light1 + '\">JSON for Modern C++</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/nlohmann/json/blob/develop/LICENSE.MIT"><font color=\"' + Colors.light1 + '\">MIT</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/nothings/stb"><font color=\"' + Colors.light1 + '\">stb</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: 'Public domain'
                color: Colors.light1
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://glm.g-truc.net/0.9.9/index.html"><font color=\"' + Colors.light1 + '\">glm</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/g-truc/glm/blob/master/copying.txt"><font color=\"' + Colors.light1 + '\">MIT</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/chaoticbob/SPIRV-Reflect"><font color=\"' + Colors.light1 + '\">SPIRV-Reflect</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://www.apache.org/licenses/LICENSE-2.0"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/KhronosGroup/KTX-Software"><font color=\"' + Colors.light1 + '\">KTX-Software</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/KhronosGroup/KTX-Software/blob/master/LICENSE.md"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/PixarAnimationStudios/USD"><font color=\"' + Colors.light1 + '\">USD</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/PixarAnimationStudios/USD/blob/release/LICENSE.txt"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/google/usd_from_gltf"><font color=\"' + Colors.light1 + '\">USD from glTF</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/google/usd_from_gltf/blob/master/LICENSE"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/google/draco"><font color=\"' + Colors.light1 + '\">Draco</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/google/draco/blob/master/LICENSE"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/assimp/assimp"><font color=\"' + Colors.light1 + '\">Open Asset Import Library</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/assimp/assimp/blob/master/LICENSE"><font color=\"' + Colors.light1 + '\">3-clause BSD</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification"><font color=\"' + Colors.light1 + '\">Fast-Quadric-Mesh-Simplification</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/sp4cerat/Fast-Quadric-Mesh-Simplification/blob/master/LICENSE.md"><font color=\"' + Colors.light1 + '\">MIT</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/mmikk/MikkTSpace"><font color=\"' + Colors.light1 + '\">MikkTSpace</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: 'Custom'
                color: Colors.light1
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/openssl/openssl"><font color=\"' + Colors.light1 + '\">OpenSSL</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/openssl/openssl/blob/master/LICENSE.txt"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/libressl-portable/portable"><font color=\"' + Colors.light1 + '\">LibreSSL</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/libressl-portable/openbsd/blob/master/src/lib/libssl/LICENSE"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/lieff/minimp3"><font color=\"' + Colors.light1 + '\">minimp3</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/lieff/minimp3/blob/master/LICENSE"><font color=\"' + Colors.light1 + '\">CC0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://www.openal.org/"><font color=\"' + Colors.light1 + '\">OpenAL</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/kcat/openal-soft/blob/master/COPYING"><font color=\"' + Colors.light1 + '\">GNU</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/KhronosGroup/glTF-Validator"><font color=\"' + Colors.light1 + '\">glTF-Validator</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/KhronosGroup/glTF-Validator/blob/main/LICENSE"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/benman64/subprocess"><font color=\"' + Colors.light1 + '\">subprocess</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/benman64/subprocess/blob/master/LICENSE"><font color=\"' + Colors.light1 + '\">MIT</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/ands/lightmapper"><font color=\"' + Colors.light1 + '\">Lightmapper</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/ands/lightmapper/blob/master/lightmapper.h"><font color=\"' + Colors.light1 + '\">Public domain</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }
                
        Components.PrefPanelRowComp {
            height: 20
            rightCustomPadding: 30
            leftCustomPadding: 30
            Text {
                text: '<a href="https://github.com/oneapi-src/oneTBB"><font color=\"' + Colors.light1 + '\">oneTBB</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }

            Item {
                Layout.fillWidth: true
            }

            Text {
                text: '<a href="https://github.com/oneapi-src/oneTBB/blob/master/LICENSE.txt"><font color=\"' + Colors.light1 + '\">Apache 2.0</font></a>'
                color: Colors.light1
                font.underline: true
                onLinkActivated: Qt.openUrlExternally(link)
            }
        }

        Components.PrefPanelRowComp {
            height: 20
            Item {
                Layout.fillWidth: true
            }
        }
    } // end of column layout
}
