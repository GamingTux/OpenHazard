# Credits & Acknowledgements

**OpenHazard** builds on decades of open-source id Tech 3 and *Elite Force* community engineering. Every upstream component below is gratefully credited to its authors.

## The Game

- **Star Trek: Voyager – Elite Force** — developed by **Raven Software**, published by **Activision**, under license from **Paramount**. All *Star Trek* and *Elite Force* trademarks, characters, and assets are the property of their respective owners. **No retail assets are distributed in this repository.**

## Single-Player Game Module (`game-sp/`)

- Based on the official **Elite Force Single-Player Mod Source SDK**, released by **Raven Software** for community modding (<https://github.com/UberGames/SP-Mod-Source-Code>).
- Licensed under the **STEF Game Source License** (`game-sp/STEF Game Source License.doc`) — this is **not** GPL. Only the 64-bit cross-platform compatibility fixes and export bridge are added on top.
- Thanks to the *"The Dark Project" / Voyager: Insurrection* mod team, whose advocacy led Raven Software to release the SP source SDK.

## Engine & Renderer (`engine/`)

id Tech 3 lineage (**GPLv2**, see [LICENSE](LICENSE)):

- **Quake III Arena** — id Software (<https://github.com/id-Software/Quake-III-Arena>)
- **ioquake3** — the ioquake3 contributors (<https://github.com/ioquake/ioq3>)
- **lilium-voyager** — Zack Middleton — the ioquake3-based *Elite Force* engine port (<https://github.com/clover-moe/lilium-voyager>)
- **Quake3e** — Eugene C. (`ec-`) — the modern Vulkan renderer (`renderervk`) and AArch64 / x86_64 QVM JIT (`vm_aarch64.c`, `vm_x86.c`) (<https://github.com/ec-/Quake3e>)
- **VoyagerNX** — `faithvoid` — the Nintendo Switch port pioneering ARM + console integration (<https://github.com/faithvoid/VoyagerNX>)
- **VoyagerSP-Android** — upstream single-player engine bridge, LP64/ILP32 savegame support, Bink integration, and mobile Vulkan fixes on which OpenHazard's desktop/SBC SP bridge builds.

## Libraries & Tools

- **SDL2** — Sam Lantinga and the SDL contributors (zlib license, <https://libsdl.org>)
- **FFmpeg** (7.1.1) — the FFmpeg team (LGPLv2.1+, minimal static Bink-only build for `.bik` cutscene decoding, <https://ffmpeg.org>)
- **OpenAL Soft** — Chris Robinson and contributors (LGPLv2+, 3D spatial audio)
- **libcurl** — Daniel Stenberg and contributors (MIT/curl license, dynamically loaded for HTTP multiplayer map downloads)
- **libmad** 0.15.1b — Underbit Technologies (GPLv2, MP3 audio decoding)
- **zlib** — Jean-loup Gailly and Mark Adler (zlib license)
- **libjpeg** (jpeg-8c) — the Independent JPEG Group
- **LCC** — Christopher Fraser and David Hanson — retargetable C compiler (`engine/efcode/tools/lcc`)

## References Consulted (Not Bundled)

- **OpenJK** — the JACoders team — reference for 64-bit id Tech 3 / ICARUS behavior (<https://github.com/JACoders/OpenJK>)

## Not Included (Proprietary — Supply Your Own)

- **Retail *Elite Force* game data** (`pak0.pk3`–`pak3.pk3`, `efgamex86.dll`, `.bik` videos) — not redistributed. Provide your own legally owned CD or GOG installation.
- **RAD Game Tools Bink runtime** (`binkw32.dll`) — not redistributed; OpenHazard decodes `.bik` videos natively via LGPLv2.1+ FFmpeg.

## OpenHazard Contributions

- Unified **Singleplayer (`openhazard-sp`)**, **Holomatch Multiplayer (`openhazard-mp`)**, and **Dedicated Server (`openhazard-ded`)** sharing a single 64-bit engine and Vulkan renderer codebase.
- Cross-platform 64-bit support for **Linux, macOS (Apple Silicon & Intel), and Windows (`x86_64` & `ARM64`)**, including universal ARM64 SBC support (Raspberry Pi 4/5 with 4 KB / 16 KB / 64 KB pages, RK3588, etc.).
- Vulkan TBDR & performance optimizations: half-resolution bloom pyramid, zero-load TBDR render pass layouts, zero-shader `vkCmdCopyImage` swapchain fastpath, O(1) pipeline lookup hash table, pure-depth stencil-off mode, low-latency `MAILBOX` presentation, and GPU auto-tuning (`r_gpuAutoTune`).
- Native 16:9 widescreen UI for both Singleplayer and Holomatch Multiplayer, OpenAL 3D audio with SP subtitle/Bink pacing, `libcurl` HTTP fast downloads, and automated 6-target portable CI builds.

---

If any attribution here is incomplete or inaccurate, please open an issue and it will be fixed promptly.
