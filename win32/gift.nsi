# $Id: gift.nsi,v 1.46 2003/12/26 13:34:52 mkern Exp $
# See /doc/install-win32.dbk for details on use.


# things to include
!define USE_OPENFT
!define USE_GNUTELLA
!define USE_FASTTRACK
#!define USE_GIFTCURS
#!define USE_KCEASY


# we are supposed to be in /win32-dist
!ifndef BUILD_ROOT
!define BUILD_ROOT ".."
!endif


!ifndef GIFT_DATE
!define GIFT_DATE "261203"
!endif

!ifndef DEBUG
!define DEBUG	"d"
!endif

!define PACKAGE       "giFT"

!ifndef VERSION
!define VERSION       "0.11.5"
!endif

!ifndef KCEASY_VERSION
!define KCEASY_VERSION	"0.7"
!endif

!define GIFT_DESC     "${PACKAGE} ${VERSION} ${GIFT_DATE}"

!ifdef DEBUG
 !define CONFIGURATION "Debug"
 !define DEXT          "d"
 !define D_EXT         "_d"
 !define OUTFILE       "${PACKAGE}-${VERSION}-cvs${GIFT_DATE}.exe"
!else
 !define CONFIGURATION "Release"
 !define CONF          ""
 !define DEXT          ""
 !define D_EXT         ""
 !define OUTFILE       "${PACKAGE}-${VERSION}.exe"
!endif


!ifndef DIST_DIR
!define DIST_DIR      "${BUILD_ROOT}\win32-dist"
!endif

!ifndef TMP_DIR
!define TMP_DIR       "${DIST_DIR}\tmp"
!endif

!ifndef CYGWIN_DIR
!define CYGWIN_DIR    "c:\cygwin\bin"
!endif

!ifndef GIFTCURS_DIR
!define GIFTCURS_DIR  "r:\src\giftcurs\src"
!endif

!ifndef KCEASY_DIR
!define KCEASY_DIR    "${BUILD_ROOT}\KCeasy"
!endif

!ifndef MSVCRTD_DIR
!define MSVCRTD_DIR   "c:\windows\system32"
!endif

!ifndef NSIS_DIR
!define NSIS_DIR      "C:\Progra~1\NSIS"
!endif

SetCompressor bzip2

!ifndef CLASSIC_UI
  !define MUI_PRODUCT   "${PACKAGE} ${VERSION}"
  !define MUI_VERSION   "${GIFT_DATE}"

  !include "${NSISDIR}\Contrib\Modern UI\System.nsh"

  !define MUI_WELCOMEPAGE
  !define MUI_LICENSEPAGE
  !define MUI_COMPONENTSPAGE
  !define MUI_DIRECTORYPAGE
  !define MUI_FINISHPAGE
  
#  !define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\Docs\index.html"
  !define MUI_FINISHPAGE_NOREBOOTSUPPORT
  !define MUI_COMPONENTSPAGE_NODESC
  
  !define MUI_ABORTWARNING
  !define MUI_UNINSTALLER
  !define MUI_UNCONFIRMPAGE

  ;NSIS updates no system files
  !define MUI_TEXT_WELCOME_INFO "\r\n\r\nThis will install ${MUI_PRODUCT} on your computer.\r\n\r\n\r\n\r\n"

  !insertmacro MUI_LANGUAGE "English"

#  !define MUI_UI "${NSISDIR}\Contrib\UIs\modern2.exe"
  
#  !insertmacro MUI_SYSTEM
  
!endif

#Name "${GIFT_DESC}"

#LicenseText "Copyright (C) 2001-2002 giFT project (http://giftproject.org/). All Rights Reserved."
LicenseData ${TMP_DIR}\COPYING
OutFile "${OUTFILE}"
#Icon gift.ico
#ComponentText "This will install ${GIFT_DESC} on your system."
#DirText "Select a directory to install giFT in:"

