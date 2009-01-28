m4_include(`version.m4')m4_dnl
NTFS File Information __VER_MAJOR__.__VER_MINOR__.__VER_PATCH__
FAR Manager Plugin

������ �।�����祭 ��� ����祭�� ࠧ��筮� ���ଠ樨 � 䠩��� �� 䠩����� ��⥬� NTFS. �� ����� ������ ॠ�������� �㭪樨:

1. ��।������ ����������� 䠩��� ���� �� ��᪥. �ᮡ���� ������� ��� ᦠ��� � ࠧ०��� 䠩���. ��� ��⠫���� ⠪�� ��।������ �㬬��� ��ꥬ � ���⮬ ᮤ�ন����.
2. �⮡ࠦ���� ᯨ᪠ NTFS ��ਡ�⮢, �� ������ ��⮨� 䠩�. ����� �ᯮ�짮���� ��� ��ᬮ��:
  - ����ୠ⨢��� ��� 䠩�� (���⪨� �痢�).
  - ����ୠ⨢��� ��⮪�� ������ (����������� ��ਡ�⮢ DATA).
  - 楫����� ��⠫��� / ����� ⮬� ��� ᨬ�����᪨� �痢� / �祪 ����஢����.
3. ��।������ �⥯��� �ࠣ����樨 䠩���. ���ࠣ������ 䠩��� � ��⠫����.
4. �㭪樨 ������� ������:
  - �業�� ᦨ������� ������ ��� ��㯯� 䠩���/��⠫���� (�ந�������� ᦠ⨥ ������ � ������� ����ண� �����⬠ LZO, ������� �⮡� ��।����� - �⮨� �� ᦨ���� 䠩�� �।�⢠�� NTFS ��� ��娢��஬).
  - ����� �������� �������� �襩 ��� ��࠭���� 䠩��: crc32, md5, sha1, ed2k (��ਠ�� eMule).

���⥬�� �ॡ������:
  - Far m4_ifdef(`__FARAPI17__', `1.71.2411', `2.0.680')
  - Windows 2000+

���� ���ᨨ: http://forum.farmanager.com/viewtopic.php?t=2050
���㦤����: http://forum.farmanager.com/viewtopic.php?t=2051

---------------------------------------------------------------

This plugin provides various information about files on NTFS file system. Features:

1. Determine file size on disk. Especially useful for compressed and sparse files. Total size is shown for multiple files / directories.
2. List NTFS attributes (file components on NTFS file system). Can be used to get information about:
  - alternate file names (hard links)
  - alternate data streams (named DATA attribute)
  - symbolic link targets
3. Analyze file fragmentation and perform defragmentation.
4. Analyze file contents:
  - estimate if compression of file data is possible (using very FAST LZO algorithm)
  - calculate most useful file hashes: crc32, md5, sha1, ed2k (eMule variation).

System requirements:
  - Far m4_ifdef(`__FARAPI17__', `1.71.2411', `2.0.680')
  - Windows 2000+

New versions: http://forum.farmanager.com/viewtopic.php?t=2050
Discussion: http://forum.farmanager.com/viewtopic.php?t=2051
