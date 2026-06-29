# libibootim
All the essentials you need when working with Apple iBoot images and PNGs.

## Requirements
- libpng
- CMake

## Installation
Install libibootim with CMake

```bash
git clone https://github.com/XboxOneSogie720/libibootim.git
cd libibootim
mkdir build && cd build
cmake ..
make
sudo make install
```

## Features
- Intuitive buffer and I/O API
- Support for both legacy and modern iBoot image loading
- Automatic detection and decompression of legacy iBoot images, modern iBoot images, and PNGs
- Indexed iBoot image loading support

## Acknowledgements
- xpwn's [imagetool](https://github.com/planetbeing/xpwn/blob/20c32e5c12d1b22a9d55a59a0ff6267f539b77f4/ipsw-patch/imagetool.c#L11)
- realnp's [ibootim](https://github.com/realnp/ibootim)
