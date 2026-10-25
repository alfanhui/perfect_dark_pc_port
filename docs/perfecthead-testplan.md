# PerfectHead test plan

Covers the restored PerfectHead feature end to end, on both builds:

- **ROM**: the N64 ROM built from the decomp repo (`perfect_dark_n64`,
  `PERFECTHEAD=1`), run in Mupen64Plus (`tools/emutest`) and on real hardware.
- **Port**: the PC port (`perfect_dark`, `-DPERFECTHEAD=ON`) on Windows,
  Linux and macOS.

A release passes when every case marked for a build passes on that build, or
is listed under "Known failures" in the run record with a reason. The same
document is kept in both repos; change both together.

## How to use this plan

Each case has:

| Field | Meaning |
|---|---|
| **ID** | Area prefix and number, e.g. `ED-07`. Quote it in bug reports and commits. |
| **Builds** | `R` ROM, `P` port, `R+P` both. Hardware-only cases say `HW`. |
| **Run by** | `Auto`: a script decides pass/fail. `Agent`: an AI agent can drive it with the tools below and judge from screenshots or memory reads. `Human`: needs a person (real camera, real hardware, physical paks, judging a likeness, or chance-dependent outcomes). |
| **Steps** | Start from the **Base state** below unless the case says otherwise. Menu names are the in-game text. |
| **Expect** | What must be true. Anything else, a freeze, a crash or garbage on screen, fails. |

Agents: run `Auto` and `Agent` cases, take a screenshot at every **Expect**,
and hand `Human` cases to the user with the exact steps. Don't send input
while the user may be typing in the game window. Never commit ROMs, camera
dumps, `eeprom.bin`, `mpsetups.bin`, `.mpk` files or `pd.ini`.

Humans: tick the case in a copy of the run record (end of this file) and
attach a screenshot or photo for anything that fails.

### Base state

1. A fresh save: no PerfectHead files on any Controller Pak, no loaded heads.
2. An agent file exists (so the Combat Simulator is reachable).
3. Expansion Pak in (ROM) / default memory (port).
4. Camera ready:
   - ROM in emulator: `CAM=1` (camera on controller 1, Controller Paks on
     2–4), `face.avi` as the camera picture.
   - ROM on hardware: Transfer Pak + Game Boy Camera in controller 1,
     Controller Pak in controller 2.
   - Port: `PerfectHead.VirtualCameraController=1`,
     `[Input.Player2] ControllerPak=1`, `Input.FakeGamepads=2` if only one pad;
     `PerfectHead.CameraSource=0` (test card) unless the case says webcam.
5. Path to the root menu: **Combat Simulator → Advanced Setup → Player Setup →
   PerfectHead** (port shortcut: start with `--phead-menu`).

### Regions

Run the full plan on **ntsc-final**. On **pal-final** and **jpn-final** run the
cases tagged `[region]`. JPN has no Shigeru Miyamoto head (15 base heads, not
16).

## Test environment

### Building

| Build | Command | Gate |
|---|---|---|
| Decomp, matching | `make` (IDO, `MATCHING=1`, PerfectHead off) | MD5 equals retail (`matchn64.sh`: ntsc-final `e03b088b6ac9e0080440efed07c1e40f`) |
| Decomp, PerfectHead | `make MATCHING=0 COMPILER=gcc PERFECTHEAD=1 rom` (Docker `buildn64.sh`) | Builds with no new warnings; ROM boots |
| Port, off | `cmake --build build` | Builds; no PerfectHead menu entry |
| Port, on | `cmake -Bbuild-phead -DPERFECTHEAD=ON . && cmake --build build-phead` | Builds on Windows (i686, x86_64), Linux, macOS |

### Tools

