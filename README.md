# Introduction

HTTP server written from scratch for ESP8266 (but not just for it with some decent modifications) running on RTOS

# Main features

-> Safe FOTA upgrade (password protected, MD5 control checksum of binary to assure integrity)

-> FOTA password uses random prefix and just MD5 hash of prefix+password is sent from the client back to server (password is not revealed and MD5 hash changes everytime)

![FOTA screenshot](https://raw.githubusercontent.com/wdim0/esp8266_rtos_whttpd/master/whttpd_fota_screenshot.png)

-> Own RO filesystem for served files (WFOF, no compression option at the moment, TODO - unix variant of static array generator needed)

-> Easily expandable preprocessor tags (tags in requested files can trigger callback functions and can be expanded to any data using callback functions)

-> Easily expandable POST multipart names (configurable distribution of <input ...> in HTML forms to proper callback data handlers)

-> Makefile for generating FOTA slot 1 and slot 2 binaries + MD5 control checksum files

-> Working on 1 or 2 MB flash memory with symmetric slot 1 / slot 2. (Makefile and altered LDs are configured for 1 MB)

-> Example pages include:
  - GPIO2 control
  - flash info and download
  - larger file capability demo
  - FOTA upgrade (slot 1 / slot 2)
