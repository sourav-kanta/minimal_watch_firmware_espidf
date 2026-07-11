#include <ble_manager.h>

#include <os/os_mbuf.h>
#include <nimble/ble.h>
#include <host/ble_hs.h>
#include <host/ble_gap.h>
#include <host/util/util.h>
#include <services/gatt/ble_svc_gatt.h>
#include <services/gap/ble_svc_gap.h>
#include <host/ble_uuid.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <common_consts.h>
#include <ble_consts.h>
#include <ble_types.h>
#include <ble_fifo.h>
#include <ble_packet_handler.h>
#include <common_types.h>
#include <ble_request_handler.h>
#include <ble_response_handler.h>
#include <event_manager.h>
#include <runtime_manager.h>

static const char *TAG = "BLE_MANAGER";

typedef struct {
    uint16_t conn_handle;
    bool is_connected;
    bool notifications_enabled;
    uint16_t tx_val_handle;
    uint16_t effective_mtu;
} ble_context_t;

static ble_context_t g_ble = {
    .conn_handle = BLE_HS_CONN_HANDLE_NONE,
    .effective_mtu = 20,
    .is_connected = false,
    .notifications_enabled = false
};

static struct ble_gap_adv_params adv_params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,

    .itvl_min = BLE_MIN_ADV_INTERVAL,
    .itvl_max = BLE_MAX_ADV_INTERVAL,
};

static struct ble_gap_upd_params params = {
    .itvl_min = BLE_MIN_CONN_INTERVAL,
    .itvl_max = BLE_MAX_CONN_INTERVAL,
    .latency = BLE_CONN_LATENCY,
    .supervision_timeout = BLE_CONN_SUPERVISION_TIMEOUT,
};

static const ble_uuid128_t mw_service_uuid = BLE_UUID128_INIT(BLE_UUID_SMARTWATCH_SERVICE);
static const ble_uuid128_t rx_chrc_uuid    = BLE_UUID128_INIT(BLE_UUID_RX_CHRC);
static const ble_uuid128_t tx_chrc_uuid    = BLE_UUID128_INIT(BLE_UUID_TX_CHRC);

static int mw_rx_write_cb(uint16_t conn_handle, uint16_t attr_handle, 
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
static uint8_t own_addr_type;
static void ble_host_task(void *param);
static void ble_on_sync(void);
static void start_advertising(void);

static uint8_t tx_buffer[517];
static uint8_t rx_buffer[517];

static int tx_access_cb(uint16_t conn_handle,
                        uint16_t attr_handle,
                        struct ble_gatt_access_ctxt *ctxt,
                        void *arg) {
    return 0;
}

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {.type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &mw_service_uuid.u,
     .characteristics = (struct ble_gatt_chr_def[]) {
         {
             .uuid = &tx_chrc_uuid.u,
             .access_cb = tx_access_cb,
             .flags = BLE_GATT_CHR_F_NOTIFY,
             .val_handle = &g_ble.tx_val_handle
         },
         {
             .uuid = &rx_chrc_uuid.u,
             .access_cb = mw_rx_write_cb,
             .flags = BLE_GATT_CHR_F_WRITE_NO_RSP,
         },
         {0}
     }
    },
    {0}
};

