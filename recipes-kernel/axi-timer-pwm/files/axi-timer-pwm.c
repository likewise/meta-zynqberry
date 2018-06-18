#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/io.h>

#define DEVICE_NAME "axitimer"

/* Address definitions */
#define AXI_TIMER_BASE_ADDR 0x40000000
#define AXI_TIMER_HIGH_ADDR (AXI_TIMER_BASE_ADDR + 0xFFFF)

#define AXI_TIMER_TCSR0_OFFSET  0x00
#define AXI_TIMER_TLR0_OFFSET   0x04
#define AXI_TIMER_TCR0_OFFSET   0x08
#define AXI_TIMER_TCSR1_OFFSET  0x10
#define AXI_TIMER_TLR1_OFFSET   0x14
#define AXI_TIMER_TCR1_OFFSET   0x18

/* Bit masks for CSR registers */
#define AXI_TIMER_CSR_CASC          (1 << 11)
#define AXI_TIMER_CSR_ENALL         (1 << 10)
#define AXI_TIMER_CSR_PWMA          (1 << 9)
#define AXI_TIMER_CSR_TINT          (1 << 8)
#define AXI_TIMER_CSR_ENT           (1 << 7)
#define AXI_TIMER_CSR_ENIT          (1 << 6)
#define AXI_TIMER_CSR_LOAD          (1 << 5)
#define AXI_TIMER_CSR_ARHT          (1 << 4)
#define AXI_TIMER_CSR_CAPT          (1 << 3)
#define AXI_TIMER_CSR_GENT          (1 << 2)
#define AXI_TIMER_CSR_UDT           (1 << 1)
#define AXI_TIMER_CSR_MDT           (1 << 0)

#define PWM_CONFIG                          (AXI_TIMER_CSR_PWMA | AXI_TIMER_CSR_GENT | AXI_TIMER_CSR_UDT)
#define TMR0_RELOAD                         20000
#define TMR1_RELOAD                         10000

void *timer_virt_addr;
static struct platform_device *pdev;

int timer_irq;

uint32_t pwm = 0;
int8_t step = 4;

static irqreturn_t axitimer_isr(int irq, void *dev_id)
{
    uint32_t reg0;

    /* Set duty cycle */
    pwm += step;
    if(pwm >= TMR0_RELOAD || pwm == 0)
    {
        step = -step;
    }

    /* Load new value into timer1 */
    iowrite32(pwm, timer_virt_addr + AXI_TIMER_TLR1_OFFSET);

    /* Clear interrupt flag */
    reg0 = ioread32(timer_virt_addr + AXI_TIMER_TCSR0_OFFSET) | AXI_TIMER_CSR_TINT;
    iowrite32(reg0, timer_virt_addr + AXI_TIMER_TCSR0_OFFSET);

    return IRQ_HANDLED;
}

static int __init axitimer_init(void)
{
    uint32_t temp0;
    uint8_t count = 0;
    unsigned long mask;

    printk(KERN_INFO "Initializing AXI Timer module...\n");

    timer_virt_addr = ioremap_nocache(AXI_TIMER_BASE_ADDR, AXI_TIMER_HIGH_ADDR - AXI_TIMER_BASE_ADDR + 1);

    /* Configure timers for PWM usage */
    iowrite32(PWM_CONFIG | AXI_TIMER_CSR_ENIT, timer_virt_addr + AXI_TIMER_TCSR0_OFFSET);       // Also enable interrupts for timer 0 only
    iowrite32(PWM_CONFIG, timer_virt_addr + AXI_TIMER_TCSR1_OFFSET);

    /* Set starting values */
    iowrite32(TMR0_RELOAD, timer_virt_addr + AXI_TIMER_TLR0_OFFSET);
    iowrite32(TMR1_RELOAD, timer_virt_addr + AXI_TIMER_TLR1_OFFSET);

    // Set up and try to probe for interrupts
    do
    {
        mask = probe_irq_on();

        /* Turn on timer 0 to generate an interrupt */
        temp0 = ioread32(timer_virt_addr + AXI_TIMER_TCSR0_OFFSET) | AXI_TIMER_CSR_ENT;
        iowrite32(temp0, timer_virt_addr + AXI_TIMER_TCSR0_OFFSET);

        mdelay(1);      // Delay and hopefully get an interrupt

        /* Turn off timer */
        temp0 = ioread32(timer_virt_addr + AXI_TIMER_TCSR0_OFFSET) & ~(AXI_TIMER_CSR_ENT);
        iowrite32(temp0, timer_virt_addr + AXI_TIMER_TCSR0_OFFSET);

        timer_irq = probe_irq_off(mask);

        if(timer_irq == 0)
        {
            printk(KERN_INFO "axitimer_init: No IRQ reported by probe.\n");
        }

    } while(timer_irq < 0 && count++ < 5);

    if(timer_irq == 0)
    {
        printk(KERN_ERR "axitimer_init: IRQ probe failed.\n");
        return -EIO;
    }

    /* Request IRQ */
    if(request_irq(timer_irq, axitimer_isr, 0, DEVICE_NAME, NULL))
    {
        printk(KERN_ERR "axitimer_init: Cannot register IRQ %d\n", timer_irq);
        return -EIO;
    }

    /* Register device */
    pdev = platform_device_register_simple(DEVICE_NAME, 0, NULL, 0);
    if(pdev == NULL)
    {
        printk(KERN_WARNING "axitimer_init: Adding platform device failed.\n");
        kfree(pdev);
        return -ENODEV;
    }

    /* Start timers */
    temp0 = ioread32(timer_virt_addr + AXI_TIMER_TCSR0_OFFSET) | AXI_TIMER_CSR_ENALL;       // Use ENALL to start both timers simultaneously
    iowrite32(temp0, timer_virt_addr + AXI_TIMER_TCSR0_OFFSET);

    printk(KERN_INFO "AXI Timer configured.\n");
    return 0;

}

static void __exit axitimer_exit(void)
{
    iounmap(timer_virt_addr);
    free_irq(timer_irq, NULL);
    // Unregister device
    platform_device_unregister(pdev);
    printk(KERN_INFO "AXI Timer module removed.\n");
}

module_init(axitimer_init);
module_exit(axitimer_exit);

MODULE_AUTHOR("TR");
MODULE_DESCRIPTION("Example driver for Xilinx AXI Timer.");
MODULE_LICENSE("GPL");