- **ROM in Mupen64Plus**: `tools/emutest` in the decomp repo (see its README).
  - `emu.sh <rom> [savestate]` (`CAM=1` for the camera, `DBG=1` for the debugger).
  - `keys.sh` presses keys (controller 1: Return=Start, Shift_L=A, Control_L=B,
    arrows=stick, `i j k l`=C buttons, `x`/`c`=L/R; `;` pulls and replaces
    controller 2's pak).
  - `shot.sh` takes screenshots, `boot.sh` makes a boot sheet.
  - `dsend.sh`/`dlog.sh`/`fault.sh` read memory and the faulted thread.
  - `mkface.sh` makes `face.avi`; `mkalbum.py` writes a camera save with
    numbered pictures, one deleted, in mixed slot order.
  - Supply your own `camera.gb` dump in the data folder (never in the repo).
- **Port**:
  - `--phead-menu` opens PerfectHead from the main menu.
  - `--phead-texdump` writes base head textures (for `tools/pheadmask`).
  - Saves, `controllerpak<N>.mpk` and `gbcam.sav` are in the save folder.
- **Mask tool**: `tools/pheadmask/pheadmask.html` (port repo).

### Known tool limits

- Mupen menus drop input sent faster than ~1.2 s per key; even then a key is
  sometimes lost. Check a screenshot after each step and retry the step, not
  the whole case.
- Files saved to Controller Pak 2 under `CAM=1` have not been found after an
  emulator restart. Cases that need a restart (`SAVE-09`, `MP-08`) are run on
  the port or on hardware, or by carrying the `.mpk` with `carrysaves.sh`.
- Mupen's pak switch (`;`) removes the pak reliably, but re-inserting has not
  been seen to work. Cases that need re-insertion are `Human` on hardware or
  `Agent` on the port.
- Mupen64Plus does not appear to apply the camera's exposure, gain,
  reference voltage or offset, so the emulator can't show exposure or black
  level problems (`PIC-12`, `PIC-13`, `CAM-16`); use hardware, and the port,
  whose virtual camera models them.
- The Rice video plugin draws album thumbnails striped; use
  `GFX=mupen64plus-video-glide64mk2` for `PIC-` album cases.
- Menu text types itself in over a few seconds; wait before screenshots.

## 1. Build and regression gates

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| BLD-01 `[region]` | R | Auto | Matching build of each region with PerfectHead off | MD5 equals that region's retail ROM |
| BLD-02 `[region]` | R | Auto | `PERFECTHEAD=1` build of each region (gcc: ntsc-final and ntsc-1.0 only) | Builds with no warnings in PerfectHead files; boots through the attract sequence with no exception or frozen screen. Sheets are for a person to look at; a gcc build's attract differs from retail even without PerfectHead |
| BLD-03 | P | Auto | Port builds, PerfectHead on and off, Windows x86_64 + i686 | Both build; only the "on" build has the PerfectHead symbols; CI (`.github/workflows/c-cpp.yml`) green on Linux and macOS |
| BLD-04 | R+P | Agent | PerfectHead off: Combat Simulator → Player Setup | No PerfectHead entry; menu identical to retail |
| BLD-05 | R+P | Agent | PerfectHead on, an existing EEPROM/game save from a build without PerfectHead | Save loads; agent stats, unlocks and settings unchanged |
| BLD-06 | R+P | Human | Play one solo mission and one 2-player Combat Simulator match with no PerfectHeads loaded | No change from the build without PerfectHead (framerate, heads, menus) |

## 2. Entry and access

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| ENT-01 | R+P | Agent | Open Player Setup | Items: Name, Character, **PerfectHead**, Control, Player Options, Statistics, Load Player… |
| ENT-02 | R | Agent | Boot without the Expansion Pak (emulator: 4 MB RAM) | PerfectHead entry is hidden; no crash anywhere in Player Setup |
| ENT-03 | R+P | Agent | Select PerfectHead with no loaded heads | Root: Load A Saved Head, Make A New Head, Cancel; **Edit A Loaded Head hidden** |
| ENT-04 | R+P | Agent | Root with at least one loaded head | Edit A Loaded Head shown |
| ENT-05 | R+P | Agent | 2 players. Player 1 opens PerfectHead; player 2 then selects PerfectHead | Player 2 gets "PerfectHead is in use by <player 1's name>." and can't enter |
| ENT-06 | R+P | Agent | After ENT-05, player 1 backs out of PerfectHead; player 2 tries again | Player 2 gets in |
| ENT-07 | R+P | Agent | Player 1 in PerfectHead, then player 1's menu is closed another way (e.g. starting the match from another player) | The lock is released: another player can enter afterwards |
| ENT-08 | P | Auto | Start the port with `--phead-menu` | Camera Setup opens from the main menu |

## 3. Camera detection

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| CAM-01 | R+P | Agent | Make A New Head → usage → **no Transfer Pak** on any controller (port: `VirtualCameraController=0`) | "Error": "Game Boy Camera Not Detected!" / check connections; OK returns; no freeze |
| CAM-02 | R | Human (HW) | Transfer Pak with no cartridge | Same error as CAM-01 |
| CAM-03 | R | Agent | Transfer Pak with a non-camera Game Boy game (emulator: replace `camera.gb` with any non-0xFC cart) | Same error as CAM-01 |
| CAM-04 | R | Agent | Camera dump re-headered to title `PHOTO`, licensee `00`, type `0xFC` (checksum fixed) | Detected; Take Picture works (homebrew camera rule) |
| CAM-05 | R | Agent | Camera dump re-headered to type `0x1B` (MBC5) | "Not Detected" |
| CAM-06 | R | Human (HW) | Each physical camera available: Game Boy Camera (any colour), Gold/Zelda, Pocket Camera (JP), reflashed Photo!/PXLR | Each detected, pictures taken |
| CAM-07 | R | Human (HW) | Two cameras in two Transfer Paks | "More than one camera detected." → Choose Camera lists both; the chosen one is used |
| CAM-08 | R+P | Agent | During "Calibrating Camera" | Banner shown on the menu of the **player using PerfectHead** (not the camera's controller number), preview empty, Shoot! disabled, Cancel works |
| CAM-09 | R | Human (HW) | Pull the Transfer Pak out during Take Picture, then Cancel | No freeze; error or return to the menu; re-entering works after re-inserting |
| CAM-10 | P | Agent | `CameraSource=0` | Test card shown as the camera picture |
| CAM-11 | P | Human | `CameraSource=1` with a webcam (Windows, Linux, macOS each) | Live picture of the user; greyscale, cropped to the camera's shape |
| CAM-12 | P | Agent | `CameraSource=1`, SDL3 library missing or `SDL3Library` pointing at nothing | Falls back to the test card; no crash; a log line says why |
| CAM-13 | P | Human | `CameraDevice` = second webcam; `CameraMirror=1` | Second camera used; picture mirrored |
| CAM-14 | P | Human | Leave PerfectHead | Webcam light goes off (camera closed when idle) |
| CAM-15 | P | Human | macOS first run | OS camera permission prompt; while waiting the preview stays empty, then works when allowed; denying gives the test card |
| CAM-16 | P | Agent | New `gbcam.sav` (the port writes the calibration record a real camera has), Take A Picture Now with the test card | Live picture with normal blacks and whites, not white all over: the sensor's V and O are the camera's own (`$03`, `$A5`), not taken from the calibration vector |

## 4. Usage question

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| USE-01 | R+P | Agent | Make A New Head | "Where do you want to be able to use this head?" with Combat Simulator / All Missions / Both |
| USE-02 | R+P | Agent | Finish a head with each answer, then Head Complete! | Combat: only the "character menu" lines; All Missions: only the "Mission heads" lines; Both: both |
| USE-03 | R+P | Agent | Load A Saved Head on a file | Asked the usage again; the new answer applies (MP-01, SOLO-01) |
| USE-04 | R+P | Agent | Load the same saved head twice, answering All Missions, then Combat Simulator; screenshot the editor's head each time | The same face both times: the answer doesn't change how the picture is drawn (it isn't stored in camdraw's picture settings) |

## 5. Taking and loading pictures

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| PIC-01 | R+P | Agent | Camera menu | Take A Picture Now, Load Picture From Camera (split screen: Take Picture, Load From Camera) |
| PIC-02 | R+P | Agent | Take A Picture Now, wait for calibration | Live picture with the face guide (outline, eye line, mouth line) |
| PIC-03 | R+P | Agent | Shoot! | Keep Picture / Try Again; the frozen picture shown |
| PIC-04 | R+P | Agent | Try Again | Back to the live picture |
| PIC-05 | R+P | Agent | Keep Picture | Editor opens; head painted with the picture |
| PIC-06 | R+P | Agent | In the editor: Take Another Picture → Cancel | Head and picture are the **previous** picture, not the last live frame |
| PIC-07 | R+P | Agent | Take Another Picture → Shoot! → Keep | New picture on the head; position, colours and depth kept |
| PIC-08 | R | Agent | Album from `mkalbum.py`, Load Picture From Camera (Glide64) | List runs 1, 2, 3… in album order; the deleted picture never shown |
| PIC-09 | R+P | Agent | Pick an album picture → OK | Editor with that picture |
| PIC-10 | R+P | Agent | Empty album (blank `camera.sav` / port with no `gbcam.sav`) | List empty, OK disabled, Cancel works; no crash |
| PIC-11 | P | Agent | Put a real camera save as `gbcam.sav` in the save folder | Its pictures listed and loadable |
| PIC-12 | R+P | Human | Take pictures in bright, dim and back-lit rooms (port: webcam) | Calibration finds a usable exposure; the face is recognisable on the head |
| PIC-13 | R (HW) | Human (HW) | Take A Picture Now in a bright room, by a window and outdoors in shade; then in a room lit only by a lamp | After calibration the picture settles to a normal brightness within a few seconds and stays there: not white in bright light (the reported bug), not black in the dark. Moving between light and shade, it recovers |
| PIC-14 | P | Agent | Take A Picture Now with `CameraSource=0` (test card) | Normal brightness, not white: the virtual camera follows Pan Docs' exposures, not the game's default |

## 6. Editor

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| ED-01 | R+P | Agent | Open the editor | Items: Take Another Picture, Change Head, Position Picture, Color Face & Hair, Shape Head, Remove Head, Cancel Changes, Keep Changes; the head turns beside the menu |
| ED-02 | R+P | Human | Make five new heads | The first base head differs between runs (random, not always the first). Chance-dependent: a human judges |
| ED-03 | R+P | Agent | Change Head: scroll the whole carousel | 13 standard heads, plus Elvis / dataDyne Sniper only when unlocked, plus Miyamoto (not JPN); OK applies, Cancel restores |
| ED-04 `[region]` | R+P | Agent | Fresh save (nothing unlocked) | Elvis and the dataDyne Sniper not offered |
| ED-05 | R+P | Human | Unlock Elvis (`MPFEATURE_CHR_ELVIS`) and the dataDyne characters in normal play, then Change Head | Both offered from then on |
| ED-06 | R+P | Agent | A head saved with Elvis as base, loaded on a save where Elvis is locked | Loads and keeps Elvis; carousel position doesn't break |
| ED-07 | R+P | Human | Paint each base head with the same picture | Face covers the skin below the hair; no picture over the hairline; no hard edge at the hair; sides blend with the face |
| ED-08 | R+P | Human | Elvis | Picture on his skin in greys, eyes his own, face sits where his face is |
| ED-09 | R+P | Human | dataDyne Sniper | Face under the purple visor; helmet untouched |
| ED-10 `[region]` | R+P | Agent | JPN build: Change Head | No Miyamoto; no gap or blank entry |
| ED-11 | R+P | Agent | Position Picture | Picture in a dark surround, crop frame with arrows and corner marks pulsing until first moved; head bobs at the editor's speed |
| ED-12 | R+P | Agent | C buttons, each direction | Frame moves; that side's arrow lights |
| ED-13 | R+P | Agent | L, R | L shrinks the frame, R grows it |
| ED-14 | R+P | Agent | Move the frame past every edge | Stops 16 texels outside the picture; outside the picture the head keeps its own skin; picture fades in at the edge |
| ED-15 | R+P | Agent | Brightness slider to both ends | Face darkens/lightens on the head |
| ED-16 | R+P | Agent | Position changes → Cancel | Frame and brightness back as before |
| ED-17 | R+P | Agent | Color Face & Hair: each Face (Style 1–4) and each Hairstyle (Normal, Blonde, Black, Auburn, Blue Rinse) | Head recoloured each time; Normal keeps the base head's hair; Cancel restores |
| ED-18 | R+P | Agent | Shape Head: Depth both ends, OK; again → Cancel | Face flatter/fuller; Cancel restores |
| ED-19 | R+P | Agent | Remove Head → Delete | Slot emptied; back at root; Edit A Loaded Head hidden if it was the last |
| ED-20 | R+P | Agent | Remove Head → Cancel | Head kept |
| ED-21 | R+P | Agent | Edit A Loaded Head → move the crop frame, change Depth and the hair colour → Cancel Changes → Throw Away → Edit A Loaded Head → Position Picture, Shape Head | Head as before entering the editor, crop frame and depth included (new head: slot empty) |
| ED-22 | R+P | Agent | Cancel Changes → Cancel | Still in the editor with changes |
| ED-23 | R+P | Agent | Head loaded from a pak in a session where the camera never started | Take Another Picture hidden; everything else works |
| ED-24 | R+P | Agent | Head bob: compare the editor and Position Picture side by side over 10 s | Same speed (one step per frame, not two) |
| ED-25 | R+P | Agent | Edit A Loaded Head → Brightness slider → OK → B | **Save PerfectHead**: "You have edited this head." / "Do you want to keep your changes?", with Keep and Revert |
| ED-26 | R+P | Agent | ED-25, then Keep | As Keep Changes: Save PerfectHead ("Your changes will be kept in memory" for a head on a pak, otherwise "You can now save your head") → Head Complete!; the head keeps the new brightness |
| ED-27 | R+P | Agent | ED-25 moving the crop frame instead, then Revert; Edit A Loaded Head → Position Picture | Revert leaves the editor to the menu it was opened from; the crop frame is where it was before the editor opened |
| ED-28 | R+P | Agent | ED-25, then B | Back in the editor with the change still showing; B again asks again |
| ED-29 | R+P | Agent | Edit A Loaded Head → B, changing nothing | Leaves the editor with no prompt |
| ED-30 | R+P | Agent | Edit A Loaded Head → Position Picture → move the frame → Cancel → B; repeat with Color Face & Hair, Shape Head and Change Head | No prompt each time (a cancelled change isn't a change) |
| ED-31 | R+P | Agent | Edit A Loaded Head → one change in each of Change Head, Color Face & Hair, Shape Head, and Take Another Picture → Keep Picture (separately) → B | Prompt each time |
| ED-32 | R+P | Agent | Make A New Head → Keep Picture → change something → B | No prompt; leaves as before, the new head not kept |
| ED-33 | R+P | Agent | Make A New Head → Keep Changes → Don't Save → Edit Head → change something → B | Prompt (the head is loaded now) |
| ED-34 | R+P | Agent | Load A Saved Head → change something → B → Keep | Prompt, then "Your changes will be kept in memory" (head is on a pak) |

## 7. Saving

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| SAVE-01 | R+P | Agent | New head → Keep Changes | "Save PerfectHead": "You can now save your head on to a Controller Pak…", Save / Don't Save |
| SAVE-02 | R+P | Agent | Save | Select Location: Game Pak "Full", the Controller Pak with free space shown |
| SAVE-03 | R+P | Agent | Choose the Controller Pak | Saved; Head Complete!; Finish returns to the root |
| SAVE-04 | R+P | Agent | Don't Save | Head Complete!; head stays loaded (in Character list) |
| SAVE-05 | R+P | Agent | Load a saved head, change hair → Keep Changes | "Your changes will be kept in memory. Would you like to save your changes to the Controller Pak?" Save writes over the same file (no new file) |
| SAVE-06 | R+P | Agent | Save with no Controller Pak anywhere | Only Game Pak "Full"; can't save; backing out keeps the head in memory |
| SAVE-07 | R+P | Agent | Save to a full Controller Pak (fill it with other notes first) | Pak shown as full / error "Error Saving PerfectHead" with Save Elsewhere; nothing corrupted |
| SAVE-08 | R | Human (HW) / P Agent | Pull the Controller Pak during the save | Error dialog; no freeze; the pak's other notes still readable afterwards |
| SAVE-09 | R (HW) + P | Human / Agent | Save, power off, power on, Load A Saved Head | File listed with its thumbnail; loads the same picture, base head, position, colours and depth |
| SAVE-10 | R+P | Agent | Save three heads | Each its own file; each loads back as itself |
| SAVE-11 | R+P | Auto (P) | Port: run the `.mpk` checker over `controllerpak2.mpk` after saves | ID and table checksums valid, no leaked pages |
| SAVE-12 | R+P | Human | Move a `.mpk` between port, emulator and a real pak (via a pak dumper) | Heads load on all three |
| SAVE-13 | P (macOS) | Human | Save a new head to the Controller Pak in controller 2, then open Load A Saved Head | No crash (the stack protector stopped the game here before the block buffer fix); the head is listed |

## 8. Loading

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| LOAD-01 | R+P | Agent | Load A Saved Head with files on paks 2 and 3 | Files grouped under each pak's name, thumbnails shown ("1/2", "2/2"), the highlighted one framed |
| LOAD-02 | R+P | Agent | Move through the list | The 3D head above the list changes to the highlighted file (after a short pause); first picture not cut off when the list opens |
| LOAD-03 | R+P | Agent | No files on any pak | "<none>"; no head; Cancel works |
| LOAD-04 | R | Agent (remove) / Human (re-insert) | Pull the pak while the list is open; put it back | Head preview cleared, list updates; no freeze; re-inserted pak lists again |
| LOAD-05 | R+P | Agent | 3 or 4 players: Load list | Thumbnails only, no 3D head; list two rows high |
| LOAD-06 | R+P | Agent | Load with all 4 slots full | "Edit A PerfectHead: Choose slot to edit:" with each slot's picture; the chosen slot is replaced |
| LOAD-07 | R+P | Agent | Load a head in a session where the camera never ran | Loads; no crash (no capture buffer) |
| LOAD-08 | R+P | Human | Load a file damaged on purpose (port: corrupt bytes in the `.mpk` note) | "Error Loading PerfectHead"; no crash; other files still load |

## 9. Loaded head slots

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| SLOT-01 | R+P | Agent | Make 4 new heads | All 4 in Edit A Loaded Head with pictures |
| SLOT-02 | R+P | Agent | Make a 5th | Asked which slot to replace |
| SLOT-03 | R+P | Agent | Edit A Loaded Head on each slot | Opens the editor on that head; "<none>" slots can't be edited |
| SLOT-04 | R+P | Agent | Load and play a stage, return to menus | All loaded heads still there with their pictures |

## 10. Combat Simulator

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| MP-01 | R+P | Agent | Head with usage Combat Simulator; Player Setup → Character | PerfectHeads after the usual heads, each shown painted; choose one |
| MP-02 | R+P | Agent | Head with usage All Missions only | Not offered in the Character list |
| MP-03 | R+P | Agent | Simulants → Add Simulant… → Character… → Simulant Character | PerfectHeads offered; choose one |
| MP-04 | R+P | Agent | Start a match with a player and two Simulants wearing PerfectHeads (two the same) | Faces correct in play, from every angle; correct in death animations and on the post-match screens |
| MP-05 | R+P | Human | 4-player split screen, every player and 4 Simulants wearing PerfectHeads | No missing textures, no crash, framerate comparable to plain heads |
| MP-06 | R+P | Agent | Remove a head that a player is wearing, then open Character / start a match | Falls back to a normal head; no crash |
| MP-07 | R+P | Agent | Save Game Setup with a Simulant wearing a PerfectHead; load it with a different head in that slot / an empty slot | Uses the head in that slot / falls back to Joanna; no crash |
| MP-08 | R (HW) + P | Human / Agent | Save Player with a PerfectHead; restart; Load Player with the pak in | The PerfectHead is reloaded from the pak and worn |
| MP-09 | R+P | Agent | Load Player with the head's pak removed | Normal head; no crash |
| MP-10 | R+P | Agent | Character with a PerfectHead, on Joanna Combat (the body with the most textures, 34), CIA Agent and Pelagic II Guard (when unlocked; 32 and 31) and Joanna Leather (20) | The painted face in the preview on every body, the same as on the head carousel |

## 11. Solo missions

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| SOLO-01 | R+P | Human | 1–4 heads with usage All Missions; play dataDyne Central: Defection | Guards wear the faces; which guards get them is random, so a human checks |
| SOLO-02 | R+P | Human | Heads with male and female base heads | Each face only on guards of its base head's sex |
| SOLO-03 | R+P | Human | Count guards in one area | At most about half of the guard faces are PerfectHeads |
| SOLO-04 | R+P | Human | Play three different missions back to back | Faces on every mission; no texture corruption, no crash |
| SOLO-05 | R+P | Agent | Power off/on, play a mission without loading heads | No PerfectHead guards (mission heads last one session) |
| SOLO-06 | R+P | Human | Co-Operative with two players and four mission heads | Faces appear; no crash |
| SOLO-07 | R+P | Agent | Usage Combat Simulator only | No PerfectHead guards |

## 12. Split screen menus

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| SPL-01 | R+P | Agent | 2 players, player 2 runs the whole flow (new head → save → load) | Sentences word-wrapped, including the Keep / Revert prompt (`ED-25`); head centred in player 2's dialog, not off screen; Position Picture's Brightness bar and value inside the dialog |
| SPL-02 | R+P | Agent | 3 and 4 players, player 3 then player 4 run the flow | Pictures and head shrink to fit; Shoot! visible under the picture; editor title not scrolled away; Load list fits; with four players, Position Picture's Brightness bar and value inside player 4's dialog and the Keep / Revert prompt word-wrapped |
| SPL-03 | R+P | Agent | Camera on controller 1, player 3 takes the picture | Calibration banner on player 3's quarter |
| SPL-04 | R+P | Agent | Quarter screen: Save PerfectHead and Head Complete! (Both) | Scroll to the focused choice; all choices reachable |

## 13. Performance and memory

| ID | Builds | Run by | Steps | Expect |
|---|---|---|---|---|
| PERF-01 | R | Agent | Editor open 5 minutes, changing settings repeatedly (emulator `DBG=1` heap stats) | No memory growth; no freeze |
| PERF-02 | R | Human (HW) | 4-player Combat Simulator, 4 PerfectHeads on players + Simulants, a large arena | Framerate within ~10% of the same match with normal heads; no "out of memory" crash |
| PERF-03 | R | Human (HW) | Solo mission with 4 mission heads on the largest stage (e.g. Area 51) | Stage loads; no texture dropouts |
| PERF-04 | P | Human | Same as PERF-02 on a low-end PC | No frame time regression you can feel |

## 14. Real hardware sign-off

Run on a real N64 with an EverDrive (or similar), Expansion Pak, a real Transfer
Pak and camera, and a real Controller Pak:

`CAM-02`, `CAM-06`, `CAM-07`, `CAM-09`, `PIC-02`–`PIC-09`, `PIC-12`, `PIC-13`, `ED-01`,
`ED-11`–`ED-18`, `SAVE-01`–`SAVE-09`, `LOAD-01`, `LOAD-04`, `MP-04`, `MP-05`,
`MP-08`, `SOLO-01`, `PERF-02`, `PERF-03`.

## Automation: what's possible

| Level | What | Feasible now? | How |
|---|---|---|---|
| **L0 Build gates** | `BLD-01`–`BLD-03` | Yes, done | Decomp: `tools/buildgate/buildgate.sh` (Docker; see its README). Port: `tools/buildgate.sh`. Each prints PASS/FAIL/SKIP per case and exits non-zero on a failure. The ROM gates need the user's ROM, so they run locally only. |
| **L1 Unit / golden tests (port)** | Painter, masks, Elvis mapping, crop clamping, file compress/decompress, `.mpk` emulation | Yes, with a small harness | A test executable linking `phead.c`, `camdraw.c` and `port/src/mempak.c` with a fixed 128×112 picture. Hash the painted textures per base head and compare with stored hashes (golden files hold only hashes, not game data). Needs the base head textures, so it reads the user's ROM at run time like the port does. The existing `.mpk` checker covers `SAVE-11`. |
| **L2 Scripted UI flows** | Most `Agent` cases | Partly | ROM: `emu.sh` + `keys.sh` + `shot.sh`, one savestate per starting point (root menu, editor, Position Picture, Load list) so a dropped key costs one step. Check results by reading game state with `DBG=1` (e.g. the editor's base head, crop and hair values, slot GUIDs) rather than by pixels. Port: same idea with an input-replay option (not built yet) and a state dump on exit. |
| **L3 Human** | Real cameras, hardware, likeness, random placement, framerate feel | No | Hand to a person with the steps in this plan. |

Recommended next steps, in order:

1. ~~L0 script for both repos~~ (done: `tools/buildgate/buildgate.sh` in the decomp, `tools/buildgate.sh` in the port).
2. Port L1 harness for the painter and save format (catches the bugs that
   matter most, and runs in CI on all three OSes).
3. Savestate library + memory-read checks for the emulator, turning the most
   used `Agent` flows (`PIC-05`, `ED-12`–`ED-18`, `ED-25`–`ED-28`, `SAVE-03`,
   `LOAD-02`) into
   repeatable scripts.
4. A port input-replay option (`--input-replay <file>`) so the same flows run
   on the port without a person.

## Run record

Copy this section per run.

```
Date:            Tester:            (human / agent)
Commit (port):   Commit (decomp):
Build:           ROM region:        Emulator/hardware:      Port OS:
Camera:          Paks:

Passed:
Failed (ID, what happened, screenshot):
Known failures accepted (ID, reason):
Not run (ID, reason):
```
