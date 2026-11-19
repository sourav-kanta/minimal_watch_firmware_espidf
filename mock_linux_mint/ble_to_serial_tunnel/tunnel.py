import asyncio
import logging
import struct
import serial

from dbus_next.aio import MessageBus
from dbus_next.constants import BusType, PropertyAccess
from dbus_next.service import ServiceInterface, method, dbus_property
from dbus_next import Variant


# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

DEVICE_NAME = "MW Watch"

SERVICE_UUID = "12345678-1234-5678-1234-56789abcdef0"
RX_UUID = "87654321-4321-8765-4321-876543210987"
TX_UUID = "87654321-4321-8765-4321-876543210988"

ADAPTER_PATH = "/org/bluez/hci0"
APP_PATH = "/org/example/mwwatch"
SERVICE_PATH = APP_PATH + "/service0"
RX_PATH = SERVICE_PATH + "/char0"
TX_PATH = SERVICE_PATH + "/char1"
ADVERTISEMENT_PATH = "/org/example/mwwatch/advertisement0"

BLUEZ = "org.bluez"

SERIAL_PORT = "/tmp/mw_python"
SERIAL_BAUDRATE = 115200

SERIAL_MAGIC = 0x7E
SERIAL_MAX_PAYLOAD = 512


# ---------------------------------------------------------------------------
# Logging
# ---------------------------------------------------------------------------

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
)

logger = logging.getLogger("mw-watch-bridge")


# ---------------------------------------------------------------------------
# Serial framing
# ---------------------------------------------------------------------------

def build_serial_frame(payload: bytes) -> bytes:
    if len(payload) > SERIAL_MAX_PAYLOAD:
        raise ValueError("Serial payload too large")

    return bytes([SERIAL_MAGIC]) + struct.pack("<H", len(payload)) + payload


class SerialFrameParser:
    def __init__(self):
        self.buffer = bytearray()

    def feed(self, data: bytes) -> list[bytes]:
        self.buffer.extend(data)
        frames = []

        while True:
            try:
                magic_pos = self.buffer.index(SERIAL_MAGIC)
            except ValueError:
                self.buffer.clear()
                break

            if magic_pos > 0:
                del self.buffer[:magic_pos]

            if len(self.buffer) < 3:
                break

            payload_len = struct.unpack_from("<H", self.buffer, 1)[0]

            if payload_len > SERIAL_MAX_PAYLOAD:
                logger.warning(
                    "Invalid serial frame length: %d",
                    payload_len,
                )
                del self.buffer[0]
                continue

            frame_len = 3 + payload_len

            if len(self.buffer) < frame_len:
                break

            payload = bytes(self.buffer[3:frame_len])
            del self.buffer[:frame_len]

            frames.append(payload)

        return frames


# ---------------------------------------------------------------------------
# D-Bus GATT application
# ---------------------------------------------------------------------------

class Application(ServiceInterface):
    def __init__(self):
        super().__init__("org.freedesktop.DBus.ObjectManager")

    @method()
    def GetManagedObjects(self) -> "a{oa{sa{sv}}}":
        return {
            SERVICE_PATH: {
                "org.bluez.GattService1": {
                    "UUID": Variant("s", SERVICE_UUID),
                    "Primary": Variant("b", True),
                    "Includes": Variant("ao", []),
                }
            },
            RX_PATH: {
                "org.bluez.GattCharacteristic1": {
                    "UUID": Variant("s", RX_UUID),
                    "Service": Variant("o", SERVICE_PATH),
                    "Flags": Variant("as", ["write", "write-without-response"]),
                }
            },
            TX_PATH: {
                "org.bluez.GattCharacteristic1": {
                    "UUID": Variant("s", TX_UUID),
                    "Service": Variant("o", SERVICE_PATH),
                    "Flags": Variant("as", ["notify"]),
                }
            },
        }


class WatchService(ServiceInterface):
    def __init__(self):
        super().__init__("org.bluez.GattService1")

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return SERVICE_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Primary(self) -> "b":
        return True

    @dbus_property(access=PropertyAccess.READ)
    def Includes(self) -> "ao":
        return []


# ---------------------------------------------------------------------------
# RX characteristic: BLE -> Serial
# ---------------------------------------------------------------------------

class RXCharacteristic(ServiceInterface):
    def __init__(self, ser):
        super().__init__("org.bluez.GattCharacteristic1")
        self.ser = ser

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return RX_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return SERVICE_PATH

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["write", "write-without-response"]

    @method()
    def WriteValue(self, value: "ay", options: "a{sv}"):
        data = bytes(value)

        logger.info(
            "BLE RX: %d bytes: %s",
            len(data),
            data.hex(" "),
        )

        frame = build_serial_frame(data)
        self.ser.write(frame)


# ---------------------------------------------------------------------------
# TX characteristic: Serial -> BLE
# ---------------------------------------------------------------------------

