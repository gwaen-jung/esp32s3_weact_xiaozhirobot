# Package Xiaozhi Companion Robot to Desktop
$ErrorActionPreference = "Stop"

$desktopPath = "C:\Users\trung\Desktop"
$targetFolder = Join-Path $desktopPath "Xiaozhi_Robot_ESP32S3"
$zipPath = Join-Path $desktopPath "Xiaozhi_Robot_ESP32S3.zip"

Write-Host "Creating package folder: $targetFolder"
if (Test-Path $targetFolder) {
    Remove-Item -Recurse -Force $targetFolder
}
New-Item -ItemType Directory -Path $targetFolder | Out-Null

$dirFirmware = Join-Path $targetFolder "1_FIRMWARE_XIAOZHI_NAP_NHANH"
$dirSourceXiaozhi = Join-Path $targetFolder "2_SOURCE_CODE_XIAOZHI_ESP32"
$dirSourceParrot = Join-Path $targetFolder "3_SOURCE_CODE_VOICE_PARROT_TALKING_TOM"

New-Item -ItemType Directory -Path $dirFirmware | Out-Null
New-Item -ItemType Directory -Path $dirSourceXiaozhi | Out-Null
New-Item -ItemType Directory -Path $dirSourceParrot | Out-Null

# 1. Copy Firmware Binaries
$buildDir = "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\xiaozhi-esp32\build"
Copy-Item (Join-Path $buildDir "merged-binary.bin") $dirFirmware
Copy-Item (Join-Path $buildDir "xiaozhi.bin") $dirFirmware
Copy-Item (Join-Path $buildDir "generated_assets.bin") $dirFirmware
Copy-Item (Join-Path $buildDir "ota_data_initial.bin") $dirFirmware
Copy-Item (Join-Path $buildDir "bootloader\bootloader.bin") $dirFirmware
Copy-Item (Join-Path $buildDir "partition_table\partition-table.bin") $dirFirmware

# Create 1-click batch script
$batContent = @'
@echo off
chcp 65001 >nul
title NẠP FIRMWARE XIAOZHI ROBOT (ESP32-S3)
color 0A
echo ===============================================================================
echo          NẠP FIRMWARE XIAOZHI ROBOT CHO ESP32-S3 (COMPANION ROBOT)
echo ===============================================================================
echo  - Đã tích hợp màn hình OLED SSD1306 (xoay 180 độ chuẩn)
echo  - Đã tích hợp micro INMP441 + Loa MAX98357A
echo  - Wake-word tiếng Việt: "Chào Nhi"
echo  - 6 MCP Tools điều khiển động cơ: Tiến, Lùi, Rẽ trái, Rẽ phải, Nhảy, Dừng
echo ===============================================================================
echo.

set ESPTOOL="C:\Users\trung\.platformio\penv\Scripts\esptool.exe"
if not exist %ESPTOOL% (
    set ESPTOOL=python -m esptool
)

echo [1] Nhập cổng COM của mạch (Ví dụ: COM7, COM9, COM10, COM14, ...):
set /p COMPORT=">>> Nhập cổng COM: "

if "%COMPORT%"=="" (
    echo Cổng COM không hợp lệ! Vui lòng chạy lại và nhập ví dụ COM7.
    pause
    exit /b
)

echo.
echo Đang nạp firmware vào %COMPORT%... Vui lòng không rút cáp...
echo.

%ESPTOOL% --chip esp32s3 -p %COMPORT% -b 921600 --before default-reset --after hard-reset write-flash --flash-mode dio --flash-size 16MB --flash-freq 80m 0x0 merged-binary.bin

