@echo off 
setlocal enableextensions 
for /f %%i in ('git describe --all --long') do set VER=%%i
(
echo: #ifndef __VERSION_H__
echo: #define __VERSION_H__
echo:    
echo: #define SW_VERSION	"%VER%"
echo:   
echo:  
echo:  
echo:  
echo:  
echo:  
echo:  
echo:  
echo: #endif
) > esp32-obd2-dongle\version.h