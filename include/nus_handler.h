/* nus_handler.h */
#ifndef NUS_HANDLER_H
#define NUS_HANDLER_H

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>

int nus_handler_init(void);

// 更新 People Count 字符串（参数 new_val 为传感器转换后的字符串）
void nus_update_people_count(const char *new_val);

#endif /* NUS_HANDLER_H */