class TXCharacteristic(ServiceInterface):
    def __init__(self):
        super().__init__("org.bluez.GattCharacteristic1")

        self._value = b""
        self.notifying = False

    @dbus_property(access=PropertyAccess.READ)
    def UUID(self) -> "s":
        return TX_UUID

    @dbus_property(access=PropertyAccess.READ)
    def Service(self) -> "o":
        return SERVICE_PATH

    @dbus_property(access=PropertyAccess.READ)
    def Flags(self) -> "as":
        return ["notify"]

    @dbus_property(access=PropertyAccess.READ)
    def Value(self) -> "ay":
        return self._value

    @dbus_property(access=PropertyAccess.READ)
    def Notifying(self) -> "b":
        return self.notifying

    @method()
    def StartNotify(self):
        self.notifying = True
        logger.info("BLE notifications enabled")

    @method()
    def StopNotify(self):
        self.notifying = False
        logger.info("BLE notifications disabled")

    def send_notification(self, data: bytes):
        if not self.notifying:
            logger.debug("Skipping notification: no subscriber")
            return

        self._value = bytes(data)

        self.emit_properties_changed({
            "Value": self._value,
        })

        logger.info(
            "BLE TX: %d bytes: %s",
            len(self._value),
            self._value.hex(" "),
        )


# ---------------------------------------------------------------------------
# BLE advertisement
# ---------------------------------------------------------------------------

class WatchAdvertisement(ServiceInterface):
    def __init__(self):
        super().__init__("org.bluez.LEAdvertisement1")

    @dbus_property(access=PropertyAccess.READ)
    def Type(self) -> "s":
        return "peripheral"

    @dbus_property(access=PropertyAccess.READ)
    def ServiceUUIDs(self) -> "as":
        return [SERVICE_UUID]

    @dbus_property(access=PropertyAccess.READ)
    def LocalName(self) -> "s":
        return DEVICE_NAME

    @dbus_property(access=PropertyAccess.READ)
    def Includes(self) -> "as":
        return ["tx-power"]

    @method()
    def Release(self):
        logger.info("Advertisement released")


# ---------------------------------------------------------------------------
# Serial -> BLE background reader
# ---------------------------------------------------------------------------

async def serial_reader(ser, tx):
    parser = SerialFrameParser()

    logger.info("Serial reader started")

    try:
        while True:
            data = await asyncio.to_thread(ser.read, 4096)

            if not data:
                continue

            for payload in parser.feed(data):
                logger.info(
                    "Serial RX: %d bytes: %s",
                    len(payload),
                    payload.hex(" "),
                )

                tx.send_notification(payload)

    except asyncio.CancelledError:
        logger.info("Serial reader stopped")
        raise


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

async def main():
    ser = serial.Serial(
        port=SERIAL_PORT,
        baudrate=SERIAL_BAUDRATE,
        timeout=0.1,
    )

    logger.info("Opened serial port: %s", SERIAL_PORT)

    bus = await MessageBus(bus_type=BusType.SYSTEM).connect()

    app = Application()
    service = WatchService()
    rx = RXCharacteristic(ser)
    tx = TXCharacteristic()
    advertisement = WatchAdvertisement()

    bus.export(APP_PATH, app)
    bus.export(SERVICE_PATH, service)
    bus.export(RX_PATH, rx)
    bus.export(TX_PATH, tx)
    bus.export(ADVERTISEMENT_PATH, advertisement)

    adapter = bus.get_proxy_object(
        BLUEZ,
        ADAPTER_PATH,
        await bus.introspect(BLUEZ, ADAPTER_PATH),
    )

    gatt_manager = adapter.get_interface(
        "org.bluez.GattManager1"
    )

    advertising_manager = adapter.get_interface(
        "org.bluez.LEAdvertisingManager1"
    )

    reader_task = None
    registered_app = False
    registered_advertisement = False

    try:
        await gatt_manager.call_register_application(
            APP_PATH,
            {},
        )
        registered_app = True

        logger.info("GATT application registered")

        await advertising_manager.call_register_advertisement(
            ADVERTISEMENT_PATH,
            {},
        )
        registered_advertisement = True

        logger.info("BLE advertisement registered")

        reader_task = asyncio.create_task(
            serial_reader(ser, tx)
        )

        logger.info("BLE <-> Serial bridge running")

        await asyncio.Event().wait()

    except asyncio.CancelledError:
        logger.info("Shutdown requested")

    finally:
        logger.info("Stopping BLE bridge...")

        if reader_task is not None:
            reader_task.cancel()

            try:
                await reader_task
            except asyncio.CancelledError:
                pass

        if registered_advertisement:
            try:
                await advertising_manager.call_unregister_advertisement(
                    ADVERTISEMENT_PATH
                )
                logger.info("Advertisement unregistered")
            except Exception:
                logger.exception("Failed to unregister advertisement")

        if registered_app:
            try:
                await gatt_manager.call_unregister_application(
                    APP_PATH
                )
                logger.info("GATT application unregistered")
            except Exception:
                logger.exception("Failed to unregister GATT application")

        ser.close()
        bus.disconnect()

        logger.info("BLE bridge stopped")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
