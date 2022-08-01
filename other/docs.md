# InkBox OS power daemon
#### Explanation, implementation, notes

## Configuration
All configuration is stored in the `/data/config/20-sleep_daemon` directory. Below is a list and description of the files that are present in that directory.
### `appsList`
This is a list of apps that will be frozen (`SIGSTOP`) by the power daemon by default before going to sleep, it's something like this:
```
inkbox-bin
oobe-inkbox-bin
lockscreen-bin
calculator-bin
qreversi-bin
2048-bin
scribble
lightmaps
```
It will be created as a fallback, but it should be created before.

### `1-cinematicBrightnessDelayMs`
This file contains an integer that determines how long will be the delay between switching one percent of brightness.
<br>
By default, it's `50`. If the file doesn't exist, it will be created.

### `2-cpuGovernor`
**Experimental** feature, sets a cpuGovernor at boot, or when variables are updated.
<br>
By default, it's `ondemand`. the options are:
- `interactive`
- `conservative`
- `userspace`
- `powersave`
- `ondemand`
- `performance`

### `3-whenChargerSleep`
If this file's content is `false`, the device will not go to sleep when it's charged (because some devices can't). By default it's different for many devices, because it happens in devices from 2011 to ~2017 (N905B, N905B, N705, N613, N236, N437, KT )

### `4-whenChargerWakeUp`
If this file's content is `true` and if the program detects that between going to sleep and after suspending that the device's charger state changed, it will try to sleep anyway.
<br>
By default it's set to `false`. If the `3-whenChargerSleep` option is set to `false`, then this option **should** also be false.

### `5-wifiReconnect`
If this file's content is `true`, Wi-Fi will be reconnected on wake-up.
<br>
By default, this option is set to `true`.

### `6-ledUsage`
If this file's content is `true`, the following will happen: the power LED will light up for indicating that the device going to sleep. The blinking goes as follows:
- first, slow led blinking indicates invidual steps in preparing to sleep (turn off wifi, show screensaver etc)
- fast blinking indicates write to state-extended, so all devices components are preparing to sleep
- slow blinking ( but faster than step 1 ) indicates that the device is trying to sleep, but a device component ( touchscreen propably ) refused too. thats normal behavior so don't worry

It will also turn it on when the device is charging and will be turned off when the device finishes charging.

### `7-idleSleep`
This file contains an integer that determines, in seconds, when to go to sleep when no touch input is received from the screen. `0` means 'Never', minimum is `15` seconds.
<br>
By default, it's set to `60`.

### `8-customCase`
This option is complicated, I will try my best to explain it.
<br>
If you have a 3D-printed case with a magnet in it, there is a big chance that the magnet will put the device to sleep when you flip the case to the back. This option skips the every one magnet call.
<br>
A little visualization:
- The device boots up, the case is opened, no magnet contact. This option is enabled.
- You flip the case to the back, magnet is close to the hall sensor and triggers it. You don't want that. With this option, this call will be ignored.
- Now you close the case, magnet contacts, the device goes to sleep.

A bit of synchronization is needed, it's a bit complicated to use but it works, and is great (on the Kobo Nia). It's just an option, don't care about it if you don't want to :)
<br>
By default, it's set to `false`.

### `9-deepSleep`
This file is only read by the GUI. If it's set to `true` (from the GUI power daemon settings) the power menu option of the eReader called 'Suspend' will be replaced by 'Deep sleep'. If this is clicked, `/run/ipd/sleepCall` will be created and `deepSleep` will be written to it instead of `sleep` (as it usually would). In this way inotify will be triggered, and the device will go to sleep, but with extra things, like:
- `powersave` CPU governor, which will extend the time the device takes to wake-up, but the battery will live longer.

### `updateConfig`
If `true` is written to this file, the daemon will update all its variables on-the-fly, then write `false` to this file.

## Other

### Debugging
If the environment variable argument `DEBUG` is set to `true`, the daemon will output logs to stdio and `/var/log/ipd.log`.
<br>
You can also enable debug logging from the OS itself, by creating `/boot/flags/IPD_DEBUG` with the value 'true' in it.

### Communication
In `/run/ipd` (which is an 8K temporary filesystem) is a named pipe called `fifo`. It's bind-mounted between the main root filesystem and the GUI's root filesystem (to `/kobo/dev/ipd`). User applications can access it at `/dev/ipd/fifo`, it's read-only.
#### Instructions for communicating with it:
Messages sent in this named pipe always have a length of 5 bytes. It makes them easier to parse.
<br>
Current messages sent by the power daemon:
- `start` indicates that the device is going to sleep. The application has 300 ms to react to this message. Because Qt applications get all touch input at wake-up even if the device is suspended (buffer), which may cause confusion and problems, it's advised to show a QDialog with `exec()`, which eats all touch input until its `hide()` is called. For code examples look into `src/widgets/powerDaemon` in InkBox source code.
- `stop0` is called after a device wakes up.

### Screensaver
Any image you put in `/data/onboard/.screensaver` (yes, `/data/onboard` is the directory where you would usually put books) with the extension `.png` will be randomly chosen and used as a screensaver. The "Sleeping" text or the image's size needs to be handled by the user itself. Additional notes:
- Yes, you create that directory and yes, it is a hidden directory
- If you want a single screensaver, put only one image there
- If no image is found, the normal screensaver will be used

### Miscellanous notes
- `/kobo/tmp/currentlyRunningUserApplication` is a file which has the current user application process name, to freeze it
- "Why use `sleepCall` when you have the named pipe?" I would need to create another thread to watch the named pipe, which is bad because there are already 4 other threads and it gets slowly CPU-heavy.
- The daemon grabs the `/dev/input/event0` input device for sanity reasons.
