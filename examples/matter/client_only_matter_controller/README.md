# Non-Server Matter Controller Example

## Build and Flash firmware

esp-matter: 0b9133114b61306e7a9d4086a175511b6eb9e9bc
esp-idf: v5.5.3

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example uses [network_provision](https://github.com/espressif/idf-extra-components/tree/master/network_provisioning) to provision Matter Controller into Wi-Fi network.

- After provisioning, the [Phone APP](https://github.com/espressif/esp-rainmaker/blob/master/README.md#phone-apps) can setup the controller by the [Matter Controller service](./main/rmaker_controller_service/SPEC.md).

- Use [Command Response](https://docs.rainmaker.espressif.com/docs/dev/firmware/fw_usage_guides/command-response-usage) to send Matter commands to Matter End-Devices remotely.

- Get Matter end-devices attributes' value by the `Matter-devices` parameter in controller service.

## Steps to use this example

- Use RainMaker Phone App to provision the Matter Controller example to RainMaker home as a basic RainMaker device.

- After provisioning, click the button to setup the controller in the Phone APP. You need to select which group (Matter Fabric) that the controller will join.

- After the controller is setup the controller will fetch the Matter device list and start to report the attributes' value of each device.

- You can use the esp-rainmaker-cli to send the Matter command requests remotely.

  1. Invoke Command:
  ```
  $ esp-rainmaker-cli create_cmd_request 99Skm2t5sGMui2fLATZ4i2 4352 '{"objects": [{"matter_node_id": "0x676FAF22D3151705", "matter_endpoint_id": "0x1"}], "request_payload": {"cluster_id": "0x6", "command_id": "0x02", "command_fields": {}}}' --timeout 60
  Request Id: A3VPPxj9D8BYGXLgkokKQW
  Responses: [{'node_ids': ['99Skm2t5sGMui2fLATZ4i2'], 'response': {'status': 'success', 'description': 'in_progress'}}]

  $ esp-rainmaker-cli get_cmd_requests A3VPPxj9D8BYGXLgkokKQW
  Requests: [{'node_id': '99Skm2t5sGMui2fLATZ4i2', 'request_id': 'A3VPPxj9D8BYGXLgkokKQW', 'request_timestamp': 1776065900, 'response_timestamp': 1776065902, 'response_data': {'responses': [{'matter_endpoint_id': '0x1', 'matter_node_id': '0x676FAF22D3151705', 'status': 'success'}]}, 'request_data': {'objects': [{'matter_endpoint_id': '0x1', 'matter_node_id': '0x676FAF22D3151705'}], 'request_payload': {'cluster_id': '0x6', 'command_fields': {}, 'command_id': '0x02'}}, 'status': 'success', 'device_status': 0, 'expiration_timestamp': 1776065960, 'cmd': 4352}]
  Total: 1
  ```

  2. Write Attribue:
  ```
  $ esp-rainmaker-cli create_cmd_request 99Skm2t5sGMui2fLATZ4i2 4353 '{"objects": [{"matter_node_id": "0x676FAF22D3151705", "matter_endpoint_id": "0x1"}], "request_payload": {"cluster_id": "0x06", "attribute_id": "0x4001", "attribute_value": {"0:U16": 1}}}' --timeout 60
  Request Id: TB6JMRDjrgKMg8vEPwNxBQ
  Responses: [{'node_ids': ['99Skm2t5sGMui2fLATZ4i2'], 'response': {'status': 'success', 'description': 'in_progress'}}]

  $ esp-rainmaker-cli get_cmd_requests TB6JMRDjrgKMg8vEPwNxBQ
  Requests: [{'node_id': '99Skm2t5sGMui2fLATZ4i2', 'request_id': 'TB6JMRDjrgKMg8vEPwNxBQ', 'request_timestamp': 1776066034, 'response_timestamp': 1776066035, 'response_data': {'responses': [{'matter_endpoint_id': '0x1', 'matter_node_id': '0x676FAF22D3151705', 'status': 'success'}]}, 'request_data': {'objects': [{'matter_endpoint_id': '0x1', 'matter_node_id': '0x676FAF22D3151705'}], 'request_payload': {'attribute_id': '0x4001', 'attribute_value': {'0:U16': 1}, 'cluster_id': '0x06'}}, 'status': 'success', 'device_status': 0, 'expiration_timestamp': 1776066094, 'cmd': 4353}]
  Total: 1
  ```

  3. Read Attribute:
  ```
  $ esp-rainmaker-cli create_cmd_request 99Skm2t5sGMui2fLATZ4i2 4354 '{"matter_node_id": "0x676FAF22D3151705", "attribute_paths": [{"endpoint_id": "0xFFFF", "cluster_id": "0xFFFFFFFF", "attribute_id": "0xFFF9"}]}' --timeout 60
  Request Id: tWrrGyjqrpyM4dv2ZQVJE
  Responses: [{'node_ids': ['99Skm2t5sGMui2fLATZ4i2'], 'response': {'status': 'success', 'description': 'in_progress'}}]

  $ esp-rainmaker-cli get_cmd_requests tWrrGyjqrpyM4dv2ZQVJE
  Requests: [{'node_id': '99Skm2t5sGMui2fLATZ4i2', 'request_id': 'tWrrGyjqrpyM4dv2ZQVJE', 'request_timestamp': 1776066126, 'response_timestamp': 1776066128, 'response_data': {'matter_node_id': '0x676FAF22D3151705', 'read_results': [{'attribute_id': '0x0000fff9', 'attribute_value': [], 'cluster_id': '0x0000001d', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [], 'cluster_id': '0x0000001f', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [], 'cluster_id': '0x00000028', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 2, 4], 'cluster_id': '0x00000030', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 2, 4, 6, 8], 'cluster_id': '0x00000031', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 1], 'cluster_id': '0x00000033', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 2], 'cluster_id': '0x0000003c', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 2, 4, 6, 7, 9, 10, 11, 12, 13], 'cluster_id': '0x0000003e', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 1, 3, 4], 'cluster_id': '0x0000003f', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [], 'cluster_id': '0x00000036', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [], 'cluster_id': '0x00000035', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0], 'cluster_id': '0x0000002a', 'endpoint_id': '0'}, {'attribute_id': '0x0000fff9', 'attribute_value': [], 'cluster_id': '0x0000001d', 'endpoint_id': '1'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 64], 'cluster_id': '0x00000003', 'endpoint_id': '1'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 1, 2, 3, 4, 5], 'cluster_id': '0x00000004', 'endpoint_id': '1'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 64, 65, 66, 1, 2], 'cluster_id': '0x00000006', 'endpoint_id': '1'}, {'attribute_id': '0x0000fff9', 'attribute_value': [0, 1, 2, 3, 4, 5, 6, 7], 'cluster_id': '0x00000008', 'endpoint_id': '1'}, {'attribute_id': '0x0000fff9', 'attribute_value': [10, 75, 76, 7, 8, 9, 71], 'cluster_id': '0x00000300', 'endpoint_id': '1'}, {'attribute_id': '0x0000fff9', 'attribute_value': [64, 0, 1, 2, 3, 4, 5, 6], 'cluster_id': '0x00000062', 'endpoint_id': '1'}], 'status': 'success'}, 'request_data': {'attribute_paths': [{'attribute_id': '0xFFF9', 'cluster_id': '0xFFFFFFFF', 'endpoint_id': '0xFFFF'}], 'matter_node_id': '0x676FAF22D3151705'}, 'status': 'success', 'device_status': 0, 'expiration_timestamp': 1776066186, 'cmd': 4354}]
  Total: 1
  ```

- Get Attribute report of Matter End-Devices:

```
$ esp-rainmaker-cli getparams KEAnRfXXUDphieGKjhXkjN
{
    "MatterCTLSetup": {
        "MTCtlCMD": -1,
        "MTCtlStatus": 15,
        "MTDeviceDataVer": "1.1.0",
        "MTDevices": {
            "f2675ff9a1572a88": {
                "endpoints": {
                    "0x1": {
                        "clusters": {
                            "servers": {
                                "0x3": {
                                    "attributes": {
                                        "0x0": 0,
                                        "0x1": 1
                                    }
                                },
                                "0x300": {
                                    "attributes": {
                                        "0x10": 0,
                                        "0x2": 0,
                                        "0x3": 24939,
                                        "0x4": 24701,
                                        "0x4001": 2,
                                        "0x400C": 65279,
                                        "0x4010": "Null",
                                        "0x7": 250,
                                        "0xF": 0
                                    }
                                },
                                "0x4": {
                                    "attributes": {
                                        "0x0": 128
                                    }
                                },
                                "0x6": {
                                    "attributes": {
                                        "0x0": true,
                                        "0x4000": true,
                                        "0x4001": 0,
                                        "0x4002": 0,
                                        "0x4003": "Null"
                                    }
                                },
                                "0x62": {
                                    "attributes": {
                                        "0x1": 16,
                                        "0x2": [
                                            {
                                                "0x0": 0,
                                                "0x4": 7,
                                                "0xFE": 1
                                            },
                                            {
                                                "0x0": 0,
                                                "0x1": 0,
                                                "0x2": 0,
                                                "0x3": false,
                                                "0x4": 7,
                                                "0xFE": 2
                                            }
                                        ]
                                    }
                                },
                                "0x8": {
                                    "attributes": {
                                        "0x0": 64,
                                        "0x1": 0,
                                        "0x11": 64,
                                        "0x4000": 64,
                                        "0xF": 0
                                    }
                                }
                            }
                        }
                    }
                },
                "online": true,
                "rainmaker_node_id": "nerXsg9hADqTqYez9Zutsb"
            }
        },
        "RMakerGroupID": "TAwCaYHSCi7fff6GrG9swc"
    },
    "MatterController": {
        "Name": "Matter-Controller"
    },
    "RMUserAuth": {
        "BaseURL": "https://api.rainmaker.espressif.com",
        "UserToken": "eyJjdHkiOiJKV1QiLCJlbmMiOiJBMjU2R0NNIiwiYWxnIjoiUlNBLU9BRVAifQ.oz-EW8pIaoFq-SMngtTC86g_ULVevp8-hRreKNZNKE0FtCDnKrQiNB1-Z85B3eVZsQuoU1U5mjnilrjJFV2OKTinJUcIAv9uQEg7FcXK8SO5o8_JJj8823JNghma_VO6gg4YpumwYQmJLtC2Bx3XdoYx5xG8wAF6OUa3c1c9vAp6k35KuaZrmYAfYt47O89Ptn8h-ISZArIz1mEd29KE1Hizzq1KXaEzdDV3IxH51CbU9e6epRtSt3RUS5hbC7tqeX7HhRopMyJmB0cL8zA9Boe_lFnxiE_Nqopy4k0194jEzx1HFpjTFMoYoHkyow_L30wUcaO93o-0MG7vut4h5Q.yWngdqX0nd6hi0pl.PaDODoAZY20eX9uozp4owddxNQyNYIXoL85QqCurY_JF3OlV0RJXxoGFPi3vg0xZQXkcMq1iEsxX4btcQYzg9VmC-A542V5TOjv_eDXZ6oi6-UdXOFJkHiU6c8IOy9VQd5U4hapbosnLcVSe_iuRT67uvz5hfcn8352fTJjjhyOmehHBA_SsZJL2ias4HLMAsBWbAWjnlMf5FgWRR31rthOGFedoTVcK7G9RFZr5Uo8v5H5jg4JryLZrybSO5r3R1tqYd_TrphxSbk1NDMtH_uRtXLh1A1mMmt6PJXKNlmTfCIwnZWuUNHqG_gas_jrYfEt4QKpqmOeQIWiHRCDSE0M4h5ndTEX5A1UI2luR0G_Lx2G8sm71pDgvEBfNlJTwVTgq-w-sRr9arPohPxMaMv-AYPGRn7DKkHDoFrzPjq29irfwhtsg8jLYdb98i8MtF5ultY5YOCj9ZwfxpODdgvXuko_7jtLIK0g3R0IkVHdhEJYFT8mrZrqQ_lVmbuFYXDpODIMVAKVH8f7vXypLBbQZOfCh-StQ847F-TK1-DMZ7AJxtgTrscaP_VZOY6U2CmASQ4fOr06EBLI1BH8K0rPlA_1ghKOhCtNOZsRkTc9sS1rgcBsYXI_slyvbKnqTB3rAV4R6FFR_l7yYpAxSpMKSl1OpRS_WRASGZRGox-lacchRl845FvKVDuj8I2NqgC4Z6ZWT8tsBaWwoje4lyJ9JEdmZWH_SDnDpDFtNKnZhL5jcw64szrVXvBdT13DYrmQ1N_mc_WZ6syShnwuqfyt31-c0lM4Fn94krcMRUi3NdChNX40LXEcu1Qm7leXluXsMrHEFRTBOsG0AtaUVm7kLFy2PmS1FMHunnvTxnKmil0H969RaB7GyDaHA2pR-qFi7pDuKIKALKbnBwD0N_bCPEBzgiTTmnPpFITfmKO15DcOaGOIWBsMsWPSz7jKsbw3xez0qYhXpoALnTn5Un1t2BZWClNlz-MS1MeaNrej2bOOkTBQQfPCfPMFA83XmLpWp1AnppqOFQWVRoT7DoIe7Pog93EotiVn2RnSh0bQA1rt1M5RS3d-AbW60Ot30dI1U49qjdzc-AQQumgDsHy_q-jdnZdKJKgnlQMoWiPEjXOGgFY-e8XxsGLm6Gmt6f5TPIszFxCUzkbfKG2GbbFZkn1jamb1vQlba8K4ZCxOWsaJuPcwR2O1-K355Lb2T1VtjHq_Ro26Rl1I0CtVeJG4w2yoPpMzw7sTrbnrKGSza0y_xMLb1HEVetAyrLgD-0LWOTww9DF5ZdiU7fN6on17qVfebc-3kM_zwXE899niIHDMyblKl.BjpQkQ3BtFU36rFSxwhDKg",
        "UserTokenStatus": 1
    },
    "Scenes": {
        "Scenes": []
    },
    "Schedule": {
        "Schedules": []
    },
    "System": {
        "Factory-Reset": false,
        "Reboot": false,
        "Wi-Fi-Reset": false
    },
    "Time": {
        "TZ": "Asia/Shanghai",
        "TZ-POSIX": "CST-8"
    }
}
```

### Use with Fabric Interface APIs

0. Get Matter Nodes:

```
$ curl -X 'GET' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/devices?rainmaker_group_id=N4kcszpcx8TVwJjmTD7GHY&node_details=true' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImNlMzE5Mzc3LWNiZDctNDVhMC04YjBhLTY3YzE5NTEwZjlkYiIsImV2ZW50X2lkIjoiYWU0YjZkNTUtNzFjNS00MjNmLWJlYTItYTdlNzZiN2IwMDg0IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA3NzkzNCwiZXhwIjoxNzgxMDgxNTM0LCJpYXQiOjE3ODEwNzc5MzQsImp0aSI6IjY0MmM5YjhkLTUwZjktNDhkYi04OGNkLTlhY2U3MGM0YTkxZSIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.j-gLMpcZoca1AP5y2X5bMJ0vQ888T0T0NUoe4RFOEBUrVSNinYTnaIQGaTWwS4ascDRX9xoy8iLIs3Y-u1VEP9CbI4Go1Fr0PBu18BcJTSGwzAkrjiBH_xim2Iuws1z8GG_xio1uH1KGd0STl_iCyNHDEJITa77dbLab2CtxwpHzXOMywmbz2y856nDaaQFTxrHNqGNyrNdRt5HWquLaSjOe4IW1JfTp8Tf0UOtMva4W_HjdkUTPsa2nRKAoOPW5046B3foCGJUxCXTstscZYkVx-7qBuHzEhjS9VTRqD8GL3Tw67NYFUyHvzwZVYs598SNPBgcFXCaZM9om14qXIQ'
{"rainmaker_group_id": "N4kcszpcx8TVwJjmTD7GHY", "matter_fabric_id": "93C716629CE6FCE3", "node_count": 2, "nodes": [{"rainmaker_node_id": "4Pvx3DmUBgCp8dsf3KqmPk", "matter_node_id": "150554BE87C8686A", "is_controller": true}, {"rainmaker_node_id": "YFNnd5ESL7XPGikve3va2K", "matter_node_id": "880CCF63458515AE", "node_name": "Matter Accessory", "data_model": [{"endpoint_id": 1, "device_types": [{"id": "0x010D", "name": "Extended Color Light"}], "application_attributes": [{"attribute_id": "0x0000", "attribute_name": "OnOff", "type": "boolean", "description": "The attribute indicates whether the device type implemented on the endpoint is turned off or turned on", "cluster_id": "0x6"}, {"attribute_id": "0x0000", "attribute_name": "CurrentLevel", "type": "uint8", "description": "This attribute SHALL indicate the current level of this device", "cluster_id": "0x8"}, {"attribute_id": "0x0003", "attribute_name": "CurrentX", "type": "uint16", "description": "This attribute SHALL indicate the current value of the normalized chromaticity value x, as defined in the CIE xyY Color Space. The value of x SHALL be related to the CurrentX attribute by the relationship: x = CurrentX / 65536", "cluster_id": "0x300"}, {"attribute_id": "0x0004", "attribute_name": "CurrentY", "type": "uint16", "description": "This attribute SHALL indicate the current value of the normalized chromaticity value y, as defined in the CIE xyY Color Space. The value of y SHALL be related to the CurrentY attribute by the relationship: y = CurrentY / 65536", "cluster_id": "0x300"}, {"attribute_id": "0x0007", "attribute_name": "ColorTemperatureMireds", "type": "uint16", "description": "This attribute SHALL indicate a scaled inverse of the current value of the color temperature. The color temperature value in kelvins SHALL be related to the ColorTemperatureMireds attribute in mired by the relationship: Color temperature [K] = 1,000,000 / ColorTemperatureMireds", "cluster_id": "0x300"}], "writable_attributes": [{"attribute_id": "0x4003", "attribute_name": "StartUpOnOff", "type": "uint8", "description": "The attribute defines the desired startup behavior of a device when it is supplied with power and this state SHALL be reflected in the OnOff attribute", "cluster_id": "0x6"}], "invokable_commands": [{"cluster_id": "0x6", "command_id": "0x0000", "command_name": "Off", "command_fields": [], "description": "Set the OnOff attribute to false"}, {"cluster_id": "0x6", "command_id": "0x0001", "command_name": "On", "command_fields": [], "description": "Set the OnOff attribute to true"}, {"cluster_id": "0x6", "command_id": "0x0002", "command_name": "Toggle", "command_fields": [], "description": "Toggle the OnOff attribute"}, {"cluster_id": "0x8", "command_id": "0x0000", "command_name": "MoveToLevel", "command_fields": [{"id": "0", "name": "Level", "type": "uint8"}, {"id": "1", "name": "TransitionTime", "type": "uint16"}, {"id": "2", "name": "OptionsMask", "type": "uint8"}, {"id": "3", "name": "OptionsOverride", "type": "uint8"}], "description": "This command can move the CurrentLevel to the value given in the Level field in the time of TransitionTime field. Use 1 as the default value of OptionsMask and OptionsOverride."}, {"cluster_id": "0x300", "command_id": "0x000A", "command_name": "MoveToColorTemperature", "command_fields": [{"id": "0", "name": "ColorTemperatureMireds", "type": "uint16"}, {"id": "1", "name": "TransitionTime", "type": "uint16"}, {"id": "2", "name": "OptionsMask", "type": "uint8"}, {"id": "3", "name": "OptionsOverride", "type": "uint8"}], "description": "This command can move the ColorTemperatureMired attribute to the value given in the ColorTemperatureMired field in the time of TransitionTime field. Use 1 as the default value of OptionsMask and OptionsOverride."}, {"cluster_id": "0x300", "command_id": "0x0007", "command_name": "MoveToColor", "command_fields": [{"id": "0", "name": "ColorX", "type": "uint16"}, {"id": "1", "name": "ColorY", "type": "uint16"}, {"id": "2", "name": "TransitionTime", "type": "uint16"}, {"id": "3", "name": "OptionsMask", "type": "uint8"}, {"id": "4", "name": "OptionsOverride", "type": "uint8"}], "description": "This command can move the CurrentX and CurrentY to the value given in the ColorX and ColorY field in the time of TransitionTime field. Use 1 as the default value of OptionsMask and OptionsOverride."}], "node_details": {"Online": true, "Endpoint_1": {"OnOff": {"OnOff": false}, "LevelControl": {"CurrentLevel": 1}, "ColorControl": {"CurrentX": 24939, "CurrentY": 24701, "ColorTemperatureMireds": 250}}}}]}]}
```

1. Invoke Command:

```
$ curl -X 'POST' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/interaction/invoke' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImJhMGNhMmExLTdhNjEtNGU2ZC1iNzY5LWEzMzU3ODZhOWVkNiIsImV2ZW50X2lkIjoiZTZjNWQ3ZjktNzMzYy00Yjg2LTljMzYtYTRkNjYwOTI1MzI3IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA2MTMwMywiZXhwIjoxNzgxMDY0OTAzLCJpYXQiOjE3ODEwNjEzMDMsImp0aSI6IjgyNDJhODEwLWYyOWItNDM0Yi1iMWNhLWJmNTk1MzIxZTRjYiIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.PTYhR6w_jqLvU61Fj7pZ1iOOSEW2tTGFdvOjC-ZqnM3U90mlMqn5ff6kAj_mQ9RWKPfavY831cjs21qETjYVFetEgUMsUKtqFwWCAxfR5uMJ6gSsXoLyzdqAHZR10O2f4gGA5lXp0XtDfNc2iBGr-HbEnwk9SLqxd1yhd75NA8uZH-gNZR_he1aCgvwgNFyfD-U1U83FPLuqYfiVlH_kzD8sINdPqXXCwCEh-fGxaOtKGc_ia2ycIWZnx9sRI5dD1mj21EPJ8QRUQ9FJRj_KsCCMqhUIwL3Ev2Fc8fzd2GVaZUA1Z-NCYMlR6pIQalAbVRvg7EOOW-VPsGeyPA1eUw' \
  -H 'Content-Type: application/json' \
  -d '{
        "rainmaker_group_id": "N4kcszpcx8TVwJjmTD7GHY"
        "controller_rainmaker_node_id": "4Pvx3DmUBgCp8dsf3KqmPk",
        "timeout": 30,
        "objects": [
          {
            "matter_node_id": "0x880CCF63458515AE",
            "matter_endpoint_id": "0x1"
          }
        ],
        "request_payload": {
          "cluster_id": "0x0006",
          "command_id": "0x0002",
          "command_fields": {}
        }
      }'
{"request_id": "ZjiqkMdzamEQNE46sLLH9S", "response": {"status": "success", "description": "in_progress"}}

$ curl -X 'GET' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/interaction/response?request_id=ZjiqkMdzamEQNE46sLLH9S' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImJhMGNhMmExLTdhNjEtNGU2ZC1iNzY5LWEzMzU3ODZhOWVkNiIsImV2ZW50X2lkIjoiZTZjNWQ3ZjktNzMzYy00Yjg2LTljMzYtYTRkNjYwOTI1MzI3IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA2MTMwMywiZXhwIjoxNzgxMDY0OTAzLCJpYXQiOjE3ODEwNjEzMDMsImp0aSI6IjgyNDJhODEwLWYyOWItNDM0Yi1iMWNhLWJmNTk1MzIxZTRjYiIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.PTYhR6w_jqLvU61Fj7pZ1iOOSEW2tTGFdvOjC-ZqnM3U90mlMqn5ff6kAj_mQ9RWKPfavY831cjs21qETjYVFetEgUMsUKtqFwWCAxfR5uMJ6gSsXoLyzdqAHZR10O2f4gGA5lXp0XtDfNc2iBGr-HbEnwk9SLqxd1yhd75NA8uZH-gNZR_he1aCgvwgNFyfD-U1U83FPLuqYfiVlH_kzD8sINdPqXXCwCEh-fGxaOtKGc_ia2ycIWZnx9sRI5dD1mj21EPJ8QRUQ9FJRj_KsCCMqhUIwL3Ev2Fc8fzd2GVaZUA1Z-NCYMlR6pIQalAbVRvg7EOOW-VPsGeyPA1eUw'
{"status": "success", "response_data": {"responses": [{"matter_endpoint_id": "0x1", "matter_node_id": "0x880ccf63458515ae", "status": "success"}]}}
```

2. Write Command:
```
$ curl -X 'POST' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/interaction/write' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImNlMzE5Mzc3LWNiZDctNDVhMC04YjBhLTY3YzE5NTEwZjlkYiIsImV2ZW50X2lkIjoiYWU0YjZkNTUtNzFjNS00MjNmLWJlYTItYTdlNzZiN2IwMDg0IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA3NzkzNCwiZXhwIjoxNzgxMDgxNTM0LCJpYXQiOjE3ODEwNzc5MzQsImp0aSI6IjY0MmM5YjhkLTUwZjktNDhkYi04OGNkLTlhY2U3MGM0YTkxZSIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.j-gLMpcZoca1AP5y2X5bMJ0vQ888T0T0NUoe4RFOEBUrVSNinYTnaIQGaTWwS4ascDRX9xoy8iLIs3Y-u1VEP9CbI4Go1Fr0PBu18BcJTSGwzAkrjiBH_xim2Iuws1z8GG_xio1uH1KGd0STl_iCyNHDEJITa77dbLab2CtxwpHzXOMywmbz2y856nDaaQFTxrHNqGNyrNdRt5HWquLaSjOe4IW1JfTp8Tf0UOtMva4W_HjdkUTPsa2nRKAoOPW5046B3foCGJUxCXTstscZYkVx-7qBuHzEhjS9VTRqD8GL3Tw67NYFUyHvzwZVYs598SNPBgcFXCaZM9om14qXIQ' \
  -H 'Content-Type: application/json' \
  -d '{
        "rainmaker_group_id": "N4kcszpcx8TVwJjmTD7GHY",
        "controller_rainmaker_node_id": "4Pvx3DmUBgCp8dsf3KqmPk",
        "timeout": 30,
        "objects": [
          {
            "matter_node_id": "0x880CCF63458515AE",
            "matter_endpoint_id": "0x1"
          }
        ],
        "request_payload": {
          "cluster_id": "0x0006",
          "attribute_id": "0x4003",
          "attribute_value": null
        }
      }'
{"request_id": "KADJaxmuUkfV7tqrMybKsU", "response": {"status": "success", "description": "in_progress"}}

$ curl -X 'GET' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/interaction/response?request_id=KADJaxmuUkfV7tqrMybKsU' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImNlMzE5Mzc3LWNiZDctNDVhMC04YjBhLTY3YzE5NTEwZjlkYiIsImV2ZW50X2lkIjoiYWU0YjZkNTUtNzFjNS00MjNmLWJlYTItYTdlNzZiN2IwMDg0IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA3NzkzNCwiZXhwIjoxNzgxMDgxNTM0LCJpYXQiOjE3ODEwNzc5MzQsImp0aSI6IjY0MmM5YjhkLTUwZjktNDhkYi04OGNkLTlhY2U3MGM0YTkxZSIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.j-gLMpcZoca1AP5y2X5bMJ0vQ888T0T0NUoe4RFOEBUrVSNinYTnaIQGaTWwS4ascDRX9xoy8iLIs3Y-u1VEP9CbI4Go1Fr0PBu18BcJTSGwzAkrjiBH_xim2Iuws1z8GG_xio1uH1KGd0STl_iCyNHDEJITa77dbLab2CtxwpHzXOMywmbz2y856nDaaQFTxrHNqGNyrNdRt5HWquLaSjOe4IW1JfTp8Tf0UOtMva4W_HjdkUTPsa2nRKAoOPW5046B3foCGJUxCXTstscZYkVx-7qBuHzEhjS9VTRqD8GL3Tw67NYFUyHvzwZVYs598SNPBgcFXCaZM9om14qXIQ'
{"status": "success", "response_data": {"responses": [{"matter_endpoint_id": "0x1", "matter_node_id": "0x880ccf63458515ae", "status": "success"}]}}
```

3. Read Command:
```
$ curl -X 'POST' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/interaction/read' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImJhMGNhMmExLTdhNjEtNGU2ZC1iNzY5LWEzMzU3ODZhOWVkNiIsImV2ZW50X2lkIjoiZTZjNWQ3ZjktNzMzYy00Yjg2LTljMzYtYTRkNjYwOTI1MzI3IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA2MTMwMywiZXhwIjoxNzgxMDY0OTAzLCJpYXQiOjE3ODEwNjEzMDMsImp0aSI6IjgyNDJhODEwLWYyOWItNDM0Yi1iMWNhLWJmNTk1MzIxZTRjYiIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.PTYhR6w_jqLvU61Fj7pZ1iOOSEW2tTGFdvOjC-ZqnM3U90mlMqn5ff6kAj_mQ9RWKPfavY831cjs21qETjYVFetEgUMsUKtqFwWCAxfR5uMJ6gSsXoLyzdqAHZR10O2f4gGA5lXp0XtDfNc2iBGr-HbEnwk9SLqxd1yhd75NA8uZH-gNZR_he1aCgvwgNFyfD-U1U83FPLuqYfiVlH_kzD8sINdPqXXCwCEh-fGxaOtKGc_ia2ycIWZnx9sRI5dD1mj21EPJ8QRUQ9FJRj_KsCCMqhUIwL3Ev2Fc8fzd2GVaZUA1Z-NCYMlR6pIQalAbVRvg7EOOW-VPsGeyPA1eUw' \
  -H 'Content-Type: application/json' \
  -d '{
        "rainmaker_group_id": "N4kcszpcx8TVwJjmTD7GHY",
        "controller_rainmaker_node_id": "4Pvx3DmUBgCp8dsf3KqmPk",
        "timeout": 30,
        "matter_node_id": "0x880CCF63458515AE",
        "attribute_paths": [
          {
            "endpoint_id": "0xFFFF",
            "cluster_id": "0xFFFFFFFF",
            "attribute_id": "0xFFF9"
          }
        ]
      }'
{"request_id": "mPkcBpkvDw2CGPkmcqgDeH", "response": {"status": "success", "description": "in_progress"}}

$ curl -X 'GET' \
  'https://sp8ic1jze3.execute-api.us-east-1.amazonaws.com/dev/v1/matter/interaction/response?request_id=mPkcBpkvDw2CGPkmcqgDeH' \
  -H 'accept: application/json' \
  -H 'Authorization: eyJraWQiOiJlWkJ4UThnSUpGY2ZITVRnYmJxOUYvWjZHcVpwb0dsUVo2RndZc0VSVDJNPSIsImFsZyI6IlJTMjU2In0.eyJzdWIiOiI0NGY4NzRmOC05MGUxLTcwNjQtOGFiMi01NzBhZjVmZjQzY2EiLCJpc3MiOiJodHRwczovL2NvZ25pdG8taWRwLnVzLWVhc3QtMS5hbWF6b25hd3MuY29tL3VzLWVhc3QtMV9IeklJczJGdjMiLCJjbGllbnRfaWQiOiJmdjFvczZpa3FhYW8yOHBldWdndjQ0azh2Iiwib3JpZ2luX2p0aSI6ImJhMGNhMmExLTdhNjEtNGU2ZC1iNzY5LWEzMzU3ODZhOWVkNiIsImV2ZW50X2lkIjoiZTZjNWQ3ZjktNzMzYy00Yjg2LTljMzYtYTRkNjYwOTI1MzI3IiwidG9rZW5fdXNlIjoiYWNjZXNzIiwic2NvcGUiOiJhd3MuY29nbml0by5zaWduaW4udXNlci5hZG1pbiIsImF1dGhfdGltZSI6MTc4MTA2MTMwMywiZXhwIjoxNzgxMDY0OTAzLCJpYXQiOjE3ODEwNjEzMDMsImp0aSI6IjgyNDJhODEwLWYyOWItNDM0Yi1iMWNhLWJmNTk1MzIxZTRjYiIsInVzZXJuYW1lIjoiNDRmODc0ZjgtOTBlMS03MDY0LThhYjItNTcwYWY1ZmY0M2NhIn0.PTYhR6w_jqLvU61Fj7pZ1iOOSEW2tTGFdvOjC-ZqnM3U90mlMqn5ff6kAj_mQ9RWKPfavY831cjs21qETjYVFetEgUMsUKtqFwWCAxfR5uMJ6gSsXoLyzdqAHZR10O2f4gGA5lXp0XtDfNc2iBGr-HbEnwk9SLqxd1yhd75NA8uZH-gNZR_he1aCgvwgNFyfD-U1U83FPLuqYfiVlH_kzD8sINdPqXXCwCEh-fGxaOtKGc_ia2ycIWZnx9sRI5dD1mj21EPJ8QRUQ9FJRj_KsCCMqhUIwL3Ev2Fc8fzd2GVaZUA1Z-NCYMlR6pIQalAbVRvg7EOOW-VPsGeyPA1eUw'
{"status": "success", "response_data": {"matter_node_id": "0x880ccf63458515ae", "read_results": [{"attribute_id": "0x0000fff9", "attribute_value": [], "cluster_id": "0x0000001d", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [], "cluster_id": "0x0000001f", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [], "cluster_id": "0x00000028", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 2, 4], "cluster_id": "0x00000030", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 2, 4, 6, 8], "cluster_id": "0x00000031", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 1], "cluster_id": "0x00000033", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 2], "cluster_id": "0x0000003c", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 2, 4, 6, 7, 9, 10, 11, 12, 13], "cluster_id": "0x0000003e", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 1, 3, 4], "cluster_id": "0x0000003f", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [], "cluster_id": "0x00000036", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 1, 3, 4, 5], "cluster_id": "0x00000065", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [0], "cluster_id": "0x0000002a", "endpoint_id": "0"}, {"attribute_id": "0x0000fff9", "attribute_value": [], "cluster_id": "0x0000001d", "endpoint_id": "1"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 64], "cluster_id": "0x00000003", "endpoint_id": "1"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 1, 2, 3, 4, 5], "cluster_id": "0x00000004", "endpoint_id": "1"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 64, 65, 66, 1, 2], "cluster_id": "0x00000006", "endpoint_id": "1"}, {"attribute_id": "0x0000fff9", "attribute_value": [0, 1, 2, 3, 4, 5, 6, 7], "cluster_id": "0x00000008", "endpoint_id": "1"}, {"attribute_id": "0x0000fff9", "attribute_value": [10, 75, 76, 7, 8, 9, 71], "cluster_id": "0x00000300", "endpoint_id": "1"}, {"attribute_id": "0x0000fff9", "attribute_value": [64, 0, 1, 2, 3, 4, 5, 6], "cluster_id": "0x00000062", "endpoint_id": "1"}]}}
```
