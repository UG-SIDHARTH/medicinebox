@echo off
title Smart MedBox - Custom Domain Cloudflare Tunnel Creator
color 0B
cls
echo ====================================================================
echo      ⚡ SMART MEDBOX - CUSTOM DOMAIN CLOUDFLARE TUNNEL CREATOR
echo ====================================================================
echo.
echo This script will help you create a permanent Cloudflare Tunnel ID
echo and route your custom domain (e.g., medbox.yourdomain.com) to your ESP32.
echo.

rem Check for cloudflared
where cloudflared >nul 2>nul
if %errorlevel% neq 0 (
    if exist "cloudflared.exe" (
        set CLOUDFLARED_CMD=.\cloudflared.exe
    ) else (
        echo [!] Downloading cloudflared.exe...
        powershell -Command "Invoke-WebRequest -Uri 'https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-windows-amd64.exe' -OutFile 'cloudflared.exe'"
        set CLOUDFLARED_CMD=.\cloudflared.exe
    )
) else (
    set CLOUDFLARED_CMD=cloudflared
)

echo [STEP 1/5] Authenticating with Cloudflare...
echo A browser window will open. Select your domain's Cloudflare account to authorize.
echo.
%CLOUDFLARED_CMD% tunnel login

echo.
echo ====================================================================
set /p TUNNEL_NAME="Enter a name for your new Tunnel (default: esp32-medbox): "
if "%TUNNEL_NAME%"=="" set TUNNEL_NAME=esp32-medbox

echo.
echo [STEP 2/5] Creating Cloudflare Tunnel: %TUNNEL_NAME% ...
%CLOUDFLARED_CMD% tunnel create %TUNNEL_NAME%

echo.
set /p SUBDOMAIN="Enter your full custom domain (default: mediback.ugsidharth.in): "
if "%SUBDOMAIN%"=="" set SUBDOMAIN=mediback.ugsidharth.in

echo.
echo [STEP 3/5] Routing DNS record for %SUBDOMAIN% to tunnel %TUNNEL_NAME% ...
%CLOUDFLARED_CMD% tunnel route dns %TUNNEL_NAME% %SUBDOMAIN%

echo.
set /p ESP_IP="Enter your ESP32 local IP address (default: 192.168.4.1): "
if "%ESP_IP%"=="" set ESP_IP=192.168.4.1

echo.
echo [STEP 4/5] Generating config.yml ...
(
  echo tunnel: %TUNNEL_NAME%
  echo credentials-file: %USERPROFILE%\.cloudflared\%TUNNEL_NAME%.json
  echo.
  echo ingress:
  echo   - hostname: %SUBDOMAIN%
  echo     service: http://%ESP_IP%:80
  echo   - service: http_status:404
) > config.yml

echo [✓] Created config.yml successfully!

echo.
echo [STEP 5/5] Launching Cloudflare Tunnel for %SUBDOMAIN% ...
echo.
echo ====================================================================
echo SUCCESS! Your ESP32 is now securely accessible worldwide at:
echo   https://%SUBDOMAIN%
echo ====================================================================
echo.

%CLOUDFLARED_CMD% tunnel --config config.yml run %TUNNEL_NAME%

pause
