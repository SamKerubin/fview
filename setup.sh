#!/bin/bash

service_file="/etc/systemd/system/file_listener.service"

sudo echo "Hi :3"

echo "Compiling components..."
gcc fview.c -o fview
gcc listener/file_listener.c listener/include/file_table.c -o file_listener

echo "Giving executing permissions to file_listener..."
sudo chmod -v 755 file_listener
sudo restorecon -v file_listener

echo "Moving file_listener to '/usr/sbin'..."
sudo mv -v file_listener /usr/sbin
echo "Moving fview to '/usr/local/bin'"
sudo mv -v fview /usr/local/bin

echo "Creating service file in '/etc/systemd/system'"
sudo touch $service_file
echo "Configurating '$service_file'..."
sudo tee $service_file > /dev/null <<EOF
[Unit]
Description=file_listener Daemon
After=network.target

[Service]
Type=simple
ExecStartPre=/usr/bin/ulimit -n
ExecStart=/usr/sbin/file_listener
Restart=on-failure
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable file_listener.service
sudo systemctl start file_listener.service

echo "Everything done! :D"
echo -e "Restart your device and use \e[32m fview \e[0m for executing the command!"
echo -e "\e[31m NOTE: recordings for file operations will only work with post-installation operations \e[0m"
