# How to manually create a pip installable package

- Build GestaltorOperations
- Copy content of `Gestaltor\build\bin\Release` into package folder (`Gestaltor\python_binding\deployment\GestaltorAutomated`)
- `python setup.py bdist_wheel`
-> Find the resulting wheel in `dist` 

# Install and use

- `pip install wheel .\GestaltorAutomated-1.0-cp310-cp310-win_amd64.whl`
- Alternatively call `pip install` directly on package folder (`Gestaltor\python_binding\deployment\GestaltorAutomated`)
- `from GestaltorAutomated import core_operations as ga` 
- `from GestaltorAutomated import helper as gah`