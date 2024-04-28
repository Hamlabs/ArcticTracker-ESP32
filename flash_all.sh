#!/bin/bash

esptool.py \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader.bin \
  0x30000 ArcticTracker.bin \
  0x8000 partition-table.bin \
  0x1e000 ota_data_initial.bin \
  0x430000 webapp.bin
  
