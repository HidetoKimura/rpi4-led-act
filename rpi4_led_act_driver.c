
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/interrupt.h> 		
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>


struct led_dev
{
	struct miscdevice led_misc_device;
	u32 led_mask; /* different mask if led is R,G or B */
	const char *led_name; 
	char led_value[8];
};

#define BCM2710_PERI_BASE		0xfe000000
#define GPIO_BASE			(BCM2710_PERI_BASE + 0x200000)	/* GPIO controller: FE200000 */

#define GPIO_42			42
//#define GPIO_27			27
//#define GPIO_22			22
//#define GPIO_26			26

/* to set and clear each individual LED */
#define GPIO_42_INDEX 	1 << (GPIO_42 % 32)
//#define GPIO_27_INDEX 	1 << (GPIO_27 % 32)
//#define GPIO_22_INDEX 	1 << (GPIO_22 % 32)
//#define GPIO_26_INDEX 	1 << (GPIO_26 % 32)

/* select the output function */
#define GPIO_42_FUNC	1 << ((GPIO_42 % 10) * 3)
//#define GPIO_27_FUNC	1 << ((GPIO_27 % 10) * 3)
//#define GPIO_22_FUNC 	1 << ((GPIO_22 % 10) * 3)
//#define GPIO_26_FUNC 	1 << ((GPIO_26 % 10) * 3)

/* mask the GPIO functions */
#define FSEL_42_MASK 	0b111 << ((GPIO_42 % 10) * 3) /* red since bit 21 (FSEL27) */
//#define FSEL_27_MASK 	0b111 << ((GPIO_27 % 10) * 3) /* red since bit 21 (FSEL27) */
//#define FSEL_22_MASK 	0b111 << ((GPIO_22 % 10) * 3) /* green since bit 6 (FSEL22) */
//#define FSEL_26_MASK 	0b111 << ((GPIO_26 % 10) * 3) /* blue  a partir del bit 18 (FSEL26) */

//#define GPIO_SET_FUNCTION_LEDS (GPIO_27_FUNC | GPIO_22_FUNC | GPIO_26_FUNC)
#define GPIO_MASK_ALL_LEDS 	FSEL_42_MASK
#define GPIO_SET_ALL_LEDS   GPIO_42_INDEX

#define GPFSEL4 	 GPIO_BASE + 0x10
#define GPSET1   	 GPIO_BASE + 0x20
#define GPCLR1 		 GPIO_BASE + 0x2C

/* Declare __iomem pointers that will keep virtual addresses */
static void __iomem *GPFSEL4_V;
static void __iomem *GPSET1_V;
static void __iomem *GPCLR1_V;

static char *HELLO_KEYS_NAME = "PB_KEY";

/* interrupt handler */
static irqreturn_t hello_keys_isr(int irq, void *data)
{
	struct device *dev = data;
	dev_info(dev, "interrupt received. key: %s\n", HELLO_KEYS_NAME);
	return IRQ_HANDLED;
}

static ssize_t led_write(struct file *file, const char __user *buff, size_t count, loff_t *ppos)
{
	const char *led_on = "on";
	const char *led_off = "off";
	struct led_dev *led_device;

	pr_info("led_write() is called.\n");

	led_device = container_of(file->private_data,
				  struct led_dev, led_misc_device);

	/* 
         * terminal echo add \n character.
	 * led_device->led_value = "on\n" or "off\n after copy_from_user"
	 */
	if(copy_from_user(led_device->led_value, buff, count)) {
		pr_info("Bad copied value\n");
		return -EFAULT;
	}

	/* 
	 * we add \0 replacing \n in led_device->led_value in
	 * probe() initialization.
	 */
	led_device->led_value[count-1] = '\0';

	pr_info("This message received from User Space: %s", led_device->led_value);
	
	if(!strcmp(led_device->led_value, led_on)) {
		iowrite32(led_device->led_mask, GPSET1_V);
	}
	else if (!strcmp(led_device->led_value, led_off)) {
		iowrite32(led_device->led_mask, GPCLR1_V);
	}
	else {
		pr_info("Bad value\n");
		return -EINVAL;
	}

	pr_info("led_write() is exit.\n");
	return count;
}

static ssize_t led_read(struct file *file, char __user *buff, size_t count, loff_t *ppos)
{

	struct led_dev *led_device;

	pr_info("led_read() is called.\n");

	led_device = container_of(file->private_data, struct led_dev, led_misc_device);

	if(*ppos == 0){
		if(copy_to_user(buff, &led_device->led_value, sizeof(led_device->led_value))){
		pr_info("Failed to return led_value to user space\n");
				return -EFAULT;
		}
		*ppos+=1;
		return sizeof(led_device->led_value);
	}

	pr_info("led_read() is exit.\n");

	return 0;
}

static const struct file_operations led_fops = {
	.owner = THIS_MODULE,
	.read = led_read,
	.write = led_write,
};

