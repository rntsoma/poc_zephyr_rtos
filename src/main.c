#include "main.h"

/* Static variables */
static uint8_t send_buffer[RING_BUFFER_SIZE];

void main(void)
{
	if (init_board_sensors() == false) {
		printk("Failed sensor initialization, stopping...\n");
		return;
	}

	if (init_bg96() == false) {
		printk("Failed BG96 initialization, stopping...\n");
		return;
	}

	printk("Devices initialized!\n");
	memset(send_buffer, 0, RING_BUFFER_SIZE);
	memcpy(send_buffer, "ATI", 3);

	k_msleep(SLEEP_TIME_MS*2);

	int reps = 0;
	char rsp[100];
	printk("Begin main loop\n");
	while (reps <= 1) {
		printk("Iteration: %d\n", reps);
		shtc3_sensor_read();

		send_at_command(send_buffer, strlen(send_buffer), rsp);
		printk("main, returned: %s\n", rsp);
		k_msleep(10000);

		reps++;
	}

	printk("\n\nGet GMI\n");
	send_at_command("AT+GMI", strlen("AT+GMI"), rsp);
	k_msleep(10000);
	printk("\n\nGet GMM\n");
	send_at_command("AT+GMM", strlen("AT+GMM"), rsp);
	k_msleep(10000);
	printk("\n\nGet ERR\n");
	send_at_command("AT+ERR", strlen("AT+ERR"), rsp);
	k_msleep(10000);
	printk("\n\nGet GMR\n");
	send_at_command("AT+GMR", strlen("AT+GMR"), rsp);
	k_msleep(10000);
	printk("\n\nGet IMEI\n");
	send_at_command("AT+GSN", strlen("AT+GSN"), rsp);
	k_msleep(10000);

	printk("Exiting main\n");
}
