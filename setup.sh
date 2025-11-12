#!/bin/bash

compile_flags="-g -O3 -Iinclude"
service_file="/etc/systemd/system/file-listener.service"
working_dir="/var/log/file-listener"
auto_start=0
version="0.0.0"

while getopts "s" o; do
    case "$o" in
        s)
            auto_start=1
            ;;
        *)
            echo "Not valid flag, you may only use '-s' flag."
            exit 1
    esac
done

sudo echo "Hi :3"

echo "Moving shared libraries to '/usr/local/lib'..."
sudo mv -v lib/*.so /usr/local/lib
sudo ldconfig
echo "/usr/local/lib" | sudo tee /etc/ld.so.conf.d/local.conf

echo "Moving header files of libraries to '/usr/local/include'..."
sudo mv -v include/*utils.h /usr/local/include

echo "Compiling components..."
gcc $compile_flags src/fview.c -lprocutils -o fview
gcc $compile_flags src/listener/file_listener.c src/file_table.c -lfileutils -lstrutils -o file-listener
gcc $compile_flags src/listener/listener_blacklist/addflblk.c -lprocutils -lfileutils -o addflblk

echo "Moving file-listener to '/usr/sbin'..."
sudo mv -v file-listener /usr/sbin
echo "Moving fview to '/usr/local/bin'..."
sudo mv -v fview /usr/local/bin
echo "Moving addflblk to 'usr/local/bin'..."
sudo mv -v addflblk /usr/local/bin

echo "Giving executing permissions to file-listener..."
sudo chmod -v +x /usr/sbin/file-listener

echo "Giving executing permissions to fview..."
sudo chmod -v +x /usr/local/bin/fview

echo "Giving executing permissions to addflblk..."
sudo chmod -v +x /usr/local/bin/addflblk

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

if (( auto_start )); then
    sudo systemctl daemon-reload
    sudo systemctl enable file-listener.service
    sudo systemctl start file-listener.service
fi

echo "Everything done! :D"

echo -e "\e[31mNOTE: Recordings for file operations will only work with post-installation operations\e[0m"
echo -e "\n"

if (( auto_start )); then
    echo -e "Execute \e[32msystemctl status file-listener\e[0m to check if everything works fine\n"
fi

echo -e "\e[31m[Troubleshooting] file-listener\e[0m\n"

echo -e "Service cannot be executed (exit code 203):\n"
echo -e "\e[32msudo chmod 755 /usr/sbin/file-listener\e[0m"
echo -e "\e[32msudo restorecon /usr/sbin/file-listener\e[0m"
echo -e "\e[32msudo systemctl daemon-reload\e[0m"
echo -e "\e[32msudo systemctl start file-listener.service\e[0m\n"

echo -e "Service exited with SIGABRT/SIGSEGV:\n"
echo -e "If the process is still trying to be executed, stop and disable it:\n"
echo -e "\e[32msudo systemctl stop file-listener\e[0m"
echo -e "\e[32msudo systemctl disable file-listener\e[0m\n"
echo -e "And please, \e[31mcontact\e[0m me if this happens :3\n"

echo -e "\e[31m[Troubleshooting] addflblk\e[0m\n"

echo -e "Cannot access .blacklist file:\n"
echo -e "\e[32msudo chown \$USER:\$USER $working_dir/file-listener.blacklist\e[0m"

echo -e "\nYou are currently using version \e[32m$version\e[0m"