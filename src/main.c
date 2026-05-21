/*
 * BLE Peripheral - S=8 Coded PHY (Maximum Range)
 * nRF Connect SDK / Zephyr
 */

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/printk.h>

/* ============================================================
 *                  USER-TUNABLE PARAMETERS
 * ============================================================ */

/* TX power in dBm. Set to 20 if FEM is connected and configured. */
#define TX_POWER_DBM              20 // rohan verified (-ve -> 0 -> 8 resulted in range increase)

/* Advertising interval (units of 0.625 ms). 0x30=30 ms, 0x60=60 ms. */
#define ADV_INTERVAL_MIN          0xA0    // 100 ms
#define ADV_INTERVAL_MAX          0x140   // 200 ms


/* Service UUIDs */
#define BT_UUID_DATA_EXCHANGE_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x2dccb4e6, 0x3b5b, 0x4d42, 0x8291, 0x3e9eae8c9a7a)
#define BT_UUID_DATA_EXCHANGE_SERVICE \
    BT_UUID_DECLARE_128(BT_UUID_DATA_EXCHANGE_SERVICE_VAL)

#define BT_UUID_DATA_RX_VAL \
    BT_UUID_128_ENCODE(0x2dccb4e7, 0x3b5b, 0x4d42, 0x8291, 0x3e9eae8c9a7a)
#define BT_UUID_DATA_RX BT_UUID_DECLARE_128(BT_UUID_DATA_RX_VAL)

#define BT_UUID_DATA_TX_VAL \
    BT_UUID_128_ENCODE(0x2dccb4e8, 0x3b5b, 0x4d42, 0x8291, 0x3e9eae8c9a7a)
#define BT_UUID_DATA_TX BT_UUID_DECLARE_128(BT_UUID_DATA_TX_VAL)

/* ============================================================
 *                   END OF TUNABLE SECTION
 * ============================================================ */

#define PERIPHERAL_NAME       CONFIG_BT_DEVICE_NAME
#define PERIPHERAL_NAME_SIZE  (sizeof(PERIPHERAL_NAME) - 1)

static struct bt_conn *current_conn = NULL;
static uint16_t current_mtu = 23;
static uint32_t rx_value;
static uint32_t tx_value;
static char msg[244];
static bt_addr_le_t addr;
static char ble_mac_address[30];
static struct bt_le_ext_adv *adv_set;

static struct bt_gatt_exchange_params exchange_params;
const struct bt_conn_le_phy_param preferred_phy = {
    .options = BT_CONN_LE_PHY_OPT_NONE,
    .pref_rx_phy = BT_GAP_LE_PHY_1M,
    .pref_tx_phy = BT_GAP_LE_PHY_1M,
};


K_SEM_DEFINE(mtu_updated, 0, 1);

/* ============================================================
 * ADVERTISING CONFIGURATION — S=8 Coded
 * ============================================================ */

static const struct bt_le_adv_param *adv_param = BT_LE_ADV_PARAM(
    (BT_LE_ADV_OPT_CONN |
     BT_LE_ADV_OPT_USE_IDENTITY |
     BT_LE_ADV_OPT_EXT_ADV),   // 1M
    ADV_INTERVAL_MIN,
    ADV_INTERVAL_MAX,
    NULL
);

static uint8_t mfg_data[8] = {
    0xFF, 0xFF,         /* company ID */
    0, 0, 0, 0, 0, 0    /* MAC, filled at runtime */
};

static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, PERIPHERAL_NAME, PERIPHERAL_NAME_SIZE),
    BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data)),
};

/* ============================================================
 * TX POWER (Nordic vendor-specific)
 * ============================================================ */