InstType "Full (giFT and all giFT clients)"
InstType "Lite (giFT only)"

XPStyle On

#EnabledBitmap "${NSIS_DIR}\bitmap1.bmp"
#DisabledBitmap "${NSIS_DIR}\bitmap2.bmp"

InstallDir "$PROGRAMFILES\giFT"
InstallDirRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath"
SetOverwrite on
ShowInstDetails show

#------------------------------------------------------------------------------------------
Section "giFT"
	SectionIn 1 2

	#First try to shut down giFTtray/giFT
	FindWindow $1 "giFTtray"
	SendMessage $1, WM_CLOSE, 0, 0
	Sleep 1000

!ifdef USE_OPENFT
	SetOutPath $INSTDIR\OpenFT
	File ${TMP_DIR}\OpenFT.conf
	File ${DIST_DIR}\data\OpenFT\nodes
!endif

!ifdef USE_GNUTELLA
	SetOutPath $INSTDIR\Gnutella
	File ${TMP_DIR}\Gnutella.conf
	File ${DIST_DIR}\data\Gnutella\gwebcaches
	File ${DIST_DIR}\data\Gnutella\hostiles.txt
!endif

!ifdef USE_FASTTRACK
	SetOutPath $INSTDIR\FastTrack
	File ${TMP_DIR}\FastTrack.conf
	File ${DIST_DIR}\data\FastTrack\nodes
	File ${DIST_DIR}\data\FastTrack\banlist
!endif

	SetOutPath $INSTDIR\data
	File ${DIST_DIR}\data\mime.types

	CreateDirectory "$INSTDIR\completed"
	CreateDirectory "$INSTDIR\incoming"

	# needs to come *after* the above
	SetOutPath $INSTDIR

!ifdef DEBUG
	File ${MSVCRTD_DIR}\MSVCRTD.DLL
	# c++ debug runtime, needed for debug version of libdb
	File ${MSVCRTD_DIR}\MSVCP60D.DLL
!endif

!ifndef USE_MSVC_BINARIES
	File ${DIST_DIR}\giFTtray\giFTtray.exe
	File ${DIST_DIR}\giFTsetup\giFTsetup.exe
	File ${DIST_DIR}\giFT.exe
	File ${DIST_DIR}\libgift${DEXT}.dll
	File ${DIST_DIR}\libgiftproto${DEXT}.dll
	File ${DIST_DIR}\zlib.dll
	File ${DIST_DIR}\libdb41${DEXT}.dll
	File ${DIST_DIR}\vorbis${D_EXT}.dll
	File ${DIST_DIR}\vorbisfile${D_EXT}.dll
	File ${DIST_DIR}\ogg${D_EXT}.dll
!else
	File giFTtray\${CONFIGURATION}\giFTtray.exe
	File giFTsetup\${CONFIGURATION}\giFTsetup.exe
	File ${CONFIGURATION}\giFT.exe
!endif


!ifdef USE_OPENFT
	File ${DIST_DIR}\OpenFT.dll
!endif

!ifdef USE_GNUTELLA
	File ${DIST_DIR}\Gnutella.dll
!endif

!ifdef USE_FASTTRACK
	File ${DIST_DIR}\FastTrack.dll
