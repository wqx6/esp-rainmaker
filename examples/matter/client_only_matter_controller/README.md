# Non-Server Matter Controller Example

## Build and Flash firmware

Follow the ESP RainMaker Documentation [Get Started](https://rainmaker.espressif.com/docs/get-started.html) section to build and flash this firmware. Just note the path of this example.

## What to expect in this example?

- This example uses the UART console and [esp-rainmaker-cli](https://github.com/wqx6/esp-rainmaker-cli/tree/support_JWT_token) to connect the Matter Controller to Wi-Fi and to complete RainMaker user-node association.

- After Wi-Fi connection and user-node association, the esp-rainmaker-cli can setup the controller by the [Matter Controller service](./main/rmaker_controller_service/SPEC.md).

- Use [Command Response](https://docs.rainmaker.espressif.com/docs/dev/firmware/fw_usage_guides/command-response-usage) to send Matter commands to Matter End-Devices remotely.

- Get Matter end-devices attributes' value by the `Matter-devices` parameter in controller service.

## Steps to use this example

- Login with JWT token:

  ```
  esp-rainmaker-cli login --jwt-token <jwt_token>
  ```

- Flash the firmware and monitor the serial console.

- Connect the controller to Wi-Fi from the console:

  ```
  matter esp rmaker wifi-prov <ssid> [passphrase]
  ```

  For an open network, omit the passphrase. Wait for the device to get an IP address and connect to RainMaker.

- Print the RainMaker node ID if needed:

  ```
  matter esp rmaker get-node-id
  ```

- User Node Association

  Using rainmaker cli:

  ```
  esp-rainmaker-cli test --addnode <rmake_node_id>
  ```

  This command will print shared_secret and user id which can be use in the following command

- Complete user-node association from the console:

  ```
  matter esp rmaker add-user <user_id> <secret_key>
  ```

  Use the `user_id` and `secret_key` generated for the RainMaker user-node mapping flow by the RainMaker app, CLI, or user-node mapping API. After this command succeeds, the node is associated with that RainMaker user.

- Check whether the controller is added to rainmaker:

  ```
  esp-rainmaker-cli getnodes
  ```

  You can see the controller's node ID in the results.

- The same console also hosts Matter controller commands. RainMaker setup commands are available under `matter esp rmaker ...`, while controller commands are available after Wi-Fi is connected and the Matter stack starts, for example:

  ```
  matter esp controller help
  ```

- After user-node association, use rainmaker-cli to setup the controller so that the controller can get its NOC chain:

  ```
  esp-rainmaker-cli setparams --data '{"MatterCTLR": {"BaseURL": "<base-url>", "UserToken": "<jwt-token>", "RMakerGroupID": "<rainmaker-group-id>"}}' <controller-node-id>
  ```

  **NOTE**: If you didn't create any rainmaker-group(matter fabric), you can create it with this API:

  ```
  curl -X 'POST' \
  '<base-url>/v1/user/node_group' \
  -H 'accept: application/json' \
  -H 'Authorization: <access-token>' \
  -H 'Content-Type: application/json' \
  -d '{
  "is_matter": true,
  "group_name": "group_name"
  }'
  ```

- After the controller is setup, the controller will fetch the Matter device list and start to report the attributes' value of each device.

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
$ esp-rainmaker-cli getparams 99Skm2t5sGMui2fLATZ4i2
{
    "MatterCTLR": {
        "BaseURL": "https://v0iv7y5po7.execute-api.us-east-1.amazonaws.com/dev",
        "MTCtlCMD": -1,
        "MTCtlStatus": 127,
        "Matter-Devices": {
            "676faf22d3151705": {
                "endpoints": {
                    "0x1": {
                        "clusters": {
                            "servers": {
                                "0x3": {
                                    "attributes": {
                                        "0x0": 0,
                                        "0x1": 20
                                    }
                                },
                                "0x300": {
                                    "attributes": {
                                        "0x10": 0,
                                        "0x2": 0,
                                        "0x3": 24939,
                                        "0x4": 24701,
                                        "0x4001": 2,
                                        "0x400A": 24,
                                        "0x400B": 1,
                                        "0x400C": 65279,
                                        "0x400D": 1,
                                        "0x7": 250,
                                        "0x8": 2,
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
                                        "0x4001": 1,
                                        "0x4002": 0
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
                "rainmaker_node_id": "3JphZTNLqr3MxpSrkn8dvj"
            },
            "7aaa6abef599644f": {
                "endpoints": {
                    "0x1": {
                        "clusters": {
                            "servers": {
                                "0x3": {
                                    "attributes": {
                                        "0x0": 0,
                                        "0x1": 20
                                    }
                                },
                                "0x4": {
                                    "attributes": {
                                        "0x0": 128
                                    }
                                },
                                "0x6": {
                                    "attributes": {
                                        "0x0": false,
                                        "0x4000": true,
                                        "0x4001": 0,
                                        "0x4002": 0
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
                                }
                            }
                        }
                    }
                },
                "online": true,
                "rainmaker_node_id": "23qFhjTqShYDxQLrPUFBZX"
            }
        },
        "MatterNodeID": "AFEDFB94E792001B",
        "RMakerGroupID": "N4kcszpcx8TVwJjmTD7GHY",
        "UserToken": "XXX"
    },
    "MatterController": {
        "Name": "MatterController"
    },
    "Scenes": {
        "Scenes": []
    },
    "Schedule": {
        "Schedules": []
    },
    "Time": {
        "TZ": "Asia/Shanghai",
        "TZ-POSIX": "CST-8"
    }
}
```
