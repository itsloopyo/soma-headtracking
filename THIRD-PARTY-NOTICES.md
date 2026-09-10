# Third-Party Notices

SOMA Head Tracking ships and links the components below. Everything else in
this repository is MIT licensed by itsloopyo (see `LICENSE`).

The final section covers SOMA itself. Nothing of the game is bundled or
redistributed, so it carries no licence text; it records what the mod does and
does not take from the game, and why.

## Ultimate ASI Loader

- **Version:** v9.7.4 (commit `6b440669144c4a0bef5718ab155df160d231cd42`)
- **License:** MIT
- **Upstream:** https://github.com/ThirteenAG/Ultimate-ASI-Loader
- **Usage:** Loads `SomaHeadTracking.asi` into the game. The upstream
  `dinput8.dll` binary is vendored untouched under `vendor/ultimate-asi-loader/`
  and `install.cmd` copies it beside the game exe as `version.dll`, a proxy
  name SDL2.dll already imports.
- **Bundled:** yes. Shipped in the release ZIP and copied from there at install
  time; install.cmd never reaches the network for the loader.

That `dinput8.dll` is a static binary and is not one component. Its
`Ultimate-ASI-Loader-x64` target in the v9.7.4 `premake5.lua` compiles
`external/injector/minhook/src/**.c`, `external/injector/utility/
FunctionHookMinHook.cpp` and `external/miniz/miniz.c` alongside the loader's own
sources, so redistributing it redistributes **MinHook**, **injector** and
**miniz** as well. Each has its own section below and its own file under
`licenses/` in every release ZIP. The loader's other vendored components -
MemoryModule, d3d8to9 and minidx9 - are compiled only into the 32-bit target
(`#if !X64` in `source/dllmain.h` and `source/dllmain.cpp`, and absent from the
x64 premake block), so none of them is in the binary shipped here and none is
listed below.

```
MIT License

Copyright (c) 2023 ThirteenAG

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## cameraunlock-core

- **Version:** commit `bd22895bb30ab7946d780b0af5782755e33e2cba`
- **License:** MIT
- **Upstream:** https://github.com/itsloopyo/cameraunlock-core
- **Usage:** The shared tracking pipeline - OpenTrack receiver, pose
  interpolation, smoothing and the camera helpers this mod builds its hooks on.
  Consumed as a git submodule and compiled into `SomaHeadTracking.asi`.
- **Bundled:** yes. Compiled into the shipped `.asi`.

Same copyright holder and same licence as this repository, so `LICENSE` at the
root of each release ZIP covers it.

---

## MinHook

- **Version:** v1.3.4 in the `.asi`, vendored and locally modified at
  `cameraunlock-core/vendor/minhook`: `MH_Initialize` there allocates from the
  process heap instead of creating a private one, and `MH_Uninitialize` does not
  destroy it. That change and the upstream baseline it sits on are recorded in
  that directory's `LOCAL-CHANGES.md`. BSD-2-Clause permits the modification and
  does not require it to be marked; it is named here so nobody attributes it to
  Tsuda Kageyu. The copy inside the loader binary is the same upstream project
  at whatever commit `ThirteenAG/injector` pins; the licence text is identical,
  so the block below covers both.
- **License:** BSD-2-Clause
- **Upstream:** https://github.com/TsudaKageyu/minhook
- **Usage:** Function hooking for the camera and crosshair hooks. It reaches
  the user by two separate routes, and both are binary distribution: vendored
  inside the `cameraunlock-core` submodule and linked statically into
  `SomaHeadTracking.asi`, and compiled into the vendored Ultimate ASI Loader
  `dinput8.dll` by way of `ThirteenAG/injector`, which carries MinHook as its
  own submodule. Its `hde32.c` / `hde64.c` are compiled in with it, so the
  notice below reproduces the Hacker Disassembler Engine copyright the licence
  file carries alongside Tsuda Kageyu's.
- **Bundled:** yes, twice. Compiled into the shipped `.asi` and into the
  shipped `dinput8.dll`.

```
MinHook - The Minimalistic API Hooking Library for x64/x86
Copyright (C) 2009-2017 Tsuda Kageyu.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

