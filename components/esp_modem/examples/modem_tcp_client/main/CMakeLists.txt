if (CONFIG_EXAMPLE_MODEM_DEVICE_BG96)
    set(device_srcs sock_commands_bg96.cpp)
elseif(CONFIG_EXAMPLE_MODEM_DEVICE_SIM7600)
    set(device_srcs sock_commands_sim7600.cpp)
endif()

idf_component_register(SRCS "sock_commands_sim7600.cpp" "modem_client.cpp"
                            "sock_dce.cpp"
                            "${device_srcs}"
                       INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
