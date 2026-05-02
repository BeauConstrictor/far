# `far` - **F**ile **Ar**chiver

`far` is an aggressively minimal file archiving tool and format.

## CLI Tool

`far` is primarily a CLI tool, with a small core that can be easily
integrated in other C programs. The tool is extremely simple and all
you need is `far --help` to learn everything.

## FAR Format

`far` uses the FAR (`*.far`) file format for achiving. 

The FAR format is excellent for sending data to embedded devices
where the C library makes it incredibly easy to write a parse
on-device that can read from a serial port. Of course, the CLI tool
natively supports outputting archives over stdout which you can
trivially redirect to a device.

FAR is an incredibly effective format for data streams and
can be parsed while not even loading a single full file into
memory. However, FAR does

# Breakdown

A FAR file begins with the magic value `far`, encoded as ASCII,
followed by *entries*, placed one after another (with no separators).

Entries are structured like this:

* Path - The path of the entry, where `/` is the root of the archive;
  null-terminated.
* Timestamp - When the file was last modified, as a 64-bit UNIX
  timestamp, encoded as an ASCII number. Can be set as 0;
  null-terminated.
* Permissions - A 4-char file permission string, explained below.
* Size - ASCII text of the entry size in bytes; null-terminated.
* Data - the actual content of the entry. Not null-terminated, but
  length terminated by the previously specified size.

Permissions are formatted as in `ls`, where the first character
represents the type of the entry:

* `-` - File * `d` - Directory * `l` - Symlink

Next, the 3-character octal permissions, using ASCII digits.

### File Entries

File entries are the most common. The data is, obviously, the actual
contents of the file.

### Directory Entries

Directory entries contain no data (size = 0) and should always be
placed before all the files they contain.

### Symlink Entries

A symlink in FAR must point to a file entry or directory entry, by
absolute path.
