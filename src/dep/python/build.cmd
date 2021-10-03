@echo off
cd /d %~dp0
echo ** Building MOSCade nexus w/ NUITKA
echo ** Current Folder : %~dp0
python37 -m nuitka --standalone --mingw64 --follow-imports --python-arch=x86  .\nexus.py
copy nexus.dist\* ..\..\..\build\bin\