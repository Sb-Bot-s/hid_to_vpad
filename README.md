[Current Stable Release](https://github.com/Maschell/hid_to_vpad/releases/latest) | [Nightly builds](https://github.com/Maschell/hid_to_vpad/releases) | [Issue Tracker](https://github.com/Maschell/hid_to_vpad/issues) | [Support Thread](https://gbatemp.net/threads/hid-to-vpad.424127/)
# HID to VPAD  (WUPS plugin) [![Build Status](https://api.travis-ci.org/Maschell/hid_to_vpad.svg?branch=wups)](https://travis-ci.org/Maschell/hid_to_vpad)


This is a plugin for the [Wii U Plugin System (WUPS)](https://github.com/Maschell/WiiUPluginSystem/) that let you use a HID device on the WiiU. <br />
It's based on the [controller_patcher](https://github.com/Maschell/controller_patcher/tree/wut) engine. 

# Wii U Plugin System
This is a plugin for the [Wii U Plugin System (WUPS)](https://github.com/Maschell/WiiUPluginSystem/). To be able to use this plugin you have to place the resulting `.mod` file in to the following folder:

```
sd:/wiiu/plugins
```
When the file is placed on the SDCard you can load it with [plugin loader](https://github.com/Maschell/WiiUPluginSystem/).

## Usage
Start place the .mod file into the WUPS plugin folder `sd:/wiiu/plugins` and start the plugin loader.

Place the config files into the following folder:
```
sd:/wiiu/controller
```

To set the controller mapping you need to open the WUPS configuration menu. When the plugin is loaded and active, you can open the menu via the button combo **L, DPAD DOWN and MINUS**.  
(Note: The config menu only opens at places where the home menu is allowed).

## Supported devices
The official GC Adapter, PS3/PS4 Pad, Mouse, Keyboard have built in support. Other devices can be added when a valid config file in provided.  
Check out the [controller_patcher](https://github.com/Maschell/controller_patcher) repository for more details.  

You can find deep information for creating own config files, the default mapping, in the [controller patcher wiki](https://github.com/Maschell/controller_patcher/wiki)

Default button mapping:  
- [Mouse](https://github.com/Maschell/controller_patcher/wiki/3.-Mouses#default-configuration)
- [Keyboard](https://github.com/Maschell/controller_patcher/wiki/4.-Keyboards#default-configuration)
- [GameCube](https://github.com/Maschell/controller_patcher/wiki/5.a-Controller-%7C-Configurate-the-GameCube-controller#default-button-mapping)
- [Dualshock 3](https://github.com/Maschell/controller_patcher/wiki/5.b-Controller-%7C-Configurate-the-Dualshock-3-controller#default-button-mapping)
- [Dualshock 4](https://github.com/Maschell/controller_patcher/wiki/5.c-Controller-%7C-Configurate-the-Dualshock-4-controller#default-button-mapping)

In combination with the Network Client, more controllers are supported. You can find more information [here](http://gbatemp.net/threads/hid-to-vpad-network-client.466150/).

# FAQ

### What about XBOX controllers?
Xbox controllers are not HID devices. But it can be used in combination with the [Network Client](http://gbatemp.net/threads/hid-to-vpad-network-client.466150/)!

### Is my controller supported?
Take a  look at this [repo](https://github.com/Maschell/controller_patcher_configs)

### ???
Do you have another question? First take a look at:
- the [controller_patcher repo](https://github.com/Maschell/controller_patcher)
- the thread on [gbatemp](http://gbatemp.net/threads/hid-to-vpad.424127/)
- the [wiki](https://github.com/Maschell/controller_patcher/wiki)  

If you don't find an anwser, please open an issue.

# Building
In order to build this application you need serval libs:

- [wut](https://github.com/decaf-emu/wut)
- [libutilswut](https://github.com/Maschell/libutils/tree/wut) for common functions.
- [lcontrollerpatcherwut](https://github.com/Maschell/controller_patcher/tree/wut) to emulate the controllers.

Install them (in this order) according to their README's. Don't forget the dependencies of the libs itself.

## Building via docker

```
# Build docker image (only needed once)
docker build . -t hid-to-vpad-plugin-builder

# make 
docker run -it --rm -v ${PWD}:/project hid-to-vpad-plugin-builder make

# make clean
docker run -it --rm -v ${PWD}:/project hid-to-vpad-plugin-builder make clean
```


# Credits
- A big thanks goes out to <b>dimok</b> for creating the HBL, the dynamic libs and every stuff he made. The "environment" of this app is copied from ddd, turned out to be a "hello world" with useful extra stuff.  
- Also huge thanks to <b>FIX94</b> who initally created his gc-to-vpad. Helped me a lot! Thanks!  
- And of course big thanks to everyone who has helped me testing! (dimok, dibas, EclipseSin,FunThomas,n1ghty etc.)  

## Python keyboard network client (experimental)

This branch adds a lightweight network path for testing input without the Java Network Client and without a physical XInput controller.  The Wii U side still uses the existing HID to VPAD network server, but the client is now a plain Python script that can run on Linux, macOS, Windows terminals, or Android through Termux.

### Wii U setup

1. Keep **Network Input Server** enabled in the WUPS configuration menu. It defaults to **On** and is started automatically when the plugin initializes.
2. Copy the virtual keyboard config to the SD card:

```sh
cp controller_configs/python_keyboard_xinput.ini sd:/wiiu/controller/
```

The config uses VID `0x7331` and PID `0x1337`. Those values match the compact report format emitted by the Python client; no physical XInput pad is required.

### Running the Python client

From the computer or Android/Termux device on the same network as the Wii U:

```sh
python3 tools/hid_to_vpad_keyboard_client.py <WII_U_IP_ADDRESS>
```

Default keyboard mapping:

- `W`, `A`, `S`, `D`: left stick
- Arrow keys: right stick
- `Space`: A
- `J`: B
- `K`: X
- `L`: Y
- `U` / `I`: L / R
- `Q`: Minus
- `E` or Enter: Plus
- `H`: Home
- `Ctrl+C`: disconnect

Terminals do not expose real key-release events, so the script keeps each key active for a short repeat window (`--hold`, default `0.18` seconds). If movement feels too sticky or too short, tune it, for example:

```sh
python3 tools/hid_to_vpad_keyboard_client.py <WII_U_IP_ADDRESS> --hold 0.12 --rate 60
```

This first step is meant to prove extended connectivity from Wii U to a reproducible Python client. It is intentionally small so it can later be replaced or reused by an Android touch/gamepad front end.
