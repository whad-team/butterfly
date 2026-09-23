import os, sys
import usb.core
import usb.util

def get_usb_info_from_port(port_name):
    dev_name = os.path.basename(port_name)
    sys_path = f"/sys/class/tty/{dev_name}/device"
    
    if not os.path.exists(sys_path):
        return None

    real_path = os.path.realpath(sys_path)
    parts = real_path.split('/')

    for i in range(len(parts), 0, -1):
        parent_path = '/'.join(parts[:i])
        bus_file = os.path.join(parent_path, "busnum")
        dev_file = os.path.join(parent_path, "devnum")
        
        if os.path.exists(bus_file) and os.path.exists(dev_file):
            with open(bus_file, 'r') as f:
                bus = int(f.read().strip())
            with open(dev_file, 'r') as f:
                addr = int(f.read().strip())
            return bus, addr
    return None

def trigger_dfu(target_port):
    usb_info = get_usb_info_from_port(target_port)
    if not usb_info:
        print("Error: no serial port found")
        return False
        
    target_bus, target_addr = usb_info
    dev = usb.core.find(idVendor=0xc0ff, idProduct=0xeeee, bus=target_bus, address=target_addr)

    if dev is None:
        print(f"Error: no device found on {target_port}")
        return False

    print("Triggering DFU...")
    try:
        dev.ctrl_transfer(0x21, 0, 0, 2, b"")
        return True
    except usb.core.USBError as e:
        return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python3 trigger_dfu.py /dev/ttyACMX")
        sys.exit(1)

    port = sys.argv[1]
    if trigger_dfu(port):
        print("Success !")
    else:
        print("Failure.")
