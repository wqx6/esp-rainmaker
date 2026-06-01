idf_component_register(SRCS "./app_main.cpp"
                            "./matter_attr_report.cpp"
                            "./matter_controller_cmd_resp.cpp"
                            "./matter_command_list_sync.cpp"
                            "./op_creds_issuer/app_matter_controller_creds_issuer.cpp"
                       INCLUDE_DIRS "." "op_creds_issuer")
