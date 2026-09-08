# Vulkan

## Installation

### Windows & Ubuntu

Just install the [VulkanSDK from LunarG](https://www.lunarg.com/vulkan-sdk/)

### Other Linux

You might find the vulkan-sdk in your package manager or you might need to install the package manually.

### macOS

Install [Homebrew](https://brew.sh)

In Terminal type the following command

```sh
brew tap apenngrace/vulkan
```

to select the vulkansdk repo.

Then you can install the vulkansdk with

```sh
brew cask install vulkan-sdk
```

After installation, macOS security will not allow control flow to enter the unsigned vulkansdk libs. You can do this in the GUI by running your app, and clicking **Cancel** in the popup dialog. Afterwards you need to add an exception in `System Preferences -> Security & Privacy` by clicking **Allow Anyway** in the `General` tab.
