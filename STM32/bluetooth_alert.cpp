#include "bluetooth_alert.hpp"
#include "proj_config.h"


using namespace std::chrono_literals;

BluetoothAlert::BluetoothAlert(events::EventQueue& q)
    : _ble(BLE::Instance()),
      _q(q),
      _adv_builder(_adv_buf) {
    _gap_handler.self = this;
}

void BluetoothAlert::init() {
    // Route BLE work onto EventQueue
    _ble.onEventsToProcess(makeFunctionPointer(this, &BluetoothAlert::on_ble_events));
    _ble.init(this, &BluetoothAlert::on_init_complete);
}

void BluetoothAlert::on_ble_events(BLE::OnEventsToProcessCallbackContext* /*ctx*/) {
    _q.call(callback(&_ble, &BLE::processEvents));
}

void BluetoothAlert::on_init_complete(BLE::InitializationCompleteCallbackContext* ctx) {
    if (ctx->error != BLE_ERROR_NONE) {
        printf("BLE init failed: %d\r\n", ctx->error);
        return;
    }

    // GAP event handler (connect/disconnect)
    _ble.gap().setEventHandler(&_gap_handler);

    // Add GATT server objects (service + characteristic)
    add_gatt_server();

    // Start advertising (connectable)
    start_advertising_connectable();

    _ready = true;
    printf("BLE ready: advertising and GATT set up\n");
}

// ---- GAP events ----
void BluetoothAlert::GapHandler::onConnectionComplete(const ble::ConnectionCompleteEvent &e) {
    if (e.getStatus() == BLE_ERROR_NONE) {
        printf("BLE connected\n");
    } else {
        printf("BLE connection failed: %u\n", (unsigned)e.getStatus());
    }
}
void BluetoothAlert::GapHandler::onDisconnectionComplete(const ble::DisconnectionCompleteEvent &/*e*/) {
    printf("BLE disconnected, restarting advertising\n");
    // Restart advertising so phone can reconnect
    self->start_advertising_connectable();
}

// ---- GATT server setup ----
void BluetoothAlert::add_gatt_server() {
    // Readable + Notifiable, keep storage in members
    _alert_char = new GattCharacteristic(
        _char_uuid,
        &_alert_state,
        sizeof(_alert_state),
        sizeof(_alert_state),
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_READ |
        GattCharacteristic::BLE_GATT_CHAR_PROPERTIES_NOTIFY
    );

    GattCharacteristic* chars[] = { _alert_char };
    GattService alert_service(_svc_uuid, chars, sizeof(chars)/sizeof(chars[0]));

    ble_error_t err = _ble.gattServer().addService(alert_service);
    if (err) {
        printf("addService err=%d\r\n", err);
    }

    _alert_value_handle = _alert_char->getValueHandle();
    printf("GATT AlertState handle=0x%04X\n", _alert_value_handle);
}

void BluetoothAlert::notify_alert(uint8_t state) {
    if (!_ready || _alert_value_handle == 0) return;
    ble_error_t err = _ble.gattServer().write(_alert_value_handle, &state, sizeof(state));
    if (err) {
        printf("notify write err=%d\r\n", err);
    }
}

// ---- Advertising helpers ----
void BluetoothAlert::start_advertising_connectable() {
    ble::Gap &gap = _ble.gap();

    // 1) Set connectable parameters
    ble::AdvertisingParameters params(
        ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
        ble::adv_interval_t(ble::millisecond_t(100))
    );
    if (auto err = gap.setAdvertisingParameters(ble::LEGACY_ADVERTISING_HANDLE, params)) {
        printf("setAdvertisingParameters err=%d\r\n", err);
        return;
    }

    // 2) Build advertising + scan response (flags + name + service list)
    set_adv_payload_idle();

    // 3) Start it
    if (auto err = gap.startAdvertising(ble::LEGACY_ADVERTISING_HANDLE)) {
        printf("startAdvertising err=%d\r\n", err);
    } else {
        printf("Advertising (connectable) started\n");
    }
}

void BluetoothAlert::set_adv_payload_idle() {
    // ADV payload: flags + local service list (FFF0)
    _adv_builder.clear();
    _adv_builder.setFlags(
        ble::adv_data_flags_t::LE_GENERAL_DISCOVERABLE |
        ble::adv_data_flags_t::BREDR_NOT_SUPPORTED
    );

    if (auto e = _adv_builder.setLocalServiceList(mbed::make_Span(&_svc_uuid, 1), /*complete*/ true)) {
        printf("setLocalServiceList err=%d\r\n", e);
    }

    _ble.gap().setAdvertisingPayload(
        ble::LEGACY_ADVERTISING_HANDLE,
        _adv_builder.getAdvertisingData()
    );

    // SCAN response: name
    ble::AdvertisingDataBuilder scan_rsp(_scan_resp_buf);
    scan_rsp.clear();
    scan_rsp.setName(BLE_NAME);
    _ble.gap().setAdvertisingScanResponse(
        ble::LEGACY_ADVERTISING_HANDLE,
        scan_rsp.getAdvertisingData()
    );
}

void BluetoothAlert::set_alert_state_adv(bool alert) {
    // Keep a manufacturer byte mirroring alert state (00/01)
    _adv_builder.clear();
    _adv_builder.setFlags(
        ble::adv_data_flags_t::LE_GENERAL_DISCOVERABLE |
        ble::adv_data_flags_t::BREDR_NOT_SUPPORTED
    );

    // Service UUID in payload even when toggling manufacturer data
    (void)_adv_builder.setLocalServiceList(mbed::make_Span(&_svc_uuid, 1), true);

    const uint8_t mfg[] = { 0xFF, 0xFF, static_cast<uint8_t>(alert ? 1 : 0) };
    _adv_builder.setManufacturerSpecificData(mbed::Span<const uint8_t>(mfg, sizeof(mfg)));

    _ble.gap().setAdvertisingPayload(
        ble::LEGACY_ADVERTISING_HANDLE,
        _adv_builder.getAdvertisingData()
    );

    // Scan response
    ble::AdvertisingDataBuilder scan_rsp(_scan_resp_buf);
    scan_rsp.clear();
    scan_rsp.setName("SECCAM");
    _ble.gap().setAdvertisingScanResponse(
        ble::LEGACY_ADVERTISING_HANDLE,
        scan_rsp.getAdvertisingData()
    );
}

// ---- Alert lifecycle ----
void BluetoothAlert::trigger_alert() {
    if (_busy || !_ready) return;
    _busy = true;

    // Update ADV (optional) and notify subscribers
    set_alert_state_adv(true);
    notify_alert(0x01);

    // Auto clear in 3s on EventQueue
    _q.call_in(3s, callback(this, &BluetoothAlert::clear_alert));
}

void BluetoothAlert::clear_alert() {
    _busy = false;
    set_alert_state_adv(false);
    notify_alert(0x00);
}
