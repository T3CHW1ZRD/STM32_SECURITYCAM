#ifndef BLUETOOTH_ALERT_HPP
#define BLUETOOTH_ALERT_HPP

#include "mbed.h"
#include "events/EventQueue.h"

#include "ble/BLE.h"
#include "ble/Gap.h"
#include "ble/GattServer.h"
#include "ble/gap/AdvertisingDataBuilder.h"
#include <chrono>

/** BLE alert helper:
 *  - Connectable advertising so a phone app can subscribe
 *  - Custom AlertState (READ | NOTIFY) characteristic: 0x00 idle, 0x01 alert
 *  - 3s alert window; exposes isBusy() for ToF gating
 *  - Restarts advertising on disconnection
 */
class BluetoothAlert {
public:
    explicit BluetoothAlert(events::EventQueue& q);

    /** Initialize BLE, add GATT service/char, start advertising. */
    void init();

    /** Trigger a 3-second ALERT; ignored if already alerting. */
    void trigger_alert();

    /** True while alert is active (blocks ToF in main). */
    bool isBusy() const { return _busy; }

private:
    // BLE plumbing (Mbed 6.13.0 needs FunctionPointerWithContext)
    void on_init_complete(BLE::InitializationCompleteCallbackContext* ctx);
    void on_ble_events(BLE::OnEventsToProcessCallbackContext* ctx);

    // GAP events (connect/disconnect)
    struct GapHandler : public ble::Gap::EventHandler {
        BluetoothAlert* self{};
        void onConnectionComplete(const ble::ConnectionCompleteEvent &e) override;
        void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &e) override;
    } _gap_handler;

    // GATT
    void add_gatt_server();
    void notify_alert(uint8_t state);

    // Advertising helpers
    void start_advertising_connectable();
    void set_adv_payload_idle(); // initial payload (name, flags)

    // Alert state
    void set_alert_state_adv(bool alert);
    void clear_alert();

private:
    BLE& _ble;
    events::EventQueue& _q;

    bool _busy = false;
    bool _ready = false;

    // Legacy ADV buffer (<=31 bytes)
    uint8_t _adv_buf[ble::LEGACY_ADVERTISING_MAX_SIZE] = {0};
    ble::AdvertisingDataBuilder _adv_builder;

    // Scan response buffer
    uint8_t _scan_resp_buf[ble::LEGACY_ADVERTISING_MAX_SIZE] = {0};

    // GATT handles
    GattAttribute::Handle_t _alert_value_handle = 0;
    GattCharacteristic* _alert_char = nullptr;  // holds the alert characteristic
    uint8_t _alert_state = 0x00;                // current alert state (0x00 = idle, 0x01 = alert)


    // UUIDs (16-bit vendor range, but expressed as 128-bit)
    const UUID _svc_uuid  = UUID("0000FFF0-0000-1000-8000-00805F9B34FB");
    const UUID _char_uuid = UUID("0000FFF1-0000-1000-8000-00805F9B34FB");
};

#endif // BLUETOOTH_ALERT_HPP
