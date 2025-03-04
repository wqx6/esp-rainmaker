idf_component_register(SRCS ./app_main.cpp
                            ./matter_cwm/dynamic_commissionable_data_provider.cpp
                            ./matter_cwm/matter_commissioning_window_management_std.cpp
                            ./matter_cwm/matter_commissioning_window_management.cpp
                       INCLUDE_DIRS .
                                    ./matter_cwm)

set_property(TARGET ${COMPONENT_LIB} PROPERTY CXX_STANDARD 17)
target_compile_options(${COMPONENT_LIB} PRIVATE "-DCHIP_HAVE_CONFIG_H")
