# VulkanSamples
A collection of Vulkan samples  
Made like the [SaschaWillems/Vulkan](https://github.com/SaschaWillems/Vulkan/) examples

## Dependencies
* [GLFW](https://github.com/glfw/glfw/) version >= 3.3.7
* [glm](https://github.com/g-truc/glm/) version >= 0.9.9.8
* [imgui](https://github.com/ocornut/imgui/) version >= 1.88
* [VulkanSDK](https://vulkan.lunarg.com/) version >= 1.1 (1.3.216.0 now in use)
* [VulkanMemoryAllocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/) version >= 3.0.1
* [stb](https://github.com/nothings/stb/)
* Any C++17 compiler

## Installation
1. Clone the repository
```
 git clone --recurse-submodules https://github.com/mrjbom/VulkanSamples
 cd VulkanSamples
```
2. Download [GLFW Windows pre-compiled binaries](https://www.glfw.org/download.html#windows-pre-compiled-binaries)
3. Download [VulkanSDK](https://vulkan.lunarg.com/)
4. Go to [this](third-party/glfw-libs) folder and read README.md
5. Go to [this](third-party/vulkansdk-include) folder and read README.md
6. Go to [this](third-party/vulkansdk-libs) folder and read README.md
7. Go to [this](third-party/vma-libs) folder and read README.md

## Samples

#### [TriangleSample](samples/TriangleSample/)
Just draws a triangle demonstrating how the base class abstracts the routine

#### [DescriptorSets](samples/DescriptorSets/)
Demonstrates how descriptors can be used to access uniform buffers from a shader

#### [DynamicDescriptorSets](samples/DynamicDescriptorSets/)
Demonstrates how one descriptor can be used to access different parts of a buffer

#### [PushConstants](samples/PushConstants/)
You don't have to use buffers to transfer values to shaders, you can use Push Constants which can transfer small amounts of data to shaders

#### [SpecializationConstants](samples/SpecializationConstants/)
Demonstrates how specialization constants can be used to set constant values in shaders when creating a pipeline.
