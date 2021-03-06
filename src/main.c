#include "main.h"

static bool init() {
	if (init_board_sensors() == false) {
		printk("Failed sensor initialization, stopping...\n");
		return false;
	}

	if (init_bg96() == false) {
		printk("Failed BG96 initialization, stopping...\n");
		return false;
	}

	return true;
}

void main(void)
{
	if (!init()) {
		return;
	}
	printk("Devices initialized!\n");

	int ret;
	int batt_lvl;
	int altitude;
	char imei[IMEI_SIZE + 1];
	char temp[TEMP_SENSOR_BUFF_LEN];
	char ur[TEMP_SENSOR_BUFF_LEN];
	char latitude[LATITUDE_LEN];
	char longitude[LONGITUDE_LEN];

	altitude = 0;
	memset(temp, 0, TEMP_SENSOR_BUFF_LEN);
	memset(ur, 0, TEMP_SENSOR_BUFF_LEN);
	memset(latitude, 0, LATITUDE_LEN);
	memset(longitude, 0, LONGITUDE_LEN);

	// TODO: Do something with this info
	ret = get_imei(imei, IMEI_SIZE);
	// TODO: Do something with this info
	ret = shtc3_sensor_read(temp, ur, TEMP_SENSOR_BUFF_LEN, TEMP_SENSOR_BUFF_LEN);
	batt_lvl = get_batt_reading();

	/* turn_on_gps(); */
	/* for (int i=0; i<6; i++) { */
	/* 	k_msleep(10000); */
	/* 	determine_position(latitude, LATITUDE_LEN, longitude, LONGITUDE_LEN, &altitude); */
	/* } */
	/* turn_off_gps(); */

	char buffer[PAYLOAD_MAX_SIZE];
	set_payload(imei, batt_lvl, temp, ur, latitude, longitude, buffer, PAYLOAD_MAX_SIZE);
	printk("-----------PAYLOAD--------------\n");
	printk("%s\n", buffer);
	printk("-----------PAYLOAD--------------\n");

	ret = server_connect();
	if (ret) {
		printk("Connected\n");
		send_payload(buffer, strlen(buffer));
	} else {
		printk("Not connected\n");
	}
	/* free(buffer); */

	printk("Exiting main\n");
}
