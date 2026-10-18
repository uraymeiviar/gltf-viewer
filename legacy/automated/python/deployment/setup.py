import glob
from setuptools import setup, Distribution


class BinaryDistribution(Distribution):
    def has_ext_modules(foo):
        return True
fileList = glob.glob("**/*", recursive=True, root_dir="GestaltorAutomated")

setup(
    name="GestaltorAutomated",
    version="1.1.0",
    description="Gestaltor Automated",
    packages=["GestaltorAutomated"],
    package_data={"": fileList},
    include_package_data=True,
    distclass=BinaryDistribution,
)
