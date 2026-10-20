; The Groucho Clock screensaver installer.
; Installs for all users (asks for administrator rights) so that it appears in the
; Windows list of screensavers like any other.
!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "x64.nsh"

!ifndef VERSION
  !define VERSION "1.0.2"
!endif

Name "The Groucho Clock ${VERSION}"
OutFile "..\..\dist\GrouchoClock-Setup-${VERSION}.exe"
InstallDir "$PROGRAMFILES64\The Groucho Clock"
RequestExecutionLevel admin
SetCompressor /SOLID lzma
Unicode true
!define MUI_ICON "groucho-clock.ico"
!define MUI_UNICON "groucho-clock.ico"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Open Screen Saver Settings"
!define MUI_FINISHPAGE_RUN_FUNCTION OpenSettings

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

!define UNKEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\GrouchoClock"

Function .onInit
  ${IfNot} ${RunningX64}
    MessageBox MB_OK|MB_ICONSTOP "The Groucho Clock needs 64-bit Windows."
    Abort
  ${EndIf}
FunctionEnd

Function OpenSettings
  ; 64-bit rundll32: a 32-bit one looks for screen savers in SysWOW64 and shows (None)
  Exec '"$WINDIR\Sysnative\rundll32.exe" shell32.dll,Control_RunDLL desk.cpl,,1'
FunctionEnd

Section "The Groucho Clock" SecMain
  SectionIn RO
  SetRegView 64
  SetOutPath "$INSTDIR"
  File "..\..\build\win\groucho-clock.exe"
  File "..\..\build\win\*.dll"
  File /oname=README.txt "..\..\README.md"
  File /oname=LICENSE.txt "..\..\LICENSE"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  WriteRegStr HKLM "Software\GrouchoClock" "InstallDir" "$INSTDIR"

  ; the screensaver file itself goes in the real (64-bit) system folder
  ${DisableX64FSRedirection}
  SetOutPath "$WINDIR\System32"
  File "..\..\build\win\GrouchoClock.scr"
  ${EnableX64FSRedirection}
  SetOutPath "$INSTDIR"

  ; make it the current screensaver for whoever is installing
  WriteRegStr HKCU "Control Panel\Desktop" "SCRNSAVE.EXE" "$WINDIR\System32\GrouchoClock.scr"
  WriteRegStr HKCU "Control Panel\Desktop" "ScreenSaveActive" "1"
  ReadRegStr $1 HKCU "Control Panel\Desktop" "ScreenSaveTimeOut"
  StrCmp $1 "" 0 +2
    WriteRegStr HKCU "Control Panel\Desktop" "ScreenSaveTimeOut" "300"

  CreateDirectory "$SMPROGRAMS\The Groucho Clock"
  CreateShortcut "$SMPROGRAMS\The Groucho Clock\The Groucho Clock (window).lnk" "$INSTDIR\groucho-clock.exe" "--window"
  CreateShortcut "$SMPROGRAMS\The Groucho Clock\The Groucho Clock (full screen).lnk" "$INSTDIR\groucho-clock.exe" "/s"
  CreateShortcut "$SMPROGRAMS\The Groucho Clock\Uninstall.lnk" "$INSTDIR\Uninstall.exe"

  WriteRegStr HKLM "${UNKEY}" "DisplayName" "The Groucho Clock screensaver"
  WriteRegStr HKLM "${UNKEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKLM "${UNKEY}" "DisplayIcon" "$INSTDIR\groucho-clock.exe"
  WriteRegStr HKLM "${UNKEY}" "UninstallString" '"$INSTDIR\Uninstall.exe"'
  WriteRegStr HKLM "${UNKEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "${UNKEY}" "NoModify" 1
  WriteRegDWORD HKLM "${UNKEY}" "NoRepair" 1
  ${GetSize} "$INSTDIR" "/S=0K" $2 $3 $4
  IntFmt $2 "0x%08X" $2
  WriteRegDWORD HKLM "${UNKEY}" "EstimatedSize" "$2"
SectionEnd

Section "Uninstall"
  SetRegView 64
  ; stop being the screensaver, if we still are
  ReadRegStr $1 HKCU "Control Panel\Desktop" "SCRNSAVE.EXE"
  StrCmp $1 "$WINDIR\System32\GrouchoClock.scr" 0 +2
    DeleteRegValue HKCU "Control Panel\Desktop" "SCRNSAVE.EXE"
  ${DisableX64FSRedirection}
  Delete "$WINDIR\System32\GrouchoClock.scr"
  ${EnableX64FSRedirection}

  Delete "$INSTDIR\groucho-clock.exe"
  Delete "$INSTDIR\SDL2.dll"
  Delete "$INSTDIR\SDL3.dll"
  Delete "$INSTDIR\README.txt"
  Delete "$INSTDIR\LICENSE.txt"
  Delete "$INSTDIR\Uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\The Groucho Clock\The Groucho Clock (window).lnk"
  Delete "$SMPROGRAMS\The Groucho Clock\The Groucho Clock (full screen).lnk"
  Delete "$SMPROGRAMS\The Groucho Clock\Uninstall.lnk"
  RMDir "$SMPROGRAMS\The Groucho Clock"
  DeleteRegKey HKLM "${UNKEY}"
  DeleteRegKey HKLM "Software\GrouchoClock"
SectionEnd
