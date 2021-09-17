#include <device.h>
#include <sensor.h>
#include <zephyr.h>
#include <string.h>
#include <devicetree.h>
#include <drivers/gpio.h>
#include <drivers/uart.h>
#include <sys/ring_buffer.h>

#define SLEEP_TIME_MS   5000

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_HAS_STATUS(LED0_NODE, okay)
#define LED0	DT_GPIO_LABEL(LED0_NODE, gpios)
#define PIN	DT_GPIO_PIN(LED0_NODE, gpios)
#define FLAGS	DT_GPIO_FLAGS(LED0_NODE, gpios)
#else
/* A build error here means your board isn't set up to blink an LED. */
#error "Unsupported board: led0 devicetree alias is not defined"
#define LED0	""
#define PIN	0
#define FLAGS	0
#endif

/* The devicetree node identifier for the "shtc3" alias. */
#define SHTC3_NODE DT_ALIAS(shtc3)

#if !DT_NODE_HAS_STATUS(SHTC3_NODE, okay)
#error "Unsupported board: shtc3 devicetree alias is not defined"
#endif

#define UART0_NODE DT_NODELABEL(uart0)

#if !DT_NODE_HAS_STATUS(UART0_NODE, okay)
#error "Unsupported board: uart0 devicetree node is not defined"
#endif

#define GPIO0_NODE DT_NODELABEL(gpio0)
#if DT_NODE_HAS_STATUS(GPIO0_NODE, okay)
#define MDM_STATUS_PIN    31
#define MDM_RST_PIN       28
#define MDM_PWR_PIN       2
#define MDM_W_DISABLE_PIN 29
#define MDM_DTR_PIN       26
#define MDM_AP_RDY_PIN    30
#define MDM_PSM_PIN       3
#else
#error "Unsupported board: gpio0 devicetree node is not defined"
#endif

#define GPIO1_NODE DT_NODELABEL(gpio1)
#if DT_NODE_HAS_STATUS(GPIO1_NODE, okay)
#define MDM_3V8_PIN       9
#else
#error "Unsupported board: gpio1 devicetree node is not defined"
#endif

#define RING_BUFFER_SIZE 1024

/* Static variables */
static uint8_t recv_buffer[RING_BUFFER_SIZE];
static uint8_t send_buffer[RING_BUFFER_SIZE];

static struct ring_buf recv_ringbuff;
static struct ring_buf send_ringbuff;

/* Static methods */
static const struct device *init_led() {
	int ret;
	const struct device *led_dev = device_get_binding(LED0);

	if (led_dev == NULL) {
		printk("LED0 not found\n");
		return NULL;
	}

	ret = gpio_pin_configure(led_dev, PIN, GPIO_OUTPUT_ACTIVE | FLAGS);
	if (ret < 0) {
		printk("LED0 not configured\n");
		return NULL;
	}

	return led_dev;
}

static const struct device *init_shtc3() {
	const struct device *shtc3_dev = device_get_binding(DT_LABEL(SHTC3_NODE));

	return shtc3_dev;
}


static void uart_callback(const struct device *uart_dev, void *data) {
	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (uart_irq_rx_ready(uart_dev)) {
			int rb_len, recv_len;
			uint8_t buffer[64];

			// calculate available size to copy
			size_t len = MIN(ring_buf_space_get(&recv_ringbuff),
					 sizeof(buffer));

			// copy data from UART to buffer, caps at len
			recv_len = uart_fifo_read(uart_dev, buffer, len);

			// copy data received into the ringbuffer
			rb_len = ring_buf_put(&recv_ringbuff, buffer, recv_len);
			if (rb_len < recv_len) {
				printk("RX: Dropped %u bytes\n", recv_len - rb_len);
			}

			printk("%s", (char*)buffer);
		}

		if (uart_irq_tx_ready(uart_dev)) {
			printk("Callback on TX\n");
			int rb_len, send_len;
			uint8_t buffer[64];

			// retrieve the command from ringbuffer and set it to buffer
			rb_len = ring_buf_get(&send_ringbuff, buffer, sizeof(buffer));
			if (!rb_len) {
				printk("TX: Ring buffer empty, disable TX IRQ\n");
				uart_irq_tx_disable(uart_dev);
				continue;
			}

			// send the command in the buffer to the UART
			send_len = uart_fifo_fill(uart_dev, buffer, rb_len);
			if (send_len < rb_len) {
				printk("TX: Drop %d bytes\n", rb_len - send_len);
			}
		}
	}
}

static const struct device *init_uart() {
	const struct device *uart0_dev = device_get_binding(DT_LABEL(UART0_NODE));
	if (uart0_dev == NULL) {
		return NULL;
	}

	uart_irq_callback_set(uart0_dev, uart_callback);
	uart_irq_rx_enable(uart0_dev);

	return uart0_dev;
}

