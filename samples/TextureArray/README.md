# Texture Array

This example uses a single VkImage object containing 3 different textures (layers), each texture has its own mip levels (3 levels).
All layers and their mip levels are also contained in only one KTX file.

A note about layers mip levels and sizes: 
All VkImage layers must have the same number of mip levels and the same size.
We specify the same number of mip levels and size for all layers when creating an image.

```
// Creating image
VkImageCreateInfo imageCreateInfo{};
imageCreateInfo.format = textureFormat;
imageCreateInfo.extent = { texture.width, texture.height, 1 };
imageCreateInfo.mipLevels = texture.mipLevels;
imageCreateInfo.arrayLayers = texture.layerCount;
```
