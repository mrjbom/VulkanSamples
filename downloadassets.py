#!/usr/bin/env python3

import sys
import gdown
from zipfile import ZipFile

ASSET_PACK_URL = 'https://drive.google.com/u/0/uc?id=1ttfvi5jMGFyaO9zMqRIZgJkhJXQcMql4'
ASSET_PACK_FILE_NAME = 'VulkanSamplesAssets.zip'

print("Downloading asset pack...")

gdown.download(ASSET_PACK_URL, ASSET_PACK_FILE_NAME, quiet=False)

print("Download finished")
print("Extracting assets...")

zip = ZipFile(ASSET_PACK_FILE_NAME, 'r')
zip.extractall("./")
zip.close()

print("Extracting finished")
