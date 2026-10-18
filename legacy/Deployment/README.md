**Automatic Deployment with installer script**
1. use create-windows-installer.sh and pass it a folder to build to, optionally already containing a cmake configuration. NOTE: CMake configurations created by the QtCreator do not work with this script

**Deployment without Installer**
1. Clear your *[BUILD DIR]\Release\bin* folder
2. Compile the Gestaltor in Release mode and do not open it (to avoid generating unnecessary files)
3. Open a command line in *[BUILD DIR]\Release\bin*
4. Run `C:\Qt\Qt[Qt VERSION]\[Qt VERSION]\[COMPILER VERSION]\bin\windeployqt.exe Gestaltor.exe` to populate the *[BUILD DIR]\Release\bin* folder with the necessary dlls
5. Additionally, if you are using MinGW or gcc:
	* Navigate to `C:\Qt\Qt[Qt VERSION]\[Qt VERSION]\[COMPILER VERSION]\bin`
	* Copy `libgcc_s_dw2-1.dll`, `libwinpthread-1.dll` and `libstdc++-6.dll` to the *[BUILD DIR]\Release\bin* folder
6. The *[BUILD DIR]\Release\bin* folder now holds a working, moveable version of the Gestaltor

**Deployment with Installer**
1. Complete the section **Deployment without Installer** from this README
2. Move all files from *[BUILD DIR]\Release\bin* to *Installer\packages\ux3d.gestaltor\data*
3. Generate the installer by running one of the following lines inside the Installer folder
	* Linux: `binarycreator --offline-only -c config/config.xml -p packages Gestaltor_Installer`
	* Windows `C:\Qt\[QtIFW-VERSION]\bin\binarycreator.exe --offline-only -c config/config.xml -p packages Gestaltor_Installer.exe`

**Further Information**
If you want to reconfigure/rename a part of the installer, have a look at this tutorial which is the basis for this README:
https://www.walletfox.com/course/qtinstallerframeworkexample.php