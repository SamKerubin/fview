#!/bin/bash

compile_flags="-g -O0"
service_file="/etc/systemd/system/file-listener.service"
working_dir="/var/log/file-listener"

sudo echo "Hi :3"

echo "Installing shared libraries..."
sudo cp -v lib/*.so /usr/local/lib
sudo ldconfig
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/local.conf

echo "Compiling components..."
gcc $compile_flags src/fview.c -Iinclude -lprocutils -o fview
gcc $compile_flags src/listener/file_listener.c src/file_table.c -Iinclude -lfileutils -lstrutils -o file-listener
gcc $compile_flags src/listener/listener_blacklist/addflblk.c -Iinclude -lprocutils -lfileutils -o addflblk

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

echo -e "\e[31m NOTE: recordings for file operations will only work with post-installation operations \e[0m"
echo -e "\n"

echo "Any problems starting file-listener daemon?"
echo -e "Execute these commands:\n"
echo -e "\e[32msudo chmod 755 /usr/sbin/file-listener\e[0m"
echo -e "\e[32msudo restorecon /usr/sbin/file-listener\e[0m"
echo -e "\e[32msudo systemctl daemon-reload\e[0m"
echo -e "\e[32msudo systemctl start file-listener.service\e[0m\n"

echo -e "You might have problems executing \e[32maddflblk\e[0m"
echo  -e "If thats the case, please execute this line:\n"
echo -e "\e[32msudo chown \$USER:\$USER $working_dir/file_listener.blacklist\e[0m"

echo -e "\nSide note: Components are compiled with the flags \e[32m$compile_flags\e[0m, feel free to use valgrind for debugging <3"
