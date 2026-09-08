#!python3

# Begin license text.
# Copyright 2013 Saumitro Dasgupta
# Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# End license text.
# 

'''
Code-signs all nested binaries inside an app bundle (excluding the app itself).
'''

import os, sys, re
import subprocess as sp

SIGN_EXTENSIONS = ['.so', '.dylib', '.framework', '.usda', '.json', '.glslfx', '.cpp', '.h', '.exr', '.qml', '.qmltypes', '.js'] # extension-less binaries are auto-included
CODE_SIGN_OPTS  = ['--verbose', '--force', '--entitlements', f'{os.getcwd()}/entitlements.plist', '--options', 'runtime', '--timestamp', '--sign']

def is_probably_binary(path):
    ext = os.path.splitext(path)[1]
    if ext in SIGN_EXTENSIONS:
        return True
    return (len(ext)==0) and os.access(path, os.X_OK) and not os.path.islink(path)

def is_definitely_binary(path):
    ext = os.path.splitext(path)[1]
    if ext in SIGN_EXTENSIONS:
        return True
    return b'Mach-O' in sp.check_output(['file', '--brief', path])

def get_signing_path(path):
    m = re.match('.*/(.*)\.framework/Versions/./(.*)', path)
    if m and (m.lastindex==2) and m.group(1)==m.group(2):
        #This is the main binary of a framework. Sign the framework version instead.
        path = path[:-(len(m.group(1))+1)]
    m = re.match('.*/(.*)\.app/Contents/MacOS/(.*)', path)
    if m and (m.lastindex==2) and m.group(1)==m.group(2):
        # This is the main binary of the app bundle. Exclude it.
        path = None
    return path

def get_signable_binaries(path):
    all_files = [os.path.join(root, fn) for root, dirs, files in os.walk(path) for fn in (files + dirs)]
    bins = filter(is_probably_binary, all_files)
    bins = filter(is_definitely_binary, bins)
    return sorted(filter(None, map(get_signing_path, bins)), reverse=True)

def code_sign_nested(identity, path, dryrun, entitlements=False):
    if entitlements:
        CODE_SIGN_OPTS.insert(0, f'{os.getcwd()}/entitlements.plist')
        CODE_SIGN_OPTS.insert(0, '--entitlements')
                
    signables = get_signable_binaries(path)
    if len(signables)==0:
        print("No signable binaries found.")
        return False
    cmd = sp.check_output if not dryrun else lambda x: ' '.join(x)
    try:
        for bin in signables:
            print(cmd(['codesign']+CODE_SIGN_OPTS+[identity, bin]))
    except sp.CalledProcessError:
        print('Code signing failed.')
        exit(1)
    print('%s successfully complete.'%('Code signing' if not dryrun else 'Dry run'))

def main():
    if (len(sys.argv)!=4) or (sys.argv[1] not in ('sign', 'list')):
        print('Usage: %s sign/list signing_identity app_path'%os.path.basename(__file__))
        exit(1)
    cs_identity, app_path = sys.argv[2:]
    code_sign_nested(cs_identity, app_path, dryrun=(sys.argv[1]=='list'))

if __name__=='__main__':
    main()
