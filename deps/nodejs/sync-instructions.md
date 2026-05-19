# V8-JSI Fork Changes

This Node.js fork adds support for building v8jsi (V8 JSI runtime for React Native Windows).

## Our Additions

The following patterns are v8-jsi additions (not in upstream Node.js):
- `v8jsi` build option in vcbuild.bat
- `--build-v8jsi` flag and `build_v8jsi` variable in configure.py
- `build_v8jsi` condition and v8jsi.gyp include in node.gyp

These additions integrate with existing Node.js build infrastructure without modifying upstream behavior.
