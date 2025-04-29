import os
import ctypes

# Get the directory of this file
module_dir = os.path.dirname(__file__)

# Construct the full path to the native library
lib_path = os.path.join(
    module_dir, "lib", "./libeverload_tags.so"
)  # adapt extension for platform

# Load the shared library
mylib = ctypes.CDLL(lib_path)

from .lib.EverloadTags import *

StyleConfig = everload_tags.StyleConfig
BehaviorConfig = everload_tags.BehaviorConfig
Config = everload_tags.Config
TagsLineEdit = everload_tags.TagsLineEdit
TagsEdit = everload_tags.TagsEdit
