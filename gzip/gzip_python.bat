@echo off
set binpath=%~1
cd /d %~dp0
for %%x in (%*) do (python gz.py "%%x")




