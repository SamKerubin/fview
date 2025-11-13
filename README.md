# fview

_**Important note: This project is a set of 3 tools, `fview` being the spotlight. Each tool is there to help each other; it is recommendable to have all 3 for an expected behavior**_

## General information about the purpose of this set of tools

The purpose of this set of tools is to provide a way of keep track to file accessing.

### file-listener

`file-listener` is a systemd daemon which purpose is to record file events on the entire system (from the root `/` directory).

This daemon uses [fanotify](https://www.man7.org/linux/man-pages/man7/fanotify.7.html) C library to record events such as opening or modifying a file.

You can manually check its activity on the file: `/var/log/file-listener/file-events`.

### addflblk

`addflblk` is a shell command which purpose is to add elements to a "blacklist".

The purpose of this, is to have a complete management of which paths `file-listener` daemon should expect activity from.

Paths excluded from activity recording are stored in: `/var/log/file-listener/file-listener.blacklist`.

#### Flag information

- `-r` `--remove`: Tries to remove a path if its already stored in the blacklist.
- `-v` `--verbose`: Displays verbose information about what the command is doing.
- `-h` `--help`: Displays a help message for the command.

#### Use example

```sh
addflblk /home/user # adds a path to the blacklist
```

### fview

`fview` is another shell command, its purpose is to search file(s) inside a directory that matches a condition.
Each condition is driven by what `file-listener` records.

For being more specific, the condition match depends on the amount of times a file has been modified/opened; 
it will search for files that have the biggest amount of events recorded.

#### Flag information

- `-o` `--opened`: Adds a condition to the match. The condition will now search for the biggest "opened" value.
- `-m` `--modified`: Adds a condition to the match. The condition will now search for the biggest "modified" value.
- `-n` `--range`: _(requires argument)_ Max output of files printed (1 by default).
- `-a` `--show-metadata`: Show file metadata.
- `-v` `--verbose`: Displays verbose information about what the command is doing.
- `-h` `--help`: Displays a help message for the command.

#### Use example

```sh
fview /home/user # Displays the first file that matches inside the directory, without checking other conditions
```

```sh
fview /home/user -mo # Displays the first file that matches being the biggest value in both "opened" and "modified"
```

## How to use?

Simply execute this [script](setup.sh). If you have any problem executing it, remember to change the permissions of the file:

```sh
chmod +x setup.sh
```

### Information about `setup.sh`

The script sets the files to their corresponding place. It compiles the source code and also copies and enables custom [shared libraries](https://github.com/SamKerubin/CLibs).

By default, the script wont start [file-listener](src/listener/file_listener.c) daemon, if you want to automatically start it with the setup, 
use the flag `-s`:

```sh 
sh setup.sh -s # starts the daemon automatically
```

If you dont want to automatically start the daemon, simply execute:

```sh
sh setup.sh
```

## Thats all

Well, thats all for now :3

Thanks for being interested in using this tool. Please be aware:

- This is my first linux tool, i tried my best on making it as good as possible in performance.
- If you find any issue, immediately notify me about it to start working on it!
- Any suggestion is always welcomed :3

See ya soon!!

And hope you enjoy using this awesome set of tools :3
