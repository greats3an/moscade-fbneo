@echo off
cd /d %~dp0
echo ** Building MOSCade nexus w/ NUITKA
echo ** Current Folder : %~dp0
python37 -m nuitka --standalone --mingw64 --follow-imports  .\nexus.py
copy nexus.dist\* ..\..\..\build\bin\