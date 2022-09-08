#!/usr/bin/env python3

import os
import re
import subprocess

def get_filepaths(directory):
    file_paths = []
    for root, directories, files in os.walk(directory):
        for filename in files:
            filepath = os.path.join(root, filename)
            file_paths.append(filepath)
    return file_paths

full_file_paths = get_filepaths(".")
vulkan_sdk_path = os.getenv("VULKAN_SDK")
#You can write your path to the folder with Vulkan SDK
#vulkan_sdk_path = ''
if len(vulkan_sdk_path) == 0:
    print('Error: The VULKAN_SDK environment variable is missing, you must manually set the Vulkan SDK path in this script')
    exit()
glslc_path = vulkan_sdk_path + '\Bin\glslc.exe'
print('Using glslc: ' + glslc_path)

for file in full_file_paths:
    match = re.search("\.vert$|\.frag$", file)
    if match:
        print('Compiling', file)
        args = [glslc_path, '-c', file, '-o', file + '.spv']
        subprocess.run(args);
