@echo off 
setlocal enableextensions 
for /f %%i in ('git describe --all --long --dirty') do set VER=%%i
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
) > %1\version.h