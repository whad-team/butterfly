# ButteRFly, a WHAD-compatible firmware for nRF52-based USB dongles

## Introduction

ButteRFly is a firmware specifically designed to be used with [WHAD](https://github.com/whad-team/whad-client)
that provides the following features:

- Bluetooth Low Energy connection sniffing, scanning, hijacking and PDU injection
- ZigBee sniffing, scanning and packet injection
- Nordic Semiconductor's Enhanced ShockBurst protocol sniffing, scanning and packet injection
- Logitech Unifying sniffing, scanning and packet injection

## Installation 

Follow [WHAD's documentation instructions](https://whad.readthedocs.io/en/stable/device/compat.html#makerdiary-nrf52840-mdk-usb-dongle) to install this firmware on a Makerdiary nRF52 MDK USB dongle
or a Nordic's nRF52 dongle.

## Reference

ButteRFly has been initially released as a proof-of-concept by Romain Cayre in his presentation
at IEEE/IFIP International Conference on Dependable Systems and Networks (DSN), Jun 2021, Taipei (virtual), Taiwan.
The related paper is [available here](https://laas.hal.science/hal-03193297v2/file/injectable_final_version.pdf).