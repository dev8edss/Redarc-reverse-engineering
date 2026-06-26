import datetime
import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CONF_SOURCE_ADDRESS = "source_address"
CONF_IDENTITY_INTERVAL = "identity_interval"
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
TVMSRogueEmulatorComponent = rogue_emulator_ns.class_(
    "TVMSRogueEmulatorComponent", cg.Component
)


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

SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TVMSRogueEmulatorComponent),
        cv.Optional(CONF_SOURCE_ADDRESS, default=0x30): cv.hex_uint8_t,
        cv.Optional(CONF_IDENTITY_INTERVAL, default="1s"): cv.positive_time_period_milliseconds,
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
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_identity_interval_ms(config[CONF_IDENTITY_INTERVAL]))
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
    cg.add(var.set_manufacturing_date(date[CONF_DAY], date[CONF_MONTH], date[CONF_YEAR]))
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

    return var
