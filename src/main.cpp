#include <lvgl.h>
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <string.h>
#include <math.h>
#include <Wire.h>
#include <../lib/MLX90640/MLX90640_API.h>
#include <../lib/MLX90640/MLX90640_I2C_Driver.h>

#define SCREEN_WIDTH (128 + 4)
#define SCREEN_HEIGHT (160 + 2)

#define TA_SHIFT 8 //Default shift for MLX90640 in open air

//#define USE_LVGL    //是否使用LVGL
//#define PRINT       //是否串口打印所有的温度信息
typedef struct
{
    float min;          //最低温度
    float max;          //最高温度
    uint16_t min_index; //最低温度在数组内的角标
    uint16_t max_index; //最高温度在数组内的角标
} Temp_MinMax;

#ifdef USE_LVGL
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];
#endif

TFT_eSPI tft = TFT_eSPI(SCREEN_WIDTH, SCREEN_HEIGHT);
TFT_eSprite show = TFT_eSprite(&tft);

const byte MLX90640_address = 0x33; //Default 7-bit unshifted address of the MLX90640
static float mlx90640To[768];
paramsMLX90640 mlx90640;
Temp_MinMax t_minMax;

/*

uint8_t img_data[19200] = {0};
static lv_img_dsc_t my_img = {
    .header.always_zero = 0,
    .header.w = 160,
    .header.h = 120,
    .data_size = 160 * 120 * LV_COLOR_DEPTH / 8,
    .header.cf = LV_IMG_CF_TRUE_COLOR,         
    .data = img_data,
};

*/

#ifdef USE_LVGL
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p); /* Display flushing */
#endif

bool MLX90640isConnected();                                //Returns true if the MLX90640 is detected on the I2C bus
void MLX90640_Init();                                      //初始化
void MLX90640_GetValue();                                  //获取温度,并将其存储在mlx90640To中
uint32_t colorTable_Get(float min, float max, float temp); //根据温度获取颜色
Temp_MinMax getMinMaxTemp();                               //获得最低最高温度及其索引
uint16_t rgb24to16(uint32_t);                              //24位色转16位
char str[30];                                              //用于sprintf


#ifdef USE_LVGL
void myWidgets()
{
    lv_obj_t *scr = lv_scr_act();

    lv_obj_t *btn1 = lv_btn_create(scr); //创建一个按钮
    lv_obj_set_size(btn1, 70, 30);
    lv_obj_set_pos(btn1, 10, 10);

    lv_obj_t *label1 = lv_label_create(btn1); //为按钮设置文本
    lv_label_set_text(label1, "Button");
    lv_obj_center(label1);
}
#endif

void setup()
{
    Serial.begin(115200);
    Serial.println("Setup Begin!");


#ifdef USE_LVGL
    lv_init();
#endif

    tft.begin();
    tft.setRotation(1); // landSacpe
    tft.fillScreen(TFT_BLACK);
    show.createSprite(160, 120);

#ifdef USE_LVGL
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    myWidgets();
#endif

    MLX90640_Init();
    Serial.println("Set up Done!");
}



void loop()
{
#ifdef USE_LVGL
    lv_timer_handler();

    delay(5);
#endif
    MLX90640_GetValue();

    t_minMax = getMinMaxTemp();

    Serial.printf("Min:%.2f,Max:%.2f\n", t_minMax.min, t_minMax.max);
    for (size_t i = 0; i < 32; i++)
    {
        for (size_t j = 0; j < 24; j++)
        {
            //显示
            show.fillRect(i * 5, j * 5, 5, 5, rgb24to16(colorTable_Get(t_minMax.min, t_minMax.max, mlx90640To[j * 32 + i])));
        }
    }

    /*跟踪显示最高温度和最低温度*/
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sprintf(str, "%.2fC", mlx90640To[t_minMax.min_index]);
    show.drawCentreString(str, (t_minMax.min_index % 32) * 5, t_minMax.min_index / 32 * 5, 1);
    sprintf(str, "%.2fC", mlx90640To[t_minMax.max_index]);
    show.drawCentreString(str, (t_minMax.max_index % 32) * 5, t_minMax.max_index / 32 * 5, 1);
    show.pushSprite(0, 0);

    //屏幕下方显示最高温度和最低温度
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.fillRect(0, 121, 160, 10, TFT_WHITE);
    sprintf(str, "%.1fC", mlx90640To[t_minMax.min_index]);
    show.drawCentreString(str, (t_minMax.min_index % 32) * 5, t_minMax.min_index / 32 * 5, 1);
    tft.drawCentreString(str, 50, 121, 1);
    sprintf(str, "%.1fC", mlx90640To[t_minMax.max_index]);
    tft.drawCentreString(str, 110, 121, 1);

    Serial.print(";\n");
}

