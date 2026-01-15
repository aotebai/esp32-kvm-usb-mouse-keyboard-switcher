#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/rmt.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_rom_sys.h" // 新增：硬件延时头文件

// ==================== 核心配置参数 ====================
// UART配置
#define BAUD_RATE          115200
#define FRAME_LENGTH       7
#define SWITCH_LOCKOUT_MS  1500

// DMA配置
#define UART_DMA_BUFF_SIZE     256
#define UART_DMA_RX_TIMEOUT    1
#define UART_DMA_RX_THRESHOLD  (UART_DMA_BUFF_SIZE / 2)

// UART端口与引脚
#define UART_LOWER_NUM     UART_NUM_1
#define UART_UPPER_A_NUM   UART_NUM_0
#define UART_UPPER_B_NUM   UART_NUM_2

#define UART_LOWER_TXD     17
#define UART_LOWER_RXD     18
#define UART_UPPER_A_TXD   11
#define UART_UPPER_A_RXD   10
#define UART_UPPER_B_TXD   43
#define UART_UPPER_B_RXD   44

// 微动开关引脚
#define K1_GPIO            12
#define K2_GPIO            13
#define K3_GPIO            14

// K3长按复位配置
#define K3_LONG_PRESS_MS   3000    // 长按3秒触发复位
#define K3_DEBOUNCE_MS     50      // 消抖时间

// 鼠标中键帧解析
#define MOUSE_FRAME_HEADER1 0x57
#define MOUSE_FRAME_HEADER2 0xAB
#define MOUSE_OPCODE       0x02
#define MOUSE_BUTTON_BYTE  3
#define MIDDLE_BUTTON_BIT  2

// WS2812配置
#define LED_STRIP_GPIO_PIN         48
#define LED_STRIP_LED_COUNT        1
#define RMT_RESOLUTION_HZ          10000000
#define RMT_CHANNEL                RMT_CHANNEL_0

// 爆闪参数
#define BURST_TIMES_PER_COLOR      10
#define BURST_FLASH_PERIOD_US      80000
#define BURST_ON_US                40000
#define PAUSE_MS_BETWEEN_COLOR     50
#define BURST_SELECT_COLOR_NUM     3

// 呼吸灯参数
#define BREATH_FREQ_PER_MIN        7.5f
#define BREATH_PERIOD_MS           (uint32_t)(2 * 60 * 1000 / BREATH_FREQ_PER_MIN)
#define BREATH_STEP_MS             10
#define BREATH_MAX_BRIGHTNESS      155
#define BREATH_SMOOTH_FACTOR       2.0f
#define BREATH_DARK_OFF_MS         500

// WS2812时序
#define WS2812_T0H_TICKS           (uint16_t)(0.4 * RMT_RESOLUTION_HZ / 1000000)
#define WS2812_T0L_TICKS           (uint16_t)(0.85 * RMT_RESOLUTION_HZ / 1000000)
#define WS2812_T1H_TICKS           (uint16_t)(0.8 * RMT_RESOLUTION_HZ / 1000000)
#define WS2812_T1L_TICKS           (uint16_t)(0.45 * RMT_RESOLUTION_HZ / 1000000)
#define WS2812_RESET_TICKS         (uint16_t)(50 * RMT_RESOLUTION_HZ / 1000000)

// ==================== 类型定义 ====================
// 连接状态
typedef enum {
    CONNECTED_TO_A,
    CONNECTED_TO_B
} ConnectionState;

// 呼吸灯颜色
typedef enum {
    BREATH_COLOR_RED,
    BREATH_COLOR_BLUE
} BreathColor;

// RGB颜色结构
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// ==================== 全局变量 ====================
static const char *TAG = "ch9350_led_switch";

// 连接状态
static ConnectionState currentState = CONNECTED_TO_A;
static volatile uint64_t lastSwitchTime = 0;
static volatile bool mouse_middle_enable = true;
// LED功能总开关
static volatile bool led_function_enable = true;

