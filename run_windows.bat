@echo off
cd /d "%~dp0"
if exist build\risha_editor.exe (start "" build\risha_editor.exe) else if exist build\Release\risha_editor.exe (start "" build\Release\risha_editor.exe) else call build.bat
