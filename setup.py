import os

from distutils.core import setup, Extension
from subprocess import Popen, PIPE


sources = ["pytox/pytox.c", "pytox/core.c", "pytox/util.c"]


libraries = ["toxcore", "sodium", "ws2_32", "wsock32", "opus", "vpx"]
cflags = ["-Wall", "-Wno-declaration-after-statement"]



if True:
    sources.append("pytox/av.c")
    cflags.append("-DENABLE_AV")
else:
    print("Warning: AV support not found, disabled.")

setup(
    name="PyTox",
    version="0.2.0",
    description="Python binding for Tox the skype replacement",
    author="Wei-Ning Huang (AZ)",
    author_email="aitjcize@gmail.com",
    url="http://github.com/TokTok/py-toxcore-c",
    license="GPL",
    ext_modules=[
        Extension(
            "pytox",
            sources,
            library_dirs=["/usr/local/lib"],
            extra_compile_args=cflags,
            libraries=libraries
        )
    ]
)