if %ERRORLEVEL% equ 0 (
    echo.
    echo ===============================================================================
    echo                     NẠP FIRMWARE THÀNH CÔNG 100%!
    echo ===============================================================================
    echo  - Nếu bo mới chưa có Wi-Fi, hãy kết nối vào Wi-Fi 'Xiaozhi-XXXX' để cấu hình.
    echo  - Sau đó mở web xiaozhi.me và nhập mã xác nhận 6 số để kích hoạt robot.
    echo ===============================================================================
) else (
    echo.
    echo [LỖI] Nạp firmware thất bại!
    echo Vui lòng kiểm tra lại cổng COM hoặc bấm giữ nút BOOT khi cắm cáp.
)
echo.
pause
'@
[System.IO.File]::WriteAllText((Join-Path $dirFirmware "NAP_FIRMWARE_1_CLICK.bat"), $batContent, [System.Text.Encoding]::UTF8)

# 2. Copy Source Code Xiaozhi
$boardSrcDir = "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\xiaozhi-esp32\main\boards\esp32s3-companion-robot"
$targetBoardDir = Join-Path $dirSourceXiaozhi "esp32s3-companion-robot"
New-Item -ItemType Directory -Path $targetBoardDir | Out-Null
Copy-Item "$boardSrcDir\*" $targetBoardDir -Recurse

Copy-Item "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\xiaozhi-esp32\main\assets.cc" $dirSourceXiaozhi
Copy-Item "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\xiaozhi-esp32\build_companion_robot.ps1" $dirSourceXiaozhi
Copy-Item "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\xiaozhi-esp32\flash_companion_robot.ps1" $dirSourceXiaozhi

# 3. Copy Source Code Voice Parrot
$parrotSrc = "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\src\voice_parrot.cpp"
Copy-Item $parrotSrc $dirSourceParrot
Copy-Item "d:\TRIAD PROJECT\Projects\esp32s3_weact_xiaozhirobot\platformio.ini" $dirSourceParrot
if (Test-Path "d:\TRIAD PROJECT\Projects\uvc_camera_test\default_16MB.csv") {
    Copy-Item "d:\TRIAD PROJECT\Projects\uvc_camera_test\default_16MB.csv" $dirSourceParrot
}

# 4. Create README / Instructions
$readmeContent = @'
===================================================================================
     BỘ TÀI LIỆU VÀ MÃ NGUỒN XIAOZHI COMPANION ROBOT (ESP32-S3)
===================================================================================

1. CẤU TRÚC THƯ MỤC:
-----------------------------------------------------------------------------------
- 1_FIRMWARE_XIAOZHI_NAP_NHANH:
    + merged-binary.bin: Toàn bộ firmware, bootloader, partition table và assets âm thanh gói gọn trong 1 file (đã xoay màn hình 180 độ chuẩn, tích hợp wake word "Chào Nhi", và 6 MCP Tool điều khiển motor).
    + NAP_FIRMWARE_1_CLICK.bat: File chạy nạp nhanh 1-click. Chỉ cần nhấp đúp chuột, nhập cổng COM và Enter là xong.

- 2_SOURCE_CODE_XIAOZHI_ESP32:
    + Chứa toàn bộ source code board profile "esp32s3-companion-robot".
    + companion_robot_board.cc: Khởi tạo I2C OLED, I2S Audio, đăng ký 6 công cụ MCP di chuyển.
    + motor_controller.h: Điều khiển động cơ DC kép bằng xung PWM 20kHz (LEDC).
    + config.h: Cấu hình chân kết nối phần cứng và xoay màn hình.
    + assets.cc: Bản vá chống crash khi màn hình tháo rời (NoDisplay guard).
    + build_companion_robot.ps1 / flash_companion_robot.ps1: Script biên dịch và nạp từ dòng lệnh ESP-IDF.

- 3_SOURCE_CODE_VOICE_PARROT_TALKING_TOM:
    + voice_parrot.cpp: Chương trình nhại giọng thông minh Talking Tom (3 chế độ giọng: Chipmunk, Bình thường, Robot) chạy trên PlatformIO Arduino.

-----------------------------------------------------------------------------------
2. SƠ ĐỒ CHÂN KẾT NỐI PHẦN CỨNG (PINOUT):
-----------------------------------------------------------------------------------
A. Màn hình OLED SSD1306 (I2C):
   - SDA  -> GPIO 1
   - SCL  -> GPIO 2
   - VCC  -> 3.3V
   - GND  -> GND