static void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
    struct bt_hci_cp_vs_write_tx_power_level *cp;
    struct bt_hci_rp_vs_write_tx_power_level *rp;
    struct net_buf *buf, *rsp = NULL;

    buf = bt_hci_cmd_alloc(K_FOREVER);
    if (!buf) {
        printk("Unable to allocate HCI cmd buffer\n");
        return;
    }

    cp = net_buf_add(buf, sizeof(*cp));
    cp->handle_type = handle_type;
    cp->handle = sys_cpu_to_le16(handle);
    cp->tx_power_level = tx_pwr_lvl;

    int err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, buf, &rsp);
    if (err) {
        printk("Set TX power failed (err %d)\n", err);
        return;
    }

    rp = (void *)rsp->data;
    printk("TX power: %d dBm (requested: %d dBm)\n",
           rp->selected_tx_power, tx_pwr_lvl);
    net_buf_unref(rsp);
}

/* ============================================================
 * CONNECTION CALLBACKS
 * ============================================================ */

static const char *phy_name(uint8_t phy);  /* forward declaration */

static void connection_cb(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
        return;
    }

    printk("[ CONNECTED ]\n");

    /* Get current PHY using public API */
    struct bt_conn_info info;
    int rc = bt_conn_get_info(conn, &info);
    // print interval
    printk("Interval: %d\n", info.le.interval_us);
    // print latency
    printk("Latency: %d\n", info.le.latency);
    // print timeout
    printk("Timeout: %d\n", info.le.timeout);
    if (!rc && info.type == BT_CONN_TYPE_LE) {
        printk("Initial PHY: TX=%s RX=%s\n",
               phy_name(info.le.phy->tx_phy),
               phy_name(info.le.phy->rx_phy));
    }

    /* Set TX power for the connection */
    uint16_t handle;
    rc = bt_hci_get_conn_handle(conn, &handle);
    if (!rc) {
        set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_CONN, handle, TX_POWER_DBM);
    }

    if (current_conn) {
        bt_conn_unref(current_conn);
    }
    current_conn = bt_conn_ref(conn);
}
static void disconnection_cb(struct bt_conn *conn, uint8_t reason)
{
    printk("[ DISCONNECTED ] (reason 0x%02x)\n", reason);
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

static void on_le_data_len_updated(struct bt_conn *conn,
                                   struct bt_conn_le_data_len_info *info)
{
    printk("[DLE] TX: %u bytes, RX: %u bytes\n",
           info->tx_max_len, info->rx_max_len);
}

static void on_le_param_updated(struct bt_conn *conn, uint16_t interval,
                                uint16_t latency, uint16_t timeout)
{
    printk("[Conn Params] Interval: %.2f ms, Latency: %d, Timeout: %d ms\n",
           (double)(interval * 1.25), latency, timeout * 10);
}

static const char *phy_name(uint8_t phy)
{
    switch (phy) {
    case BT_GAP_LE_PHY_1M:    return "1M";
    case BT_GAP_LE_PHY_2M:    return "2M";
    case BT_GAP_LE_PHY_CODED: return "Coded";
    default:                  return "Unknown";
    }
}

static void on_le_phy_updated(struct bt_conn *conn,
                              struct bt_conn_le_phy_info *param)
{
    printk("[PHY] TX: %s, RX: %s\n",
           phy_name(param->tx_phy), phy_name(param->rx_phy));
}

static void on_recycled(void)
{
    int err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        printk("Failed to restart adv (err %d)\n", err);
    } else {
        printk("Advertising restarted\n");
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected           = connection_cb,
    .disconnected        = disconnection_cb,
    .recycled            = on_recycled,        // <-- new
    .le_data_len_updated = on_le_data_len_updated,
    .le_param_updated    = on_le_param_updated,
    .le_phy_updated      = on_le_phy_updated,
};

static void mtu_exchange_cb(struct bt_conn *conn, uint8_t att_err,
                            struct bt_gatt_exchange_params *params)
{
    if (!att_err) {
        current_mtu = bt_gatt_get_mtu(conn);
        printk("[MTU] New MTU: %u, Payload: %u\n", current_mtu, current_mtu - 3);
        k_sem_give(&mtu_updated);
    } else {
        printk("[MTU] Exchange failed (err %u)\n", att_err);
    }
}

/* ============================================================
 * GATT SERVICE
 * ============================================================ */

static ssize_t read_tx_cb(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                          void *buf, uint16_t len, uint16_t offset)
{
    const char *value = attr->user_data;
    return bt_gatt_attr_read(conn, attr, buf, len, offset, value, sizeof(*value));
}

static ssize_t on_data_received(struct bt_conn *conn, const struct bt_gatt_attr *attr,
                                const void *buf, uint16_t len, uint16_t offset, uint8_t flags)
{
    memset(msg, 0, sizeof(msg));
    memcpy(msg, buf, len);
    msg[len] = '\0';
    printk("RX [%u bytes]: %s\n", len, msg);
    return len;
}

static void ccc_cfg_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    if (value == 2)      printk("[ Indications Enabled ]\n");
    else if (value == 1) printk("[ Notifications Enabled ]\n");
    else                 printk("[ Indications/Notifications Disabled ]\n");
}

