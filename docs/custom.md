# Custom firmware

If you want, you can create your own custom firmware.

## Install Tools

To build the source code, you need the following packages on Ubuntu 24.04.

```bash
sudo apt install nodejs npm nanopb
```

You need `esp-idf` version 5.4. It might be possible to build with the latest version, but it has not been confirmed.

First, install the necessary tools for esp-idf.

```bash
sudo apt install git wget flex bison gperf python3 python3-pip python3-venv \
  cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 \

```

Next, clone the esp-idf tool.

```bash
mkdir -p ~/esp
cd ~/esp
git clone -b release/v5.4 --recursive https://github.com/espressif/esp-idf.git
```

You need to install tools such as a compiler and a debugger.

```bash
cd ~/esp/esp-idf
./install.sh esp32c3
```

Please prepare to build the source by running the shell for using esp-idf.
```bash
. $HOME/esp/esp-idf/export.sh
```
## Build custom firmware based on PowerMate

First, clone the PowerMate source code.
```bash
git clone https://github.com/shinys000114/odroid-powermate.git
```

After connecting the USB Type-C cable to the PowerMate and the host PC, run the command below to build and update PowerMate.
```bash
cd odroid-powermate
idf.py app flash monitor
```

## Create a new project

Create a project with the command below and create your own firmware.

```bash
idf.py create-project proj
cd proj
idf.py set-target esp32c3
```

For a more detailed guide, please refer to the link below.  
[https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32/get-started/linux-macos-setup.html#](https://docs.espressif.com/projects/esp-idf/en/release-v5.4/esp32/get-started/linux-macos-setup.html#)