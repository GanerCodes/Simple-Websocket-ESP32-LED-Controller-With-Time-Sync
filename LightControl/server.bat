@echo off
TITLE Server
:LOOP
python -m http.server 1234
goto LOOP