BT_GATT_SERVICE_DEFINE(
    data_exchange_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_DATA_EXCHANGE_SERVICE),

    BT_GATT_CHARACTERISTIC(
        BT_UUID_DATA_RX,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE,
        NULL, on_data_received, &rx_value
    ),
    BT_GATT_CHARACTERISTIC(
        BT_UUID_DATA_TX,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_INDICATE,
        BT_GATT_PERM_READ,
        read_tx_cb, NULL, &tx_value
    ),
    BT_GATT_CCC(ccc_cfg_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* ============================================================
 * SETUP
 * ============================================================ */

static void update_dle_and_mtu(void)
{
    struct bt_conn_le_data_len_param dle_param = {
        .tx_max_len  = BT_GAP_DATA_LEN_MAX,
        .tx_max_time = BT_GAP_DATA_TIME_MAX,
    };
    int err = bt_conn_le_data_len_update(current_conn, &dle_param);
    if (err) {
        printk("DLE update failed (err %d)\n", err);
    }

    exchange_params.func = mtu_exchange_cb;
    err = bt_gatt_exchange_mtu(current_conn, &exchange_params);
    if (err == -EALREADY) {
        current_mtu = bt_gatt_get_mtu(current_conn);
        k_sem_give(&mtu_updated);
    } else if (err) {
        printk("MTU exchange failed (err %d)\n", err);
    }
}

static void setup_ble(void)
{
    int err = bt_enable(NULL);
    if (err) {
        printk("BLE enable failed (err %d)\n", err);
        return;
    }
    printk("BLE enabled\n");

    size_t count = 1;
    bt_id_get(&addr, &count);
    bt_addr_le_to_str(&addr, ble_mac_address, sizeof(ble_mac_address));
    printk("MAC: %s\n", ble_mac_address);

    /* Copy MAC big-endian into mfg_data */
    for (int i = 0; i < 6; i++) {
        mfg_data[2 + i] = addr.a.val[5 - i];
    }

    bt_conn_cb_register(&conn_callbacks);
}



static void start_advertising(void)
{
    int err = bt_le_ext_adv_create(adv_param, NULL, &adv_set);
    if (err) {
        printk("Failed to create ext adv set (err %d)\n", err);
        return;
    }

    err = bt_le_ext_adv_set_data(adv_set, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        printk("Failed to set ext adv data (err %d)\n", err);
        return;
    }

    err = bt_le_ext_adv_start(adv_set, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        printk("Failed to start ext adv (err %d)\n", err);
        return;
    }

    set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, TX_POWER_DBM);
    printk("Extended advertising started on 1M PHY with TX power %d dBm\n", TX_POWER_DBM);
}

static void wait_for_connection(void)
{
    printk("Waiting for connection...\n");
    while (current_conn == NULL) {
        k_msleep(500);
    }
}

/* ============================================================
 * MAIN
 * ============================================================ */

int main(void)
{
        setup_ble();
        start_advertising();
        wait_for_connection();
       
        while (1) {
        k_msleep(1000);
        }
        return 0;
}