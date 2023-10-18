#include "HardwareManager.h"

#include "ConfigurationRS/CFGCommonRS/CFGCommonRS.h"
#include "libusb.h"

#define HM_USB_DESC_LENGTH (256)
#define HM_DEFAULT_CABLE_SPEED_KHZ (1000)

const std::vector<HardwareManager_CABLE_INFO> HardwareManager::m_cable_db = {
    {"RsFtdi", FTDI, 0x0403, 0x6011},
    {"RsFtdi", FTDI, 0x0403, 0x6010},
    {"RsFtdi", FTDI, 0x0403, 0x6014},
    {"Jlink", JLINK, 0x1366, 0x0101}};

HardwareManager::HardwareManager(JtagAdapter* adapter) : m_adapter(adapter) {
  CFG_ASSERT(m_adapter != nullptr);
}

HardwareManager::~HardwareManager() {}

std::vector<Cable> HardwareManager::get_cables() {
  struct libusb_context* ctx = nullptr;         /**< Libusb context **/
  struct libusb_device** device_list = nullptr; /**< The usb device list **/
  struct libusb_device_handle* device_handle = nullptr;
  uint32_t cable_index = 1;
  char desc_string[HM_USB_DESC_LENGTH]; /* Max size of string descriptor */
  int rc;
  int device_count;
  std::vector<Cable> cables;

  rc = libusb_init(&ctx);
  CFG_ASSERT_MSG(rc == 0, "libusb_init() fail. Error: %s",
                 libusb_error_name(rc));

  device_count = (int)libusb_get_device_list(ctx, &device_list);
  for (int index = 0; index < device_count; index++) {
    struct libusb_device_descriptor device_descriptor;

    if (libusb_get_device_descriptor(device_list[index], &device_descriptor) !=
        0) {
      continue;
    }

    for (auto& cable_info : m_cable_db) {
      if (device_descriptor.idVendor == cable_info.vid &&
          device_descriptor.idProduct == cable_info.pid) {
        Cable cable{};

        cable.index = cable_index++;
        cable.vendor_id = device_descriptor.idVendor;
        cable.product_id = device_descriptor.idProduct;
        cable.port_addr = libusb_get_port_number(device_list[index]);
        cable.device_addr = libusb_get_device_address(device_list[index]);
        cable.bus_addr = libusb_get_bus_number(device_list[index]);
        cable.name = cable_info.name + "_" + std::to_string(cable.bus_addr) +
                     "_" + std::to_string(cable.port_addr);
        cable.cable_type = cable_info.type;
        cable.description = "";
        cable.serial_number =
            "";  // Note: not all usb cable has a serial number
        cable.speed = HM_DEFAULT_CABLE_SPEED_KHZ;
        cable.transport = TransportType::JTAG;

        rc = libusb_open(device_list[index], &device_handle);
        if (rc < 0) {
          // free libusb resource
          libusb_free_device_list(device_list, 1);
          libusb_exit(ctx);
          CFG_ASSERT_MSG(false, "libusb_open() fail. Error: %s",
                         libusb_error_name(rc));
        }

        rc = libusb_get_string_descriptor_ascii(
            device_handle, device_descriptor.iProduct,
            (unsigned char*)desc_string, sizeof(desc_string));
        if (rc == 0) {
          cable.description = desc_string;
        }

        rc = libusb_get_string_descriptor_ascii(
            device_handle, device_descriptor.iSerialNumber,
            (unsigned char*)desc_string, sizeof(desc_string));
        if (rc == 0) {
          cable.serial_number = desc_string;
        }

        libusb_close(device_handle);
        cables.push_back(cable);
      }
    }
  }

  if (device_list != nullptr) {
    libusb_free_device_list(device_list, 1);
  }

  if (ctx != nullptr) {
    libusb_exit(ctx);
  }

  return cables;
}

std::vector<Tap> HardwareManager::get_taps(const Cable& cable) {
  auto taps = m_adapter->get_taps(cable);

  for (auto& tap : taps) {
    for (auto& device_info : m_device_db) {
      if ((tap.idcode & device_info.irmask) ==
          (device_info.idcode & device_info.irmask)) {
        tap.irlength = device_info.irlength;
      }
    }
  }

  return taps;
}

std::vector<Device> HardwareManager::get_devices(const Cable& cable,
                                                 std::vector<Tap>* output) {
  auto taps = m_adapter->get_taps(cable);
  uint32_t device_index = 1;
  std::vector<Device> devices{};

  for (auto& tap : taps) {
    bool found = false;
    for (auto& device_info : m_device_db) {
      if ((tap.idcode & device_info.irmask) ==
          (device_info.idcode & device_info.irmask)) {
        found = true;
        tap.irlength = device_info.irlength;
        if (device_info.type == GEMINI || device_info.type == OCLA) {
          Device device{};
          device.index = device_index++;
          device.type = device_info.type;
          device.name = device_info.name;
          device.tap.index = tap.index;
          device.tap.idcode = tap.idcode;
          device.tap.irlength = device_info.irlength;
          devices.push_back(device);
        }
        break;
      }
    }

    CFG_ASSERT_MSG(found, "Unknown tap id 0x%08x", tap.idcode);
  }

  // return tap info
  if (output) {
    output->clear();
    *output = taps;
  }

  return devices;
}
