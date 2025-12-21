import json

def generate():
    boards = [
        {"name": "pico", "variable": "pico_w", "display_name": "Pico"},
        {"name": "pico2", "variable": "pico2_w", "display_name": "Pico 2"}
    ]

    build_types = [
        {"name": "release", "variable": "Release", "suffix": "", "display_suffix": ""},
        {"name": "debug", "variable": "Debug", "suffix": "-debug", "display_suffix": " (Debug)"}
    ]

    microsd_options = [
        {"name": "spi_card", "variables": {"SPI_MICROSD": "ON"}, "display_name": "SPI MicroSD"},
        {"name": "sdio_card", "variables": {"SPI_MICROSD": ""}, "display_name": "SDIO MicroSD"}
    ]

    display_options = [
        {"name": "i2c_disp", "variables": {"DISP_PROTO": "PIO_I2C"}, "display_name": "I2C display", "proto": "I2C"},
        {"name": "spi_disp", "variables": {"DISP_PROTO": "SPI"}, "display_name": "SPI display", "proto": "SPI"}
    ]

    fork_options = [
        {"name": "linear_fork", "variables": {"FORK_LINEAR": "ON"}, "display_name": "linear fork"},
        {"name": "as5600_fork", "variables": {"FORK_LINEAR": ""}, "display_name": "AS5600 fork"}
    ]

    shock_options = [
        {"name": "linear_shock", "variables": {"SHOCK_LINEAR": "ON"}, "display_name": "linear shock"},
        {"name": "as5600_shock", "variables": {"SHOCK_LINEAR": ""}, "display_name": "AS5600 shock"}
    ]

    imu_options = [
        {"name": "no_imu", "variables": {"IMU_MODEL": "NONE"}, "display_name": "", "proto": "NONE"},
        {"name": "lsm6dso_i2c", "variables": {"IMU_MODEL": "LSM6DSO", "IMU_PROTO": "I2C"}, "display_name": ", LSM6DSO (I2C)", "proto": "I2C"},
        {"name": "lsm6dso_spi", "variables": {"IMU_MODEL": "LSM6DSO", "IMU_PROTO": "SPI"}, "display_name": ", LSM6DSO (SPI)", "proto": "SPI"},
        {"name": "mpu6050_i2c", "variables": {"IMU_MODEL": "MPU6050", "IMU_PROTO": "I2C"}, "display_name": ", MPU6050 (I2C)", "proto": "I2C"}
    ]

    configure_presets = []
    build_presets = []

    # 1. Add the 4 Base presets
    for board in boards:
        for build_type in build_types:
            name = f"base-{board['name']}{build_type['suffix']}"
            preset = {
                "name": name,
                "hidden": True,
                "binaryDir": "${sourceDir}/build/" + build_type['name'] + "/${presetName}",
                "cacheVariables": {
                    "CMAKE_BUILD_TYPE": build_type['variable'],
                    "PICO_BOARD": board['variable'],
                    "DISP_PROTO": "PIO_I2C",
                    "SPI_MICROSD": "",
                    "FORK_LINEAR": "",
                    "SHOCK_LINEAR": "",
                    "IMU_MODEL": "NONE",
                    "IMU_PROTO": "I2C"
                }
            }
            configure_presets.append(preset)

    # 2. Add Combination presets
    for board in boards:
        for build_type in build_types:
            for microsd in microsd_options:
                for display in display_options:
                    for fork in fork_options:
                        for shock in shock_options:
                            for imu in imu_options:
                                # Constraint: No SPI Display + SPI IMU
                                if display['proto'] == "SPI" and imu['proto'] == "SPI":
                                    continue
                                
                                # Construct name
                                name_parts = [microsd['name'], display['name'], fork['name'], shock['name']]
                                if imu['name'] != "no_imu":
                                    name_parts.append(imu['name'].replace("_", "-"))
                                
                                if board['name'] != "pico":
                                    name_parts.append(board['name'])
                                    
                                if build_type['name'] == "debug":
                                    name_parts.append("debug")
                                    
                                name = "-".join(name_parts)
                                
                                # Construct display name
                                display_name = f"{microsd['display_name']}, {display['display_name']}, {fork['display_name']}, {shock['display_name']}{imu['display_name']} ({board['display_name']}){build_type['display_suffix']}"
                                
                                # Inherit from base preset
                                base_preset = f"base-{board['name']}{build_type['suffix']}"
                                
                                # Merge variables
                                cache_variables = {}
                                cache_variables.update(microsd['variables'])
                                cache_variables.update(display['variables'])
                                cache_variables.update(fork['variables'])
                                cache_variables.update(shock['variables'])
                                cache_variables.update(imu['variables'])
                                
                                configure_preset = {
                                    "name": name,
                                    "displayName": display_name,
                                    "hidden": False,
                                    "inherits": base_preset,
                                    "cacheVariables": cache_variables
                                }
                                configure_presets.append(configure_preset)
                                
                                build_preset = {
                                    "name": name,
                                    "displayName": display_name,
                                    "configurePreset": name
                                }
                                build_presets.append(build_preset)

    full_json = {
        "version": 3,
        "cmakeMinimumRequired": {
            "major": 3,
            "minor": 21,
            "patch": 1
        },
        "configurePresets": configure_presets,
        "buildPresets": build_presets
    }

    with open("CMakePresets.json", "w") as f:
        json.dump(full_json, f, indent=2)
    
    print(f"Generated {len(configure_presets)} configure presets and {len(build_presets)} build presets.")

if __name__ == "__main__":
    generate()
