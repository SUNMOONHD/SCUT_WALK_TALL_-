@echo off
cd /d D:\SCUT_WALK_TALL
cmake --build build --config Release > build.log 2>&1
type build.log
