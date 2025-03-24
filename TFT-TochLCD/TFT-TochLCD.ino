#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

#define I2C_SDA 27
#define I2C_SCL 22
TwoWire I2CBMP = TwoWire(0);
Adafruit_BMP280 bmp; 

// 将温度的变量设置为 0
#define TEMP_CELSIUS 1
#define BMP_NUM_READINGS 20
float bmp_last_readings[BMP_NUM_READINGS] = {-20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0, -20.0};
float scale_min_temp;
float scale_max_temp;

// 触摸屏引脚
#define XPT2046_IRQ 35   // T_IRQ
#define XPT2046_MOSI 13  // T_DIN
#define XPT2046_MISO 12  // T_OUT
#define XPT2046_CLK 14   // T_CLK
#define XPT2046_CS 33    // T_CS
#define TFT_CS 15        // TFT_CS

// 初始化 TFT 屏幕和触摸屏
TFT_eSPI tft = TFT_eSPI();
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(TOUCH_CS, XPT2046_IRQ);

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define FONT_SIZE 2
int x, y, z;

#define DRAW_BUF_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT / 10 * (LV_COLOR_DEPTH / 8))
uint32_t draw_buf[DRAW_BUF_SIZE / 4];

unsigned long previousMillis = 0;

const long interval = 10000;

void log_print(lv_log_level_t level, const char * buf) {
  LV_UNUSED(level);
  Serial.println(buf);
  Serial.flush();
}

