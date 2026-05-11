#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/unistd.h>
#include <linux/kdev_t.h>
#include <linux/cdev.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#define GPIO1 (24)
#define GPIO2 (27)

static dev_t my_dev;
static struct cdev my_cdev;

unsigned int led_state = 0;
int irq_num = 0;
int irqNum[2];

// 함수 원형 선언
int device_open(struct inode *inode, struct file *filp);
int device_release(struct inode *inode, struct file *filp);
ssize_t device_read(struct file *filp, char *buf, size_t count, loff_t *f_pos);
ssize_t device_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos);

// File Operations 구조체
static struct file_operations fops = {
    .read = device_read,
    .write = device_write,
    .open = device_open,
    .release = device_release,
};

// 인터럽트 핸들러
static irqreturn_t switch_irq_handler(int irq, void *dev_id) {
    irq_num = irq;
    static unsigned long flags = 0;

    local_irq_save(flags);

    led_state ^= 1;
    gpio_set_value(GPIO1, led_state);

    local_irq_restore(flags);

    return IRQ_HANDLED;
}

// 장치 열기 (Open)
int device_open(struct inode *inode, struct file *filp) {
    int err;
    int id = GPIO1;

    // LED GPIO 설정
    err = gpio_request(id, "LED");
    if (err) printk("LED failed to request GPIO %d.\n", id);
    gpio_direction_output(GPIO1, 0);

    // Switch GPIO 설정
    id = GPIO2;
    err = gpio_request(id, "Switch");
    if (err) printk("Switch failed to request GPIO %d.\n", id);
    gpio_direction_input(GPIO2);

    err = gpio_set_debounce(GPIO2, 200);
    if (err) printk("Switch failed to set debounce GPIO %d.\n", id);

    // 인터럽트 설정
    irqNum[0] = gpio_to_irq(GPIO2);
    err = request_irq(irqNum[0], (irq_handler_t)switch_irq_handler, IRQF_TRIGGER_RISING, "my_led_dev", NULL);
    if (err) printk("Switch failed to request irq %d.\n", id);

    printk(KERN_INFO "GPIO LED & switch open\n");

    return 0;
}

// 장치 닫기 (Release)
int device_release(struct inode *inode, struct file *filp) {
    printk(KERN_INFO "GPIO LED release\n");
    free_irq(irqNum[0], NULL);
    gpio_free(GPIO1);
    gpio_free(GPIO2);
    return 0;
}

// 장치 쓰기 (Write)
ssize_t device_write(struct file *filp, const char *buf, size_t count, loff_t *f_pos) {
    unsigned char cbuf[10] = {0,};
    copy_from_user(cbuf, buf, count);

    printk("Write(): GPIO1 to %d.\n", cbuf[0]);

    if (cbuf[0] == 1)
        gpio_set_value(GPIO1, 1); // HIGH
    else
        gpio_set_value(GPIO1, 0); // LOW

    return count;
}

// 장치 읽기 (Read)
ssize_t device_read(struct file *filp, char *buf, size_t count, loff_t *fpos) {
    unsigned char switch_state = 0;

    switch_state = gpio_get_value(GPIO2);
    copy_to_user(buf, &switch_state, 1);

    printk("Read(): GPIO2 to %d.\n", switch_state);

    return count;
}

// 모듈 초기화 (Init)
int device_init(void) {
    if (alloc_chrdev_region(&my_dev, 0, 1, "my_led_dev")) {
        printk(KERN_ALERT "[%s] alloc_chrdev_region failed\n", __func__);
        return -1;
    }
    printk("LED Major = %d, Minor = %d\n", MAJOR(my_dev), MINOR(my_dev));

    cdev_init(&my_cdev, &fops);
    if (cdev_add(&my_cdev, my_dev, 1)) {
        printk(KERN_ALERT "[%s] cdev_add failed\n", __func__);
        return -1;
    }

    if (gpio_is_valid(GPIO1) == false) {
        printk(KERN_ALERT "[%s] GPIO is not valid.\n", __func__);
        return -1;
    }
    if (gpio_is_valid(GPIO2) == false) {
        printk(KERN_ALERT "[%s] GPIO is not valid.\n", __func__);
        return -1;
    }

    return 0;
}

// 모듈 종료 (Exit)
void device_exit(void) {                                                                                                
    cdev_del(&my_cdev);
    unregister_chrdev_region(my_dev, 1);
}

module_init(device_init);
module_exit(device_exit);
MODULE_LICENSE("GPL");