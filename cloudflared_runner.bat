@echo off
title Smart MedBox - Cloudflare Tunnel Launcher
color 0A
cls
echo ====================================================================
echo             ⚡ SMART MEDBOX - CLOUDFLARE TUNNEL LAUNCHER
echo ====================================================================
echo.
echo This script will expose your ESP32 local Web Server to a secure
echo HTTPS public Cloudflare Tunnel URL (e.g., https://xxx.trycloudflare.com).
echo.

set /p ESP_IP="Enter your ESP32 IP Address (e.g. 192.168.1.100 or 192.168.4.1): "

if "%ESP_IP%"=="" (
    set ESP_IP=192.168.4.1
)

echo.
echo [1/2] Checking for cloudflared installation...
where cloudflared >nul 2>nul
if %errorlevel% neq 0 (
    echo.
    echo [!] 'cloudflared' command is not installed or not in system PATH.
    echo.
    echo Downloading cloudflared executable from Cloudflare official CDN...
    powershell -Command "Invoke-WebRequest -Uri 'https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-windows-amd64.exe' -OutFile 'cloudflared.exe'"
    if exist "cloudflared.exe" (
        echo [✓] Downloaded cloudflared.exe successfully!
        set CLOUDFLARED_CMD=.\cloudflared.exe
    ) else (
        echo [X] Failed to download cloudflared.exe automatically.
        echo Please download cloudflared manually from https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/get-started/
        pause
        exit /b 1
    )
) else (
    set CLOUDFLARED_CMD=cloudflared
)

echo.
echo [2/2] Launching Cloudflare Quick Tunnel for http://%ESP_IP%:80 ...
echo.
echo ====================================================================
echo  Copy the generated https://xxxx.trycloudflare.com URL below
echo  and paste it into the Web App settings -> 'Cloudflare Tunnel URL'
echo ====================================================================
echo.

%CLOUDFLARED_CMD% tunnel --url http://%ESP_IP%:80

pause
