# h9

## Build

### MacOS
```
sudo port install spdlog
sudo port install nlohmann-json
CMAKE_PREFIX_PATH=/opt/local//lib/libfmt10/cmake/fmt/ cmake -DBISON_EXECUTABLE=/opt/local/bin/bison ..
CMAKE_PREFIX_PATH=/opt/local/ cmake -DBISON_EXECUTABLE=/opt/local/bin/bison -DFLEX_EXECUTABLE=/opt/local/bin/flex ..
make
```

### Enable core dump
```
sudo chmod 1777 /cores
ulimit -c unlimited
cat <<EOF > tmp.entitlements
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>com.apple.security.get-task-allow</key>
	<true/>
</dict>
</plist>
EOF
/usr/libexec/PlistBuddy -c "Add :com.apple.security.get-task-allow bool true" tmp.entitlements
codesign -s - -f --entitlements tmp.entitlements CrashSelf ./src/h9d
./src/h9d -c conf/h9d.conf -vvvD
```

### Raspberry Pi OS
/etc/udev/rules.d/99-persistent-network.rules
```
SUBSYSTEM=="net", ACTION=="change|add", KERNEL=="can0" ATTR{tx_queue_len}="1000"
```

/etc/systemd/network/80-can0.network
```
[Match]
Name=can0

[CAN]
BitRate=125000
```
```
sudo apt-get install python3.9-dev
sudo apt-get install flex bison
sudo apt-get install libconfuse-dev
sudo apt-get install libreadline-dev
sudo apt-get install libspdlog-dev
sudo apt-get install nlohmann-json3-dev
cmake .
make

sudo adduser --system --no-create-home --group --disabled-login h9d
```
