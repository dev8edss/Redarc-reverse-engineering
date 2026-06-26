import datetime
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, light, sensor, text_sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS,
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT,
    CONF_EFFECTS,
    CONF_ENTITY_CATEGORY,
    CONF_FLASH_TRANSITION_LENGTH,
    CONF_FORCE_UPDATE,
    CONF_GAMMA_CORRECT,
    CONF_ICON,
    CONF_ID,
    CONF_NAME,
    CONF_RESTORE_MODE,
    CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC,
    ENTITY_CATEGORY_NONE,
)

CONF_SOURCE_ADDRESS = "source_address"
CONF_IDENTITY_INTERVAL = "identity_interval"
CONF_STATUS_INTERVAL = "status_interval"
CONF_RANDOM_UPDATE_INTERVAL = "random_update_interval"
CONF_RANDOMIZE_INPUTS = "randomize_inputs"
CONF_SERIAL_PREFIX = "serial_prefix"
CONF_SERIAL_SUFFIX = "serial_suffix"
CONF_DEVICE_SUBTYPE = "device_subtype"
CONF_VERSION_RECORDS = "version_records"
CONF_PRODUCT_NUMBER = "product_number"
CONF_MAJOR = "major"
CONF_MINOR = "minor"
CONF_RECORD_INDEX = "record_index"
CONF_MANUFACTURING_DATE = "manufacturing_date"
CONF_DAY = "day"
CONF_MONTH = "month"
CONF_YEAR = "year"
CONF_PRODUCT_NAME = "product_name"
CONF_UNIQUE_IDENTIFIER = "unique_identifier"
CONF_UNIQUE_IDENTIFIER_RECORD_INDEX = "unique_identifier_record_index"

rogue_emulator_ns = cg.esphome_ns.namespace("redarc_tvms_rogue_emulator")
TVMSRogueActiveEmulatorComponent = rogue_emulator_ns.class_(
    "TVMSRogueActiveEmulatorComponent", cg.Component
)
TVMSRogueEmulatorLight = rogue_emulator_ns.class_(
    "TVMSRogueEmulatorLight", light.LightOutput
)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_bs_ns = cg.esphome_ns.namespace("binary_sensor")
_BSClass = _bs_ns.class_("BinarySensor")
_ts_ns = cg.esphome_ns.namespace("text_sensor")
_TSClass = _ts_ns.class_("TextSensor")

try:
    _RESTORE_MODE_OFF = (
        light.RESTORE_MODES.get("RESTORE_DEFAULT_OFF")
        or light.RESTORE_MODES.get("RESTORE_AND_OFF")
        or next(iter(light.RESTORE_MODES.values()))
    )
except AttributeError:
    _light_ns = cg.esphome_ns.namespace("light")
    _RESTORE_MODE_OFF = _light_ns.enum(
        "LightRestoreMode", is_class=True
    ).RESTORE_AND_OFF


def _validate_manufacturing_date(value):
    value = dict(value)
    day = cv.int_range(min=1, max=31)(value[CONF_DAY])
    month = cv.int_range(min=1, max=12)(value[CONF_MONTH])
    year = cv.int_range(min=2000, max=9999)(value[CONF_YEAR])
    try:
        datetime.date(year, month, day)
    except ValueError as err:
        raise cv.Invalid(f"invalid manufacturing_date: {err}") from err
    return {CONF_DAY: day, CONF_MONTH: month, CONF_YEAR: year}


def _validate_unique_identifier(value):
    if isinstance(value, str):
        compact = re.sub(r"[^0-9A-Fa-f]", "", value)
        if len(compact) != 14:
            raise cv.Invalid(
                "unique_identifier must contain exactly 7 bytes (14 hex digits)"
            )
        return [int(compact[i : i + 2], 16) for i in range(0, 14, 2)]
    values = cv.ensure_list(cv.hex_uint8_t)(value)
    if len(values) != 7:
        raise cv.Invalid("unique_identifier must contain exactly 7 bytes")
    return values


def _validate_product_name(value):
    value = cv.string_strict(value)
    try:
        encoded = value.encode("ascii")
    except UnicodeEncodeError as err:
        raise cv.Invalid("product_name must contain ASCII characters only") from err
    if len(encoded) > 1792:
        raise cv.Invalid("product_name is limited to 1792 ASCII characters")
    return value


