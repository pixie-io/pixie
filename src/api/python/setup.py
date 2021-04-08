import setuptools

with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

with open("requirements.txt", "r", encoding="utf-8") as fh:
    requirements = fh.read()

setuptools.setup(
    name="pxapi",
    version="0.2.0",
    author="Pixie Labs",
    license='Apache-2.0',
    author_email="help@pixielabs.ai",
    description="The python client for the Pixie API",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/pixie-labs/pixie",
    packages=setuptools.find_packages(),
    install_requires=requirements,
    classifiers=[
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.8",
        "Operating System :: OS Independent",
        "License :: OSI Approved :: Apache Software License",
        "Typing :: Typed",
    ],
    python_requires='>=3.8',
)
