# arclite #
**This plugin is shipped with Far Manager**

Archive management based on 7z.dll from [7-Zip](http://7-zip.org) project.

Features:
  * Support for all archive formats from 7-Zip project.
  * Archival profiles (named groups of commonly used parameters).
  * Support for encrypted, SFX and multi-volume archives.
  * Ability to extract and test multiple archives.
  * Command line interface via plugin prefix


---


# WM Explorer #

Plugin is designed for file management of Windows Mobile based PDA/SmartPhone connected through ActiveSync.

Features:
  * Copy and move files between PPC and host machine.
  * Copy and move files on PPC without transferring information to host.
  * Rename, delete and set file attributes.
  * Launch programs on PPC.
  * Free memory and card space information.


---


# NTFS File Information #

Plugin provides various information about files on NTFS file system.

Features:
  1. Determine file size on disk. Especially useful for compressed and sparse files. Total size is shown for multiple files / directories.
  1. List NTFS attributes (file components on NTFS file system). Can be used to get information about:
    * alternate file names (hard links)
    * alternate data streams (named DATA attribute)
    * symbolic link targets
  1. Analyze file fragmentation and perform defragmentation.
  1. Analyze file contents:
    * estimate if compression of file data is possible (using very FAST LZO algorithm)
    * calculate most useful file hashes: crc32, md5, sha1, ed2k (eMule variation).
  1. Perform fast file search over entire volume using MFT index mode.
  1. Manage alternate data streams.
  1. Examine file version info.
  1. Compress files using NTFS compression.


---


# MsiUpdate #

Plugin notifies about new versions of Far Manager and performs automatic updates using official MSI packages.