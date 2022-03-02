#!/bin/bash

# This script automatically downloads and installs potion in a linux system ~ See potion at - https://github.com/perl11/potion

# Check if run as root

if [ "$EUID" -ne 0 ]; then
    echo "[*] ~ Please run as root"
    exit
fi

# Main

echo "[*] ~ Cloning source code from github"

github_url=https://github.com/perl11/potion

git clone $github_url

cd potion

echo "[*] ~ Compiling source code"

make DEBUG=1

echo "[*] ~ Testing build"

make test

echo "[*] ~ Installing in filesystem"

cd ..
rm -rf /usr/local/bin/potion
touch /usr/local/bin/potion
chmod +x /usr/local/bin/potion
echo "/usr/local/bin/potion-bin/bin/potion" > /usr/local/bin/potion
mv potion/ potion-bin/
cp -r potion-bin/ /usr/local/bin/
mv potion-bin/ potion/

read -p "Do you want to remove extra files? y/N > " option

if [ "$option" = "N" ]; then
    exit
else
    rm -rf potion
    exit
fi
