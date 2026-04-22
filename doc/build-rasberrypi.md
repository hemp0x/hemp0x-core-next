RASBERRY PI BUILD NOTES
====================
Origin: traysi.org/hemp0x_rpi.php

# Install necessary packages:
```
sudo apt-get install git
sudo apt-get install build-essential libtool autotools-dev automake pkg-config libssl-dev libevent-dev bsdmainutils python3
sudo apt-get install libboost-all-dev
sudo apt-get install software-properties-common
sudo apt-get update
sudo apt-get install libminiupnpc-dev
sudo apt-get install libzmq3-dev
```

# Increase your swap size:
```
sudo nano /etc/dphys-swapfile
- In this file, change CONF_SWAPSIZE=100 to CONF_SWAPSIZE=1000
sudo reboot
```

# Build Berkeley DB 4.8:
```
cd ~
mkdir build
cd build
wget http://download.oracle.com/berkeley-db/db-4.8.30.NC.tar.gz
tar -xzvf db-4.8.30.NC.tar.gz
cd db-4.8.30.NC/build_unix/
../dist/configure --enable-cxx
make -j4 # If error, remove the -j4
sudo make install
```

# Build Hemp0x
```
cd ~/build/
git clone https://github.com/hemp0x/hemp0x-core
cd Hemp0x/
./autogen.sh
./configure --disable-tests --without-gui CPPFLAGS="-I/usr/local/BerkeleyDB.4.8/include -O2" LDFLAGS="-L/usr/local/BerkeleyDB.4.8/lib"
make
```
