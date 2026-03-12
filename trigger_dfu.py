import usb.core
import usb.util

def trigger_dfu():
    dev = usb.core.find(idVendor=0xc0ff, idProduct=0xeeee)
    
    if dev is None:
        return False

    try:
        dev.ctrl_transfer(0x21, 0, 0, 2, b"0")
        return True
    except usb.core.USBError:
        return True


if trigger_dfu():
    print("OK")