static void init_gpio1() {
	int ret;
	const struct device *gpio_dev = device_get_binding(DT_LABEL(GPIO1_NODE));

	if (gpio_dev == NULL) {
		printk("GPIO1 not found");
	}

	/* 3V8 */
	ret = gpio_pin_configure(gpio_dev, MDM_3V8_PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error configuring MDM_3V8_PIN: %d", ret);
	}

	gpio_pin_set(gpio_dev, MDM_3V8_PIN, 1);
}

static const struct device *init_gpio0() {
	int ret;
	const struct device *gpio_dev = device_get_binding(DT_LABEL(GPIO0_NODE));

	if (gpio_dev == NULL) {
		printk("GPIO0 not found");
		return NULL;
	}

	/* status pin */
	ret = gpio_pin_configure(gpio_dev, MDM_STATUS_PIN, GPIO_INPUT);
	if (ret < 0) {
		printk("Error configuring MDM_STATUS_PIN: %d", ret);
	}

	/* dtr pin */
	ret = gpio_pin_configure(gpio_dev, MDM_DTR_PIN, GPIO_INPUT);
	if (ret < 0) {
		printk("Error configuring MDM_DTR_PIN: %d", ret);
	}

	/* ap ready pin */
	ret = gpio_pin_configure(gpio_dev, MDM_AP_RDY_PIN, GPIO_INPUT);
	if (ret < 0) {
		printk("Error configuring MDM_AP_RDY_PIN: %d", ret);
	}

	/* psm pin */
	ret = gpio_pin_configure(gpio_dev, MDM_PSM_PIN, GPIO_INPUT);
	if (ret < 0) {
		printk("Error configuring MDM_PSM_PIN: %d", ret);
	}

	/* reset pin */
	ret = gpio_pin_configure(gpio_dev, MDM_RST_PIN, GPIO_OUTPUT_INACTIVE);
	if (ret < 0) {
		printk("Error configuring MDM_RST_PIN: %d", ret);
	}

	/* power pin */
	ret = gpio_pin_configure(gpio_dev, MDM_PWR_PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error configuring MDM_PWR_PIN: %d", ret);
	}

	/* w disable pin */
	ret = gpio_pin_configure(gpio_dev, MDM_W_DISABLE_PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error configuring MDM_W_DISABLE_PIN: %d", ret);
	}

	gpio_pin_set(gpio_dev, MDM_RST_PIN, 0);
	gpio_pin_set(gpio_dev, MDM_PWR_PIN, 1);
	gpio_pin_set(gpio_dev, MDM_W_DISABLE_PIN, 1);

	return gpio_dev;
}

static void shtc3_sensor_read(const struct device *shtc3_dev) {
	int ret;
	struct sensor_value shtc3_sv;

	ret = sensor_sample_fetch(shtc3_dev);
	if (ret != 0) {
		printk("sensor_sample_fetch error: %d\n", ret);
	}

	ret = sensor_channel_get(shtc3_dev, SENSOR_CHAN_AMBIENT_TEMP, &shtc3_sv);
	if (ret != 0) {
		printk("sensor_channel_get error: %d\n", ret);
	}

	/* printk("Temperature: %f C\n", sensor_value_to_double(&shtc3_sv)); */
	printk("Temperature: %d.%06d C\n", shtc3_sv.val1, shtc3_sv.val2);

	ret = sensor_channel_get(shtc3_dev, SENSOR_CHAN_HUMIDITY, &shtc3_sv);
	if (ret != 0) {
		printk("sensor_channel_get error: %d\n", ret);
	}

	/* printk("Humidity: %f C\n", sensor_value_to_double(&shtc3_sv)); */
	printk("Humidity: %d.%06d C\n", shtc3_sv.val1, shtc3_sv.val2);
}

void main(void)
{
	const struct device *led_dev = init_led();
	const struct device *uart_dev = init_uart();
	const struct device *gpio0_dev = init_gpio0();
	const struct device *shtc3_dev = init_shtc3();
	init_gpio1();
	ring_buf_init(&recv_ringbuff, sizeof(recv_buffer), recv_buffer);
	ring_buf_init(&send_ringbuff, sizeof(send_buffer), send_buffer);

	if (led_dev == NULL) {
		printk("Led not found, stopping...\n");
		return;
	}

	if (shtc3_dev == NULL) {
		printk("SHTC3 sensor not found, stopping...\n");
		return;
	}

	if (uart_dev == NULL) {
		printk("UART0 not found, stopping...\n");
		return;
	}

	if (gpio0_dev == NULL) {
		printk("GPIO_0 not found, stopping...\n");
		return;
	}

	printk("Devices initialized!\n");
	gpio_pin_set(led_dev, PIN, 0);

	k_msleep(SLEEP_TIME_MS*2);

	int sent = ring_buf_put(&send_ringbuff, (uint8_t*)"AT+GMR", 7*sizeof(uint8_t));
	printk("Wrote %d bytes\n", sent);

	uart_irq_tx_enable(uart_dev);

	/* printk("Led set, begin main loop\n"); */
	/* while (true) { */
	/* 	shtc3_sensor_read(shtc3_dev); */

	/* 	k_msleep(SLEEP_TIME_MS); */
	/* } */
}