VERSION_RECORD_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_PRODUCT_NUMBER): cv.int_range(min=0, max=0xFFFF),
        cv.Required(CONF_MAJOR): cv.hex_uint8_t,
        cv.Required(CONF_MINOR): cv.hex_uint8_t,
        cv.Optional(CONF_RECORD_INDEX, default=0): cv.hex_uint8_t,
    }
)


def _validate_version_records(value):
    records = cv.ensure_list(VERSION_RECORD_SCHEMA)(value)
    if not records:
        raise cv.Invalid("version_records must contain at least one record")
    indexes = set()
    for record in records:
        index = record[CONF_RECORD_INDEX]
        if index in indexes:
            raise cv.Invalid(f"duplicate version record_index {index}")
        indexes.add(index)
    return records


MANUFACTURING_DATE_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_DAY): cv.int_range(min=1, max=31),
            cv.Required(CONF_MONTH): cv.int_range(min=1, max=12),
            cv.Required(CONF_YEAR): cv.int_range(min=2000, max=9999),
        }
    ),
    _validate_manufacturing_date,
)

_AUTO_IDS = {}
for _i in range(1, 11):
    _AUTO_IDS[cv.GenerateID(f"light_out_{_i}")] = cv.declare_id(
        TVMSRogueEmulatorLight
    )
    _AUTO_IDS[cv.GenerateID(f"light_state_{_i}")] = cv.declare_id(
        light.LightState
    )
    _AUTO_IDS[cv.GenerateID(f"level_sensor_{_i}")] = cv.declare_id(_SensorClass)
for _i in range(1, 9):
    _AUTO_IDS[cv.GenerateID(f"button_sensor_{_i}")] = cv.declare_id(_BSClass)
_AUTO_IDS[cv.GenerateID("tank1_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("tank2_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("input_voltage_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("input_current_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("output_status_id")] = cv.declare_id(_TSClass)

SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TVMSRogueActiveEmulatorComponent),
        cv.Optional(CONF_SOURCE_ADDRESS, default=0x30): cv.hex_uint8_t,
        cv.Optional(
            CONF_IDENTITY_INTERVAL, default="1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_STATUS_INTERVAL, default="1s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(
            CONF_RANDOM_UPDATE_INTERVAL, default="5s"
        ): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_RANDOMIZE_INPUTS, default=True): cv.boolean,
        cv.Optional(CONF_SERIAL_PREFIX, default=0): cv.hex_uint32_t,
        cv.Optional(CONF_SERIAL_SUFFIX, default=1): cv.hex_uint16_t,
        cv.Optional(CONF_DEVICE_SUBTYPE, default=0): cv.hex_uint8_t,
        cv.Optional(
            CONF_VERSION_RECORDS,
            default=[
                {
                    CONF_PRODUCT_NUMBER: 323,
                    CONF_MAJOR: 1,
                    CONF_MINOR: 0,
                    CONF_RECORD_INDEX: 0,
                }
            ],
        ): _validate_version_records,
        cv.Optional(
            CONF_MANUFACTURING_DATE,
            default={CONF_DAY: 1, CONF_MONTH: 1, CONF_YEAR: 2026},
        ): MANUFACTURING_DATE_SCHEMA,
        cv.Optional(CONF_PRODUCT_NAME, default="TVMS Rogue"): _validate_product_name,
        cv.Optional(
            CONF_UNIQUE_IDENTIFIER,
            default=[0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01],
        ): _validate_unique_identifier,
        cv.Optional(CONF_UNIQUE_IDENTIFIER_RECORD_INDEX, default=0): cv.hex_uint8_t,
        **_AUTO_IDS,
    }
).extend(cv.COMPONENT_SCHEMA)