static int led_probe(struct platform_device *pdev)
{

	struct led_dev *led_device;
	int ret_val;
	char led_val[8] = "off\n";

	int irq;
	struct gpio_desc *gpio;
	struct device *dev = &pdev->dev;

	pr_info("leds_probe enter\n");
	
	/* First method to get the Linux IRQ number */
	gpio = devm_gpiod_get(dev, NULL, GPIOD_IN);
	if (IS_ERR(gpio)) {
		dev_err(dev, "gpio get failed\n");
		return PTR_ERR(gpio);
	}

	irq = gpiod_to_irq(gpio);
	if (irq < 0)
		return irq;
	dev_info(dev, "The IRQ number is: %d\n", irq);

	/* Second method to get the Linux IRQ number */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0){
		dev_err(dev, "irq is not available\n");
		return -EINVAL;
	}
	dev_info(dev, "IRQ_using_platform_get_irq: %d\n", irq);
	
	/* Allocate the interrupt line */
	ret_val = devm_request_irq(dev, irq, hello_keys_isr, 
				   IRQF_TRIGGER_FALLING,
				   HELLO_KEYS_NAME, dev);
	if (ret_val) {
		dev_err(dev, "Failed to request interrupt %d, error %d\n", irq, ret_val);
		return ret_val;
	}
		
	led_device = devm_kzalloc(&pdev->dev, sizeof(struct led_dev), GFP_KERNEL);

//	of_property_read_string(pdev->dev.of_node, "label", &led_device->led_name);
	led_device->led_name = "ledact";
	led_device->led_misc_device.minor = MISC_DYNAMIC_MINOR;
	led_device->led_misc_device.name = led_device->led_name;
	led_device->led_misc_device.fops = &led_fops;

	led_device->led_mask = GPIO_42_INDEX;

#if 0
	if (strcmp(led_device->led_name,"act") == 0) {
		led_device->led_mask = GPIO_42_INDEX;
	}
	else {
		pr_info("Bad device tree value\n");
		return -EINVAL;
	}
#endif

	/* Initialize the led status to off */
	memcpy(led_device->led_value, led_val, sizeof(led_val));

	ret_val = misc_register(&led_device->led_misc_device);
	if (ret_val) return ret_val; /* misc_register returns 0 if success */

	platform_set_drvdata(pdev, led_device);

	pr_info("leds_probe exit\n");

	return 0;
}

static int led_remove(struct platform_device *pdev)
{
	struct led_dev *led_device = platform_get_drvdata(pdev);

	pr_info("leds_remove enter\n");

	misc_deregister(&led_device->led_misc_device);

	pr_info("leds_remove exit\n");

	return 0;
}

static const struct of_device_id my_of_ids[] = {
	{ .compatible = "arrow,ledact"},
	{},
};

MODULE_DEVICE_TABLE(of, my_of_ids);

static struct platform_driver led_platform_driver = {
	.probe = led_probe,
	.remove = led_remove,
	.driver = {
		.name = "ledact",
		.of_match_table = my_of_ids,
		.owner = THIS_MODULE,
	}
};

static int led_init(void)
{
	int ret_val;
	u32 GPFSEL_read, GPFSEL_write;
	pr_info("demo_init enter\n");

	ret_val = platform_driver_register(&led_platform_driver);
	if (ret_val !=0)
	{
		pr_err("platform value returned %d\n", ret_val);
		return ret_val;

	}

	GPFSEL4_V = ioremap(GPFSEL4, sizeof(u32));
	GPSET1_V =  ioremap(GPSET1, sizeof(u32));
	GPCLR1_V =  ioremap(GPCLR1, sizeof(u32));

	GPFSEL_read = ioread32(GPFSEL4_V); /* read current value */

	/* 
	 * set to 0 3 bits of each FSEL and keep equal the rest of bits,
	 * then set to 1 the first bit of each FSEL (function) to set 3 GPIOS to output
	 */
	GPFSEL_write = (GPFSEL_read & ~GPIO_MASK_ALL_LEDS) |
				  (GPIO_42_FUNC & FSEL_42_MASK);

	iowrite32(GPFSEL_write, GPFSEL4_V); /* set leds to output */
	iowrite32(GPIO_SET_ALL_LEDS, GPCLR1_V); /* Clear all the leds, output is low */

	pr_info("demo_init exit\n");
	return 0;
}

static void led_exit(void)
{
	pr_info("led driver enter\n");

	iowrite32(GPIO_SET_ALL_LEDS, GPCLR1_V); /* Clear all the leds */

	iounmap(GPFSEL4_V);
	iounmap(GPSET1_V);
	iounmap(GPCLR1_V);

	platform_driver_unregister(&led_platform_driver);

	pr_info("led driver exit\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Alberto Liberal <aliberal@arroweurope.com>");
MODULE_DESCRIPTION("This is a platform driver that turns on/off \
					three led devices");

