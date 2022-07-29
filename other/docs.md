## InkBox OS power daemon
#### Explanation, implementation, notes

# Configuration
All configuration is stored in `/data/config/20-sleep_daemon` directory. Those are Files in there
### 1. appList.txt
This is a list of apps that will be freezed ( SIGSTOP ) by the power daemon
at default, its something like this:
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
It will be created as a fallback, but it should be created before

### 2. 1-CinematicBrightnessdelayMs
This file contains an int that determines how long will be the delay between switching one percent of brightness. 

At default its 50. If the file doesn't exist, it will be created

### 3. updateConfig
if this file is updated to `true`, the daemon will update all its variables on the fly, and set the file text to `false`

### 4. 2-cpuGovernor
**Experimental** feature, sets a cpuGovernor at boot, or when variables are updated. at default its `ondemand`. the options are:
- interactive 
- conservative 
- userspace 
- powersave 
- ondemand 
- performance

### 5. 3-WhenChargerSleep
If this file content is `false`, the device will not go to sleep when its charged ( becouse some devices can't ). At default its diffrent for many devices, becouse It happens in devices from 2011 to ~2017 ( n905c n905b n705 n613 n236 n437 KT )

### 6. 4-ChargerWakeUp
If this file content is `true` and if the program detects that between going to sleep, and after suspending the device the charger state changed, it will try to sleep anyway. At default its `false`. If `3-WhenChargerSleep` option is false this function also **should** be false.

### 7. 5-WifiRecconect
If this file content is `true` wifi will be recconected after suspending. At default this option is enabled.

### 8. 6-LedUsage
`true` in this file makes folowing action: Use the power led for indicating that its going to suspend ( Every step in the suspend process changes the led state! ). Also turn it on when the device is charging. Currently only kobo nia is supported

### 9. 7-IdleSleep
This file contains an int that indicates in seconds when to go to sleep. 0 means never, minimum is 15 seconds. Of course the gui handles it in a better way. At default its 60

### 10. 8-CustomCase
This option is complicated, i will try my best explaining it.

if you have a 3D printed case, with a magnet in it there is a big chance that the magnet will put the device in sleep when you flip the case to the back. This option skips the every one magnet call. A bit visualization:
- the device boots up, the case is opened, no magnet contact. This option is enabled.
- You flip the case to the back, magnet is close to the hal sensor and it triggers it. You dont want that. with this option, this call will be ignored
- now you close the case, magnet contacts, the device goes to sleep.

A bit "synchronization" is needed, its a bit complicated to use but it works, and is great ( kobo nia ). Its an option, at default its false, dont care about it :)

### 9-DeepSleep
This file is only readed by the gui, if its set to true ( in the gui settings ) then in the power menu of the ereader Suspend will be replaced by Deep Sleep. If this is clicked, created will be `/run/ipd/sleepCall` with `deepsleep` instead of `sleep` ( as usually it would). In this way inotify will be triggered, and the device will go to sleep, but with extra things, like:
- `powersave` cpu governor ( which will cause the device to slower get out of sleep ) but the battery will live longer.

# Other

### Debugging
if env argument `DEBUG` is set to `true` the program will output logs to stdio and to `/tmp/PowerDaemonLogs.txt`

### Communication
in `/run/ipd` ( which is a tmpfs of size 8K ) is a pipe `fifo`. Its bind mounted between alpine rootfs and the gui rootfs ( to `/kobo/dev/ipd` ). Userapps can access this pipe in `/dev/ipd`, its read only.

Instructions for communication with it:
Messages send in this pipe have always 5 bytes in lenght. Its easier, simpler and simpler. Current messages senden by power daemon:
- `start` - indicates that the device is going to sleep. the application has 300 ms to react to this message. Becouse qt applications will get all touch input when the device is in sleep, which may cause confussion and problems, its advised to show a QDialog with exec(), with stops all touch input until its hide() is called. For code examples look into `src/powerDaemon` in inkbox source code
- `stop0` - called after a device goes back from sleep.

### Screensaver
Any image you put in `/data/onboard/.screensaver` ( yes, thats the directory where you put books ) with the extension `.png` will be randomly choosed and used as a screensaver. Text "sleeping" or image size should be handled by the user itself. Additional notes:
- yes, you create that directory and yes its a hidden directory
- if you want a single screensaver, put there a single image
- if no image is found, the normal screensaver will be used

### Other other notes
- in `/kobo/tmp/currentlyRunningUserApplication` is a file which has the current user application process name, to freeze it
- why create `/data/config/20-sleep_daemon/SleepCall` when you have the pipe? - i would need to create another thread to watch the pipe, which is bad becouse there are already 4 other threads and it gets slowly cpu heavy. I have already the inotify setted up for this directory soo.
- the daemon grabs event0 input device
