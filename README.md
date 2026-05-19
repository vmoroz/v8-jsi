# React Native V8 JSI adapter
A V8 adapter implemention of the JSI interface for the react-native framework.

[![Build Status](https://dev.azure.com/ms/v8-jsi/_apis/build/status/microsoft.v8-jsi?branchName=master)](https://dev.azure.com/ms/v8-jsi/_build/latest?definitionId=321&branchName=master)

## Building

V8-JSI builds via the Node.js GYP pipeline (V8 is vendored at `deps/nodejs/deps/v8/`,
synced from upstream Node.js by [`sync-manifest.json`](sync-manifest.json) — no
`depot_tools` / `gclient` / external V8 source fetching).

#### Windows

##### Prerequisites

1. **Windows 10 SDK** (NOT Windows 11) —
   <https://developer.microsoft.com/en-us/windows/downloads/sdk-archive>
1. **Microsoft Visual C++ Redistributables** (x64 + x86) —
   <https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist>
1. **Visual Studio 2022** — <https://visualstudio.microsoft.com/vs> — with these
   Individual Components:
   * MSVC v143 - VS 2022 C++ x64/x86 Spectre-mitigated libs (Latest)
   * C++ ATL for latest v143 build tools with Spectre Mitigations (x86 & x64)
1. **Python 3.x** on PATH (required by the Node.js configure step).
1. **Node.js v24+** on PATH (required by `scripts/build.ts`).
1. Enable PowerShell script execution (launch Cmd as Administrator once):
   ```Cmd
   powershell Set-ExecutionPolicy RemoteSigned
   ```

##### Build

The build entry point is `deps/nodejs/vcbuild.bat`. Release builds populate
`deps/nodejs/out/Release/`; Debug builds populate `deps/nodejs/out/Debug/`.

**Release** (whole solution including `v8jsi`):
```Cmd
cd deps\nodejs
vcbuild.bat release v8jsi without-intl
```

**Debug** — note the explicit `cctest` keyword. `vcbuild.bat`'s `release` keyword
implicitly sets `cctest=1` which keeps the MSBuild target as the whole solution;
`debug` does not, so the target narrows to just `node` and v8jsi is silently
skipped without `cctest`:
```Cmd
cd deps\nodejs
vcbuild.bat debug v8jsi cctest without-intl
```

**Other platforms** — pass `x86` or `arm64` as an additional keyword:
```Cmd
vcbuild.bat release v8jsi without-intl arm64
```

**Clean** (if stale artifacts cause issues):
```Cmd
cd deps\nodejs
vcbuild.bat clean
```

##### Package + tests

[`scripts/build.ts`](scripts/build.ts) is the orchestration entry point for
packaging, BinSkim, and end-to-end smoke validation. See
[`scripts/CLAUDE.md`](scripts/CLAUDE.md) and [`CLAUDE.md`](CLAUDE.md) for the
full command catalog. Common invocations:
```Cmd
node scripts/build.ts                          ; Release x64 build
node scripts/build.ts --no-build --pack        ; Pack the NuGet
node scripts/build.ts --no-build --binskim     ; Run BinSkim on the built DLL
test-harness\run-smoke.ps1                     ; Real-consumer smoke (Release/Release)
test-harness\run-smoke.ps1 -Configuration Debug ; Debug-CRT consumer / Release v8jsi.dll
```

### Out-of-sync issues
Until the JSI headers find a more suitable home, they're currently duplicated between the various repos. Code in jsi\jsi should be synchronized with the matching version of JSI from react-native (from https://github.com/facebook/hermes/tree/master/API/jsi/jsi).

## Contributing
See [Contributing guidelines](./docs/CONTRIBUTING.md) for how to setup your fork of the repo and start a PR to contribute to React Native V8 JSI adapter.

## License

The V8 JSI adapter, and all newly contributed code is provided under the [MIT License](LICENSE). Portions of the JSI interface derived from Hermes are copyright Facebook.

## Code of Conduct

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