B. Microphone INMP441 (I2S Mic):
   - SCK/BCLK -> GPIO 11
   - WS/LRCK  -> GPIO 12
   - SD/DIN   -> GPIO 14
   - L/R      -> GND (Kênh trái)
   - VDD      -> 3.3V
   - GND      -> GND

C. Mạch khuếch đại âm thanh MAX98357A (I2S Amp):
   - BCLK -> GPIO 11 (Chung BCLK với Mic)
   - LRC  -> GPIO 12 (Chung WS với Mic)
   - DIN  -> GPIO 13
   - GAIN -> GND (Mặc định 9dB) hoặc để trống
   - VIN  -> 5V (hoặc 3.3V)
   - GND  -> GND

D. Mạch điều khiển động cơ L298N Mini (Dual DC Motor):
   - Motor A IN1 -> GPIO 4
   - Motor A IN2 -> GPIO 5
   - Motor B IN3 -> GPIO 6
   - Motor B IN4 -> GPIO 7

E. Nút bấm & Đèn LED:
   - Nút BOOT: GPIO 0 (Bấm 1 lần bật/tắt chat, bấm giữ vào cấu hình Wi-Fi)
   - Đèn LED RGB: GPIO 48

-----------------------------------------------------------------------------------
3. HƯỚNG DẪN NẠP VÀ KÍCH HOẠT ROBOT:
-----------------------------------------------------------------------------------
Bước 1: Cắm bo ESP32-S3 vào máy tính.
Bước 2: Mở thư mục "1_FIRMWARE_XIAOZHI_NAP_NHANH", nhấp đúp chạy "NAP_FIRMWARE_1_CLICK.bat".
Bước 3: Nhập cổng COM (ví dụ COM7, COM10, COM14...) và bấm Enter. Chờ nạp 100%.
Bước 4: Cấu hình Wi-Fi (nếu là bo mới):
        - Dùng điện thoại kết nối vào Wi-Fi do robot phát ra: "Xiaozhi-XXXX"
        - Mở trình duyệt truy cập: http://192.168.4.1
        - Chọn Wi-Fi nhà bạn và nhập mật khẩu.
Bước 5: Thêm thiết bị trên web Xiaozhi:
        - Màn hình OLED sẽ hiển thị mã 6 số (hoặc loa đọc mã 6 số).
        - Vào trang web console Xiaozhi (xiaozhi.me), bấm "Add Device" và nhập mã 6 số này để hoàn tất kết nối.

-----------------------------------------------------------------------------------
4. CÁC CÂU LỆNH ĐIỀU KHIỂN CHUYỂN ĐỘNG BẰNG GIỌNG NÓI:
-----------------------------------------------------------------------------------
Nói từ khóa đánh thức: "Chào Nhi"
Sau đó bạn có thể ra lệnh bằng tiếng Việt:
1. "Đi thẳng", "Tiến lên phía trước" -> Robot tiến lên
2. "Lùi lại", "Đi lùi" -> Robot lùi lại
3. "Rẽ trái", "Quay sang bên trái" -> Robot rẽ trái
4. "Rẽ phải", "Quay sang bên phải" -> Robot rẽ phải
5. "Nhảy múa đi", "Múa một bài" -> Robot thực hiện chuỗi động tác xoay múa
6. "Dừng lại", "Đứng yên" -> Robot dừng mọi động cơ
===================================================================================
'@
[System.IO.File]::WriteAllText((Join-Path $targetFolder "HUONG_DAN_SU_DUNG.txt"), $readmeContent, [System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText((Join-Path $targetFolder "README.md"), $readmeContent, [System.Text.Encoding]::UTF8)

# 5. Compress into ZIP
Write-Host "Creating ZIP file: $zipPath"
if (Test-Path $zipPath) {
    Remove-Item -Force $zipPath
}
Compress-Archive -Path "$targetFolder\*" -DestinationPath $zipPath -Force

Write-Host "Packaging completed successfully!"
Write-Host "Folder: $targetFolder"
Write-Host "Zip:    $zipPath"