// K3长按检测变量
static volatile uint64_t k3_press_start_time = 0;
static volatile bool k3_is_pressed = false;

// 同步信号量/标志
static SemaphoreHandle_t switchSemaphore = NULL;
static SemaphoreHandle_t ledSemaphore = NULL;
static volatile gpio_num_t triggerGpio = GPIO_NUM_NC;
static volatile BreathColor currentBreathColor = BREATH_COLOR_BLUE;
static volatile bool led_stop_flag = false; // 呼吸灯停止标志

// UART队列
static QueueHandle_t uart_lower_queue = NULL;
static QueueHandle_t uart_upper_a_queue = NULL;
static QueueHandle_t uart_upper_b_queue = NULL;

// LED颜色池
static const rgb_color_t burst_color_pool[] = {
    {255, 0, 0},     // 红
    {255, 165, 0},   // 橙
    {255, 255, 0},   // 黄
    {0, 255, 0},     // 绿
    {0, 0, 255},     // 蓝
    {255, 0, 255},   // 紫
    {255, 255, 255}  // 白
};
#define BURST_POOL_COUNT (sizeof(burst_color_pool) / sizeof(rgb_color_t))

// ==================== 函数声明 ====================
// UART相关
static uart_port_t getActiveUpperUart(void);
static QueueHandle_t getActiveUpperUartQueue(void);
static void uart_config(void);
static void handleUartInterruptEvent(QueueHandle_t uart_queue, uart_port_t src_uart, uart_port_t dest_uart);
static void uart_forward_task(void *arg); // 新增：UART转发独立任务

// 切换逻辑
static void switchConnection(void);
static void toggleMouseMiddleFunc(void);
static void toggleLedFunction(void);
static bool parseMouseFrame(uint8_t *frame, int len);

// GPIO中断
static void IRAM_ATTR gpio_isr_handler(void *arg);
static void gpio_interrupt_config(void);

// K3长按检测
static void k3_long_press_detect_task(void *arg);

// LED相关
static esp_err_t rmt_ws2812_init(void);
static void ws2812_color_to_rmt_items(uint8_t r, uint8_t g, uint8_t b, rmt_item32_t *items, size_t *item_num);
static void rmt_send_ws2812_color(uint8_t r, uint8_t g, uint8_t b);
static void burst_select_random_colors(rgb_color_t *selected_colors, size_t select_num);
static void burst_flash_random_3color(void);
static void breath_light_loop(BreathColor color);
static void led_control_task(void *arg);