async def _make_sensor(
    config_id,
    name,
    unit=None,
    device_class=None,
    decimals=None,
    entity_category=None,
):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_FORCE_UPDATE: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: (
            entity_category
            if entity_category is not None
            else ENTITY_CATEGORY_NONE
        ),
        CONF_STATE_CLASS: _StateClass.STATE_CLASS_MEASUREMENT,
    }
    if unit is not None:
        cfg[CONF_UNIT_OF_MEASUREMENT] = unit
    if device_class is not None:
        cfg[CONF_DEVICE_CLASS] = device_class
    if decimals is not None:
        cfg[CONF_ACCURACY_DECIMALS] = decimals
    return await sensor.new_sensor(cfg)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_identity_interval_ms(config[CONF_IDENTITY_INTERVAL]))
    cg.add(var.set_status_interval_ms(config[CONF_STATUS_INTERVAL]))
    cg.add(
        var.set_random_update_interval_ms(config[CONF_RANDOM_UPDATE_INTERVAL])
    )
    cg.add(var.set_randomize_inputs(config[CONF_RANDOMIZE_INPUTS]))
    cg.add(var.set_serial_prefix(config[CONF_SERIAL_PREFIX]))
    cg.add(var.set_serial_suffix(config[CONF_SERIAL_SUFFIX]))
    cg.add(var.set_device_subtype(config[CONF_DEVICE_SUBTYPE]))

    for record in config[CONF_VERSION_RECORDS]:
        cg.add(
            var.add_version_record(
                record[CONF_PRODUCT_NUMBER],
                record[CONF_MAJOR],
                record[CONF_MINOR],
                record[CONF_RECORD_INDEX],
            )
        )

    date = config[CONF_MANUFACTURING_DATE]
    cg.add(
        var.set_manufacturing_date(
            date[CONF_DAY], date[CONF_MONTH], date[CONF_YEAR]
        )
    )
    cg.add(var.set_product_name(config[CONF_PRODUCT_NAME]))

    unique_identifier = config[CONF_UNIQUE_IDENTIFIER]
    cg.add(
        var.set_unique_identifier(
            unique_identifier[0],
            unique_identifier[1],
            unique_identifier[2],
            unique_identifier[3],
            unique_identifier[4],
            unique_identifier[5],
            unique_identifier[6],
        )
    )
    cg.add(
        var.set_unique_identifier_record_index(
            config[CONF_UNIQUE_IDENTIFIER_RECORD_INDEX]
        )
    )

    prefix = config[CONF_ID].id.replace("_", " ")

    s = await _make_sensor(
        config["tank1_id"], f"{prefix} Tank 1", unit="%", decimals=0
    )
    cg.add(var.set_tank1_sensor(s))
    s = await _make_sensor(
        config["tank2_id"], f"{prefix} Tank 2", unit="%", decimals=0
    )
    cg.add(var.set_tank2_sensor(s))
    s = await _make_sensor(
        config["input_voltage_id"],
        f"{prefix} Input Voltage",
        unit="V",
        device_class="voltage",
        decimals=3,
    )
    cg.add(var.set_input_voltage_sensor(s))
    s = await _make_sensor(
        config["input_current_id"],
        f"{prefix} Input Current",
        unit="A",
        device_class="current",
        decimals=3,
    )
    cg.add(var.set_input_current_sensor(s))

    status = await text_sensor.new_text_sensor(
        {
            CONF_ID: config["output_status_id"],
            CONF_NAME: f"{prefix} Output Status",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_ICON: "mdi:format-list-bulleted",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        }
    )
    cg.add(var.set_output_status_text_sensor(status))

    for i in range(1, 11):
        level = await _make_sensor(
            config[f"level_sensor_{i}"],
            f"{prefix} Output {i} Level",
            unit="%",
            decimals=0,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        )
        cg.add(var.set_level_sensor(i, level))

    for i in range(1, 9):
        input_sensor = cg.new_Pvariable(config[f"button_sensor_{i}"])
        await binary_sensor.register_binary_sensor(
            input_sensor,
            {
                CONF_ID: config[f"button_sensor_{i}"],
                CONF_NAME: f"{prefix} Input {i}",
                CONF_DISABLED_BY_DEFAULT: False,
                CONF_ICON: "mdi:electric-switch",
                CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
            },
        )
        cg.add(var.set_input_sensor(i, input_sensor))

    for i in range(1, 11):
        light_out = cg.new_Pvariable(config[f"light_out_{i}"])
        cg.add(light_out.set_parent(var))
        cg.add(light_out.set_output_number(i))
        cg.add(var.register_light(i, light_out))
        await light.register_light(
            light_out,
            {
                CONF_ID: config[f"light_state_{i}"],
                CONF_NAME: f"{prefix} Output {i}",
                CONF_GAMMA_CORRECT: 1.0,
                CONF_DEFAULT_TRANSITION_LENGTH: 0,
                CONF_FLASH_TRANSITION_LENGTH: 250,
                CONF_DISABLED_BY_DEFAULT: False,
                CONF_RESTORE_MODE: _RESTORE_MODE_OFF,
                CONF_EFFECTS: [],
            },
        )

    return var
