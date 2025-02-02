/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include "model_handler.h"
#include "nus_handler.h" 

#define GET_DATA_INTERVAL 999

static struct k_work_delayable get_data_work;

/* 传感器客户端回调函数 - 只处理People Count */
static void sensor_cli_data_cb(struct bt_mesh_sensor_cli *cli,
                               struct bt_mesh_msg_ctx *ctx,
                               const struct bt_mesh_sensor_type *sensor,
                               const struct bt_mesh_sensor_value *value)
{
    if (sensor->id == bt_mesh_sensor_people_count.id) {
        //printk("People count is %s\n", bt_mesh_sensor_ch_str(value));
         const char *count_str = bt_mesh_sensor_ch_str(value);
        printk("People count is %s\n", count_str);

        /* 更新 NUS 模块中的 people count 字符串 */
        nus_update_people_count(count_str);
    }
}

/* 简化的传感器客户端处理程序 */
static const struct bt_mesh_sensor_cli_handlers bt_mesh_sensor_cli_handlers = {
    .data = sensor_cli_data_cb,
};

static struct bt_mesh_sensor_cli sensor_cli =
    BT_MESH_SENSOR_CLI_INIT(&bt_mesh_sensor_cli_handlers);

/* 数据获取工作处理函数 - 只请求People Count */
static void get_data(struct k_work *work)
{
    if (!bt_mesh_is_provisioned()) {
        k_work_schedule(&get_data_work, K_MSEC(GET_DATA_INTERVAL));
        return;
    }

    int err = bt_mesh_sensor_cli_get(&sensor_cli, 
                                   NULL, 
                                   &bt_mesh_sensor_people_count, 
                                   NULL);
    if (err) {
        printk("Error getting people count (%d)\n", err);
    }
    
    k_work_schedule(&get_data_work, K_MSEC(GET_DATA_INTERVAL));
}

/* 健康服务回调（保留基本功能） */
static void attention_on(const struct bt_mesh_model *mod) {}
static void attention_off(const struct bt_mesh_model *mod) {}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
    .attn_on = attention_on,
    .attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
    .cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

/* 模型元素配置 */
static struct bt_mesh_elem elements[] = {
    BT_MESH_ELEM(1,
        BT_MESH_MODEL_LIST(
            BT_MESH_MODEL_CFG_SRV,
            BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
            BT_MESH_MODEL_SENSOR_CLI(&sensor_cli)
        ),
        BT_MESH_MODEL_NONE
    ),
};

static const struct bt_mesh_comp comp = {
    .cid = CONFIG_BT_COMPANY_ID,
    .elem = elements,
    .elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
    k_work_init_delayable(&get_data_work, get_data);
    k_work_schedule(&get_data_work, K_MSEC(GET_DATA_INTERVAL));
    
    return &comp;
}