#ifdef USE_LVGL

/* Display flushing */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);//
    tft.pushColors((uint16_t *)&color_p->full, w * h, true);
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

#endif

void MLX90640_Init()
{
    Wire.begin();
    Wire.setClock(800000); //Increase I2C clock speed to 800kHz


    while (!Serial)
        ; //Wait for user to open terminal

    if (MLX90640isConnected() == false)
    {
        Serial.println("MLX90640 not detected at default I2C address. Please check wiring. Freezing.");
        while (1)
            ;
    }
    Serial.println("MLX90640 online!");

    //Get device parameters - We only have to do this once
    int status;
    uint16_t eeMLX90640[832];
    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
    if (status != 0)
        Serial.println("Failed to load system parameters");

    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0)
        Serial.println("Parameter extraction failed");

    MLX90640_SetRefreshRate(MLX90640_address, 0x04); //Set rate to 4Hz effective - Works
}

//Returns true if the MLX90640 is detected on the I2C bus
bool MLX90640isConnected()
{
    Wire.beginTransmission((uint8_t)MLX90640_address);
    if (Wire.endTransmission() != 0)
        return (false); //Sensor did not ACK
    return (true);
}

void MLX90640_GetValue()
{
    for (byte x = 0; x < 2; x++) //Read both subpages
    {
        uint16_t mlx90640Frame[834];
        int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
        if (status < 0)
        {
            Serial.print("GetFrame Error: ");
            Serial.println(status);
        }

       // float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
        float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

        float tr = Ta - TA_SHIFT; //Reflected temperature based on the sensor ambient temperature
        float emissivity = 0.95;

        MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, mlx90640To);
    }


#ifdef PRINT
    for (int x = 0; x < 768; x++)
    {
        Serial.printf("%.2f,",mlx90640To[x]);
    }
#endif
}

Temp_MinMax getMinMaxTemp()
{
    Temp_MinMax minMax;

    minMax.max = mlx90640To[0];
    minMax.max_index = 0;
    minMax.min = mlx90640To[0];
    minMax.min_index = 0;

    for (size_t i = 1; i < 768; i++)
    {
        if (minMax.max < mlx90640To[i])
        {
            minMax.max = mlx90640To[i];
            minMax.max_index = i;
        }
        else if (minMax.min > mlx90640To[i])
        {
            minMax.min = mlx90640To[i];
            minMax.min_index = i;
        }
    }
    return minMax;
}

uint32_t colorTable_Get(float min, float max, float temp)
{
    float diff = max - min;
    uint32_t ret = 0;
    if (diff == 0)
    {
        return 0x00ffffff;
    }

    if (temp >= min - 0.001 && temp < min + 0.25 * diff)
    {
        ret = ((uint32_t)((temp - min) / (0.25 * diff) * 255) << 8 | 0xff);
    }
    if (temp >= min + 0.25 * diff && temp < min + 0.5 * diff)
    {
        ret = (0x00ff00 | (uint32_t)((min + 0.5 * diff - temp) / (0.25 * diff) * 255));
    }
    if (temp >= min + 0.5 * diff && temp < min + 0.75 * diff)
    {
        ret = ((uint32_t)((temp - min - 0.5 * diff) / (0.25 * diff) * 255) << 16 | 0xff00);
    }
    if (temp >= min + 0.75 * diff && temp <= max + 0.001)
    {
        ret = (0xff0000 | (255 - (uint32_t)((temp - min - 0.75 * diff) / (0.25 * diff) * 255)) << 8);
    }
    return ret;
}

inline uint16_t rgb24to16(uint32_t x)
{
    uint16_t r = x << 8 >> 24;
    uint16_t g = x << 16 >> 24;
    uint16_t b = x << 24 >> 24;
    return (r >> 3 << 11) | (g >> 2 << 5) | (b >> 3);
}