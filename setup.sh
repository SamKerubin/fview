#!/bin/bash

service_file="/etc/systemd/system/file_listener.service"

sudo echo "Hi :3"

echo "Compiling components..."
make listener
gcc fview.c -o fview

echo "Moving file_listener to '/usr/sbin'..."
sudo mv file_listener /usr/sbin
echo "Moving fview to '/usr/local/bin'"
sudo mv fview /usr/local/bin

echo "Creating service file in '/etc/systemd/system'"
sudo touch $service_file
echo "Configurating '$service_file'..."
sudo tee $service_file > /dev/null <<EOF
[Unit]
Description=file_listener Daemon
After=network.target

[Service]
Type=simple
ExecStart=/usr/sbin/file_listener
Restart=on-failure

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable file_listener.service
sudo systemctl start file_listener.service

echo "Everything done! :D"
echo -e "Restart your device and use \e[32m fview \e[0m for executing the command!"
echo -e "\e[31m NOTE: recordings for file operations will only work with post-installation operations \e[0m"