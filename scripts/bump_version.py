#!/usr/bin/env python3

"""
bump_version.py -- global versioning synchronization script.
project url: https://github.com/tiw302/cjsonx

this script ensures structural consistency for versioning.
it safely executes targeted regex replacements to guarantee atomic version increments.
"""
import sys
import os
import re
import subprocess

# cross-platform ansi color support
if os.name == 'nt':
    os.system('color')

class c:
    ok = '\033[92m'   # green
    err = '\033[91m'  # red
    warn = '\033[93m' # yellow
    info = '\033[96m' # cyan
    rs = '\033[0m'    # reset

def bump_file(filepath, patterns_and_replacements):
    if not os.path.exists(filepath):
        print(f"{c.err}[✗] error: {filepath} not found.{c.rs}")
        return False
    
    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()
        
    new_content = content
    for pattern, replacement in patterns_and_replacements:
        new_content = re.sub(pattern, replacement, new_content, count=1)
    
    if content != new_content:
        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"{c.ok}[✓] updated {os.path.basename(filepath)}{c.rs}")
        return True
    else:
        print(f"{c.warn}[-] skipped {os.path.basename(filepath)} (no match or already set){c.rs}")
        return False

def main():
    if len(sys.argv) != 2:
        print(f"{c.info}usage: ./bump_version.py <new_version>{c.rs}")
        print(f"{c.info}example: ./bump_version.py 1.3.0{c.rs}")
        sys.exit(1)
        
    new_version = sys.argv[1].strip()
    
    # remove 'v' prefix if present
    if new_version.startswith('v'):
        new_version = new_version[1:]
    
    # basic semver check
    match = re.match(r'^(\d+)\.(\d+)\.(\d+)$', new_version)
    if not match:
        print(f"{c.err}[✗] error: version must be in format x.y.z (e.g., 1.3.0){c.rs}")
        sys.exit(1)

    major, minor, patch = match.groups()

    print(f"{c.info}[⚙] bumping version to {new_version}...{c.rs}\n")

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

    tasks = [
        (
            "CMakeLists.txt",
            [
                (r'project\(cjsonx VERSION \d+\.\d+\.\d+ LANGUAGES C\)', f'project(cjsonx VERSION {new_version} LANGUAGES C)')
            ]
        ),
        (
            "include/cjsonx.h",
            [
                (r'#define CJSONX_VERSION_MAJOR \d+', f'#define CJSONX_VERSION_MAJOR {major}'),
                (r'#define CJSONX_VERSION_MINOR \d+', f'#define CJSONX_VERSION_MINOR {minor}'),
                (r'#define CJSONX_VERSION_PATCH \d+', f'#define CJSONX_VERSION_PATCH {patch}'),
                (r'#define CJSONX_VERSION_STRING "\d+\.\d+\.\d+"', f'#define CJSONX_VERSION_STRING "{new_version}"')
            ]
        ),
        (
            "js/package.json",
            [
                (r'"version": "\d+\.\d+\.\d+"', f'"version": "{new_version}"')
            ]
        ),
        (
            "setup.py",
            [
                (r"version='\d+\.\d+\.\d+'", f"version='{new_version}'")
            ]
        )
    ]

    all_success = True
    for rel_path, patterns_replacements in tasks:
        filepath = os.path.join(root_dir, rel_path)
        success = bump_file(filepath, patterns_replacements)
        if not success:
            all_success = False

    print(f"\n{c.info}[⚙] synchronizing single_include with amalgamate.py...{c.rs}")
    try:
        subprocess.run([sys.executable, "scripts/amalgamate.py"], cwd=root_dir, check=True)
        print(f"{c.ok}[✓] synchronized single_include/cjsonx.h{c.rs}")
    except Exception as e:
        print(f"{c.err}[✗] failed to run amalgamate.py: {e}{c.rs}")
        all_success = False

    print(f"\n{c.ok}[✓] all tasks completed.{c.rs}")
    if not all_success:
        print(f"{c.warn}[!] some files were not updated. check warnings above.{c.rs}")

if __name__ == '__main__':
    main()
