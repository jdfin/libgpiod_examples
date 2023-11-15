
* libgpiod v2.x Example Programs on Raspberry Pi

Built and run on Raspberry Pi 4 with libgpiod v2.1.

* Preparation

$ sudo apt update
$ sudo apt full-upgrade -y
$ sudo reboot

$ sudo apt install -y git vim cmake

$ cd

$ git clone git://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git
  --or--
$ git clone https://github.com/jdfin/libgpiod.git

$ cd libgpiod

$ # v2.1 is latest at the moment (git tag -l)
$ git checkout v2.1
  --or--
$ git checkout v2.1-jdf

$ ./autogen.sh  --enable-tools=yes --prefix=/usr/local
$ make -j4
$ sudo make install
$ sudo ldconfig
