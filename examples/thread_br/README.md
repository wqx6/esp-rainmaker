# Thread Border Router Example

## Hardware Required

Please use the ESP Thread Border Router Board for this example. It provides an integrated module of an ESP32-S3 SoC and an ESP32-H2 RCP.

![br_dev_kit](./image/esp-thread-border-router-board.png)

## Build and Flash firmware

### Build the RCP firmware

The Border Router supports updating the RCP upon boot.

First build the [ot_rcp](https://github.com/espressif/esp-idf/tree/master/examples/openthread/ot_rcp) example in IDF.

### Build and Flash Thread Border Router Firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

If you are using IDF v5.3.1 or later, there might be an error of `#error CONFIG_LWIP_IPV6_NUM_ADDRESSES should be set to 12` when building this example. Please change the IPv6 addresses number for LwIP network interface in menuconfig and rebuild the example again.

In the building process of Thread Border Router firmware, the built RCP image in IDF path will be automatically packed into the Border Router firmware.

## What to expect in this example?

- This example demonstrates an OpenThread Border Router on [ESP Thread Border Router Board](https://github.com/espressif/esp-thread-br/tree/main?tab=readme-ov-file#esp-thread-border-router-board).
- The ESP32-H2 on ESP Thread Border Router Board acts as an [OpenThread Radio-Co Processor](https://openthread.io/platforms/co-processor) and the ESP32-S3 is the host processor. They are connected by UART.
- You could set the Thread active dataset and start Thread network of the Thread Border Router with the phone APP or [esp-rainamaker-cli](https://pypi.org/project/esp-rainmaker-cli/)

### Start Thread network

After provisioning with the RainMaker phone APP, you can setup the Thread network on the phone app. After clicking `Update Thread Dataset` button, the phone app will send its preferred Thread credential to the Thread BR if the phone app finds an existing preferred credential.
Otherwise the phone app will send a command to the Thread BR so that the BR can genetate a new Thread dataset, and it will read the new generate dataset and store it.

And you can also use esp-rainmaker-cli to setup the Thread network.

Generate a random Thread dataset, set it as active dataset and start Thread network:

```
$ esp-rainmaker-cli setparams --data '{"TBRService":{"ThreadCmd": 1}}' 3485187E7F68
Node state updated successfully.
```

Or set a specific active dataset and start Thread network:

```
$ esp-rainmaker-cli setparams --data '{"TBRService":{"ActiveDataset": "0E080000000000010000000300001235060004001FFFE00208DE45772E58CAC8CE0708FD01321F6B80688105101CBF6F4E68CBC611B52ED9A39EFD80A9030F4F70656E5468726561642D616534370102AE470410AB7CDEB095B2C453E6CE7E7DB2BC52980C0402A0F7F8"}}' 3485187E7F68
Node state updated successfully.
```

### Matter Commissining

After RainMaker provisioning, you can open the Matter commissioning window by writing the WindowOpen to true with esp-rainmaker-cli.

- Login with the same account that the phone APP uses

```
esp-rainmaker-cli login
```

- Write the WindowOpen to true

```
esp-rainmaker-cli setparams --data '{"MatterCWM":{"WindowOpen": true}}' <rainmaker-node-id>
```

After opening the commissioning window, you can get the SetupPIN, Discriminator, VendorID, and ProductID, which can be used to generate QRCode/ManaulCode for Matter commissioing.

```
esp-rainmaker-cli getparams <<rainmaker-node-id>>
```

Matter provides APIs to generate QRcode with the SetupPIN, Discriminator, VendorID, and ProductID. Here we use chip-tool to generate the QRCode

```
./chip-tool payload generate-qrcode --discriminator <discriminator> --setup-pin-code <setup-pin> --vendor-id <vendor-id> --product-id <product-id>

```