// 获取触摸屏数据
void touchscreen_read(lv_indev_t * indev, lv_indev_data_t * data) {
  // 检查是否触摸了触摸屏，并打印
  if(touchscreen.tirqTouched() && touchscreen.touched()) {
    // Get Touchscreen points
    TS_Point p = touchscreen.getPoint();

    // 高级屏幕校准
    float alpha_x, beta_x, alpha_y, beta_y, delta_x, delta_y;

    // 替换为您自己的校准值
    alpha_x = -0.000;
    beta_x = 0.090;
    delta_x = -33.771;
    alpha_y = 0.066;
    beta_y = 0.000;
    delta_y = -14.632;

    x = alpha_y * p.x + beta_y * p.y + delta_y;
   
    x = max(0, x);
    x = min(SCREEN_WIDTH - 1, x);

    y = alpha_x * p.x + beta_x * p.y + delta_x;

    y = max(0, y);
    y = min(SCREEN_HEIGHT - 1, y);

    z = p.z;

    data->state = LV_INDEV_STATE_PRESSED;

    // 设置坐标
    data->point.x = x;
    data->point.y = y;

    // 在串行监视器上打印有关 X、Y、Z 的触摸屏信息
    Serial.print("X = ");
    Serial.print(x);
    Serial.print(" | Y = ");
    Serial.print(y);
    Serial.print(" | Pressure = ");
    Serial.print(z);
    Serial.println();
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// 在该图表上绘制一个标签，其中包含按下点的值
static void chart_draw_label_cb(lv_event_t * e) {
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * chart = (lv_obj_t*) lv_event_get_target(e);

  if(code == LV_EVENT_VALUE_CHANGED) {
    lv_obj_invalidate(chart);
  }
  if(code == LV_EVENT_REFR_EXT_DRAW_SIZE) {
    int32_t * s = (int32_t*)lv_event_get_param(e);
    *s = LV_MAX(*s, 20);
  }
  // Draw the label on the chart based on the pressed point
  else if(code == LV_EVENT_DRAW_POST_END) {
    int32_t id = lv_chart_get_pressed_point(chart);
    if(id == LV_CHART_POINT_NONE) return;

    LV_LOG_USER("Selected point %d", (int)id);

    lv_chart_series_t * ser = lv_chart_get_series_next(chart, NULL);
    while(ser) {
      lv_point_t p;
      lv_chart_get_point_pos_by_id(chart, ser, id, &p);

      int32_t * y_array = lv_chart_get_y_array(chart, ser);
      int32_t value = y_array[id];
      char buf[16];
      #if TEMP_CELSIUS
        const char degree_symbol[] = "\u00B0C";
      #else
        const char degree_symbol[] = "\u00B0F";
      #endif

      // 为所选数据点准备标签文本
      lv_snprintf(buf, sizeof(buf), LV_SYMBOL_DUMMY " %3.2f %s ", float(bmp_last_readings[id]), degree_symbol);

      // 绘制将显示温度值的矩形标签
      lv_draw_rect_dsc_t draw_rect_dsc;
      lv_draw_rect_dsc_init(&draw_rect_dsc);
      draw_rect_dsc.bg_color = lv_color_black();
      draw_rect_dsc.bg_opa = LV_OPA_60;
      draw_rect_dsc.radius = 2;
      draw_rect_dsc.bg_image_src = buf;
      draw_rect_dsc.bg_image_recolor = lv_color_white();
      // 矩形标签大小
      lv_area_t a;
      a.x1 = chart->coords.x1 + p.x - 35;
      a.x2 = chart->coords.x1 + p.x + 35;
      a.y1 = chart->coords.y1 + p.y - 30;
      a.y2 = chart->coords.y1 + p.y - 10;
      lv_layer_t * layer = lv_event_get_layer(e);
      lv_draw_rect(layer, &draw_rect_dsc, &a);
      ser = lv_chart_get_series_next(chart, ser);
    }
  }
  else if(code == LV_EVENT_RELEASED) {
    lv_obj_invalidate(chart);
  }
}

// 绘制图表
void lv_draw_chart(void) {
  // Clear screen
  lv_obj_clean(lv_scr_act());

  // 创建顶部对齐的文本标签
  lv_obj_t * label = lv_label_create(lv_screen_active());
  lv_label_set_text(label, "BMP280 Temperature Readings");
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

  // 创建容器以显示图表和刻度
  lv_obj_t * container_row = lv_obj_create(lv_screen_active());
  lv_obj_set_size(container_row, SCREEN_HEIGHT-20,  SCREEN_WIDTH-40);
  lv_obj_align(container_row, LV_ALIGN_BOTTOM_MID, 0, -10);
  // Set the container in a flexbox row layout aligned center
  lv_obj_set_flex_flow(container_row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // 创建图表
  lv_obj_t * chart = lv_chart_create(container_row);
  lv_obj_set_size(chart, SCREEN_HEIGHT-90, SCREEN_WIDTH-70);
  lv_chart_set_point_count(chart, BMP_NUM_READINGS);
  lv_obj_add_event_cb(chart, chart_draw_label_cb, LV_EVENT_ALL, NULL);
  lv_obj_refresh_ext_draw_size(chart);

  // 添加数据系列
  lv_chart_series_t * chart_series = lv_chart_add_series(chart, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

  for(int i = 0; i < BMP_NUM_READINGS; i++) {
    if(float(bmp_last_readings[i]) != -20.0) { 
      // 在图表中设置点，并使用 *100 乘数对其进行缩放，以删除 2 个浮点数
      chart_series->y_points[i] = float(bmp_last_readings[i]) * 100;
    }
  }
  //设置图表范围，并使用 *100 乘数对其进行缩放，以删除 2 个浮点数
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, int(scale_min_temp-1)*100, int(scale_max_temp+1)*100);
  lv_chart_refresh(chart); // Required to update the chart with the new values

  // 创建一个垂直向右对齐的刻度（温度的 y 轴）
  lv_obj_t * scale = lv_scale_create(container_row);
  lv_obj_set_size(scale, 15, SCREEN_WIDTH-90);
  lv_scale_set_mode(scale, LV_SCALE_MODE_VERTICAL_RIGHT);
  lv_scale_set_label_show(scale, true);
  // Set the scale ticks count 
  lv_scale_set_total_tick_count(scale, int(scale_max_temp+2) - int(scale_min_temp-1));
  if((int(scale_max_temp+2) - int(scale_min_temp-1)) < 10) {
    lv_scale_set_major_tick_every(scale, 1); // set y axis to have 1 tick every 1 degree
  }
  else {
    lv_scale_set_major_tick_every(scale, 10); // set y axis to have 1 tick every 10 degrees
  }
  // 设置刻度样式和范围
  lv_obj_set_style_length(scale, 5, LV_PART_ITEMS);
  lv_obj_set_style_length(scale, 10, LV_PART_INDICATOR);
  lv_scale_set_range(scale, int(scale_min_temp-1), int(scale_max_temp+1));
}

void get_bmp_readings(void) {
  #if TEMP_CELSIUS
    float bmp_temp = bmp.readTemperature();
  #else
    float bmp_temp = 1.8 * bmp.readTemperature() + 32;  
  #endif
  
  //重置刻度范围（图表 y 轴）变量
  scale_min_temp = 120.0;
  scale_max_temp = -20.0;

  // 将值移动到数组的左侧，并在末尾插入最新的读数
  for (int i = 0; i < BMP_NUM_READINGS; i++) {
    if(i == (BMP_NUM_READINGS-1) && float(bmp_temp) < 120.0) {
      bmp_last_readings[i] = float(bmp_temp);  
    }
    else {
      bmp_last_readings[i] = float(bmp_last_readings[i + 1]);  // Shift values to the left of the array
    }
    //获取数组中的最小值/最大值以设置刻度范围（图表 y 轴）
    if((float(bmp_last_readings[i]) < scale_min_temp) && (float(bmp_last_readings[i]) != -20.0 )) {
      scale_min_temp = bmp_last_readings[i];
    }
    if((float(bmp_last_readings[i]) > scale_max_temp) && (float(bmp_last_readings[i]) != -20.0 )) {
      scale_max_temp = bmp_last_readings[i];
    }
  }
  Serial.print("Min temp: ");
  Serial.println(float(scale_min_temp));
  Serial.print("Max temp: ");
  Serial.println(float(scale_max_temp));
  Serial.print("BMP last reading: ");
  Serial.println(float(bmp_last_readings[BMP_NUM_READINGS-1]));
  lv_draw_chart();
}

void setup() {

String LVGL_Arduino = String("LVGL Library Version: ") + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();
  Serial.begin(115200);
  Serial.println(LVGL_Arduino);

  Wire.begin(I2C_SDA,I2C_SCL);
  bool status;
  //设置自定义 I2C 端口
  if (!bmp.begin(BMP280_ADDRESS_ALT, BMP280_CHIPID)) {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
    while (1);
  }
  
  //启动 LVGL
  lv_init();

  lv_log_register_print_cb(log_print);

  pinMode(XPT2046_CS, OUTPUT);
  pinMode(TFT_CS, OUTPUT);
  
  // 确保触摸屏和TFT屏幕的CS引脚拉高，防止初始化冲突
  digitalWrite(XPT2046_CS, HIGH);  
  digitalWrite(TFT_CS, HIGH); 

  digitalWrite(XPT2046_CS, LOW); 
  // 初始化触摸屏
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);  // 设置触摸屏旋转

   lv_display_t * disp;
  // 使用 TFT_eSPI 库初始化 TFT 显示器
  disp = lv_tft_espi_create(SCREEN_WIDTH, SCREEN_HEIGHT, draw_buf, sizeof(draw_buf));
  lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_270);
  
  //初始化 LVGL 输入设备对象（触摸屏）
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  // 设置回调函数以读取触摸屏输入
  lv_indev_set_read_cb(indev, touchscreen_read);

  get_bmp_readings();

}


void loop() {
  lv_task_handler();  
  lv_tick_inc(5);    
  delay(5);           

    unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;
    get_bmp_readings();    
  }
}
