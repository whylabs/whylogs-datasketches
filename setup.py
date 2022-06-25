import os
import sys
import sysconfig
import platform
import subprocess

from setuptools import setup, find_packages, Extension
from setuptools.command.build_ext import build_ext

class CMakeExtension(Extension):
    def __init__(self, name, sourcedir=''):
        Extension.__init__(self, name, sources=[])
        self.sourcedir = os.path.abspath(sourcedir)

class CMakeBuild(build_ext):
    def run(self):
        try:
            subprocess.check_output(['cmake', '--version'])
        except OSError:
            raise RuntimeError(
                "CMake >= 3.12 must be installed to build the following extensions: " +
                ", ".join(e.name for e in self.extensions))

        for ext in self.extensions:
            self.build_extension(ext)

    def build_extension(self, ext):
        extdir = os.path.abspath(
            os.path.dirname(self.get_ext_fullpath(ext.name)))
        cmake_args =  ['-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=' + extdir]
        cmake_args += ['-DWITH_PYTHON=True']
        cmake_args += ['-DCMAKE_CXX_STANDARD=17']
        # ensure we use a consistent python version
        python_executable = '' + sys.executable
        python_path = python_executable[:len(python_executable) - 6]
        cmake_args += ['-DPython3_EXECUTABLE=' + python_executable]
        cmake_args += ['-DPython_EXECUTABLE=' + python_executable]
        cmake_args += ['-DPython3_INCLUDE_DIRS=' + python_path]
        cmake_args += ['-DPython_INCLUDE_DIRS=' + python_path]
        
        cfg = 'Debug' if self.debug else 'Release'
        build_args = ['--config', cfg]

        if platform.system() == "Windows":
            cmake_args += ['-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}'.format(
                           cfg.upper(),
                           extdir)]
            if sys.maxsize > 2**32:
                cmake_args += ['-T', 'host=x64']
                cmake_args += ['-DCMAKE_GENERATOR_PLATFORM=x64']
                build_args += ['--', '/m']
        else:
            cmake_args += ['-DCMAKE_BUILD_TYPE=' + cfg]
            build_args += ['--', '-j2']

        env = os.environ.copy()
        env['CXXFLAGS'] = '{} -DVERSION_INFO=\\"{}\\"'.format(
            env.get('CXXFLAGS', ''),
            self.distribution.get_version())
        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        subprocess.check_call(['cmake', ext.sourcedir] + cmake_args,
                              cwd=self.build_temp, env=env)
        subprocess.check_call(['cmake', '--build', '.', '--target', 'python'] + build_args,
                              cwd=self.build_temp, env=env)
        print() # add an empty line to pretty print

setup(
    name='whylogs-sketching',
    version='3.4.1.dev1',
    author='whylogs team',
    author_email='support@whylabs.ai',
    description='sketching library of whylogs',
    license='Apache License 2.0',
    url='https://github.com/whylabs/whylogs-sketching',
    long_description=open('python/README.md').read(),
    long_description_content_type='text/markdown',
    packages=find_packages(include='python', exclude=["python/tests"]), # python pacakges only in this dir
    package_dir={'':'python'},
    exclude=["*.tests", "*.tests.*", "tests.*", "tests"],    # may need to add all source paths for sdist packages w/o MANIFEST.in
    ext_modules=[CMakeExtension('whylogs-sketching')],
    cmdclass={'build_ext': CMakeBuild},
    install_requires=[],
    zip_safe=False
)
