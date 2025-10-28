# fview

_NOTE: this README might not be up to date_

`fview` its command for a Linux-based OS.
Its used to keep track of the usage of files in a directory.

## How does it work?

Requires a directory path to open it.
This command works along side a [daemon](listener/file_listener.c). You need to execute it as a startup
process to start recording file operations.

Please, execute the script `setup.sh` to make this command 100% functional.

```bash
fview /home/user
```

## What should it return?

If the command is execute without flags, it will return by default a file that matches all these conditions:

- Most times modified
- Most times opened

After selecting a file that matches both conditions, returns the next information:

- File name
- File extension
- File path
- File permission bits
- File weight (in MB)
- Times it has been modified \- Last time modified
- TImes it has been opened \- Last time opened

```txt
Name: file
Extension: txt
Path: ~/Documents
Permission: 0777
Weight: 0.057 MB
Times modified: 5 - Last modification: 2025-09-30 12:00:00 -0500
Times opened: 4 - Last time opened: 2025-09-30 12:00:00 -0500
```

## Flags

The behavior of the command will depend on the flag used as well.
Among the most important flags this command use we have:

`-m`: Only searchs for the most modified file.
`-o`: Only searchs for the most opened file.
`-l`: Searchs for the smallest file.
`-h`: Searchs for the biggest file.

Executing the command with the flags `-moh` is the same as executing it without any flags.
Each flag is a condition a certain file must match to be shown.

```bash
fview /home/user -ml
```

### Other flags

Besides the flags already mentioned, there are also some other thare are quite helpful:

`-v`: Verbose output.
`-r`: Inspects recursively each subdirectory a parent directory has.
`-n`: Specifies a range of files to be shown.

#### Example

```bash
fview /home/user -mo -n 5
```

```txt
Name: file1
Extension: txt
Path: /home/user
Permission bits: 0500
Weight: 0.040 MB
Times modified: 10 - Last modification: 2025-09-30 12:00:00 -0500
Times opened: 20 - Last time opened: 2025-09-30 12:00:00 -0500

.
.
.
```
