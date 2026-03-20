#### run with build option:   python3 btfwinpymake.py build
from setuptools import setup, Extension

module1 = Extension(name='btfpy',sources=['btlibw.c','btfcmdline.c'],extra_compile_args=['/DBTFPYTHON','/D_CRT_SECURE_NO_WARNINGS'])
ret = setup(name = 'BtfpyPackage',
             version = '1.0',
             description = 'Bluetooth interface',
             ext_modules = [module1])

print('If build has succeeded the Python pyd module will be in a subdirectory /build/lib*/*.pyd')
print('Copy this to your Python code directory and rename it btfpy.pyd (so import btfpy can find it)')

             
