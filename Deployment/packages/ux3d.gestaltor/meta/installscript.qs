function Component()
{
        // default constructor
}

Component.prototype.createOperations = function()
{
        component.createOperations();

        if (systemInfo.productType === "windows") {
                component.addOperation("Mkdir", "@StartMenuDir@")
                component.addOperation("CreateShortcut",
                        "@TargetDir@/Gestaltor.exe",
                        "@StartMenuDir@/Gestaltor.lnk",
                        "workingDirectory=@TargetDir@",
                        "iconPath=@TargetDir@/Gestaltor.exe",
                        "description=Open Gestaltor");

                component.addOperation("CreateShortcut", 
                        "@TargetDir@/Gestaltor.exe",
                        "@DesktopDir@/Gestaltor.lnk",
                        "workingDirectory=@TargetDir@",
                        "iconPath=@TargetDir@/Gestaltor.exe",
                        "description=Open Gestaltor");


                let escapedPath = installer.value("TargetDir"); //@TargetDir@
                escapedPath = escapedPath.replace(new RegExp("/", 'g'), "\\");
                console.log("TargetDir="+escapedPath)

                component.addOperation("RegisterFileType",
                       "gltf",
                       escapedPath+"\\Gestaltor.exe \"%1\"",
                       "glTF file",
                       "model/gltf+json", 
                       escapedPath+"\\Gestaltor_FileIcon_glTF_256.ico",
                       "ProgId=QtProject.QtInstallerFramework.gltf");

                component.addOperation("RegisterFileType",
                       "glb",
                       escapedPath+"\\Gestaltor.exe \"%1\"",
                       "glb file",
                       "model/gltf-binary", 
                       escapedPath+"\\Gestaltor_FileIcon_glb_256.ico",
                       "ProgId=QtProject.QtInstallerFramework.glb"); 

                component.addOperation("RegisterFileType",
                       "gestalt",
                       escapedPath+"\\Gestaltor.exe \"%1\"",
                       "gestalt file",
                       "model/gltf-gestalt", 
                       escapedPath+"\\Gestaltor_FileIcon_gestalt_1024.ico",
                       "ProgId=QtProject.QtInstallerFramework.gestalt"); 

                component.addOperation("Execute",
                       "{0,1602,1638,3010}",
                       "@TargetDir@\\VC_redist.x64.exe",
                       "/passive",
                       "/norestart");
        }
}
