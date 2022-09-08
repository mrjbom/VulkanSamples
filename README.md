# VulkanSamples
A collection of Vulkan samples  
Made like the [SaschaWillems/Vulkan](https://github.com/SaschaWillems/Vulkan/) examples

## Dependencies
* Vulkan SDK
* GLFW
* GLM
* ImGui
* KTX lib
* Vulkan Memory Allocator
* Any C++17 compiler

## Preparations
To compile shaders you can use a Python script ***compileglsl.py*** in the `data` folder

## Assets
To download assets(models and textures) you can run the Python script ***downloadassets.py***

## Samples

#### [TriangleSample](samples/TriangleSample/)
Just draws a triangle demonstrating how the base class abstracts the routine

#### [DescriptorSets](samples/DescriptorSets/)
Demonstrates how descriptors can be used to access uniform buffers from a shader

#### [DynamicDescriptorSets](samples/DynamicDescriptorSets/)
Demonstrates how one descriptor can be used to access different parts of a buffer

#### [PushConstants](samples/PushConstants/)
It is not necessary to use buffers to transfer data to shaders, you can use Push Constants which can transfer small amounts of data to shaders

#### [SpecializationConstants](samples/SpecializationConstants/)
Demonstrates how specialization constants can be used to set constant values in shaders when creating a pipeline

#### [TextureMapping](samples/TextureMapping/)
This example shows how to load and use textures in KTX format  
The example uses a texture with mip levels and demonstrates how the LOD bias affects which mip level is chosen

#### [TextureArray](samples/TextureArray/)
The example demonstrates the use of an image with layers, this allows you to use a single image object as an array

#### [glTFloading](samples/glTFloading/)
This example demonstrates loading a glTF file with a model
