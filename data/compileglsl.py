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
vulkan_sdk_path = os.environ['VULKAN_SDK']
glslc_path = vulkan_sdk_path + '\Bin\glslc.exe'
print('Using glslc: ' + glslc_path)

for file in full_file_paths:
    match = re.search("\.vert$|\.frag$", file)
    if match:
        print('Compiling', file)
        args = [glslc_path, '-c', file, '-o', file + '.spv']
        subprocess.run(args);