!endif


	File ${TMP_DIR}\giftd.conf
	File ${TMP_DIR}\COPYING
	File ${TMP_DIR}\AUTHORS
	File ${TMP_DIR}\README
	File ${TMP_DIR}\NEWS
 
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "DisplayName" "${GIFT_DESC}"
	WriteRegStr HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "UninstallString" '"$INSTDIR\ungiFT.exe"'
	WriteUninstaller "ungift.exe"

	# Registering install path, so future installs will use the same path
	WriteRegStr HKEY_LOCAL_MACHINE "Software\giFT\giFT" "instpath" $INSTDIR

	WriteRegStr HKEY_CURRENT_USER "Software\giFT\giFT" "instpath" $INSTDIR
	
	# It writes it as C:\Program Files\giFT\incoming
	# as giFTsetup will read it and translate it to
	# /C/Program Files/giFT/incoming
	# when it is first run
	# this is because $INSTDIR may not be c:\program files
	WriteINIStr $INSTDIR\giftd.conf download incoming $INSTDIR\incoming
	WriteINIStr $INSTDIR\giftd.conf download completed $INSTDIR\completed
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFTtray to Startup Folder"
	SectionIn 1 2
	# WriteRegStr HKEY_CURRENT_USER "Software\Microsoft\Windows\CurrentVersion\Run" "giFTtray" '"$INSTDIR\giFTtray.exe"'
	CreateShortCut "$SMSTARTUP\giFTtray.lnk" "$INSTDIR\giFTtray.exe" "" "$INSTDIR\giFTtray.exe" 0 SW_SHOWMINIMIZED
SectionEnd

#------------------------------------------------------------------------------------------
Section "Add giFT to Start/Programs Menu"
	SectionIn 1 2
	CreateDirectory "$SMPROGRAMS\giFT"
# now that we have giFTtray, we don't want a deamon icon
#	CreateShortCut "$SMPROGRAMS\giFT\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\giFTtray.lnk" "$INSTDIR\giFTtray.exe" "" "$INSTDIR\giFTtray.exe" 0
	CreateShortCut "$SMPROGRAMS\giFT\giFTsetup.lnk" "$INSTDIR\giFTsetup.exe" "" "$INSTDIR\giFTsetup.exe" 0

	CreateShortCut "$SMPROGRAMS\giFT\Readme file.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\README"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit giftd.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\giftd.conf"
#	CreateShortCut "$SMPROGRAMS\giFT\Edit OpenFT.conf.lnk" "$WINDIR\NOTEPAD.EXE" "$INSTDIR\OpenFT\OpenFT.conf"

	WriteINIStr    "$SMPROGRAMS\giFT\giFT Home Page.url" "InternetShortcut" "URL" "http://giftproject.org/"
	WriteINIStr    "$SMPROGRAMS\giFT\giFT Project Home Page.url" "InternetShortcut" "URL" "http://sourceforge.net/projects/gift/"

	CreateShortCut "$SMPROGRAMS\giFT\Uninstall giFT.lnk" "$INSTDIR\ungiFT.exe"
SectionEnd


!ifdef USE_GIFTCURS

Section "giFTcurs (ncurses front end)"
	SectionIn 1

	SetOutPath $INSTDIR

	File "${GIFTCURS_DIR}\giFTcurs.exe"
	File "${CYGWIN_DIR}\cygiconv-2.dll"
	File "${CYGWIN_DIR}\cygintl-2.dll"
	File "${CYGWIN_DIR}\cygncurses6.dll"
	File "${CYGWIN_DIR}\cygwin1.dll"
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\giFTcurs (ncurses front end).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
SectionEnd


!endif # USE_GIFTCURS


!ifdef USE_KCEASY

Section "KCeasy ${KCEASY_VERSION} (C++ Builder front end)"
	SectionIn 1

	SetOutPath $INSTDIR
	File ${KCEASY_DIR}\*.exe
	File ${KCEASY_DIR}\*.conf
	
	CreateDirectory "$SMPROGRAMS\giFT"
	CreateShortCut "$SMPROGRAMS\giFT\KCeasy (C++ Builder front end).lnk" "$INSTDIR\KCeasy.exe" "" "$INSTDIR\KCeasy.exe" 0
SectionEnd


!endif # USE_KCEASY



