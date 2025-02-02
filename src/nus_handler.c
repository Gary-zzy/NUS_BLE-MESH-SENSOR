#include "nus_handler.h"
#include <zephyr/types.h>
#include <zephyr/sys/printk.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <soc.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>

#include <bluetooth/services/nus.h>
#include <dk_buttons_and_leds.h>
#include <zephyr/bluetooth/conn.h>
#include <string.h>

#define PEOPLE_COUNT_STR_SIZE 32
#define DEVICE_NAME CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(CONFIG_BT_DEVICE_NAME) - 1)

static struct bt_conn *current_conn;
static struct k_work_delayable send_work;
static struct bt_le_ext_adv *nus_adv; // 独立的扩展广告实例
//static const char cmd_str[] = "123456789";
static uint8_t nus_conn_id = 1;

static int start_extended_advertising(void);
static void send_work_handler(struct k_work *work);
static int init_extended_advertising(void);





static char people_count_str[PEOPLE_COUNT_STR_SIZE] = "0";
void nus_update_people_count(const char *new_val)
{
    /* 将传入的字符串复制到全局变量中，注意防止溢出 */
    strncpy(people_count_str, new_val, sizeof(people_count_str) - 1);
    people_count_str[sizeof(people_count_str) - 1] = '\0';
}






/* Advertising配置 */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static const struct bt_data sd[] = {
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_VAL),
};

/* 蓝牙连接回调函数 */
static void connected(struct bt_conn *conn, uint8_t err)
{
    struct bt_conn_info conn_info;

    if (err) {
        printk("Connection failed (err 0x%02x)\n", err);
        return;
    }

    // 获取连接信息
    if (bt_conn_get_info(conn, &conn_info) != 0) {
        printk("Failed to get connection info\n");
        return;
    }

    // 检查连接是否属于NUS服务的标识
    if (conn_info.id == nus_conn_id) {
        printk("NUS Connected\n");
        current_conn = bt_conn_ref(conn); // 保存连接引用

        // 初始化定时发送任务
        k_work_init_delayable(&send_work, send_work_handler);
        k_work_schedule(&send_work, K_SECONDS(1)); // 每 1 秒发送一次数据
    } else {
        printk("Connected to another service (Mesh), not handling\n");
    }
}


static void disconnected(struct bt_conn *conn, uint8_t reason) {
    printk("Disconnected (reason 0x%02x)\n", reason);

    if (current_conn) {
        // 强制断开物理连接
        bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(current_conn);
        current_conn = NULL; 
    }

     k_work_cancel_delayable(&send_work);


        if (nus_adv) {
        bt_le_ext_adv_stop(nus_adv);
        bt_le_ext_adv_delete(nus_adv);
        nus_adv = NULL;
    }
    // 重新启动广告
    init_extended_advertising();
    start_extended_advertising();

}

/* 定义连接回调结构 */
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* 数据发送工作处理 */
static void send_work_handler(struct k_work *work)
{
    if (current_conn) {
        /* 直接发送更新后的 people_count_str */
        int err = bt_nus_send(current_conn, people_count_str, strlen(people_count_str));
        if (err == -ENOMEM) {
            printk("Failed to send NUS data: Out of memory (err %d)\n", err);
        } else if (err) {
            printk("Failed to send NUS data (err %d)\n", err);
        } else {
            printk("Sent People Count: %s\n", people_count_str);
        }
    } else {
        printk("No active connection, skipping send\n");
    }

    /* 定时调度下一次发送任务 */
    k_work_schedule(&send_work, K_SECONDS(1));
}

/* NUS回调函数 */
static void bt_receive_cb(struct bt_conn *conn, const uint8_t *const data, uint16_t len)
{
    printk("Received data: %.*s\n", len, data);
}

/* NUS回调结构 */
static struct bt_nus_cb nus_cb = {
    .received = bt_receive_cb,
};

/* 初始化扩展广告 */
static int init_extended_advertising(void)
{
    int err;
    size_t id_count = 0xFF;
    //struct bt_le_adv_param adv_params = *BT_LE_ADV_CONN;

	struct bt_le_adv_param adv_params = {
        .id = nus_conn_id,
        .options = BT_LE_ADV_OPT_CONNECTABLE, // 明确启用扩展广告和连接
        .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
        .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
        .peer = NULL,
    };
    /* 检查当前标识符数量 */
    (void)bt_id_get(NULL, &id_count);

    if (id_count < CONFIG_BT_ID_MAX) {
        int id = bt_id_create(NULL, NULL);

        if (id < 0) {
            printk("Unable to create a new identity for NUS (err %d). Using the default one.\n", id);
            nus_conn_id = BT_ID_DEFAULT;
        } else {
            nus_conn_id = id;
            printk("Created a new identity for NUS: %d\n", nus_conn_id);
        }
    } else {
        nus_conn_id = 1;
        printk("Recovered identity for NUS: %d\n", nus_conn_id);
    }

     

    adv_params.id = nus_conn_id;


    /* 创建扩展广告实例 */
    err = bt_le_ext_adv_create(&adv_params, NULL, &nus_adv);
    if (err) {
        printk("Failed to create extended advertising instance (err %d)\n", err);
        return err;
    }

    /* 设置广告和扫描响应数据 */
    err = bt_le_ext_adv_set_data(nus_adv, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    if (err) {
        printk("Failed to set extended advertising data (err %d)\n", err);
        return err;
    }

    printk("Extended advertising instance created\n");
    return 0;
}

/* 启动扩展广告 */
static int start_extended_advertising(void)
{
    int err;

    err = bt_le_ext_adv_start(nus_adv, BT_LE_EXT_ADV_START_DEFAULT);
    if (err) {
        printk("Failed to start extended advertising (err %d)\n", err);
        return err;
    }

    printk("Extended advertising started\n");
    return 0;
}

/* 初始化NUS服务的入口函数 */
int nus_handler_init(void)
{
    int err;

    /* 初始化NUS服务 */
    err = bt_nus_init(&nus_cb);
    if (err) {
        printk("Failed to initialize NUS (err %d)\n", err);
        return err;
    }

    printk("NUS service initialized\n");

    /* 初始化扩展广告 */
    err = init_extended_advertising();
    if (err) {
        return err;
    }

    /* 启动扩展广告 */
    err = start_extended_advertising();
    if (err) {
        return err;
    }

    

    printk("NUS Handler initialized\n");
    return 0;
}