static int mw_rx_write_cb(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(TAG, "Write RX packet len=%d", ctxt->om->om_len);
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > sizeof(rx_buffer)) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }
    os_mbuf_copydata(ctxt->om, 0, len, rx_buffer);
    ble_retrieve_packet(rx_buffer, len);
    return 0;
}

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                ESP_LOGI(TAG, "Connected");
                g_ble.conn_handle = event->connect.conn_handle;
                g_ble.is_connected = true;
                rx_reset_context();

                int rc = ble_gap_update_params(g_ble.conn_handle, &params);
                if (rc != 0) {
                    ESP_LOGW(TAG, "Couldn't request connection parameter update (%d)", rc);
                }

                rc = ble_gap_set_prefered_le_phy(g_ble.conn_handle,
                                                 BLE_GAP_LE_PHY_2M_MASK,  
                                                 BLE_GAP_LE_PHY_2M_MASK,   
                                                 0);
                if (rc != 0) {
                    ESP_LOGW(TAG, "PHY update request failed (%d)", rc);
                }
            }
            else {
                ESP_LOGW(TAG, "Connection failed");
                start_advertising();
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "Disconnected");
            g_ble.is_connected = false;
            g_ble.notifications_enabled = false;
            g_ble.conn_handle = BLE_HS_CONN_HANDLE_NONE;
            g_ble.effective_mtu = 20;
            rx_reset_context();
            start_advertising();
            break;

        case BLE_GAP_EVENT_MTU:
            g_ble.effective_mtu = (event->mtu.value > 3) ? (event->mtu.value - 3) : 0;
            ESP_LOGI(TAG, "MTU updated: %d", g_ble.effective_mtu);
            break;

        case BLE_GAP_EVENT_SUBSCRIBE:
            g_ble.notifications_enabled = (event->subscribe.cur_notify != 0);
            ESP_LOGI(TAG, "Notifications %s", g_ble.notifications_enabled ? "enabled" : "disabled");
            break;
        case BLE_GAP_EVENT_CONN_UPDATE:
        {
            struct ble_gap_conn_desc desc;
            if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
                ESP_LOGI(TAG,
                         "Interval=%u Latency=%u Timeout=%u",
                         desc.conn_itvl,
                         desc.conn_latency,
                         desc.supervision_timeout);
            }
            break;
        }
        case BLE_GAP_EVENT_PHY_UPDATE_COMPLETE:
            ESP_LOGI(TAG,
                     "PHY updated TX=%u RX=%u",
                     event->phy_updated.tx_phy,
                     event->phy_updated.rx_phy);
            break;
        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "Advertising completed");
            start_advertising();
            break;
        default :
            ESP_LOGI(TAG, "Unhandled Gap Event : %d", event->type);
            break;
    }
    return 0;
}

static void populate_tx_buffers(void) {
    if (!g_ble.is_connected || !g_ble.notifications_enabled) return;

    uint16_t len;
    while (ble_build_next_packet(tx_buffer, g_ble.effective_mtu, &len)) {
        struct os_mbuf *om = ble_hs_mbuf_from_flat(tx_buffer, len);
        if (!om) {
            ESP_LOGE(TAG, "Failed allocating mbuf");
            break;
        }

        int rc = ble_gatts_notify_custom(g_ble.conn_handle, g_ble.tx_val_handle, om);
        if (rc != 0) {
            os_mbuf_free_chain(om);
            ESP_LOGE(TAG, "Notify failed: %d", rc);
        }
    }
}

static void process_rx_buffers(void) {
    ble_msg_t msg;
    while(get_next_ble_msg(&msg, BLE_RX)) {
        handle_ble_response(&msg);
    }
}

static void ble_on_sync(void)
{
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to infer own address (%d)", rc);
        return;
    }
    
    start_advertising();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "BLE reset %d", reason);
}

static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    fields.flags =
        BLE_HS_ADV_F_DISC_GEN |
        BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (uint8_t *)BT_DEVICE_NAME;
    fields.name_len = strlen(BT_DEVICE_NAME);
    fields.name_is_complete = 1;

    fields.uuids128 = &mw_service_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed setting advertising fields (%d)", rc);
        return;
    }

    rc = ble_gap_adv_start(own_addr_type,
                           NULL,
                           BLE_HS_FOREVER,
                           &adv_params,
                           ble_gap_event,
                           NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed starting advertising (%d)", rc);
    }
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void submit_ble_request(const event_t* event) {
    if(!event) return;
    assert(event->payload_len == sizeof(ble_req_t));
    handle_ble_request((ble_req_t*)event->data);
}

void ble_manager_init(void) {
    ble_fifo_init();   
    ESP_ERROR_CHECK(nimble_port_init());    
    int rc;
    rc = ble_svc_gap_device_name_set(BT_DEVICE_NAME);
    assert(rc == 0);
    
    ble_svc_gap_init();
    ble_svc_gatt_init();
    
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    assert(rc == 0);
    
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    assert(rc == 0); 

    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;
    nimble_port_freertos_init(ble_host_task);
    bool ok = event_subscribe(EVENT_BLE_REQUEST, submit_ble_request);
    assert(ok);
    runtime_manager_register_hook(process_rx_buffers);    
    runtime_manager_register_hook(populate_tx_buffers);
    ESP_LOGI(TAG, "BLE initialized");
}

void ble_manager_deinit(void) {
    runtime_manager_unregister_hook(populate_tx_buffers);
    runtime_manager_unregister_hook(process_rx_buffers);
    event_unsubscribe(EVENT_BLE_REQUEST, submit_ble_request);
    int rc = nimble_port_stop();
    assert(rc == 0);
    nimble_port_deinit();
    ble_fifo_deinit();
    ESP_LOGI(TAG, "BLE deinitialized");
}

