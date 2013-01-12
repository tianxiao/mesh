# Microsoft Developer Studio Project File - Name="SMlib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=SMlib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "SMlib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "SMlib.mak" CFG="SMlib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "SMlib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "SMlib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "SMlib - Win32 Release"

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
# ADD CPP /nologo /W3 /GX /O2 /I "inc" /I "stl" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
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
PostBuild_Cmds=copy Release\SMlib.lib lib\SMlib.lib
# End Special Build Tool

!ELSEIF  "$(CFG)" == "SMlib - Win32 Debug"

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
PostBuild_Cmds=copy Debug\SMlib.lib lib\SMlib.lib
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "SMlib - Win32 Release"
# Name "SMlib - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\src\dynamicqueue.cpp
# End Source File
# Begin Source File

SOURCE=.\src\dynamicvector.cpp
# End Source File
# Begin Source File

SOURCE=.\src\floatcompressor.cpp
# End Source File
# Begin Source File

SOURCE=.\src\integercompressor.cpp
# End Source File
# Begin Source File

SOURCE=.\src\integercompressor_new.cpp
# End Source File
# Begin Source File

SOURCE=.\src\ply.c
# End Source File
# Begin Source File

SOURCE=.\src\rangedecoder.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rangeencoder.cpp
# End Source File
# Begin Source File

SOURCE=.\src\rangemodel.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smconverter.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreader_ply.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreader_sma.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreader_smb.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreader_smc.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreader_smc_old.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreader_smd.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreadpostascompactpre.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smreadpreascompactpre.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwritebuffered.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwriter_off.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwriter_sma.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwriter_smb.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwriter_smc.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwriter_smc_old.cpp
# End Source File
# Begin Source File

SOURCE=.\src\smwriter_smd.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=.\src\dynamicqueue.h
# End Source File
# Begin Source File

SOURCE=.\src\dynamicvector.h
# End Source File
# Begin Source File

SOURCE=.\src\floatcompressor.h
# End Source File
# Begin Source File

SOURCE=.\src\integercompressor.h
# End Source File
# Begin Source File

SOURCE=.\src\integercompressor_new.h
# End Source File
# Begin Source File

SOURCE=.\src\littlecache.h
# End Source File
# Begin Source File

SOURCE=.\src\mydefs.h
# End Source File
# Begin Source File

SOURCE=.\src\ply.h
# End Source File
# Begin Source File

SOURCE=.\src\positionquantizer.h
# End Source File
# Begin Source File

SOURCE=.\src\positionquantizer_new.h
# End Source File
# Begin Source File

SOURCE=.\inc\psreader.h
# End Source File
# Begin Source File

SOURCE=.\src\rangedecoder.h
# End Source File
# Begin Source File

SOURCE=.\src\rangeencoder.h
# End Source File
# Begin Source File

SOURCE=.\src\rangemodel.h
# End Source File
# Begin Source File

SOURCE=.\inc\smconverter.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader_ply.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader_sma.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader_smb.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader_smc.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader_smc_old.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreader_smd.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreadpostascompactpre.h
# End Source File
# Begin Source File

SOURCE=.\inc\smreadpreascompactpre.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwritebuffered.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwriter.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwriter_sma.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwriter_smb.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwriter_smc.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwriter_smc_old.h
# End Source File
# Begin Source File

SOURCE=.\inc\smwriter_smd.h
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
