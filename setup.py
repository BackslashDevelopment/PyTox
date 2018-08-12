import os

from distutils.core import setup, Extension
from subprocess import Popen, PIPE


sources = ["pytox/pytox.c", "pytox/core.c", "pytox/util.c",  "pytox/av.c"]


libraries = ["toxcore", "sodium", "toxav", "vpx", "opus", "ws2_32", "wsock32", "iphlpapi", 'pthread', 'm' ]
cflags = ["-Wall", "-Wno-declaration-after-statement", "-DENABLE_AV" ]




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
            library_dirs=["/usr/x86_64-w64-mingw32/sys-root/mingw/lib"],
            extra_compile_args=cflags,
            libraries=libraries
        )
    ]
)
