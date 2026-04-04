#include <Arduino.h>
#include <core/wdt.h>
#include <LittleFS.h>
#include <nvs_flash.h>
#include "app/espnow/slave.h"
#include "app/boot/boot.h"
#include "app/tasks/networkTask.h"
#include "app/tasks/inputTask.h"
#include <app_config.h>

void init(){
}

void setup() {
	#if BOARD_HAS_PSRAM
	heap_caps_malloc_extmem_enable(0);
	#endif

	#ifdef ESP32
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
	#endif

	// boot-time hooks (sensor init + boot sample logs)
	app::boot::run();

	// network task initializes ESP-NOW and handles radio/link work
	app::tasks::startNetworkTask();

	#if !ENABLE_POWERSAVE
	// start input task (battery + DHT reads)
	if (!app::tasks::startInputTask()){
		ESP_LOGE("MAIN", "Input task failed to start");
	}
	#endif

}

void loop() {
	  // network + input tasks run the work; keep loop idle
	  vTaskDelete(NULL);
}
