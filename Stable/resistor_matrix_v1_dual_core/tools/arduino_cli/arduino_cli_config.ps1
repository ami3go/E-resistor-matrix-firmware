# E-Resistor Arduino CLI reproducible build configuration.
# Copy this file to arduino_cli.local.ps1 and change only local values there.

$ArduinoCliScriptVersion = "1.1.0"
$ArduinoCliVersion = "1.5.1"
$Rp2040CoreVersion = "5.6.1"
$NeoPixelVersion = "1.15.5"

$BoardFqbn = "rp2040:rp2040:waveshare_rp2040_zero"
$DefaultPort = "COM17"

# Exact Arduino IDE settings requested for the Waveshare RP2040 Zero.
$BoardOptions = @(
    "dbglvl=None",                    # Debug Level: None
    "dbgport=Disabled",               # Debug Port: Disabled
    "exceptions=Disabled",            # C++ Exceptions: Disabled
    "flash=2097152_1048576",          # 2MB flash: 1MB sketch + 1MB LittleFS
    "freq=200",                       # CPU Speed: 200 MHz (overclock)
    "ipbtstack=ipv4onlybig",          # IPv4 Only - 32K
    "opt=Small",                      # Small (-Os) (standard)
    "os=none",                        # Operating System: None
    "profile=Disabled",               # Profiling: Disabled
    "rtti=Disabled",                  # RTTI: Disabled
    "stackprotect=Disabled",          # Stack Protector: Disabled
    "uploadmethod=default",           # Upload Method: Default (UF2)
    "usbstack=picosdk"                # USB Stack: Pico SDK
)

$Rp2040PackageIndex = "https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json"
