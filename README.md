# uheater-firmware

## MQTT topics

| Name | Operation | Value | Description |
| ---- | ----------| ----- | ----------- |
| heater/boot/get | read | string | publish every 10 minutes |
| heater/mode/get | read | string | get current mode |
| heater/temp/idle/get | read | integer | get current temperature for idle mode |
| heater/temp/boiler/get | read | integer | get current temperature for boiler mode |
| heater/temp/floor/get | read | integer | get current temperature for floor mode |
| heater/boot/set | write | string | reboot device |
| heater/mode/set | write | string | set mode |
| heater/temp/idle/set | write | integer | set temperature for idle mode |
| heater/temp/boiler/set | write | integer | set temperature for boiler mode |
| heater/temp/floor/set | write | integer | set temperature for floor mode |
