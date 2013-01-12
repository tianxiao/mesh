# Microsoft Developer Studio Project File - Name="PSlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=PSlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "PSlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "PSlib.mak" CFG="PSlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "PSlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "PSlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "PSlib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /W3 /GX /O2 /I "stl" /I "inc" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy Release\PSlib.lib lib\PSlib.lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "PSlib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /W3 /Gm /GX /ZI /Od /I "inc" /I "stl" /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo
# Begin Special Build Tool
SOURCE="$(InputPath)"
PostBuild_Cmds=copy Debug\PSlib.lib lib\PSlib.lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "PSlib - Win32 Release"
# Name "PSlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\codec_new_dec.cpp
# End Source File
# Begin Source File

SOURCE=.\src\codec_new_dec.h
# End Source File
# Begin Source File

SOURCE=.\src\crazyvector_for_pointers.cpp
# End Source File
# Begin Source File

SOURCE=.\src\crazyvector_for_pointers.h
# End Source File
# Begin Source File

SOURCE=.\src\mydefs.h
# End Source File
# Begin Source File

SOURCE=.\src\ply.c
# End Source File
# Begin Source File

SOURCE=.\src\ply.h
# End Source File
# Begin Source File

SOURCE=.\src\positionquantizer.h
# End Source File
# Begin Source File

SOURCE=.\src\psconverter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\psreader_lowspan.cpp
# End Source File
# Begin Source File

SOURCE=.\src\psreader_oocc.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rangedecoder.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rangedecoder.h
# End Source File
# Begin Source File

SOURCE=.\src\rangemodel.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rangemodel.h
# End Source File
# Begin Source File

SOURCE=.\src\vector.cpp
# End Source File
# Begin Source File

SOURCE=.\src\vector.h
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\inc\psconverter.h
# End Source File
# Begin Source File

SOURCE=.\inc\psreader.h
# End Source File
# Begin Source File

SOURCE=.\inc\psreader_lowspan.h
# End Source File
# Begin Source File

SOURCE=.\inc\psreader_oocc.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader.h
# End Source File
# Begin Source File

SOURCE=.\inc\vec3fv.h
# End Source File
# Begin Source File

SOURCE=.\inc\vec3iv.h
# End Source File
# End Group
# End Target
# End Project
