#!/bin/bash

compile_flags="-g -O0"
service_file="/etc/systemd/system/file-listener.service"

sudo echo "Hi :3"

echo "Making working directory on '/var/log'..."
sudo mkdir -vp /var/log/file-listener

echo "Compiling components..."
gcc $compile_flags fview.c -o fview
gcc $compile_flags listener/file_listener.c listener/include/file_table.c -o file-listener
gcc $compile_flags listener/listener_blacklist/addflblk.c -o addflblk

echo "Giving executing permissions to file-listener..."
sudo chmod -v 755 file-listener
sudo restorecon -v file-listener

echo "Moving file-listener to '/usr/sbin'..."
sudo mv -v file-listener /usr/sbin
echo "Moving fview to '/usr/local/bin'..."
sudo mv -v fview /usr/local/bin
echo "Moving addflblk to 'usr/local/bin'..."
sudo mv -v addflblk /usr/local/bin

echo "Creating service file in '/etc/systemd/system'"
sudo touch $service_file
echo "Configurating '$service_file'..."
sudo tee $service_file > /dev/null <<EOF
[Unit]
Description=file-listener Daemon
After=network.target

[Service]
Type=simple
ExecStartPre=/usr/bin/ulimit -n
ExecStart=/usr/sbin/file-listener
Restart=on-failure
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable file-listener.service
sudo systemctl start file-listener.service

echo "Everything done! :D"
echo -e "Restart your device and use \e[32mfview\e[0m for executing the command!"
echo -e "\e[31m NOTE: recordings for file operations will only work with post-installation operations \e[0m"
echo -e "\n"
echo "Any problems starting file-listener daemon?"
echo -e "Execute these commands:\n"
echo -e "\e[32msudo chmod 755 /usr/sbin/file-listener\e[0m"
echo -e "\e[32msudo restorecon /usr/sbin/file-listener\e[0m"
echo -e "\e[32msudo systemctl daemon-reload\e[0m"
echo -e "\e[32msudo systemctl start file-listener.service\e[0m"