#------------------------------------------------------------------------------------------
#Section "Add giFT to Desktop"
#	SectionIn 1 2
#	CreateShortCut "$DESKTOP\giFTcurs (recommended).lnk" "$INSTDIR\giFTcurs.exe" "" "$INSTDIR\giFTcurs.exe" 0
#	CreateShortCut "$DESKTOP\giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "Add giFT to QuickLaunch Folder"
#	SectionIn 1 2
#	CreateShortCut "$QUICKLAUNCH\Start giFT daemon.lnk" "$INSTDIR\giFT.exe" "" "$INSTDIR\giFT.exe" 0 SW_SHOWMINIMIZED
#SectionEnd

#------------------------------------------------------------------------------------------
#Section "View Readme File"
#	SectionIn 1 2
#	ExecWait "$WINDIR\NOTEPAD.EXE $INSTDIR\README"
#SectionEnd

Function .onInstSuccess
#  MessageBox MB_YESNO|MB_ICONQUESTION \
#             "Setup has completed. Edit giftd.conf now?" \
#             IDNO NoGift
#    ExecWait "$WINDIR\NOTEPAD.EXE $INSTDIR\giftd.conf"
#

#  ExecWait "$INSTDIR\giFTsetup.exe"

  MessageBox MB_YESNO|MB_ICONQUESTION \
             "Would you like to start giFT now?" \
             IDNO NoGift
    Exec "$INSTDIR\giFTtray.exe"
  NoGift:
#  NoGiftConf:
FunctionEnd

#------------------------------------------------------------------------------------------
Section -PostInstall
	ExecWait "$INSTDIR\giFTsetup.exe"
#	MessageBox MB_OK|MB_ICONINFORMATION|MB_TOPMOST `${GIFT_DESC} has been successfully installed.`
SectionEnd

#UninstallText "This will uninstall ${GIFT_DESC}"

#------------------------------------------------------------------------------------------
# Uninstall part begins here:
Section Uninstall
	#First trying to shut down the node, the system tray Window class is called: giFTTray
	FindWindow $1 "giFTtray"
	SendMessage $1, WM_CLOSE, 0, 0
	Sleep 1000

	DeleteRegValue HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "UninstallString"
	DeleteRegValue HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT" "DisplayName"
	DeleteRegKey HKEY_LOCAL_MACHINE "Software\Microsoft\Windows\CurrentVersion\Uninstall\giFT"

	DeleteRegKey HKEY_LOCAL_MACHINE "Software\giFT\giFT"

	DeleteRegKey HKEY_CURRENT_USER "Software\giFT\giFT"

!ifdef USE_OPENFT
	Delete $INSTDIR\OpenFT\*.*
	RMDir /r $INSTDIR\OpenFT
	Delete $INSTDIR\data\OpenFT\*.*
	RMDir /r $INSTDIR\data\OpenFT
!endif


!ifdef USE_GNUTELLA
	Delete $INSTDIR\Gnutella\*.*
	RMDir /r $INSTDIR\Gnutella
	Delete $INSTDIR\data\Gnutella\*.*
	RMDir /r $INSTDIR\data\Gnutella
!endif


!ifdef USE_FASTTRACK
	Delete $INSTDIR\FastTrack\*.*
	RMDir /r $INSTDIR\FastTrack
	Delete $INSTDIR\data\FastTrack\*.*
	RMDir /r $INSTDIR\data\FastTrack
!endif


	Delete $INSTDIR\data\*.*
	RMDir /r $INSTDIR\data

	# only remove when empty
	RMDir $INSTDIR\completed
	RMDir $INSTDIR\incoming

	Delete $INSTDIR\*.*
	RMDir $INSTDIR


	Delete "$SMPROGRAMS\giFT\*.*"
	RMDir /r "$SMPROGRAMS\giFT"

#	Delete "$DESKTOP\giFTcurs.lnk"
#	Delete "$DESKTOP\giFT daemon.lnk"

#	Delete "$QUICKLAUNCH\Start giFT.lnk"
#	Delete "$QUICKLAUNCH\Start giFT daemon.lnk"

	Delete "$SMSTARTUP\giFTtray.lnk"
SectionEnd
