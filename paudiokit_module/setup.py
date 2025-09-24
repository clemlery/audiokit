from setuptools import setup

setup(
	name='paudiokit',
	version='1',
	py_modules=['paudiokit'],
	install_requires=['cffi'],
	setup_requires=['cffi'],
	cffi_modules=['build_paudiokit.py:ffibuilder'],
)
