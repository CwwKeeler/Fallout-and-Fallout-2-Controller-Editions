# Easy Windows installation

These instructions are for the prebuilt Windows downloads on the GitHub
Releases page. You do not need to compile anything.

## What you need

- A 64-bit Windows PC.
- A legally installed copy of Fallout 1, Fallout 2, or both. Steam and GOG
  versions work.
- A controller supported by Windows. Xbox, PlayStation, Nintendo, 8BitDo, and
  many generic controllers are covered through SDL.

This download contains a replacement game engine. It does not contain the
commercial Fallout game files, so the original game must already be installed.

## Fallout 1

1. On the Releases page, download **fallout1-ce-controller-windows-x64.zip**.
2. Right-click the downloaded ZIP and choose **Extract All**.
3. Find your Fallout installation folder:
   - **Steam:** open your Library, right-click **Fallout**, then choose
     **Manage → Browse local files**.
   - **GOG Galaxy:** select Fallout, click the settings button, then choose
     **Manage installation → Show folder**.
4. Open the extracted folder. Keep opening folders until you can see
   `fallout-ce.exe` and `gamecontrollerdb.txt`.
5. Select everything in that folder and copy it into the Fallout installation
   folder from step 3. Put the files beside `falloutw.exe`, `master.dat`, and
   `critter.dat`—do not put them inside the `data` folder.
6. Connect your controller and double-click **fallout-ce.exe**.

The original `falloutw.exe` remains in place. Use `fallout-ce.exe` whenever you
want to play this Controller Edition.

## Fallout 2

1. On the Releases page, download **fallout2-ce-controller-windows-x64.zip**.
2. Right-click the downloaded ZIP and choose **Extract All**.
3. Find your Fallout 2 installation folder:
   - **Steam:** open your Library, right-click **Fallout 2**, then choose
     **Manage → Browse local files**.
   - **GOG Galaxy:** select Fallout 2, click the settings button, then choose
     **Manage installation → Show folder**.
4. Open the extracted folder. Keep opening folders until you can see
   `fallout2-ce.exe` and `gamecontrollerdb.txt`.
5. Select everything in that folder and copy it into the Fallout 2 installation
   folder from step 3. Put the files beside `fallout2.exe`, `master.dat`,
   `critter.dat`, and `patch000.dat`—do not put them inside the `data` folder.
6. Connect your controller and double-click **fallout2-ce.exe**.

The original `fallout2.exe` remains in place. Use `fallout2-ce.exe` whenever you
want to play this Controller Edition.

## First controls to know

- **Left stick:** move your character. Push partway to walk and fully to run.
- **L3 / left-stick click:** switch between direct movement and point-and-click.
- **Right stick:** pan the camera in gameplay or scroll lists in menus.
- **Bottom face button:** click/select.
- **Right face button:** right-click or change the game cursor mode.
- **Back / Share / Minus:** open the Controller Edition panel.
- **F11:** open the same panel from a keyboard.
- **Alt+Enter:** switch between borderless fullscreen and windowed mode.

The panel contains the full control list, remapping, controller settings,
shortcuts, display mode, and an on-screen keyboard.

## If Windows shows a warning

These executables are community builds and are not digitally signed, so Windows
SmartScreen might say it protected your PC. Confirm that the file came from this
repository's Releases page and optionally verify its SHA-256 value using the
included checksum file. Then choose **More info → Run anyway**. Do not disable
SmartScreen or antivirus protection globally.

## Troubleshooting

**The game says it cannot find its data files**

The Controller Edition executable is probably in the wrong folder. It must be
beside the original game's `master.dat` and `critter.dat`; Fallout 2 also needs
`patch000.dat`. Do not place the executable in the `data` folder.

**My controller does nothing**

Connect it before launching, then try reconnecting it once at the main menu.
Open the controller panel with F11 and check the controller name shown at the
top. Some generic controllers need a mapping added to `gamecontrollerdb.txt`.

**Inputs happen twice or the mouse moves by itself**

Steam Input or another controller mapper may be sending mouse/keyboard input in
addition to the game's native controller input. In the game's Steam controller
settings, use ordinary gamepad output or disable the extra desktop-style mapping.

**The game is stretched, changes resolution, or is hard to Alt-Tab from**

Press Alt+Enter, or open the controller panel and choose **Settings → Display
Mode → Borderless**. Borderless mode uses the current desktop resolution and
keeps the game's aspect ratio with black bars when needed.

**I am upgrading an older Controller Edition build**

Close the game, copy the new release files into the same installation folder,
and allow Windows to replace the old Community Edition executable and support
files. Your saves and original game data are separate from the release archive.
Backing up important saves before installing any game modification is still a
good precaution.