// ==================== UART配置 ====================
static void uart_config() {
    uart_config_t cfg = {
        .baud_rate = BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // 下位机UART
    uart_param_config(UART_LOWER_NUM, &cfg);
    uart_set_pin(UART_LOWER_NUM, UART_LOWER_TXD, UART_LOWER_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(
        UART_LOWER_NUM,
        UART_DMA_BUFF_SIZE,
        UART_DMA_BUFF_SIZE,
        10,
        &uart_lower_queue,
        0
    );
    uart_set_rx_timeout(UART_LOWER_NUM, UART_DMA_RX_TIMEOUT);
    uart_set_rx_full_threshold(UART_LOWER_NUM, UART_DMA_RX_THRESHOLD);
    uart_flush_input(UART_LOWER_NUM);

    // 上位机A UART
    uart_param_config(UART_UPPER_A_NUM, &cfg);
    uart_set_pin(UART_UPPER_A_NUM, UART_UPPER_A_TXD, UART_UPPER_A_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(
        UART_UPPER_A_NUM,
        UART_DMA_BUFF_SIZE,
        UART_DMA_BUFF_SIZE,
        10,
        &uart_upper_a_queue,
        0
    );
    uart_set_rx_timeout(UART_UPPER_A_NUM, UART_DMA_RX_TIMEOUT);
    uart_set_rx_full_threshold(UART_UPPER_A_NUM, UART_DMA_RX_THRESHOLD);
    uart_flush_input(UART_UPPER_A_NUM);

    // 上位机B UART
    uart_param_config(UART_UPPER_B_NUM, &cfg);
    uart_set_pin(UART_UPPER_B_NUM, UART_UPPER_B_TXD, UART_UPPER_B_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(
        UART_UPPER_B_NUM,
        UART_DMA_BUFF_SIZE,
        UART_DMA_BUFF_SIZE,
        10,
        &uart_upper_b_queue,
        0
    );
    uart_set_rx_timeout(UART_UPPER_B_NUM, UART_DMA_RX_TIMEOUT);
    uart_set_rx_full_threshold(UART_UPPER_B_NUM, UART_DMA_RX_THRESHOLD);
    uart_flush_input(UART_UPPER_B_NUM);

    ESP_LOGI(TAG, "UART（DMA模式）初始化完成");
}

// ==================== 连接切换逻辑 ====================
static void switchConnection() {
    uint64_t t = esp_timer_get_time() / 1000;
    if (t - lastSwitchTime <= SWITCH_LOCKOUT_MS) return;
    lastSwitchTime = t;

    // 1. 立即停止当前呼吸灯
    led_stop_flag = true;
    rmt_send_ws2812_color(0, 0, 0); // 强制熄灭LED
    vTaskDelay(pdMS_TO_TICKS(10));  // 确保停止信号生效

    // 2. 切换连接状态
    ConnectionState oldState = currentState;
    currentState = (currentState == CONNECTED_TO_A) ? CONNECTED_TO_B : CONNECTED_TO_A;
    const char* target = (currentState == CONNECTED_TO_A) ? "上位机A" : "上位机B";

    // 3. 设置新呼吸灯颜色
    currentBreathColor = (currentState == CONNECTED_TO_A) ? BREATH_COLOR_BLUE : BREATH_COLOR_RED;

    // 4. 清空UART缓冲区
    uart_flush_input(getActiveUpperUart());
    uart_flush_input(UART_LOWER_NUM);

    ESP_LOGI(TAG, "[K1/中键] 切换到 %s，呼吸灯颜色：%s", 
             target, 
             (currentBreathColor == BREATH_COLOR_RED) ? "红色" : "蓝色");

    // 5. 触发LED任务（执行新特效）
    xSemaphoreGive(ledSemaphore);
}

static void toggleMouseMiddleFunc() {
    uint64_t t = esp_timer_get_time() / 1000;
    if (t - lastSwitchTime <= SWITCH_LOCKOUT_MS) return;
    lastSwitchTime = t;

    mouse_middle_enable = !mouse_middle_enable;
    ESP_LOGI(TAG, "鼠标中键功能 → %s", mouse_middle_enable ? "开启" : "关闭");
}

// K3键控制LED功能启停
static void toggleLedFunction() {
    uint64_t t = esp_timer_get_time() / 1000;
    if (t - lastSwitchTime <= SWITCH_LOCKOUT_MS) return;
    lastSwitchTime = t;

    led_function_enable = !led_function_enable;
    if (!led_function_enable) {
        // 关闭时停止所有LED特效并熄灭
        led_stop_flag = true;
        rmt_send_ws2812_color(0, 0, 0);
        ESP_LOGI(TAG, "LED特效功能 → 关闭（爆闪/呼吸灯均禁用）");
    } else {
        // 开启时恢复当前呼吸灯
        ESP_LOGI(TAG, "LED特效功能 → 开启（恢复%s呼吸灯）", 
                 (currentBreathColor == BREATH_COLOR_RED) ? "红色" : "蓝色");
        xSemaphoreGive(ledSemaphore);
    }
}

// ==================== 鼠标帧解析 ====================
static bool parseMouseFrame(uint8_t *frame, int len) {
    if (!mouse_middle_enable) return false;

    if (len != FRAME_LENGTH ||
        frame[0] != MOUSE_FRAME_HEADER1 ||
        frame[1] != MOUSE_FRAME_HEADER2 ||
        frame[2] != MOUSE_OPCODE) {
        return false;
    }

    if (((frame[MOUSE_BUTTON_BYTE] >> MIDDLE_BUTTON_BIT) & 0x01) == 1) {
        ESP_LOGD(TAG, "[鼠标中键触发] 中键按下 → 切换上位机（此帧不转发）");
        switchConnection();
        return true;
    }

    return false;
}

// ==================== GPIO中断 ====================
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    gpio_num_t gpio = (gpio_num_t)arg;
    // 支持K1/K2/K3_GPIO中断
    if (gpio != K1_GPIO && gpio != K2_GPIO && gpio != K3_GPIO) return;

    uint64_t now = esp_timer_get_time() / 1000;

    // K3按键单独处理：记录按下/释放时间
    if (gpio == K3_GPIO) {
        if (gpio_get_level(K3_GPIO) == 0) { // 按下（低电平）
            k3_press_start_time = now;
            k3_is_pressed = true;
        } else { // 释放（高电平）
            // 短按：触发LED启停（需消抖）
            if (k3_is_pressed && (now - k3_press_start_time) < K3_LONG_PRESS_MS && (now - k3_press_start_time) > K3_DEBOUNCE_MS) {
                triggerGpio = K3_GPIO; // 标记为短按事件
                BaseType_t hp = pdFALSE;
                xSemaphoreGiveFromISR(switchSemaphore, &hp);
                if (hp) portYIELD_FROM_ISR();
            }
            k3_is_pressed = false;
            k3_press_start_time = 0;
        }
        return;
    }

    // K1/K2正常处理
    triggerGpio = gpio;
    BaseType_t hp = pdFALSE;
    xSemaphoreGiveFromISR(switchSemaphore, &hp);
    if (hp) portYIELD_FROM_ISR();
}

static void gpio_interrupt_config() {
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << K1_GPIO) | (1ULL << K2_GPIO) | (1ULL << K3_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_ANYEDGE // 双边沿触发（检测按下/释放）
    };
    gpio_config(&io);

    switchSemaphore = xSemaphoreCreateBinary();
    gpio_install_isr_service(0);

    gpio_isr_handler_add(K1_GPIO, gpio_isr_handler, (void*)K1_GPIO);
    gpio_isr_handler_add(K2_GPIO, gpio_isr_handler, (void*)K2_GPIO);
    gpio_isr_handler_add(K3_GPIO, gpio_isr_handler, (void*)K3_GPIO);

    ESP_LOGI(TAG, "K按键中断配置完成（K1/K2/K3均已配置）");
}

// ==================== K3长按检测任务 ====================
static void k3_long_press_detect_task(void *arg) {
    while (1) {
        if (k3_is_pressed) {
            uint64_t now = esp_timer_get_time() / 1000;
            // 长按3秒触发复位
            if (now - k3_press_start_time >= K3_LONG_PRESS_MS) {
                ESP_LOGI(TAG, "K3长按3秒 → 触发系统复位！");
                rmt_send_ws2812_color(255, 0, 0); // 复位前闪红灯提示
                esp_rom_delay_us(500000); // 硬件延时500ms（不依赖FreeRTOS调度）
                esp_restart(); // 系统复位（等效RST按键）
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms检测一次，降低CPU占用
    }
    vTaskDelete(NULL);
}

// ==================== UART事件处理 ====================
static void handleUartInterruptEvent(QueueHandle_t uart_queue,
                                     uart_port_t src_uart,
                                     uart_port_t dest_uart) {
    uart_event_t event;
    uint8_t buf[UART_DMA_BUFF_SIZE];
    int len;

    if (xQueueReceive(uart_queue, &event, 0)) {
        switch (event.type) {
            case UART_DATA:
                len = uart_read_bytes(src_uart, buf, event.size, pdMS_TO_TICKS(10));
                if (len > 0) {
                    bool block = false;

                    if (src_uart == UART_LOWER_NUM) {
                        int pos = 0;
                        while (pos + FRAME_LENGTH <= len) {
                            block = parseMouseFrame(&buf[pos], FRAME_LENGTH);
                            if (block) {
                                pos += FRAME_LENGTH;
                                ESP_LOGD(TAG, "[阻止] 中键帧已被丢弃，不转发");
                                block = true;
                            } else {
                                uart_write_bytes(dest_uart, (const char*)&buf[pos], FRAME_LENGTH);
                                pos += FRAME_LENGTH;
                            }
                        }
                        if (pos < len && !block) {
                            uart_write_bytes(dest_uart, (const char*)&buf[pos], len - pos);
                        }
                    } else {
                        uart_write_bytes(dest_uart, (const char*)buf, len);
                    }
                }
                break;

            case UART_FIFO_OVF:
                ESP_LOGE(TAG, "UART(%d) FIFO溢出！清空缓冲区", src_uart);
                uart_flush_input(src_uart);
                xQueueReset(uart_queue);
                break;

            case UART_BUFFER_FULL:
                ESP_LOGE(TAG, "UART(%d)缓冲区满！清空缓冲区", src_uart);
                uart_flush_input(src_uart);
                xQueueReset(uart_queue);
                break;

            default:
                break;
        }
    }
}

// ==================== 新增：UART转发独立任务 ====================
static void uart_forward_task(void *arg) {
    (void)arg; // 未使用参数
    while (1) {
        // 处理下位机→当前激活的上位机
        handleUartInterruptEvent(uart_lower_queue, UART_LOWER_NUM, getActiveUpperUart());
        // 处理当前激活的上位机→下位机
        handleUartInterruptEvent(getActiveUpperUartQueue(), getActiveUpperUart(), UART_LOWER_NUM);
        // 低频率轮询，降低CPU占用
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    vTaskDelete(NULL);
}

// ==================== 辅助函数 ====================
static uart_port_t getActiveUpperUart() {
    return (currentState == CONNECTED_TO_A) ? UART_UPPER_A_NUM : UART_UPPER_B_NUM;
}

static QueueHandle_t getActiveUpperUartQueue() {
    return (currentState == CONNECTED_TO_A) ? uart_upper_a_queue : uart_upper_b_queue;
}

// ==================== WS2812 LED控制 ====================
static esp_err_t rmt_ws2812_init(void) {
    rmt_config_t rmt_cfg = {
        .rmt_mode = RMT_MODE_TX,
        .channel = RMT_CHANNEL,
        .gpio_num = LED_STRIP_GPIO_PIN,
        .clk_div = 80 / (RMT_RESOLUTION_HZ / 1000000),
        .mem_block_num = 1,
        .tx_config = {
            .carrier_freq_hz = 38000,
            .carrier_level = RMT_CARRIER_LEVEL_HIGH,
            .carrier_en = false,
            .loop_en = false,
            .idle_level = RMT_IDLE_LEVEL_LOW,
            .idle_output_en = true,
        },
    };
    ESP_ERROR_CHECK(rmt_config(&rmt_cfg));
    ESP_ERROR_CHECK(rmt_driver_install(rmt_cfg.channel, 0, 0));

    return ESP_OK;
}

static void ws2812_color_to_rmt_items(uint8_t r, uint8_t g, uint8_t b, rmt_item32_t *items, size_t *item_num) {
    *item_num = 24;
    uint32_t grb = (g << 16) | (r << 8) | b;

    for (int i = 0; i < 24; i++) {
        uint32_t bit = (grb >> (23 - i)) & 0x1;
        if (bit) {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T1H_TICKS;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T1L_TICKS;
        } else {
            items[i].level0 = 1;
            items[i].duration0 = WS2812_T0H_TICKS;
            items[i].level1 = 0;
            items[i].duration1 = WS2812_T0L_TICKS;
        }
    }

    items[24].level0 = 0;
    items[24].duration0 = WS2812_RESET_TICKS;
    items[24].level1 = 0;
    items[24].duration1 = 0;
    *item_num += 1;
}

static void rmt_send_ws2812_color(uint8_t r, uint8_t g, uint8_t b) {
    rmt_item32_t items[25];
    size_t item_num = 0;

    ws2812_color_to_rmt_items(r, g, b, items, &item_num);
    ESP_ERROR_CHECK(rmt_write_items(RMT_CHANNEL, items, item_num, true));
    rmt_wait_tx_done(RMT_CHANNEL, portMAX_DELAY);
}

static void burst_select_random_colors(rgb_color_t *selected_colors, size_t select_num) {
    if (select_num > BURST_POOL_COUNT) {
        select_num = BURST_POOL_COUNT;
    }

    uint8_t indices[BURST_POOL_COUNT];
    for (int i = 0; i < BURST_POOL_COUNT; i++) {
        indices[i] = i;
    }

    for (int i = BURST_POOL_COUNT - 1; i > 0; i--) {
        uint8_t j = esp_random() % (i + 1);
        uint8_t temp = indices[i];
        indices[i] = indices[j];
        indices[j] = temp;
    }

    for (int i = 0; i < select_num; i++) {
        selected_colors[i] = burst_color_pool[indices[i]];
    }
}

static void burst_flash_random_3color(void) {
    // LED功能关闭时直接返回
    if (!led_function_enable) return;
    
    rgb_color_t selected_colors[BURST_SELECT_COLOR_NUM];
    burst_select_random_colors(selected_colors, BURST_SELECT_COLOR_NUM);

    // 爆闪前先熄灭
    rmt_send_ws2812_color(0, 0, 0);

    for (uint8_t color_idx = 0; color_idx < BURST_SELECT_COLOR_NUM; color_idx++) {
        rgb_color_t current_color = selected_colors[color_idx];

        for (int i = 0; i < BURST_TIMES_PER_COLOR; i++) {
            // 执行中检测开关状态
            if (!led_function_enable) {
                rmt_send_ws2812_color(0, 0, 0);
                return;
            }
            rmt_send_ws2812_color(current_color.r, current_color.g, current_color.b);
            vTaskDelay(pdMS_TO_TICKS(BURST_ON_US / 1000));

            rmt_send_ws2812_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS((BURST_FLASH_PERIOD_US - BURST_ON_US) / 1000));
        }

        if (color_idx < BURST_SELECT_COLOR_NUM - 1) {
            vTaskDelay(pdMS_TO_TICKS(PAUSE_MS_BETWEEN_COLOR));
        }
    }
}

// 循环呼吸灯（持续运行直到收到停止标志）
static void breath_light_loop(BreathColor color) {
    // LED功能关闭时直接返回
    if (!led_function_enable) return;
    
    const uint32_t total_steps = (BREATH_PERIOD_MS / BREATH_STEP_MS) / 2;
    const float rad_step = M_PI / total_steps;
    uint8_t last_bright = 0;

    // 重置停止标志
    led_stop_flag = false;

    while (!led_stop_flag) { // 循环直到收到停止信号
        // 执行中检测开关状态
        if (!led_function_enable) {
            rmt_send_ws2812_color(0, 0, 0);
            return;
        }
        
        // 渐亮阶段
        for (uint32_t i = 0; i < total_steps && !led_stop_flag; i++) {
            if (!led_function_enable) {
                rmt_send_ws2812_color(0, 0, 0);
                return;
            }
            const float rad = i * rad_step;
            float sin_val = sin(rad);
            
            sin_val = pow(sin_val, BREATH_SMOOTH_FACTOR);
            uint8_t brightness = (uint8_t)(sin_val * BREATH_MAX_BRIGHTNESS);

            // 平滑亮度变化
            if (abs((int)brightness - (int)last_bright) > 1) {
                brightness = last_bright + (brightness > last_bright ? 1 : -1);
            }
            last_bright = brightness;

            // 输出对应颜色
            if (color == BREATH_COLOR_RED) {
                rmt_send_ws2812_color(brightness, 0, 0);
            } else {
                rmt_send_ws2812_color(0, 0, brightness);
            }

            vTaskDelay(pdMS_TO_TICKS(BREATH_STEP_MS));
        }

        // 渐暗阶段
        for (uint32_t i = total_steps; i > 0 && !led_stop_flag; i--) {
            if (!led_function_enable) {
                rmt_send_ws2812_color(0, 0, 0);
                return;
            }
            const float rad = i * rad_step;
            float sin_val = sin(rad);
            
            sin_val = pow(sin_val, BREATH_SMOOTH_FACTOR);
            uint8_t brightness = (uint8_t)(sin_val * BREATH_MAX_BRIGHTNESS);

            // 平滑亮度变化
            if (abs((int)brightness - (int)last_bright) > 1) {
                brightness = last_bright + (brightness > last_bright ? 1 : -1);
            }
            last_bright = brightness;

            // 输出对应颜色
            if (color == BREATH_COLOR_RED) {
                rmt_send_ws2812_color(brightness, 0, 0);
            } else {
                rmt_send_ws2812_color(0, 0, brightness);
            }

            vTaskDelay(pdMS_TO_TICKS(BREATH_STEP_MS));
        }

        // 暗态停留（可选）
        if (!led_stop_flag && led_function_enable) {
            rmt_send_ws2812_color(0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(BREATH_DARK_OFF_MS));
        }
    }

    // 退出循环后强制熄灭
    rmt_send_ws2812_color(0, 0, 0);
}

static void led_control_task(void *arg) {
    // 初始状态：上位机A，蓝色呼吸灯（循环）
    if (led_function_enable) {
        breath_light_loop(BREATH_COLOR_BLUE);
    }

    while (1) {
        // 等待切换信号
        if (xSemaphoreTake(ledSemaphore, portMAX_DELAY) == pdTRUE) {
            // LED功能关闭时跳过特效
            if (!led_function_enable) continue;
            
            ESP_LOGI(TAG, "开始执行LED效果：三色爆闪 + %s呼吸灯", 
                     (currentBreathColor == BREATH_COLOR_RED) ? "红色" : "蓝色");
            
            // 执行三色爆闪
            burst_flash_random_3color();
            
            // 执行对应颜色的循环呼吸灯（直到下一次切换）
            breath_light_loop(currentBreathColor);
        }
    }

    vTaskDelete(NULL);
}

// ==================== 主函数 ====================
void app_main(void) {
    ESP_LOGI(TAG, "=== CH9350 键鼠切换+LED系统启动 ===");

    // 初始化信号量
    ledSemaphore = xSemaphoreCreateBinary();

    // 初始化UART
    uart_config();

    // 初始化GPIO中断
    gpio_interrupt_config();

    // 初始化RMT和LED
    rmt_ws2812_init();
    xTaskCreate(led_control_task, "led_control", 8192, NULL, 5, NULL); // LED控制任务

    // 创建K3长按检测任务
    xTaskCreate(k3_long_press_detect_task, "k3_long_press", 2048, NULL, 4, NULL);

    // 新增：创建UART转发独立任务（低优先级，降低主任务负载）
    xTaskCreate(uart_forward_task, "uart_forward", 4096, NULL, 1, NULL);

    // 简化后的主循环：仅阻塞等待按键事件，释放CPU给IDLE任务
    while (1) {
        if (switchSemaphore && xSemaphoreTake(switchSemaphore, portMAX_DELAY) == pdTRUE) {
            if (triggerGpio == K1_GPIO) {
                switchConnection();
            } else if (triggerGpio == K2_GPIO) {
                toggleMouseMiddleFunc();
            } else if (triggerGpio == K3_GPIO) { // 处理K3短按
                toggleLedFunction();
            }
            triggerGpio = GPIO_NUM_NC;
        }
    }
}