================================================================================
Portions of this software are Copyright (c) 2008-2009, Vyacheslav Patkov.
================================================================================
Hacker Disassembler Engine 32 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-------------------------------------------------------------------------------
Hacker Disassembler Engine 64 C
Copyright (c) 2008-2009, Vyacheslav Patkov.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

## miniz

- **Version:** 3.0.0, as vendored at `external/miniz/` in Ultimate ASI Loader
  v9.7.4
- **License:** MIT
- **Upstream:** https://github.com/richgel999/miniz
- **Usage:** Zip reading behind the loader's overload-from-folder feature.
  Nothing in this repository calls it and nothing links it; it arrives entirely
  inside the vendored `dinput8.dll`, whose x64 target compiles
  `external/miniz/miniz.c`.
- **Bundled:** yes. Compiled into the shipped `dinput8.dll`.

```
Copyright 2013-2014 RAD Game Tools and Valve Software
Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

---

## injector

- **Version:** as vendored at `external/injector/` in Ultimate ASI Loader v9.7.4
- **License:** Zlib
- **Upstream:** https://github.com/ThirteenAG/injector
- **Usage:** The loader's `FunctionHookMinHook` wrapper, which its x64 target
  compiles from `external/injector/utility/FunctionHookMinHook.cpp`, and the
  MinHook submodule that repository carries. Nothing in this repository calls
  it or links it; it arrives entirely inside the vendored `dinput8.dll`.
- **Bundled:** yes. Compiled into the shipped `dinput8.dll`.

The binary is unaltered upstream, so the "altered source versions" condition
below does not arise. It is reproduced whole regardless.

```
Copyright (C) 2012-2014 LINK/2012 <dma_2012@hotmail.com>

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
```

---

## OpenTrack

- **Version:** Not applicable. No OpenTrack code is used.
- **License:** ISC
- **Upstream:** https://github.com/opentrack/opentrack
- **Usage:** The mod receives head pose over OpenTrack's UDP datagram format on
  port 4242. Protocol compatibility only.
- **Bundled:** no.

---

## SOMA and the HPL3 engine

- **Rights holder:** Frictional Games
- **License:** proprietary. Nothing here is licensed from them and nothing of
  theirs is redistributed.
- **Bundled:** no. The mod requires a legitimately purchased copy of SOMA, which
  the player supplies. No part of the game is included in this repository or in
  any release ZIP, and the mod builds and its tests run with no copy of the game
  present.

SOMA is a trademark of Frictional Games. This mod is unofficial, and is not
affiliated with, endorsed by or supported by them.

**What the mod contains that came from looking at the game, and why.** To attach
itself to a running SOMA the mod has to be able to find a handful of engine
functions and fields inside the player's own executable. Two kinds of identifier
in `src/game_offsets.h` are what let it:

- **Structure offsets** - the byte position of a field within an engine class,
  e.g. that `cCamera` keeps its vertical field of view at `+0x28`. A number
  describing a memory layout.
- **Byte signatures** - for each hooked function, a short byte string read from
  its opening instructions (24 to 81 bytes, wildcarded wherever the bytes are an
  address or a jump offset) and matched at load time to locate that function. A
  fingerprint, not a copy: it identifies a function and cannot execute, and
  nothing in the game can be reconstructed from it.

Both are recorded for interoperability alone, are the minimum needed for it, and
are matched against the copy of the game the player already owns. Neither is
game source, game logic or game content.

Engine type and member names (`cCamera`, `cLuxPlayer`, `mfFOV` and so on) appear
in comments and constant names so a reader can tell what each number refers to.
Those names are published by SOMA itself: the game ships `hps_api.hps`, which
declares the engine's AngelScript API.

**What the mod deliberately does not do.** It copies, decompiles and
redistributes no game source or assets. It quotes no game script. It touches no
DRM, licence check or anti-cheat, and is useless without a purchased copy. It
writes nothing to the game's own files, edits no saves and modifies nothing on
disk beyond its own `.asi`, `.ini` and `.log` alongside the executable. It sends
nothing anywhere: its only socket is a local UDP listener that receives head pose
on port 4242, and it has no outbound network path, telemetry or analytics of any
kind.

**Head tracking is display-only.** The mod changes what the player sees and never
what the game believes the camera is doing, so gameplay, interaction and physics
behave identically with tracking on or off. SOMA is a single-player game and the
mod confers no multiplayer or competitive advantage.

---
