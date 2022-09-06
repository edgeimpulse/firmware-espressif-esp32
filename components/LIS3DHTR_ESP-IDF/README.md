# LIS3DHTR_ESP-IDF

## Introduction 
Modified from an Arduino library for 3-Axis Digital Accelerometer ±2g to 16g (LIS3DHTR) to work with ESP IDF framework. Acceleration data can be obtained using IIC interface.

## How to install 
Git clone to your project ```components``` folder.

## Usage

```C++
#include <stdio.h>
// Include FreeRTOS for delay
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
// This example uses I2C.
#include "LIS3DHTR.h"

LIS3DHTR lis; //IIC
static float imu_data[3];

extern "C" int app_main()
{
  lis.begin(LIS3DHTR_DEFAULT_ADDRESS);

  if(lis.isConnection() == false) {
      printf("ERR: failed to connect to LIS3DHTR!\n");
      return false;
  }

  vTaskDelay(100 / portTICK_RATE_MS);
  lis.setFullScaleRange(LIS3DHTR_RANGE_2G);
  lis.setOutputDataRate(LIS3DHTR_DATARATE_100HZ);

  while (true) {
  //3 axis
  lis.getAcceleration(&imu_data[0], &imu_data[1], &imu_data[2]);
  printf("x: %f \t y: %f \t z: %f \n", imu_data[0], imu_data[1], imu_data[2]);
  }
}
```

----
## License
This software is written by Seeed studio<br>
and is licensed under [The MIT License](http://opensource.org/licenses/mit-license.php). Check License.txt for more information.<br>

Modified by Dmitry Maslov for EdgeImpulse Inc.

Contributing to this software is warmly welcomed. You can do this basically by<br>
[forking](https://help.github.com/articles/fork-a-repo), committing modifications and then [pulling requests](https://help.github.com/articles/using-pull-requests) (follow the links above<br>
for operating guide). Adding change log and your contact into file header is encouraged.<br>
Thanks for your contribution.

Seeed Studio is an open hardware facilitation company based in Shenzhen, China. <br>
Benefiting from local manufacture power and convenient global logistic system, <br>
we integrate resources to serve new era of innovation. Seeed also works with <br>
global distributors and partners to push open hardware movement.<br>